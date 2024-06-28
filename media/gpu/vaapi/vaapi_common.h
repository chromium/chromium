// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
#define MEDIA_GPU_VAAPI_VAAPI_COMMON_H_

#include "build/chromeos_buildflags.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp9_picture.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/gpu/h265_dpb.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

// These picture classes derive from platform-independent, codec-specific
// classes to allow augmenting them with VA-API-specific traits; used when
// providing associated hardware resources and parameters to VA-API drivers.

class VaapiH264Picture : public H264Picture {
 public:
  explicit VaapiH264Picture(std::unique_ptr<VASurfaceHandle> va_surface);

  VaapiH264Picture(const VaapiH264Picture&) = delete;
  VaapiH264Picture& operator=(const VaapiH264Picture&) = delete;

  VaapiH264Picture* AsVaapiH264Picture() override;

  VASurfaceID va_surface_id() const { return va_surface_->id(); }

 protected:
  ~VaapiH264Picture() override;

 private:
  std::unique_ptr<VASurfaceHandle> va_surface_;
};

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
class VaapiH265Picture : public H265Picture {
 public:
  explicit VaapiH265Picture(std::unique_ptr<VASurfaceHandle> va_surface);

  VaapiH265Picture(const VaapiH265Picture&) = delete;
  VaapiH265Picture& operator=(const VaapiH265Picture&) = delete;

  VaapiH265Picture* AsVaapiH265Picture() override;

  VASurfaceID va_surface_id() const { return va_surface_->id(); }

 protected:
  ~VaapiH265Picture() override;

 private:
  std::unique_ptr<VASurfaceHandle> va_surface_;
};
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

class VaapiVP8Picture : public VP8Picture {
 public:
  explicit VaapiVP8Picture(std::unique_ptr<VASurfaceHandle> va_surface);

  VaapiVP8Picture(const VaapiVP8Picture&) = delete;
  VaapiVP8Picture& operator=(const VaapiVP8Picture&) = delete;

  VaapiVP8Picture* AsVaapiVP8Picture() override;

  VASurfaceID va_surface_id() const { return va_surface_->id(); }

 protected:
  ~VaapiVP8Picture() override;

 private:
  std::unique_ptr<VASurfaceHandle> va_surface_;
};

class VaapiVP9Picture : public VP9Picture {
 public:
  explicit VaapiVP9Picture(std::unique_ptr<VASurfaceHandle> va_surface);

  VaapiVP9Picture(const VaapiVP9Picture&) = delete;
  VaapiVP9Picture& operator=(const VaapiVP9Picture&) = delete;

  VaapiVP9Picture* AsVaapiVP9Picture() override;

  VASurfaceID va_surface_id() const { return va_surface_->id(); }

 protected:
  ~VaapiVP9Picture() override;

 private:
  scoped_refptr<VP9Picture> CreateDuplicate() override;

  std::unique_ptr<VASurfaceHandle> va_surface_;
};

class VaapiAV1Picture : public AV1Picture {
 public:
  VaapiAV1Picture(std::unique_ptr<VASurfaceHandle> display_va_surface,
                  std::unique_ptr<VASurfaceHandle> reconstruct_va_surface);
  VaapiAV1Picture(const VaapiAV1Picture&) = delete;
  VaapiAV1Picture& operator=(const VaapiAV1Picture&) = delete;

  VASurfaceID display_va_surface_id() const {
    return display_va_surface_->id();
  }
  VASurfaceID reconstruct_va_surface_id() const {
    return reconstruct_va_surface_->id();
  }

 protected:
  ~VaapiAV1Picture() override;

 private:
  scoped_refptr<AV1Picture> CreateDuplicate() override;

  // |display_va_surface_| refers to the final decoded frame, both when using
  // film grain synthesis and when not using film grain.
  // |reconstruct_va_surface_| is only useful when using film grain synthesis:
  // it's the decoded frame prior to applying the film grain.
  // When not using film grain synthesis, |reconstruct_va_surface_| is equal to
  // |display_va_surface_|. This is necessary to simplify the reference frame
  // code when filling the VA-API structures and to be able to always use
  // reconstruct_va_surface() when calling ExecuteAndDestroyPendingBuffers()
  // (the driver expects the reconstructed surface as the target in the case
  // of film grain synthesis).
  std::unique_ptr<VASurfaceHandle> display_va_surface_;
  std::unique_ptr<VASurfaceHandle> reconstruct_va_surface_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
