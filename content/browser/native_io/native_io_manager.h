// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_
#define CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/quota_client.mojom-forward.h"
#include "content/browser/native_io/native_io_quota_client.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-forward.h"
#include "url/origin.h"

namespace content {

class NativeIOHost;

// Implements the NativeIO Web Platform feature for a StoragePartition.
//
// Each StoragePartition owns exactly one instance of this class. This class
// creates and destroys NativeIOHost instances to meet the demands for NativeIO
// from different origins.
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
#if defined(OS_MAC)
      bool allow_set_length_ipc,
#endif  // defined(OS_MAC)
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  ~NativeIOManager();

  NativeIOManager(const NativeIOManager&) = delete;
  NativeIOManager& operator=(const NativeIOManager&) = delete;

  // Binds `receiver` to the NativeIOHost serving `origin`.
  //
  // `receiver` must belong to a frame or worker serving `origin`.
  void BindReceiver(const url::Origin& origin,
                    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver,
                    mojo::ReportBadMessageCallback bad_message_callback);

  // Removes an origin's data and closes any open files.
  void DeleteOriginData(
      const url::Origin& origin,
      storage::QuotaClient::DeleteOriginDataCallback callback);

  // Computes all origins with data for a given type.
  void GetOriginsForType(
      blink::mojom::StorageType type,
      storage::QuotaClient::GetOriginsForTypeCallback callback);

  // Computes all origins with data for a given hostname.
  void GetOriginsForHost(
      blink::mojom::StorageType type,
      const std::string& host,
      storage::QuotaClient::GetOriginsForHostCallback callback);

  // Computes the amount of bytes for the given origin.
  //
  // This method walks the origin's entire directory and is therefore not
  // particularly speedy.
  // TODO(rstz): Consider a caching mechanism to improve performance.
  void GetOriginUsage(const url::Origin& origin,
                      blink::mojom::StorageType type,
                      storage::QuotaClient::GetOriginUsageCallback callback);

  // Computes the amount of bytes for all origins.
  //
  // This method walks the origin's entire directory and is therefore not
  // particularly speedy.
  // TODO(rstz): Consider a caching mechanism to improve performance.
  void GetOriginUsageMap(
      base::OnceCallback<void(const std::map<url::Origin, int64_t>)> callback);

  // Computes the path to the directory storing an origin's NativeIO files.
  //
  // Returns an empty path if the origin isn't supported for NativeIO.
  base::FilePath RootPathForOrigin(const url::Origin& origin);

  // Computes the path to the directory storing a profile's NativeIO files.
  static base::FilePath GetNativeIORootPath(const base::FilePath& profile_root);

  // Transform a base::File::Error into a NativeIOError with default error
  // message if none is provided.
  static blink::mojom::NativeIOErrorPtr FileErrorToNativeIOError(
      base::File::Error file_error,
      std::string message = "");

  // Called when a receiver is disconnected from a NativeIOHost.
  //
  // `host` must be owned by this manager. This method should only be called by
  // NativeIOHost.
  void OnHostReceiverDisconnect(NativeIOHost* host);

  // Callback function when DeleteOriginData has completed.
  //
  // `host` must be owned by this manager.
  void OnDeleteOriginDataCompleted(
      storage::QuotaClient::DeleteOriginDataCallback callback,
      base::File::Error result,
      NativeIOHost* host);

  storage::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Deletes the NativeIOHost if it serves no further purpose.
  //
  // `host` must be owned by this manager.
  void MaybeDeleteHost(NativeIOHost* host);

  // Called after the I/O part of GetOriginsForType() completed.
  void DidGetOriginsForType(
      storage::QuotaClient::GetOriginsForTypeCallback callback,
      std::vector<url::Origin> origins);

  // Called after the I/O part of GetOriginsForHost() completed.
  void DidGetOriginsForHost(
      storage::QuotaClient::GetOriginsForTypeCallback callback,
      const std::string& host,
      std::vector<url::Origin> origins);

  // Called after the I/O part of GetOriginUsage() completed.
  void DidGetOriginUsage(storage::QuotaClient::GetOriginUsageCallback callback,
                         int64_t usage);

  // Called after the I/O part of GetOriginUsageMap() completed.
  void DidGetOriginUsageMap(
      base::OnceCallback<void(const std::map<url::Origin, int64_t>)> callback,
      std::map<url::Origin, int64_t> usage_map);

  // Points to the root directory for NativeIO files.
  //
  // This path is empty for in-memory (Incognito) profiles.
  const base::FilePath root_path_;

#if defined(OS_MAC)
  const bool allow_set_length_ipc_;
#endif  // defined(OS_MAC)

  // Tracks special rights for apps and extensions, may be null.
  const scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  std::map<url::Origin, std::unique_ptr<NativeIOHost>> hosts_;

  NativeIOQuotaClient quota_client_;

  // Once the QuotaClient receiver is destroyed, the underlying mojo connection
  // is closed. Callbacks associated with mojo calls received over this
  // connection may only be dropped after the connection is closed. For this
  // reason, it's preferable to have the receiver be destroyed as early as
  // possible during the NativeIOManager destruction process.
  mojo::Receiver<storage::mojom::QuotaClient> quota_client_receiver_;

  base::WeakPtrFactory<NativeIOManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_IO_NATIVE_IO_MANAGER_H_
