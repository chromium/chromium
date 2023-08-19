// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_
#define MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_

#include <va/va.h>

#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_config.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_context.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_surface.h"
#include "media/gpu/vaapi/test/fake_libva_driver/object_tracker.h"

namespace media::internal {

// FakeDriver is used to keep track of all the state that exists between a call
// to vaInitialize() and a call to vaTerminate(). All public methods are
// thread-safe.
class FakeDriver {
 public:
  FakeDriver();
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

 private:
  ObjectTracker<FakeConfig> config_;
  ObjectTracker<FakeSurface> surface_;
  ObjectTracker<FakeContext> context_;
  ObjectTracker<FakeBuffer> buffers_;
};

}  // namespace media::internal

#endif  // MEDIA_GPU_VAAPI_TEST_FAKE_LIBVA_DRIVER_FAKE_DRIVER_H_