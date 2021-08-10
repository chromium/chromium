// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_manager.h"

#include <memory>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/browser/native_io/native_io_quota_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/native_io/native_io_utils.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {

std::vector<url::Origin> DoGetOrigins(const base::FilePath& native_io_root) {
  std::vector<url::Origin> result;
  // If the NativeIO directory wasn't created yet, there's no file to report.
  if (!base::PathExists(native_io_root))
    return result;

  base::FileEnumerator file_enumerator(native_io_root, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // If the directory name has a non-ASCII character, `file_path` will be the
    // empty string. This indicates corruption as any origin creates an
    // ASCII-only directory name, so those directories are ignored.
    std::string directory_name = file_path.BaseName().MaybeAsASCII();
    if (directory_name == "")
      continue;
    url::Origin origin = storage::GetOriginFromIdentifier(directory_name);
    result.push_back(std::move(origin));
  }
  return result;
}

int64_t DoGetOriginUsage(const base::FilePath& origin_root) {
  // Returns 0 if `origin_root` does not exist.
  return base::ComputeDirectorySize(origin_root);
}

std::map<url::Origin, int64_t> DoGetOriginUsageMap(
    const base::FilePath& native_io_root) {
  std::map<url::Origin, int64_t> result;

  // If the NativeIO directory wasn't created yet, there's no file to report.
  if (!base::PathExists(native_io_root))
    return result;

  base::FileEnumerator file_enumerator(native_io_root, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // If the directory name has a non-ASCII character, `file_path` will be the
    // empty string. This indicates corruption as any origin creates an
    // ASCII-only directory name, so those directories are ignored.
    std::string directory_name = file_path.BaseName().MaybeAsASCII();
    if (directory_name == "")
      continue;
    url::Origin origin = storage::GetOriginFromIdentifier(directory_name);
    int64_t usage = base::ComputeDirectorySize(file_path);
    auto inserted = result.insert(std::make_pair(origin, usage));
    DCHECK(inserted.second)
        << "Origins in NativeIO's directory should have a unique folder.";
  }
  return result;
}

constexpr base::FilePath::CharType kNativeIODirectoryName[] =
    FILE_PATH_LITERAL("NativeIO");
}  // namespace

NativeIOManager::NativeIOManager(
    const base::FilePath& profile_root,
#if defined(OS_MAC)
    bool allow_set_length_ipc,
#endif  // defined(OS_MAC)
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : root_path_(GetNativeIORootPath(profile_root)),
#if defined(OS_MAC)
      allow_set_length_ipc_(allow_set_length_ipc),
#endif  // defined(OS_MAC)
      special_storage_policy_(std::move(special_storage_policy)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      // Using a raw pointer is safe since NativeIOManager be owned by
      // NativeIOQuotaClient and is guaranteed to outlive it.
      quota_client_(this),
      quota_client_receiver_(&quota_client_) {
  if (quota_manager_proxy_) {
    // Quota client assumes all backends have registered.
    quota_manager_proxy_->RegisterClient(
        quota_client_receiver_.BindNewPipeAndPassRemote(),
        storage::QuotaClientType::kNativeIO,
        {blink::mojom::StorageType::kTemporary});
  }
}

NativeIOManager::~NativeIOManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOManager::BindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(origin);
  if (it == hosts_.end()) {
    // This feature should only be exposed to potentially trustworthy origins
    // (https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy).
    // Notably this includes the https and chrome-extension schemes, among
    // others.
    if (!network::IsOriginPotentiallyTrustworthy(origin)) {
      std::move(bad_message_callback)
          .Run("Called NativeIO from an insecure context");
      return;
    }

    base::FilePath origin_root_path = RootPathForOrigin(origin);
    DCHECK(origin_root_path.empty() || root_path_.IsParent(origin_root_path))
        << "Per-origin data should be in a sub-directory of NativeIO/ for "
        << "non-incognito mode ";

    bool insert_succeeded;
    std::tie(it, insert_succeeded) = hosts_.emplace(
        origin,
        std::make_unique<NativeIOHost>(origin, std::move(origin_root_path),
#if defined(OS_MAC)
                                       allow_set_length_ipc_,
#endif  // defined(OS_MAC)
                                       this));
    DCHECK(insert_succeeded);
  }

  it->second->BindReceiver(std::move(receiver));
}

void NativeIOManager::OnHostReceiverDisconnect(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeDeleteHost(host);
}

void NativeIOManager::DidDeleteHostData(NativeIOHost* host,
                                        base::PassKey<NativeIOHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK(!host->delete_all_data_in_progress());

  MaybeDeleteHost(host);
}

void NativeIOManager::MaybeDeleteHost(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK(hosts_.count(host->origin()) > 0);
  DCHECK_EQ(hosts_[host->origin()].get(), host);

  if (!host->has_empty_receiver_set() || host->delete_all_data_in_progress())
    return;

  hosts_.erase(host->origin());
}

void NativeIOManager::DeleteOriginData(
    const url::Origin& origin,
    storage::QuotaClient::DeleteOriginDataCallback callback) {
  auto it = hosts_.find(origin);
  if (it == hosts_.end()) {
    // TODO(rstz): Consider turning these checks into DCHECKS when NativeIO is
    // no longer bundled with the Filesystem API during data removal.
    if (!network::IsOriginPotentiallyTrustworthy(origin)) {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }
    base::FilePath origin_root_path = RootPathForOrigin(origin);
    if (origin_root_path.empty()) {
      // NativeIO is not supported for the origin, no data can be deleted.
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }

    DCHECK(root_path_.IsParent(origin_root_path))
        << "Per-origin data should be in a sub-directory of NativeIO/";

    bool insert_succeeded;
    // Create a NativeIOHost so that future API calls for the origin are queued
    // behind the data deletion. This should not meaningfully slow down the
    // removal process.
    std::tie(it, insert_succeeded) = hosts_.emplace(
        origin,
        std::make_unique<NativeIOHost>(origin, std::move(origin_root_path),
#if defined(OS_MAC)
                                       allow_set_length_ipc_,
#endif  // defined(OS_MAC)
                                       this));
    DCHECK(insert_succeeded);
  }

  // DeleteAllData() will call DidDeleteHostData() asynchronously, which may
  // delete this entry from `hosts_`.
  it->second->DeleteAllData(base::BindOnce(
      [](storage::mojom::QuotaClient::DeleteOriginDataCallback callback,
         base::File::Error error) {
        std::move(callback).Run((error == base::File::FILE_OK)
                                    ? blink::mojom::QuotaStatusCode::kOk
                                    : blink::mojom::QuotaStatusCode::kUnknown);
      },
      std::move(callback)));
}

void NativeIOManager::GetOriginsForType(
    blink::mojom::StorageType type,
    storage::QuotaClient::GetOriginsForTypeCallback callback) {
  if (type != blink::mojom::StorageType::kTemporary) {
    std::move(callback).Run({});
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          // Needed for file I/O.
          base::MayBlock(),

          // Reasonable compromise, given that the sitedata UI depends on this
          // functionality.
          base::TaskPriority::USER_VISIBLE,

          // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to
          // move to CONTINUE_ON_SHUTDOWN after very careful analysis.
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      },
      base::BindOnce(&DoGetOrigins, root_path_),
      base::BindOnce(&NativeIOManager::DidGetOriginsForType,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}
void NativeIOManager::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    storage::QuotaClient::GetOriginsForHostCallback callback) {
  if (type != blink::mojom::StorageType::kTemporary) {
    std::move(callback).Run({});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          // Needed for file I/O.
          base::MayBlock(),

          // Reasonable compromise, given that the sitedata UI depends on this
          // functionality.
          base::TaskPriority::USER_VISIBLE,

          // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to
          // move to CONTINUE_ON_SHUTDOWN after very careful analysis.
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      },
      base::BindOnce(&DoGetOrigins, root_path_),
      base::BindOnce(&NativeIOManager::DidGetOriginsForHost,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(host)));
}

void NativeIOManager::GetOriginUsage(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    storage::QuotaClient::GetOriginUsageCallback callback) {
  if (type != blink::mojom::StorageType::kTemporary) {
    std::move(callback).Run(0);
    return;
  }

  base::FilePath origin_root = RootPathForOrigin(origin);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          // Needed for file I/O.
          base::MayBlock(),

          // Reasonable compromise, given that the sitedata UI depends on this
          // functionality.
          base::TaskPriority::USER_VISIBLE,

          // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to
          // move to CONTINUE_ON_SHUTDOWN after very careful analysis.
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      },
      base::BindOnce(&DoGetOriginUsage, origin_root),
      base::BindOnce(&NativeIOManager::DidGetOriginUsage,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeIOManager::GetOriginUsageMap(
    base::OnceCallback<void(const std::map<url::Origin, int64_t>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {
          // Needed for file I/O.
          base::MayBlock(),

          // Site data removal has a visible UI.
          base::TaskPriority::USER_VISIBLE,

          // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to
          // move to CONTINUE_ON_SHUTDOWN after very careful analysis.
          base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
      },
      base::BindOnce(&DoGetOriginUsageMap, root_path_),
      base::BindOnce(&NativeIOManager::DidGetOriginUsageMap,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeIOManager::DidGetOriginsForType(
    storage::QuotaClient::GetOriginsForTypeCallback callback,
    std::vector<url::Origin> origins) {
  std::move(callback).Run(origins);
}

void NativeIOManager::DidGetOriginsForHost(
    storage::QuotaClient::GetOriginsForTypeCallback callback,
    const std::string& host,
    std::vector<url::Origin> origins) {
  std::vector<url::Origin> out_origins;
  for (const url::Origin& origin : origins) {
    if (host == origin.host())
      out_origins.push_back(origin);
  }
  std::move(callback).Run(std::move(out_origins));
}

void NativeIOManager::DidGetOriginUsage(
    storage::QuotaClient::GetOriginUsageCallback callback,
    int64_t usage) {
  std::move(callback).Run(usage);
}

void NativeIOManager::DidGetOriginUsageMap(
    base::OnceCallback<void(const std::map<url::Origin, int64_t>)> callback,
    std::map<url::Origin, int64_t> usage_map) {
  std::move(callback).Run(usage_map);
}

base::FilePath NativeIOManager::RootPathForOrigin(const url::Origin& origin) {
  // TODO(pwnall): Implement in-memory files instead of bouncing in incognito.
  if (root_path_.empty())
    return root_path_;

  std::string origin_identifier = storage::GetIdentifierFromOrigin(origin);
  base::FilePath origin_path = root_path_.AppendASCII(origin_identifier);
  DCHECK(root_path_.IsParent(origin_path));
  return origin_path;
}

// static
base::FilePath NativeIOManager::GetNativeIORootPath(
    const base::FilePath& profile_root) {
  if (profile_root.empty())
    return base::FilePath();

  return profile_root.Append(kNativeIODirectoryName);
}

// static
blink::mojom::NativeIOErrorPtr NativeIOManager::FileErrorToNativeIOError(
    base::File::Error file_error,
    std::string message) {
  blink::mojom::NativeIOErrorType native_io_error_type =
      blink::native_io::FileErrorToNativeIOErrorType(file_error);
  std::string final_message =
      message.empty()
          ? blink::native_io::GetDefaultMessage(native_io_error_type)
          : message;
  return blink::mojom::NativeIOError::New(native_io_error_type, final_message);
}

}  // namespace content
