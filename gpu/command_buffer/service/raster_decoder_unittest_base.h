// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_context_mock.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/raster_decoder.h"
#include "gpu/command_buffer/service/raster_decoder_mock.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

namespace gles2 {
class ImageManager;
class MockCopyTextureResourceManager;
}  // namespace gles2

namespace raster {

class RasterDecoderTestBase : public ::testing::TestWithParam<bool>,
                              public DecoderClient {
 public:
  RasterDecoderTestBase();
  ~RasterDecoderTestBase() override;

  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheShader(const std::string& key, const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  bool OnWaitSyncToken(const gpu::SyncToken&) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override {}

  // Template to call glGenXXX functions.
  template <typename T>
  void GenHelper(GLuint client_id) {
    int8_t buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  // This template exists solely so we can specialize it for
  // certain commands.
  template <typename T, int id>
  void SpecializedSetup(bool valid) {}

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  void ClearSharedMemory() {
    memset(shared_memory_base_, kInitialMemoryValue, kSharedBufferSize);
  }

  void SetUp() override;
  void TearDown() override;

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd) + data_size),
                                &entries_processed);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_);
  }

  template <typename T>
  T GetSharedMemoryAsWithOffset(uint32_t offset) {
    void* ptr = reinterpret_cast<int8_t*>(shared_memory_address_) + offset;
    return reinterpret_cast<T>(ptr);
  }

  gles2::TextureRef* GetTexture(GLuint client_id) {
    return group_->texture_manager()->GetTexture(client_id);
  }

  void SetBucketData(uint32_t bucket_id, const void* data, uint32_t data_size);
  void SetBucketAsCString(uint32_t bucket_id, const char* str);
  // If we want a valid bucket, just set |count_in_header| as |count|,
  // and set |str_end| as 0.
  void SetBucketAsCStrings(uint32_t bucket_id,
                           GLsizei count,
                           const char** str,
                           GLsizei count_in_header,
                           char str_end);

  void AddExpectationsForVertexAttribManager();
  void AddExpectationsForBindVertexArrayOES();
  void AddExpectationsForRestoreAttribState(GLuint attrib);

  struct InitState {
    InitState();
    ~InitState();

    std::vector<std::string> extensions = {"GL_ARB_sync"};
    bool lose_context_when_out_of_memory = false;
    gpu::GpuDriverBugWorkarounds workarounds;
    std::string gl_version = "2.1";
  };

  void InitDecoder(const InitState& init);
  void ResetDecoder();

  const gles2::ContextGroup& group() const { return *group_.get(); }

  void LoseContexts(error::ContextLostReason reason) const {
    group_->LoseContexts(reason);
  }

  error::ContextLostReason GetContextLostReason() const {
    return command_buffer_service_->GetState().context_lost_reason;
  }

  ::testing::StrictMock<::gl::MockGLInterface>* GetGLMock() const {
    return gl_.get();
  }

  RasterDecoder* GetDecoder() const { return decoder_.get(); }
  gles2::ImageManager* GetImageManagerForTest() {
    return decoder_->GetImageManagerForTest();
  }

  typedef gles2::TestHelper::AttribInfo AttribInfo;
  typedef gles2::TestHelper::UniformInfo UniformInfo;

  void SetupInitCapabilitiesExpectations(bool es3_capable);
  void SetupInitStateExpectations(bool es3_capable);
  void SetupInitStateManualExpectations(bool es3_capable);
  void SetupInitStateManualExpectationsForDoLineWidth(GLfloat width);
  void ExpectEnableDisable(GLenum cap, bool enable);

  void SetupTexture();

  // Note that the error is returned as GLint instead of GLenum.
  // This is because there is a mismatch in the types of GLenum and
  // the error values GL_NO_ERROR, GL_INVALID_ENUM, etc. GLenum is
  // typedef'd as unsigned int while the error values are defined as
  // integers. This is problematic for template functions such as
  // EXPECT_EQ that expect both types to be the same.
  GLint GetGLError();

  void DoBindTexture(GLenum target, GLuint client_id, GLuint service_id);
  void DoDeleteTexture(GLuint client_id, GLuint service_id);
  void SetScopedTextureBinderExpectations(GLenum target);
  void DoTexStorage2D(GLuint client_id,
                      GLsizei width,
                      GLsizei height);

  void SetupClearTextureExpectations(GLuint service_id,
                                     GLuint old_service_id,
                                     GLenum bind_target,
                                     GLenum target,
                                     GLint level,
                                     GLenum format,
                                     GLenum type,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLuint bound_pixel_unpack_buffer);

  GLvoid* BufferOffset(unsigned i) {
    return static_cast<int8_t*>(nullptr) + (i);
  }

 protected:
  static const GLint kMaxTextureSize = 2048;
  static const GLint kNumTextureUnits = 8;
  static const GLint kNumVertexAttribs = 16;

  static const GLint kViewportX = 0;
  static const GLint kViewportY = 0;
  static const GLint kViewportWidth = 1;
  static const GLint kViewportHeight = 1;

  static const GLuint kServiceBufferId = 301;
  static const GLuint kServiceTextureId = 304;
  static const GLuint kServiceVertexArrayId = 310;

  static const size_t kSharedBufferSize = 2048;
  static const uint32_t kSharedMemoryOffset = 132;
  static const int32_t kInvalidSharedMemoryId =
      FakeCommandBufferServiceBase::kTransferBufferBaseId - 1;
  static const uint32_t kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32_t kInitialResult = 0xBDBDBDBDu;
  static const uint8_t kInitialMemoryValue = 0xBDu;

  static const uint32_t kNewClientId = 501;
  static const uint32_t kNewServiceId = 502;
  static const uint32_t kInvalidClientId = 601;

  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  scoped_refptr<GLContextMock> context_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  gles2::TraceOutputter outputter_;
  std::unique_ptr<MockRasterDecoder> mock_decoder_;
  std::unique_ptr<RasterDecoder> decoder_;

  GLuint client_texture_id_;

  int32_t shared_memory_id_;
  uint32_t shared_memory_offset_;
  void* shared_memory_address_;
  void* shared_memory_base_;

  uint32_t immediate_buffer_[64];

  const bool ignore_cached_state_for_test_;

 private:
  GpuPreferences gpu_preferences_;
  gles2::MailboxManagerImpl mailbox_manager_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  gles2::ImageManager image_manager_;
  ServiceDiscardableManager discardable_manager_;
  SharedImageManager shared_image_manager_;
  scoped_refptr<gles2::ContextGroup> group_;
  base::MessageLoop message_loop_;
  gles2::MockCopyTextureResourceManager* copy_texture_manager_;  // not owned
};

class RasterDecoderManualInitTest : public RasterDecoderTestBase {
 public:
  RasterDecoderManualInitTest() = default;

  // Override default setup so nothing gets setup.
  void SetUp() override {}
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_BASE_H_
