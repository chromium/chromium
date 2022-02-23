// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/media/media_license_manager.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// Per-storage-key backend for media license (CDM) files. MediaLicenseManager
// owns an instance of this class for each storage key that is actively using
// CDM files. Each instance owns all CdmStorage receivers for the corresponding
// storage key.
class CONTENT_EXPORT MediaLicenseStorageHost : public media::mojom::CdmStorage {
 public:
  using BindingContext = MediaLicenseManager::BindingContext;

  MediaLicenseStorageHost(MediaLicenseManager* manager,
                          const storage::BucketLocator& bucket_locator);
  ~MediaLicenseStorageHost() override;

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

  void BindReceiver(const BindingContext& binding_context,
                    mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  // True if there are no receivers connected to this host.
  //
  // The MediaLicenseManager that owns this host is expected to destroy the host
  // when it isn't serving any receivers.
  bool has_empty_receiver_set() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receivers_.empty();
  }

  const blink::StorageKey& storage_key() { return bucket_locator_.storage_key; }

 private:
  void OnReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // MediaLicenseManager instance which owns this object.
  const raw_ptr<MediaLicenseManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const storage::BucketLocator bucket_locator_;

  // TODO: hold SequenceBound sql::Database to run operations through.

  // All receivers for frames and workers whose storage key is `storage_key()`.
  mojo::ReceiverSet<media::mojom::CdmStorage, BindingContext> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of all media::mojom::CdmFile receivers, as each CdmFileImpl
  // object keeps a reference to |this|. If |this| goes away unexpectedly,
  // all remaining CdmFile receivers will be closed.
  mojo::UniqueAssociatedReceiverSet<media::mojom::CdmFile> cdm_file_receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<MediaLicenseStorageHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_STORAGE_HOST_H_
