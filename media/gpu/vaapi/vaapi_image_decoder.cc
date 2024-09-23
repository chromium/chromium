// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include <utility>

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

void VAContextAndScopedVASurfaceDeleter::operator()(
    ScopedVASurface* scoped_va_surface) const {
  scoped_va_surface->vaapi_wrapper_->DestroyContext();
  delete scoped_va_surface;
}

VaapiImageDecoder::VaapiImageDecoder(VAProfile va_profile)
    : va_profile_(va_profile) {
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
}

VaapiImageDecoder::~VaapiImageDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

bool VaapiImageDecoder::Initialize(const ReportErrorToUMACB& error_uma_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  if (vaapi_wrapper_) {
    return true;
  }
  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile_,
                           EncryptionScheme::kUnencrypted, error_uma_cb)
          .value_or(nullptr);
  return !!vaapi_wrapper_;
}

VaapiImageDecodeStatus VaapiImageDecoder::Decode(
    base::span<const uint8_t> encoded_image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  if (!vaapi_wrapper_) {
    VLOGF(1) << "VaapiImageDecoder has not been initialized";
    scoped_va_context_and_surface_.reset();
    return VaapiImageDecodeStatus::kInvalidState;
  }

  const VaapiImageDecodeStatus status =
      AllocateVASurfaceAndSubmitVABuffers(encoded_image);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    scoped_va_context_and_surface_.reset();
    return status;
  }

  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
          scoped_va_context_and_surface_->id())) {
    VLOGF(1) << "ExecuteAndDestroyPendingBuffers() failed";
    scoped_va_context_and_surface_.reset();
    return VaapiImageDecodeStatus::kExecuteDecodeFailed;
  }
  return VaapiImageDecodeStatus::kSuccess;
}

const ScopedVASurface* VaapiImageDecoder::GetScopedVASurface() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  return scoped_va_context_and_surface_.get();
}

std::unique_ptr<NativePixmapAndSizeInfo>
VaapiImageDecoder::ExportAsNativePixmapDmaBuf(VaapiImageDecodeStatus* status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(status);

  // We need to take ownership of the ScopedVASurface so that the next Decode()
  // doesn't attempt to use the same ScopedVASurface. Otherwise, it could
  // overwrite the result of the current decode before it's used by the caller.
  // This ScopedVASurface will self-clean at the end of this scope, but the
  // underlying buffer should stay alive because of the exported FDs.
  ScopedVAContextAndSurface temp_scoped_va_surface =
      std::move(scoped_va_context_and_surface_);

  if (!temp_scoped_va_surface) {
    DVLOGF(1) << "No decoded image available";
    *status = VaapiImageDecodeStatus::kInvalidState;
    return nullptr;
  }
  DCHECK(temp_scoped_va_surface->IsValid());

  std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
      vaapi_wrapper_->ExportVASurfaceAsNativePixmapDmaBuf(
          *temp_scoped_va_surface);
  if (!exported_pixmap) {
    *status = VaapiImageDecodeStatus::kCannotExportSurface;
    return nullptr;
  }

  DCHECK_EQ(temp_scoped_va_surface->size(),
            exported_pixmap->pixmap->GetBufferSize());
  *status = VaapiImageDecodeStatus::kSuccess;
  return exported_pixmap;
}

}  // namespace media
