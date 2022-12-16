// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_

#include <va/va.h>

#include <vector>

namespace media::internal {

// Class used for tracking a VASurface and all information relevant to it.
// All objects of this class are immutable and thread safe.
class FakeSurface {
 public:
  using IdType = VASurfaceID;

  FakeSurface(IdType id,
              unsigned int format,
              unsigned int width,
              unsigned int height,
              std::vector<VASurfaceAttrib> attrib_list);
  FakeSurface(const FakeSurface&) = delete;
  FakeSurface& operator=(const FakeSurface&) = delete;
  ~FakeSurface();

  IdType GetID() const;
  unsigned int GetFormat() const;
  unsigned int GetWidth() const;
  unsigned int GetHeight() const;
  const std::vector<VASurfaceAttrib>& GetSurfaceAttribs() const;

 private:
  const IdType id_;
  const unsigned int format_;
  const unsigned int width_;
  const unsigned int height_;
  const std::vector<VASurfaceAttrib> attrib_list_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_