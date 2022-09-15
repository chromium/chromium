// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/quota_client.mojom-forward.h"
#include "content/browser/native_io/native_io_quota_client.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

namespace content {

class NativeIOHost;

// Implements the NativeIO Web Platform feature for a StoragePartition.
//
// Each StoragePartition owns exactly one instance of this class. This class
// creates and destroys NativeIOHost instances to meet the demands for NativeIO
// from different storage keys.
//
// This class is not thread-safe, and all access to an instance must happen on
// the same sequence.
class CONTENT_EXPORT NativeIOManager {
 public:
  // `profile_root` is empty for in-memory (Incognito) profiles. Otherwise,
  // `profile_root` must point to an existing directory. NativeIO will store its
  // data in a subdirectory of the profile root.
  //
  // `allow_set_length_ipc` gates NativeIOFileHost::SetLength(), which works
  // around a sandboxing limitation on macOS < 10.15. This is plumbed as a flag
  // all the from NativeIOManager to facilitate testing.
  explicit NativeIOManager(
      const base::FilePath& profile_root,
#if BUILDFLAG(IS_MAC)
      bool allow_set_length_ipc,
#endif  // BUILDFLAG(IS_MAC)
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  ~NativeIOManager();

  NativeIOManager(const NativeIOManager&) = delete;
  NativeIOManager& operator=(const NativeIOManager&) = delete;

  // Binds `receiver` to the NativeIOHost serving `storage_key`.
  //
  // `receiver` must belong to a frame or worker serving `storage_key`.
  void BindReceiver(const blink::StorageKey& storage_key,
                    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
                    mojo::ReportBadMessageCallback bad_message_callback);

  // Removes an storage key's data and closes any open files.
  void DeleteStorageKeyData(
      const blink::StorageKey& storage_key,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);

  // Computes all storage keys with data for a given type.
  void GetStorageKeysForType(
      blink::mojom::StorageType type,
      storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback);

  // Computes the amount of bytes for the given storage key.
  //
  // This method walks the storage key's entire directory and is therefore not
  // particularly speedy.
  // TODO(rstz): Consider a caching mechanism to improve performance.
  void GetStorageKeyUsage(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      storage::mojom::QuotaClient::GetBucketUsageCallback callback);

  // Computes the amount of bytes for all storage keys.
  //
  // This method walks the storage key's entire directory and is therefore not
  // particularly speedy.
  // TODO(rstz): Consider a caching mechanism to improve performance.
  void GetStorageKeyUsageMap(
      base::OnceCallback<void(const std::map<blink::StorageKey, int64_t>&)>
          callback);

  // Computes the path to the directory storing an storage key's NativeIO files.
  //
  // Returns an empty path if the storage key isn't supported for NativeIO.
  base::FilePath RootPathForStorageKey(const blink::StorageKey& storage_key);

  // Computes the path to the directory storing a profile's NativeIO files.
  static base::FilePath GetNativeIORootPath(const base::FilePath& profile_root);

  // Transform a base::File::Error into a NativeIOError with default error
  // message if none is provided.
  static blink::mojom::NativeIOErrorPtr FileErrorToNativeIOError(
      base::File::Error file_error,
      std::string message = "");

  // Called when a receiver is disconnected from a NativeIOHost.
  //
  // `host` must be owned by this manager. `host` may be deleted.
  void OnHostReceiverDisconnect(NativeIOHost* host,
                                base::PassKey<NativeIOHost>);

  // Called when a NativeIOHost finishes processing a data deletion request.
  //
  // `host` must be owned by this manager. `host` may be deleted.
  void DidDeleteHostData(NativeIOHost* host, base::PassKey<NativeIOHost>);

  const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

 private:
  // Adds and binds receiver on default bucket retrieval to ensure that a bucket
  // always exists for the storage key.
  void BindReceiverWithBucketInfo(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
      storage::QuotaErrorOr<storage::BucketInfo> result);

  // Deletes the NativeIOHost if it serves no further purpose.
  //
  // `host` must be owned by this manager.
  void MaybeDeleteHost(NativeIOHost* host);

  SEQUENCE_CHECKER(sequence_checker_);

  // Points to the root directory for NativeIO files.
  //
  // This path is empty for in-memory (Incognito) profiles.
  const base::FilePath root_path_;

#if BUILDFLAG(IS_MAC)
  const bool allow_set_length_ipc_;
#endif  // BUILDFLAG(IS_MAC)

  // Tracks special rights for apps and extensions, may be null.
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  std::map<blink::StorageKey, std::unique_ptr<NativeIOHost>> hosts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  NativeIOQuotaClient quota_client_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Once the QuotaClient receiver is destroyed, the underlying mojo connection
  // is closed. Callbacks associated with mojo calls received over this
  // connection may only be dropped after the connection is closed. For this
  // reason, it's preferable to have the receiver be destroyed as early as
  // possible during the NativeIOManager destruction process.
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<NativeIOManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_
