// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {
namespace {

const char kSessionStorageDirectory[] = "Session Storage";

void GotMojoCallback(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    base::OnceClosure callback) {
  reply_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void GotMojoLocalStorageUsage(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    DOMStorageContext::GetLocalStorageUsageCallback callback,
    std::vector<StorageUsageInfo> usage) {
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(usage)));
}

void GotMojoSessionStorageUsage(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    DOMStorageContext::GetSessionStorageUsageCallback callback,
    std::vector<SessionStorageUsageInfo> usage) {
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(usage)));
}

}  // namespace

scoped_refptr<DOMStorageContextWrapper> DOMStorageContextWrapper::Create(
    const base::FilePath& profile_path,
    const base::FilePath& local_partition_path,
    storage::SpecialStoragePolicy* special_storage_policy) {
  base::FilePath data_path;
  if (!profile_path.empty())
    data_path = profile_path.Append(local_partition_path);

  auto mojo_task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});

  LocalStorageContextMojo* mojo_local_state = new LocalStorageContextMojo(
      data_path, mojo_task_runner,
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      special_storage_policy);
  SessionStorageContextMojo* mojo_session_state = nullptr;
  mojo_session_state = new SessionStorageContextMojo(
      data_path,
      base::CreateSequencedTaskRunner(
          {base::MayBlock(), base::ThreadPool(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      mojo_task_runner,
#if defined(OS_ANDROID)
      // On Android there is no support for session storage restoring, and
      // since the restoring code is responsible for database cleanup, we must
      // manually delete the old database here before we open it.
      SessionStorageContextMojo::BackingMode::kClearDiskStateOnOpen,
#else
      profile_path.empty()
          ? SessionStorageContextMojo::BackingMode::kNoDisk
          : SessionStorageContextMojo::BackingMode::kRestoreDiskState,
#endif
      std::string(kSessionStorageDirectory));

  return base::WrapRefCounted(new DOMStorageContextWrapper(
      mojo_task_runner, mojo_local_state, mojo_session_state));
}

DOMStorageContextWrapper::DOMStorageContextWrapper(
    scoped_refptr<base::SequencedTaskRunner> mojo_task_runner,
    LocalStorageContextMojo* mojo_local_storage_context,
    SessionStorageContextMojo* mojo_session_storage_context)
    : mojo_state_(mojo_local_storage_context),
      mojo_session_state_(mojo_session_storage_context),
      mojo_task_runner_(std::move(mojo_task_runner)) {
  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::BindRepeating(&DOMStorageContextWrapper::OnMemoryPressure,
                          base::Unretained(this))));
}

DOMStorageContextWrapper::~DOMStorageContextWrapper() {
  DCHECK(!mojo_state_) << "Shutdown should be called before destruction";
  DCHECK(!mojo_session_state_)
      << "Shutdown should be called before destruction";
}

void DOMStorageContextWrapper::GetLocalStorageUsage(
    GetLocalStorageUsageCallback callback) {
  if (!mojo_state_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<StorageUsageInfo>());
    return;
  }

  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalStorageContextMojo::GetStorageUsage,
                     base::Unretained(mojo_state_),
                     base::BindOnce(&GotMojoLocalStorageUsage,
                                    base::ThreadTaskRunnerHandle::Get(),
                                    std::move(callback))));
}

void DOMStorageContextWrapper::GetSessionStorageUsage(
    GetSessionStorageUsageCallback callback) {
  if (!mojo_session_state_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<SessionStorageUsageInfo>());
    return;
  }
  // base::Unretained is safe here, because the mojo_session_state_ won't be
  // deleted until a ShutdownAndDelete task has been ran on the
  // mojo_task_runner_, and as soon as that task is posted,
  // mojo_session_state_ is set to null, preventing further tasks from being
  // queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionStorageContextMojo::GetStorageUsage,
                     base::Unretained(mojo_session_state_),
                     base::BindOnce(&GotMojoSessionStorageUsage,
                                    base::ThreadTaskRunnerHandle::Get(),
                                    std::move(callback))));
}

void DOMStorageContextWrapper::DeleteLocalStorage(const url::Origin& origin,
                                                  base::OnceClosure callback) {
  DCHECK(callback);
  if (!mojo_state_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LocalStorageContextMojo::DeleteStorage,
          base::Unretained(mojo_state_), origin,
          base::BindOnce(&GotMojoCallback, base::ThreadTaskRunnerHandle::Get(),
                         std::move(callback))));
}

void DOMStorageContextWrapper::PerformLocalStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!mojo_state_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LocalStorageContextMojo::PerformStorageCleanup,
          base::Unretained(mojo_state_),
          base::BindOnce(&GotMojoCallback, base::ThreadTaskRunnerHandle::Get(),
                         std::move(callback))));
}

void DOMStorageContextWrapper::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info,
    base::OnceClosure callback) {
  if (!mojo_session_state_) {
    // Shutdown() has been called.
    std::move(callback).Run();
  }
  // base::Unretained is safe here, because the mojo_session_state_ won't be
  // deleted until a ShutdownAndDelete task has been ran on the
  // mojo_task_runner_, and as soon as that task is posted,
  // mojo_session_state_ is set to null, preventing further tasks from being
  // queued.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SessionStorageContextMojo::DeleteStorage,
                                base::Unretained(mojo_session_state_),
                                url::Origin::Create(usage_info.origin),
                                usage_info.namespace_id, std::move(callback)));
}

void DOMStorageContextWrapper::PerformSessionStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!mojo_session_state_) {
    // Shutdown() has been called.
    std::move(callback).Run();
  }
  // base::Unretained is safe here, because the mojo_session_state_ won't be
  // deleted until a ShutdownAndDelete task has been ran on the
  // mojo_task_runner_, and as soon as that task is posted,
  // mojo_session_state_ is set to null, preventing further tasks from being
  // queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionStorageContextMojo::PerformStorageCleanup,
                     base::Unretained(mojo_session_state_),
                     std::move(callback)));
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextWrapper::RecreateSessionStorage(
    const std::string& namespace_id) {
  return SessionStorageNamespaceImpl::Create(this, namespace_id);
}

void DOMStorageContextWrapper::StartScavengingUnusedSessionStorage() {
  if (!mojo_session_state_) {
    // Shutdown() has been called.
    return;
  }
  // base::Unretained is safe here, because the mojo_session_state_ won't be
  // deleted until a ShutdownAndDelete task has been ran on the
  // mojo_task_runner_, and as soon as that task is posted,
  // mojo_session_state_ is set to null, preventing further tasks from being
  // queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionStorageContextMojo::ScavengeUnusedNamespaces,
                     base::Unretained(mojo_session_state_),
                     base::OnceClosure()));
}

void DOMStorageContextWrapper::SetForceKeepSessionState() {
  if (!mojo_session_state_) {
    // Shutdown() has been called.
    return;
  }

  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalStorageContextMojo::SetForceKeepSessionState,
                     base::Unretained(mojo_state_)));
}

void DOMStorageContextWrapper::Shutdown() {
  if (mojo_state_) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::ShutdownAndDelete,
                                  base::Unretained(mojo_state_)));
    mojo_state_ = nullptr;
  }
  if (mojo_session_state_) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::ShutdownAndDelete,
                                  base::Unretained(mojo_session_state_)));
    mojo_session_state_ = nullptr;
  }
  memory_pressure_listener_.reset();
}

void DOMStorageContextWrapper::Flush() {
  if (mojo_state_) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&LocalStorageContextMojo::Flush,
                                               base::Unretained(mojo_state_)));
  }
  if (mojo_session_state_) {
    // base::Unretained is safe here, because the mojo_session_state_ won't be
    // deleted until a ShutdownAndDelete task has been ran on the
    // mojo_task_runner_, and as soon as that task is posted,
    // mojo_session_state_ is set to null, preventing further tasks from being
    // queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::Flush,
                                  base::Unretained(mojo_session_state_)));
  }
}

void DOMStorageContextWrapper::OpenLocalStorage(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(mojo_state_);
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalStorageContextMojo::OpenLocalStorage,
                                base::Unretained(mojo_state_), origin,
                                std::move(receiver)));
}

void DOMStorageContextWrapper::OpenSessionStorage(
    int process_id,
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  DCHECK(mojo_session_state_);
  // The bad message callback must be called on the same sequenced task runner
  // as the binding set. It cannot be called from our own mojo task runner.
  auto wrapped_bad_message_callback = base::BindOnce(
      [](mojo::ReportBadMessageCallback bad_message_callback,
         scoped_refptr<base::SequencedTaskRunner> bindings_runner,
         const std::string& error) {
        bindings_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(bad_message_callback), error));
      },
      std::move(bad_message_callback), base::SequencedTaskRunnerHandle::Get());
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionStorageContextMojo::OpenSessionStorage,
                     base::Unretained(mojo_session_state_), process_id,
                     namespace_id, std::move(wrapped_bad_message_callback),
                     std::move(receiver)));
}

void DOMStorageContextWrapper::SetLocalStorageDatabaseOpenCallbackForTesting(
    LocalStorageDatabaseOpenCallback callback) {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LocalStorageContextMojo::SetDatabaseOpenCallbackForTesting,
          base::Unretained(mojo_state_),
          base::BindOnce(std::move(callback), mojo_state_)));
}

scoped_refptr<SessionStorageNamespaceImpl>
DOMStorageContextWrapper::MaybeGetExistingNamespace(
    const std::string& namespace_id) const {
  base::AutoLock lock(alive_namespaces_lock_);
  auto it = alive_namespaces_.find(namespace_id);
  return (it != alive_namespaces_.end()) ? it->second : nullptr;
}

void DOMStorageContextWrapper::AddNamespace(
    const std::string& namespace_id,
    SessionStorageNamespaceImpl* session_namespace) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) == alive_namespaces_.end());
  alive_namespaces_[namespace_id] = session_namespace;
}

void DOMStorageContextWrapper::RemoveNamespace(
    const std::string& namespace_id) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) != alive_namespaces_.end());
  alive_namespaces_.erase(namespace_id);
}

void DOMStorageContextWrapper::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  PurgeOption purge_option = PURGE_UNOPENED;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    purge_option = PURGE_AGGRESSIVE;
  }
  PurgeMemory(purge_option);
}

void DOMStorageContextWrapper::PurgeMemory(PurgeOption purge_option) {
  if (!mojo_state_) {
    // Shutdown was called.
    return;
  }
  if (purge_option == PURGE_AGGRESSIVE) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::PurgeMemory,
                                  base::Unretained(mojo_state_)));
    // base::Unretained is safe here, because the mojo_session_state_ won't be
    // deleted until a ShutdownAndDelete task has been ran on the
    // mojo_task_runner_, and as soon as that task is posted,
    // mojo_session_state_ is set to null, preventing further tasks from being
    // queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::PurgeMemory,
                                  base::Unretained(mojo_session_state_)));
  }
}

}  // namespace content
