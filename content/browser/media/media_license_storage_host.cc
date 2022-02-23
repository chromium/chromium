// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_storage_host.h"

#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/browser/media/media_license_manager.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

MediaLicenseStorageHost::MediaLicenseStorageHost(
    MediaLicenseManager* manager,
    const storage::BucketLocator& bucket_locator)
    : manager_(manager), bucket_locator_(bucket_locator) {
  DCHECK(manager_);

  // base::Unretained is safe here because this MediaLicenseStorageHost owns
  // `receivers_`. So, the unretained MediaLicenseStorageHost is guaranteed to
  // outlive `receivers_` and the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &MediaLicenseStorageHost::OnReceiverDisconnect, base::Unretained(this)));
}

MediaLicenseStorageHost::~MediaLicenseStorageHost() = default;

void MediaLicenseStorageHost::Open(const std::string& file_name,
                                   OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (file_name.empty()) {
    DVLOG(1) << "No file specified.";
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  if (!CdmFileImpl::IsValidName(file_name)) {
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  // TODO(crbug.com/1231162): Modify CdmFileImpl to route operations through
  // this class.
  const BindingContext& binding_context = receivers_.current_context();
  auto cdm_file_impl = std::make_unique<CdmFileImpl>(
      file_name, binding_context.storage_key.origin(), binding_context.cdm_type,
      /*file_system_root_uri_=*/"", nullptr);

  if (!cdm_file_impl->Initialize()) {
    // Unable to initialize with the file requested.
    std::move(callback).Run(Status::kInUse, mojo::NullAssociatedRemote());
    return;
  }

  // File was opened successfully, so create the binding and return success.
  mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file;
  cdm_file_receivers_.Add(std::move(cdm_file_impl),
                          cdm_file.InitWithNewEndpointAndPassReceiver());
  std::move(callback).Run(Status::kSuccess, std::move(cdm_file));
}

void MediaLicenseStorageHost::BindReceiver(
    const BindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(binding_context.storage_key, bucket_locator_.storage_key);

  receivers_.Add(this, std::move(receiver), binding_context);
}

void MediaLicenseStorageHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // May delete `this`.
  manager_->OnHostReceiverDisconnect(this,
                                     base::PassKey<MediaLicenseStorageHost>());
}

}  // namespace content