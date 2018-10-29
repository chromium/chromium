// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "sql/database.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

const char kLocalStorageDirectory[] = "Local Storage";
const char kSessionStorageDirectory[] = "Session Storage";

void GetLegacyLocalStorageUsage(
    const base::FilePath& directory,
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    DOMStorageContext::GetLocalStorageUsageCallback callback) {
  std::vector<LocalStorageUsageInfo> infos;
  base::FileEnumerator enumerator(directory, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.MatchesExtension(DOMStorageArea::kDatabaseFileExtension)) {
      LocalStorageUsageInfo info;
      info.origin = DOMStorageArea::OriginFromDatabaseFileName(path).GetURL();
      base::FileEnumerator::FileInfo find_info = enumerator.GetInfo();
      info.data_size = find_info.GetSize();
      info.last_modified = find_info.GetLastModifiedTime();
      infos.push_back(info);
    }
  }
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(infos)));
}

void InvokeLocalStorageUsageCallbackHelper(
    DOMStorageContext::GetLocalStorageUsageCallback callback,
    std::unique_ptr<std::vector<LocalStorageUsageInfo>> infos) {
  std::move(callback).Run(*infos);
}

void GetSessionStorageUsageHelper(
    base::SingleThreadTaskRunner* reply_task_runner,
    DOMStorageContextImpl* context,
    DOMStorageContext::GetSessionStorageUsageCallback callback) {
  std::vector<SessionStorageUsageInfo> infos;
  context->GetSessionStorageUsage(&infos);
  reply_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(infos)));
}

void CollectLocalStorageUsage(
    std::vector<LocalStorageUsageInfo>* out_info,
    base::OnceClosure done_callback,
    const std::vector<LocalStorageUsageInfo>& in_info) {
  out_info->insert(out_info->end(), in_info.begin(), in_info.end());
  std::move(done_callback).Run();
}

void GotMojoCallback(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    base::OnceClosure callback) {
  reply_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void GotMojoLocalStorageUsage(
    scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
    DOMStorageContext::GetLocalStorageUsageCallback callback,
    std::vector<LocalStorageUsageInfo> usage) {
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

DOMStorageContextWrapper::DOMStorageContextWrapper(
    service_manager::Connector* connector,
    const base::FilePath& profile_path,
    const base::FilePath& local_partition_path,
    storage::SpecialStoragePolicy* special_storage_policy) {
  base::FilePath data_path;
  if (!profile_path.empty())
    data_path = profile_path.Append(local_partition_path);

  scoped_refptr<base::SequencedTaskRunner> primary_sequence =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  scoped_refptr<base::SequencedTaskRunner> commit_sequence =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  legacy_localstorage_path_ =
      data_path.empty() ? data_path
                        : data_path.AppendASCII(kLocalStorageDirectory);

  context_ = new DOMStorageContextImpl(
      data_path.empty() ? data_path
                        : data_path.AppendASCII(kSessionStorageDirectory),
      special_storage_policy,
      new DOMStorageWorkerPoolTaskRunner(std::move(primary_sequence),
                                         std::move(commit_sequence)));

  base::FilePath storage_dir;
  if (!profile_path.empty())
    storage_dir = local_partition_path.AppendASCII(kLocalStorageDirectory);
  // TODO(dmurph): Change this to a sequenced task runner after
  // https://crbug.com/809255 is fixed.
  mojo_task_runner_ =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO});
  mojo_state_ = new LocalStorageContextMojo(
      mojo_task_runner_, connector, context_->task_runner(),
      legacy_localstorage_path_, storage_dir, special_storage_policy);

  if (base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage)) {
    mojo_session_state_ = new SessionStorageContextMojo(
        mojo_task_runner_, connector,

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
        local_partition_path, std::string(kSessionStorageDirectory));
  }

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
  DCHECK(context_.get());
  auto infos = std::make_unique<std::vector<LocalStorageUsageInfo>>();
  auto* infos_ptr = infos.get();
  base::RepeatingClosure got_local_storage_usage = base::BarrierClosure(
      2, base::BindOnce(&InvokeLocalStorageUsageCallbackHelper,
                        std::move(callback), std::move(infos)));
  auto collect_callback = base::BindRepeating(
      CollectLocalStorageUsage, infos_ptr, std::move(got_local_storage_usage));
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
                                    collect_callback)));
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&GetLegacyLocalStorageUsage, legacy_localstorage_path_,
                     base::ThreadTaskRunnerHandle::Get(),
                     std::move(collect_callback)));
}

void DOMStorageContextWrapper::GetSessionStorageUsage(
    GetSessionStorageUsageCallback callback) {
  if (mojo_session_state_) {
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
    return;
  }
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&GetSessionStorageUsageHelper,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     base::RetainedRef(context_), std::move(callback)));
}

void DOMStorageContextWrapper::DeleteLocalStorage(const GURL& origin,
                                                  base::OnceClosure callback) {
  DCHECK(context_.get());
  DCHECK(callback);
  if (!legacy_localstorage_path_.empty()) {
    context_->task_runner()->PostShutdownBlockingTask(
        FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
        base::BindOnce(base::IgnoreResult(&sql::Database::Delete),
                       legacy_localstorage_path_.Append(
                           DOMStorageArea::DatabaseFileNameFromOrigin(
                               url::Origin::Create(origin)))));
  }
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LocalStorageContextMojo::DeleteStorage,
          base::Unretained(mojo_state_), url::Origin::Create(origin),
          base::BindOnce(&GotMojoCallback, base::ThreadTaskRunnerHandle::Get(),
                         std::move(callback))));
}

void DOMStorageContextWrapper::PerformLocalStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(context_.get());
  DCHECK(callback);
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LocalStorageContextMojo::PerformCleanup,
          base::Unretained(mojo_state_),
          base::BindOnce(&GotMojoCallback, base::ThreadTaskRunnerHandle::Get(),
                         std::move(callback))));
}

void DOMStorageContextWrapper::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info) {
  if (mojo_session_state_) {
    // base::Unretained is safe here, because the mojo_session_state_ won't be
    // deleted until a ShutdownAndDelete task has been ran on the
    // mojo_task_runner_, and as soon as that task is posted,
    // mojo_session_state_ is set to null, preventing further tasks from being
    // queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::DeleteStorage,
                                  base::Unretained(mojo_session_state_),
                                  url::Origin::Create(usage_info.origin),
                                  usage_info.namespace_id));
    return;
  }
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::DeleteSessionStorage, context_,
                     usage_info));
}

void DOMStorageContextWrapper::SetSaveSessionStorageOnDisk() {
  DCHECK(context_.get());
  context_->SetSaveSessionStorageOnDisk();
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextWrapper::RecreateSessionStorage(
    const std::string& namespace_id) {
  return SessionStorageNamespaceImpl::Create(this, namespace_id);
}

void DOMStorageContextWrapper::StartScavengingUnusedSessionStorage() {
  if (mojo_session_state_) {
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
    return;
  }
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(
          &DOMStorageContextImpl::StartScavengingUnusedSessionStorage,
          context_));
}

void DOMStorageContextWrapper::SetForceKeepSessionState() {
  DCHECK(context_.get());
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::SetForceKeepSessionState,
                     context_));
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
  DCHECK(context_.get());
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalStorageContextMojo::ShutdownAndDelete,
                                base::Unretained(mojo_state_)));
  mojo_state_ = nullptr;
  if (mojo_session_state_) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::ShutdownAndDelete,
                                  base::Unretained(mojo_session_state_)));
    mojo_session_state_ = nullptr;
  }
  memory_pressure_listener_.reset();
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::Shutdown, context_));
}

void DOMStorageContextWrapper::Flush() {
  DCHECK(context_.get());

  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::PRIMARY_SEQUENCE,
      base::BindOnce(&DOMStorageContextImpl::Flush, context_));
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&LocalStorageContextMojo::Flush,
                                             base::Unretained(mojo_state_)));
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
    blink::mojom::StorageAreaRequest request) {
  DCHECK(mojo_state_);
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalStorageContextMojo::OpenLocalStorage,
                                base::Unretained(mojo_state_), origin,
                                std::move(request)));
}

void DOMStorageContextWrapper::OpenSessionStorage(
    int process_id,
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::mojom::SessionStorageNamespaceRequest request) {
  if (!mojo_session_state_)
    return;
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SessionStorageContextMojo::OpenSessionStorage,
                     base::Unretained(mojo_session_state_), process_id,
                     namespace_id, std::move(bad_message_callback),
                     std::move(request)));
}

void DOMStorageContextWrapper::SetLocalStorageDatabaseForTesting(
    leveldb::mojom::LevelDBDatabaseAssociatedPtr database) {
  // base::Unretained is safe here, because the mojo_state_ won't be deleted
  // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
  // as soon as that task is posted, mojo_state_ is set to null, preventing
  // further tasks from being queued.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalStorageContextMojo::SetDatabaseForTesting,
                     base::Unretained(mojo_state_), std::move(database)));
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
  DOMStorageContextImpl::PurgeOption purge_option =
      DOMStorageContextImpl::PURGE_UNOPENED;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    purge_option = DOMStorageContextImpl::PURGE_AGGRESSIVE;
  }
  PurgeMemory(purge_option);
}

void DOMStorageContextWrapper::PurgeMemory(DOMStorageContextImpl::PurgeOption
    purge_option) {
  context_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::PurgeMemory, context_,
                                purge_option));
  if (mojo_state_ && purge_option == DOMStorageContextImpl::PURGE_AGGRESSIVE) {
    // base::Unretained is safe here, because the mojo_state_ won't be deleted
    // until a ShutdownAndDelete task has been ran on the mojo_task_runner_, and
    // as soon as that task is posted, mojo_state_ is set to null, preventing
    // further tasks from being queued.
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalStorageContextMojo::PurgeMemory,
                                  base::Unretained(mojo_state_)));
  }

  if (mojo_session_state_ &&
      purge_option == DOMStorageContextImpl::PURGE_AGGRESSIVE) {
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
