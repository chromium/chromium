// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include <utility>

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
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
    : va_profile_(va_profile) {}

VaapiImageDecoder::~VaapiImageDecoder() = default;

bool VaapiImageDecoder::Initialize(const base::RepeatingClosure& error_uma_cb) {
  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile_, error_uma_cb);
  return !!vaapi_wrapper_;
}

VaapiImageDecodeStatus VaapiImageDecoder::Decode(
    base::span<const uint8_t> encoded_image) {
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
  return scoped_va_context_and_surface_.get();
}

gpu::ImageDecodeAcceleratorSupportedProfile
VaapiImageDecoder::GetSupportedProfile() const {
  if (!vaapi_wrapper_) {
    DVLOGF(1) << "The VAAPI has not been initialized";
    return gpu::ImageDecodeAcceleratorSupportedProfile();
  }

  gpu::ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = GetType();
  DCHECK_NE(gpu::ImageDecodeAcceleratorType::kUnknown, profile.image_type);

  // Note that since |vaapi_wrapper_| was created successfully, we expect the
  // following calls to be successful. Hence the DCHECKs.
  const bool got_min_resolution = VaapiWrapper::GetDecodeMinResolution(
      va_profile_, &profile.min_encoded_dimensions);
  DCHECK(got_min_resolution);
  const bool got_max_resolution = VaapiWrapper::GetDecodeMaxResolution(
      va_profile_, &profile.max_encoded_dimensions);
  DCHECK(got_max_resolution);

  // TODO(andrescj): Ideally, we would advertise support for all the formats
  // supported by the driver. However, for now, we will only support exposing
  // YUV 4:2:0 surfaces as DmaBufs.
  DCHECK(VaapiWrapper::GetDecodeSupportedInternalFormats(va_profile_).yuv420);
  profile.subsamplings.push_back(gpu::ImageDecodeAcceleratorSubsampling::k420);
  return profile;
}

std::unique_ptr<NativePixmapAndSizeInfo>
VaapiImageDecoder::ExportAsNativePixmapDmaBuf(VaapiImageDecodeStatus* status) {
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
