// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_IMAGE_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_IMAGE_H_

#include <va/va.h>

#include "base/memory/raw_ref.h"

namespace media::internal {

class FakeBuffer;
class FakeDriver;

// Class used for tracking a VAImage and all information relevant to it.
//
// The metadata (ID, format, dimensions, number of planes, and plane
// stride/offset) of a FakeImage is immutable. The accessors for such metadata
// are thread-safe. The contents of the backing FakeBuffer object are mutable,
// but the reference to that FakeBuffer is immutable, i.e., the backing buffer
// is always the same, but the contents may change. Thus, while the accessor for
// the FakeBuffer is thread-safe, writes and reads to this buffer must be
// synchronized externally.
//
// Note: Currently the FakeImage only supports the NV12 image format.
class FakeImage {
 public:
  using IdType = VAImageID;

  // Creates a FakeImage using the specified metadata (|id|, |format|, |width|,
  // and |height|). The |fake_driver| is used to create a backing FakeBuffer and
  // manage its lifetime. Thus, the |fake_driver| must outlive the created
  // `FakeImage`. Upon return, *|va_image| is filled with all the fields needed
  // by the libva client to use the image.
  static std::unique_ptr<FakeImage> Create(IdType id,
                                           const VAImageFormat& format,
                                           int width,
                                           int height,
                                           FakeDriver& fake_driver,
                                           VAImage* va_image);

  FakeImage(const FakeImage&) = delete;
  FakeImage& operator=(const FakeImage&) = delete;
  ~FakeImage();

  IdType GetID() const;
  const VAImageFormat& GetFormat() const;
  int GetWidth() const;
  int GetHeight() const;
  const FakeBuffer& GetBuffer() const;
  uint32_t GetPlaneStride(size_t plane) const;
  uint32_t GetPlaneOffset(size_t plane) const;

 private:
  struct Plane {
    Plane(uint32_t stride, uint32_t offset);

    const uint32_t stride;
    const uint32_t offset;
  };

  FakeImage(IdType id,
            const VAImageFormat& format,
            int width,
            int height,
            std::vector<Plane> planes,
            const FakeBuffer& buffer,
            FakeDriver& driver);

  const IdType id_;
  const VAImageFormat format_;
  const int width_;
  const int height_;
  const std::vector<Plane> planes_;
  const raw_ref<const FakeBuffer> buffer_;
  const raw_ref<FakeDriver> driver_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_IMAGE_H_
