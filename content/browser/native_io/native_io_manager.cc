// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/browser/native_io/native_io_quota_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/native_io/native_io_utils.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

namespace {

std::vector<blink::StorageKey> DoGetStorageKeys(
    const base::FilePath& native_io_root) {
  std::vector<blink::StorageKey> result;
  // If the NativeIO directory wasn't created yet, there's no file to report.
  if (!base::PathExists(native_io_root))
    return result;

  base::FileEnumerator file_enumerator(native_io_root, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // If the directory name has a non-ASCII character, `file_path` will be the
    // empty string. This indicates corruption as any storage key creates an
    // ASCII-only directory name, so those directories are ignored.
    std::string directory_name = file_path.BaseName().MaybeAsASCII();
    if (directory_name == "")
      continue;
    blink::StorageKey storage_key =
        blink::StorageKey(storage::GetOriginFromIdentifier(directory_name));
    result.push_back(std::move(storage_key));
  }
  return result;
}

int64_t DoGetStorageKeyUsage(const base::FilePath& storage_key_root) {
  // base::ComputeDirectorySize() spins on Windows when given an empty path.
  // `storage_key_root` can be empty in Incognito.
  if (storage_key_root.empty())
    return 0;

  // Returns 0 if `storage_key_root` does not exist.
  return base::ComputeDirectorySize(storage_key_root);
}

std::map<blink::StorageKey, int64_t> DoGetStorageKeyUsageMap(
    const base::FilePath& native_io_root) {
  std::map<blink::StorageKey, int64_t> result;

  // If the NativeIO directory wasn't created yet, there's no file to report.
  if (!base::PathExists(native_io_root))
    return result;

  base::FileEnumerator file_enumerator(native_io_root, /*recursive=*/false,
                                       base::FileEnumerator::DIRECTORIES);

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // If the directory name has a non-ASCII character, `file_path` will be the
    // empty string. This indicates corruption as any storage key creates an
    // ASCII-only directory name, so those directories are ignored.
    std::string directory_name = file_path.BaseName().MaybeAsASCII();
    if (directory_name == "")
      continue;
    blink::StorageKey storage_key =
        blink::StorageKey(storage::GetOriginFromIdentifier(directory_name));
    int64_t usage = base::ComputeDirectorySize(file_path);
    auto inserted = result.insert(std::make_pair(storage_key, usage));
    DCHECK(inserted.second)
        << "StorageKeys in NativeIO's directory should have a unique folder.";
  }
  return result;
}

constexpr base::FilePath::CharType kNativeIODirectoryName[] =
    FILE_PATH_LITERAL("NativeIO");
}  // namespace

NativeIOManager::NativeIOManager(
    const base::FilePath& profile_root,
#if BUILDFLAG(IS_MAC)
    bool allow_set_length_ipc,
#endif  // BUILDFLAG(IS_MAC)
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : root_path_(GetNativeIORootPath(profile_root)),
#if BUILDFLAG(IS_MAC)
      allow_set_length_ipc_(allow_set_length_ipc),
#endif  // BUILDFLAG(IS_MAC)
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
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(storage_key);
  if (it != hosts_.end()) {
    it->second->BindReceiver(std::move(receiver));
    return;
  }

  // This feature should only be exposed to potentially trustworthy origins
  // (https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy).
  // Notably this includes the https and chrome-extension schemes, among
  // others.
  if (!network::IsOriginPotentiallyTrustworthy(storage_key.origin())) {
    std::move(bad_message_callback)
        .Run("Called NativeIO from an insecure context");
    return;
  }

  // Ensure that the default bucket for the storage key exists on access and
  // bind receiver on retrieval.
  quota_manager_proxy_->UpdateOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&NativeIOManager::BindReceiverWithBucketInfo,
                     weak_factory_.GetWeakPtr(), storage_key,
                     std::move(receiver)));
}

void NativeIOManager::OnHostReceiverDisconnect(NativeIOHost* host,
                                               base::PassKey<NativeIOHost>) {
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

void NativeIOManager::BindReceiverWithBucketInfo(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result.ok());

  auto it = hosts_.find(storage_key);
  if (it == hosts_.end()) {
    base::FilePath storage_key_root_path = RootPathForStorageKey(storage_key);
    DCHECK(storage_key_root_path.empty() ||
           root_path_.IsParent(storage_key_root_path))
        << "Per-storage-key data should be in a sub-directory of NativeIO/ for "
        << "non-incognito mode ";

    bool insert_succeeded;
    std::tie(it, insert_succeeded) = hosts_.emplace(
        storage_key, std::make_unique<NativeIOHost>(
                         storage_key, std::move(storage_key_root_path),
#if BUILDFLAG(IS_MAC)
                         allow_set_length_ipc_,
#endif  // BUILDFLAG(IS_MAC)
                         this));
    DCHECK(insert_succeeded);
  }

  it->second->BindReceiver(std::move(receiver));
}

void NativeIOManager::MaybeDeleteHost(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);

  DCHECK(hosts_.count(host->storage_key()) > 0);
  DCHECK_EQ(hosts_[host->storage_key()].get(), host);

  if (!host->has_empty_receiver_set() || host->delete_all_data_in_progress())
    return;

  hosts_.erase(host->storage_key());
}

void NativeIOManager::DeleteStorageKeyData(
    const blink::StorageKey& storage_key,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  auto it = hosts_.find(storage_key);
  if (it == hosts_.end()) {
    // TODO(rstz): Consider turning these checks into DCHECKS when NativeIO is
    // no longer bundled with the Filesystem API during data removal.
    if (!network::IsOriginPotentiallyTrustworthy(storage_key.origin())) {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }
    base::FilePath storage_key_root_path = RootPathForStorageKey(storage_key);
    if (storage_key_root_path.empty()) {
      // NativeIO is not supported for the storage key, no data can be deleted.
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }

    DCHECK(root_path_.IsParent(storage_key_root_path))
        << "Per-storage-key data should be in a sub-directory of NativeIO/";

    bool insert_succeeded;
    // Create a NativeIOHost so that future API calls for the storage key are
    // queued behind the data deletion. This should not meaningfully slow down
    // the removal process.
    std::tie(it, insert_succeeded) = hosts_.emplace(
        storage_key, std::make_unique<NativeIOHost>(
                         storage_key, std::move(storage_key_root_path),
#if BUILDFLAG(IS_MAC)
                         allow_set_length_ipc_,
#endif  // BUILDFLAG(IS_MAC)
                         this));
    DCHECK(insert_succeeded);
  }

  // DeleteAllData() will call DidDeleteHostData() asynchronously, which may
  // delete this entry from `hosts_`.
  it->second->DeleteAllData(base::BindOnce(
      [](storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
         base::File::Error error) {
        std::move(callback).Run((error == base::File::FILE_OK)
                                    ? blink::mojom::QuotaStatusCode::kOk
                                    : blink::mojom::QuotaStatusCode::kUnknown);
      },
      std::move(callback)));
}

void NativeIOManager::GetStorageKeysForType(
    blink::mojom::StorageType type,
    storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(callback);

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
      base::BindOnce(&DoGetStorageKeys, root_path_), std::move(callback));
}

void NativeIOManager::GetStorageKeyUsage(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    storage::mojom::QuotaClient::GetBucketUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, blink::mojom::StorageType::kTemporary);
  DCHECK(callback);

  base::FilePath storage_key_root = RootPathForStorageKey(storage_key);

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
      base::BindOnce(&DoGetStorageKeyUsage, storage_key_root),
      std::move(callback));
}

void NativeIOManager::GetStorageKeyUsageMap(
    base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

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
      base::BindOnce(&DoGetStorageKeyUsageMap, root_path_),
      std::move(callback));
}

base::FilePath NativeIOManager::RootPathForStorageKey(
    const blink::StorageKey& storage_key) {
  // TODO(pwnall): Implement in-memory files instead of bouncing in incognito.
  if (root_path_.empty())
    return root_path_;

  std::string storage_key_identifier =
      storage::GetIdentifierFromOrigin(storage_key.origin());
  base::FilePath storage_key_path =
      root_path_.AppendASCII(storage_key_identifier);
  DCHECK(root_path_.IsParent(storage_key_path));
  return storage_key_path;
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
