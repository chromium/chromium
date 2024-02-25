// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

using testing::NiceMock;

namespace gpu {

// This is a mock with a small amount of fake functionality too.
class MockTextureOwner : public TextureOwner {
 public:
  MockTextureOwner(GLuint fake_texture_id,
                   gl::GLContext* fake_context,
                   gl::GLSurface* fake_surface,
                   bool binds_texture_on_update = false);

  MOCK_CONST_METHOD0(GetTextureId, GLuint());
  MOCK_CONST_METHOD0(GetContext, gl::GLContext*());
  MOCK_CONST_METHOD0(GetSurface, gl::GLSurface*());
  MOCK_CONST_METHOD0(CreateJavaSurface, gl::ScopedJavaSurface());
  MOCK_METHOD0(UpdateTexImage, void());
  MOCK_METHOD0(ReleaseBackBuffers, void());
  MOCK_METHOD0(ReleaseResources, void());
  MOCK_METHOD1(SetFrameAvailableCallback, void(const base::RepeatingClosure&));
  MOCK_METHOD3(GetCodedSizeAndVisibleRect,
               bool(gfx::Size rotated_visible_size,
                    gfx::Size* coded_size,
                    gfx::Rect* visible_rect));
  MOCK_METHOD1(RunWhenBufferIsAvailable, void(base::OnceClosure));

  MOCK_METHOD2(OnMemoryDump,
               bool(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd));

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override {
    get_a_hardware_buffer_count++;
    return nullptr;
  }

  raw_ptr<gl::GLContext> fake_context;
  raw_ptr<gl::GLSurface> fake_surface;
  int get_a_hardware_buffer_count = 0;

 protected:
  ~MockTextureOwner() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCK_TEXTURE_OWNER_H_
