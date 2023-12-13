// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_

#include <va/va.h>

#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_config.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_context.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_image.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"
#include "media/gpu/vaapi/test/fake_libva_driver/object_tracker.h"
#include "media/gpu/vaapi/test/fake_libva_driver/scoped_bo_mapping_factory.h"

namespace media::internal {

// FakeDriver is used to keep track of all the state that exists between a call
// to vaInitialize() and a call to vaTerminate(). All public methods are
// thread-safe.
class FakeDriver {
 public:
  // FakeDriver doesn't dup() or close() |drm_fd|, i.e., it's expected that the
  // driver's user maintains the FD valid at least until after vaTerminate()
  // returns.
  explicit FakeDriver(int drm_fd);
  FakeDriver(const FakeDriver&) = delete;
  FakeDriver& operator=(const FakeDriver&) = delete;
  ~FakeDriver();

  FakeConfig::IdType CreateConfig(VAProfile profile,
                                  VAEntrypoint entrypoint,
                                  std::vector<VAConfigAttrib> attrib_list);
  bool ConfigExists(FakeConfig::IdType id);
  const FakeConfig& GetConfig(FakeConfig::IdType id);
  void DestroyConfig(FakeConfig::IdType id);

  FakeSurface::IdType CreateSurface(unsigned int format,
                                    unsigned int width,
                                    unsigned int height,
                                    std::vector<VASurfaceAttrib> attrib_list);
  bool SurfaceExists(FakeSurface::IdType id);
  const FakeSurface& GetSurface(FakeSurface::IdType id);
  void DestroySurface(FakeSurface::IdType id);

  FakeContext::IdType CreateContext(VAConfigID config_id,
                                    int picture_width,
                                    int picture_height,
                                    int flag,
                                    std::vector<VASurfaceID> render_targets);
  bool ContextExists(FakeContext::IdType id);
  const FakeContext& GetContext(FakeContext::IdType id);
  void DestroyContext(FakeContext::IdType id);

  FakeBuffer::IdType CreateBuffer(VAContextID context,
                                  VABufferType type,
                                  unsigned int size_per_element,
                                  unsigned int num_elements,
                                  const void* data);
  bool BufferExists(FakeBuffer::IdType id);
  const FakeBuffer& GetBuffer(FakeBuffer::IdType id);
  void DestroyBuffer(FakeBuffer::IdType id);

  void CreateImage(const VAImageFormat& format,
                   int width,
                   int height,
                   VAImage* va_image);
  bool ImageExists(FakeImage::IdType id);
  const FakeImage& GetImage(FakeImage::IdType id);
  void DestroyImage(FakeImage::IdType id);

 private:
  // |scoped_bo_mapping_factory_| is used by FakeSurface to map BOs. It needs
  // to be declared before |surface_| since we pass a reference to
  // |scoped_bo_mapping_factory_| when creating a FakeSurface. Therefore,
  // |scoped_bo_mapping_factory_| should outlive all FakeSurface instances.
  ScopedBOMappingFactory scoped_bo_mapping_factory_;
  ObjectTracker<FakeConfig> config_;
  ObjectTracker<FakeSurface> surface_;
  ObjectTracker<FakeContext> context_;
  ObjectTracker<FakeBuffer> buffers_;

  // The FakeImage instances in |images_| reference FakeBuffer instances in
  // |buffers_|, so the latter should outlive the former.
  ObjectTracker<FakeImage> images_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_