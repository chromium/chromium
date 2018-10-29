// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
#define MEDIA_GPU_VAAPI_VAAPI_COMMON_H_

#include "media/gpu/h264_dpb.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vp8_picture.h"
#include "media/gpu/vp9_picture.h"

namespace media {

// These picture classes derive from platform-independent, codec-specific
// classes to allow augmenting them with VA-API-specific traits; used when
// providing associated hardware resources and parameters to VA-API drivers.

class VaapiH264Picture : public H264Picture {
 public:
  explicit VaapiH264Picture(scoped_refptr<VASurface> va_surface);

  VaapiH264Picture* AsVaapiH264Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiH264Picture() override;

 private:
  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiH264Picture);
};

class VaapiVP8Picture : public VP8Picture {
 public:
  explicit VaapiVP8Picture(scoped_refptr<VASurface> va_surface);

  VaapiVP8Picture* AsVaapiVP8Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiVP8Picture() override;

 private:
  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Picture);
};

class VaapiVP9Picture : public VP9Picture {
 public:
  explicit VaapiVP9Picture(scoped_refptr<VASurface> va_surface);

  VaapiVP9Picture* AsVaapiVP9Picture() override;

  scoped_refptr<VASurface> va_surface() const { return va_surface_; }
  VASurfaceID GetVASurfaceID() const { return va_surface_->id(); }

 protected:
  ~VaapiVP9Picture() override;

 private:
  scoped_refptr<VP9Picture> CreateDuplicate() override;

  scoped_refptr<VASurface> va_surface_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVP9Picture);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_COMMON_H_
