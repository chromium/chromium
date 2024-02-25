// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_

#include <va/va.h>

#include <vector>

#include "media/gpu/vaapi/test/fake_libva_driver/scoped_bo_mapping_factory.h"

namespace media::internal {

// Class used for tracking a VASurface and all information relevant to it.
//
// The metadata (ID, format, fourcc, dimensions, and attribute list) of a
// FakeSurface is immutable. The accessors for such metadata are thread-safe.
// The contents of the backing buffer object (if applicable) are mutable, but
// the reference to that buffer object is immutable, i.e., the backing buffer
// object is always the same, but the contents may change. Thus, while the
// accessor for the mapped buffer object is thread-safe, writes and reads to
// this mapping must be synchronized externally.
class FakeSurface {
 public:
  using IdType = VASurfaceID;

  FakeSurface(const FakeSurface&) = delete;
  FakeSurface& operator=(const FakeSurface&) = delete;
  ~FakeSurface();

  // Note: |scoped_bo_mapping_factory| must outlive the `FakeSurface` since
  // it's used to unmap the backing buffer object (if applicable).
  static std::unique_ptr<FakeSurface> Create(
      IdType id,
      unsigned int format,
      unsigned int width,
      unsigned int height,
      std::vector<VASurfaceAttrib> attrib_list,
      ScopedBOMappingFactory& scoped_bo_mapping_factory);

  IdType GetID() const;
  unsigned int GetFormat() const;
  uint32_t GetVAFourCC() const;
  unsigned int GetWidth() const;
  unsigned int GetHeight() const;
  const std::vector<VASurfaceAttrib>& GetSurfaceAttribs() const;
  const ScopedBOMapping& GetMappedBO() const;

 private:
  FakeSurface(IdType id,
              unsigned int format,
              uint32_t va_fourcc,
              unsigned int width,
              unsigned int height,
              std::vector<VASurfaceAttrib> attrib_list,
              ScopedBOMapping mapped_bo);

  const IdType id_;
  const unsigned int format_;
  const uint32_t va_fourcc_;
  const unsigned int width_;
  const unsigned int height_;
  const std::vector<VASurfaceAttrib> attrib_list_;
  ScopedBOMapping mapped_bo_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_SURFACE_H_