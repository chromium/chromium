// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests for GLES2Implementation.

#include "gpu/command_buffer/client/gles2_implementation.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/mock_transfer_buffer.h"
#include "gpu/command_buffer/client/program_info_manager.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/ring_buffer.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
#define GLES2_SUPPORT_CLIENT_SIDE_ARRAYS
#endif

using testing::_;
using testing::AtLeast;
using testing::AnyNumber;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Pointee;
using testing::SaveArg;
using testing::Sequence;
using testing::StrictMock;
using testing::Truly;
using testing::Return;
using testing::ReturnRef;

namespace gpu {
namespace gles2 {

ACTION_P2(SetMemory, dst, obj) {
  memcpy(dst, &obj, sizeof(obj));
}

ACTION_P3(SetMemoryFromArray, dst, array, size) {
  memcpy(dst, array, size);
}

// Used to help set the transfer buffer result to SizedResult of a single value.
template <typename T>
class SizedResultHelper {
 public:
  explicit SizedResultHelper(T result)
      : size_(sizeof(result)) {
    memcpy(result_, &result, sizeof(T));
  }

 private:
  uint32_t size_;
  char result_[sizeof(T)];
};

// Struct to make it easy to pass a vec4 worth of floats.
struct FourFloats {
  FourFloats(float _x, float _y, float _z, float _w)
      : x(_x),
        y(_y),
        z(_z),
        w(_w) {
  }

  float x;
  float y;
  float z;
  float w;
};

#pragma pack(push, 1)
// Struct that holds 7 characters.
struct Str7 {
  char str[7];
};
#pragma pack(pop)

// API wrapper for Buffers.
class GenBuffersAPI {
 public:
  static void Gen(GLES2Implementation* gl_impl, GLsizei n, GLuint* ids) {
    gl_impl->GenBuffers(n, ids);
  }

  static void Delete(GLES2Implementation* gl_impl,
                     GLsizei n,
                     const GLuint* ids) {
    gl_impl->DeleteBuffers(n, ids);
  }
};

// API wrapper for Renderbuffers.
class GenRenderbuffersAPI {
 public:
  static void Gen(GLES2Implementation* gl_impl, GLsizei n, GLuint* ids) {
    gl_impl->GenRenderbuffers(n, ids);
  }

  static void Delete(GLES2Implementation* gl_impl,
                     GLsizei n,
                     const GLuint* ids) {
    gl_impl->DeleteRenderbuffers(n, ids);
  }
};

// API wrapper for Textures.
class GenTexturesAPI {
 public:
  static void Gen(GLES2Implementation* gl_impl, GLsizei n, GLuint* ids) {
    gl_impl->GenTextures(n, ids);
  }

  static void Delete(GLES2Implementation* gl_impl,
                     GLsizei n,
                     const GLuint* ids) {
    gl_impl->DeleteTextures(n, ids);
  }
};

class GLES2ImplementationTest : public testing::Test {
 protected:
  static const int kNumTestContexts = 2;
  static const uint8_t kInitialValue = 0xBD;
  static const int32_t kNumCommandEntries = 500;
  static const int32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const size_t kTransferBufferSize = 512;

  static const GLint kMaxCombinedTextureImageUnits = 8;
  static const GLint kMaxCubeMapTextureSize = 64;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxRenderbufferSize = 64;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxTextureSize = 128;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexAttribs = 8;
  static const GLint kMaxVertexTextureImageUnits = 0;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kMaxViewportWidth = 8192;
  static const GLint kMaxViewportHeight = 6144;
  static const GLint kNumCompressedTextureFormats = 0;
  static const GLint kNumShaderBinaryFormats = 0;
  static const GLuint kMaxTransformFeedbackSeparateAttribs = 4;
  static const GLuint kMaxUniformBufferBindings = 36;
  static const GLuint kStartId = 1024;
  static const GLuint kBuffersStartId = 1;
  static const GLuint kFramebuffersStartId = 1;
  static const GLuint kProgramsAndShadersStartId = 1;
  static const GLuint kRenderbuffersStartId = 1;
  static const GLuint kSamplersStartId = 1;
  static const GLuint kTexturesStartId = 1;
  static const GLuint kTransformFeedbacksStartId = 1;
  static const GLuint kQueriesStartId = 1;
  static const GLuint kVertexArraysStartId = 1;

  typedef MockTransferBuffer::ExpectedMemoryInfo ExpectedMemoryInfo;

  class TestContext {
   public:
    TestContext() : commands_(nullptr), token_(0) {}

    bool Initialize(ShareGroup* share_group,
                    bool bind_generates_resource_client,
                    bool bind_generates_resource_service,
                    bool lose_context_when_out_of_memory,
                    bool transfer_buffer_initialize_fail,
                    bool sync_query,
                    bool occlusion_query_boolean,
                    bool timer_queries,
                    int major_version,
                    int minor_version) {
      SharedMemoryLimits limits = SharedMemoryLimitsForTesting();
      command_buffer_.reset(new StrictMock<MockClientCommandBuffer>());

      transfer_buffer_.reset(
          new MockTransferBuffer(command_buffer_.get(),
                                 kTransferBufferSize,
                                 GLES2Implementation::kStartingOffset,
                                 GLES2Implementation::kAlignment,
                                 transfer_buffer_initialize_fail));

      helper_.reset(new GLES2CmdHelper(command_buffer()));
      helper_->Initialize(limits.command_buffer_size);

      gpu_control_.reset(new StrictMock<MockClientGpuControl>());
      gl_capabilities_.VisitPrecisions(
          [](GLenum shader, GLenum type,
             GLCapabilities::ShaderPrecision* precision) {
            precision->min_range = 3;
            precision->max_range = 5;
            precision->precision = 7;
          });
      gl_capabilities_.max_combined_texture_image_units =
          kMaxCombinedTextureImageUnits;
      gl_capabilities_.max_cube_map_texture_size = kMaxCubeMapTextureSize;
      gl_capabilities_.max_fragment_uniform_vectors =
          kMaxFragmentUniformVectors;
      gl_capabilities_.max_renderbuffer_size = kMaxRenderbufferSize;
      gl_capabilities_.max_texture_image_units = kMaxTextureImageUnits;
      capabilities_.max_texture_size = kMaxTextureSize;
      gl_capabilities_.max_texture_size = kMaxTextureSize;
      gl_capabilities_.max_varying_vectors = kMaxVaryingVectors;
      gl_capabilities_.max_vertex_attribs = kMaxVertexAttribs;
      gl_capabilities_.max_vertex_texture_image_units =
          kMaxVertexTextureImageUnits;
      gl_capabilities_.max_vertex_uniform_vectors = kMaxVertexUniformVectors;
      gl_capabilities_.max_viewport_width = kMaxViewportWidth;
      gl_capabilities_.max_viewport_height = kMaxViewportHeight;
      gl_capabilities_.num_compressed_texture_formats =
          kNumCompressedTextureFormats;
      gl_capabilities_.num_shader_binary_formats = kNumShaderBinaryFormats;
      gl_capabilities_.max_transform_feedback_separate_attribs =
          kMaxTransformFeedbackSeparateAttribs;
      gl_capabilities_.max_uniform_buffer_bindings = kMaxUniformBufferBindings;
      gl_capabilities_.bind_generates_resource_chromium =
          bind_generates_resource_service ? 1 : 0;
      capabilities_.sync_query = sync_query;
      gl_capabilities_.sync_query = sync_query;
      gl_capabilities_.occlusion_query_boolean = occlusion_query_boolean;
      gl_capabilities_.timer_queries = timer_queries;
      gl_capabilities_.major_version = major_version;
      gl_capabilities_.minor_version = minor_version;
      EXPECT_CALL(*gpu_control_, GetCapabilities())
          .WillOnce(ReturnRef(capabilities_));
      EXPECT_CALL(*gpu_control_, GetGLCapabilities())
          .WillOnce(ReturnRef(gl_capabilities_));

      {
        InSequence sequence;

        const bool support_client_side_arrays = true;
        gl_.reset(new GLES2Implementation(helper_.get(),
                                          share_group,
                                          transfer_buffer_.get(),
                                          bind_generates_resource_client,
                                          lose_context_when_out_of_memory,
                                          support_client_side_arrays,
                                          gpu_control_.get()));
      }

      // The client should be set to something non-null.
      EXPECT_CALL(*gpu_control_, SetGpuControlClient(gl_.get())).Times(1);

      if (gl_->Initialize(limits) != gpu::ContextResult::kSuccess)
        return false;

      helper_->CommandBufferHelper::Finish();
      Mock::VerifyAndClearExpectations(gl_.get());

      scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
      commands_ = static_cast<CommandBufferEntry*>(ring_buffer->memory()) +
                  command_buffer()->GetServicePutOffset();
      ClearCommands();
      EXPECT_TRUE(transfer_buffer_->InSync());

      Mock::VerifyAndClearExpectations(command_buffer());
      return true;
    }

    void TearDown() {
      Mock::VerifyAndClear(gl_.get());
      EXPECT_CALL(*command_buffer(), OnFlush()).Times(AnyNumber());
      // For command buffer.
      EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
          .Times(AtLeast(1));
      // The client should be unset.
      EXPECT_CALL(*gpu_control_, SetGpuControlClient(nullptr)).Times(1);
      gl_.reset();
    }

    MockClientCommandBuffer* command_buffer() const {
      return command_buffer_.get();
    }

    int GetNextToken() { return ++token_; }

    void ClearCommands() {
      scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
      memset(ring_buffer->memory(), kInitialValue, ring_buffer->size());
    }

    std::unique_ptr<MockClientCommandBuffer> command_buffer_;
    std::unique_ptr<MockClientGpuControl> gpu_control_;
    std::unique_ptr<GLES2CmdHelper> helper_;
    std::unique_ptr<MockTransferBuffer> transfer_buffer_;
    std::unique_ptr<GLES2Implementation> gl_;
    raw_ptr<CommandBufferEntry> commands_;
    int token_;
    Capabilities capabilities_;
    GLCapabilities gl_capabilities_;
  };

  GLES2ImplementationTest() : commands_(nullptr) {}

  void SetUp() override;
  void TearDown() override;

  bool NoCommandsWritten() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    const uint8_t* cmds = static_cast<const uint8_t*>(ring_buffer->memory());
    const uint8_t* end = cmds + ring_buffer->size();
    for (; cmds < end; ++cmds) {
      if (*cmds != kInitialValue) {
        return false;
      }
    }
    return true;
  }

  QueryTracker::Query* GetQuery(GLuint id) {
    return gl_->query_tracker_->GetQuery(id);
  }

  QueryTracker* GetQueryTracker() {
    return gl_->query_tracker_.get();
  }

  struct ContextInitOptions {
    ContextInitOptions()
        : bind_generates_resource_client(true),
          bind_generates_resource_service(true),
          lose_context_when_out_of_memory(false),
          transfer_buffer_initialize_fail(false),
          sync_query(true),
          occlusion_query_boolean(true),
          timer_queries(true),
          major_version(2),
          minor_version(0) {}

    bool bind_generates_resource_client;
    bool bind_generates_resource_service;
    bool lose_context_when_out_of_memory;
    bool transfer_buffer_initialize_fail;
    bool sync_query;
    bool occlusion_query_boolean;
    bool timer_queries;
    int major_version;
    int minor_version;
  };

  bool Initialize(const ContextInitOptions& init_options) {
    bool success = true;
    share_group_ = new ShareGroup(init_options.bind_generates_resource_client,
                                  0 /* tracing_id */);

    for (int i = 0; i < kNumTestContexts; i++) {
      if (!test_contexts_[i].Initialize(
              share_group_.get(),
              init_options.bind_generates_resource_client,
              init_options.bind_generates_resource_service,
              init_options.lose_context_when_out_of_memory,
              init_options.transfer_buffer_initialize_fail,
              init_options.sync_query,
              init_options.occlusion_query_boolean,
              init_options.timer_queries,
              init_options.major_version,
              init_options.minor_version))
        success = false;
    }

    // Default to test context 0.
    gpu_control_ = test_contexts_[0].gpu_control_.get();
    helper_ = test_contexts_[0].helper_.get();
    transfer_buffer_ = test_contexts_[0].transfer_buffer_.get();
    gl_ = test_contexts_[0].gl_.get();
    commands_ = test_contexts_[0].commands_;
    return success;
  }

  MockClientCommandBuffer* command_buffer() const {
    return test_contexts_[0].command_buffer_.get();
  }

  int GetNextToken() { return test_contexts_[0].GetNextToken(); }

  const void* GetPut() {
    return helper_->GetSpace(0);
  }

  void ClearCommands() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    memset(ring_buffer->memory(), kInitialValue, ring_buffer->size());
  }

  size_t MaxTransferBufferSize() {
    return transfer_buffer_->MaxTransferBufferSize();
  }

  void SetMappedMemoryLimit(size_t limit) {
    gl_->mapped_memory_->set_max_allocated_bytes(limit);
  }

  ExpectedMemoryInfo GetExpectedMemory(size_t size) {
    return transfer_buffer_->GetExpectedMemory(size);
  }

  ExpectedMemoryInfo GetExpectedResultMemory(size_t size) {
    return transfer_buffer_->GetExpectedResultMemory(size);
  }

  ExpectedMemoryInfo GetExpectedMappedMemory(size_t size) {
    ExpectedMemoryInfo mem;

    // Temporarily allocate memory and expect that memory block to be reused.
    mem.ptr = static_cast<uint8_t*>(
        gl_->mapped_memory_->Alloc(size, &mem.id, &mem.offset));
    gl_->mapped_memory_->Free(mem.ptr);

    return mem;
  }

  // Sets the ProgramInfoManager. The manager will be owned
  // by the ShareGroup.
  void SetProgramInfoManager(ProgramInfoManager* manager) {
    gl_->share_group()->SetProgramInfoManagerForTesting(manager);
  }

  int CheckError() {
    ExpectedMemoryInfo result =
        GetExpectedResultMemory(sizeof(cmds::GetError::Result));
    EXPECT_CALL(*command_buffer(), OnFlush())
        .WillOnce(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
        .RetiresOnSaturation();
    return gl_->GetError();
  }

  const std::string& GetLastError() {
    return gl_->GetLastError();
  }

  bool GetBucketContents(uint32_t bucket_id, std::vector<int8_t>* data) {
    return gl_->GetBucketContents(bucket_id, data);
  }

  bool AllowExtraTransferBufferSize() {
    return gl_->max_extra_transfer_buffer_size_ > 0;
  }

  static SharedMemoryLimits SharedMemoryLimitsForTesting() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = kCommandBufferSizeBytes;
    limits.start_transfer_buffer_size = kTransferBufferSize;
    limits.min_transfer_buffer_size = kTransferBufferSize;
    limits.max_transfer_buffer_size = kTransferBufferSize;
    limits.mapped_memory_reclaim_limit = SharedMemoryLimits::kNoLimit;
    return limits;
  }

  void ResetErrorMessageCallback() { gl_->error_message_callback_.Reset(); }

  TestContext test_contexts_[kNumTestContexts];

  scoped_refptr<ShareGroup> share_group_;
  raw_ptr<MockClientGpuControl> gpu_control_;
  raw_ptr<GLES2CmdHelper> helper_;
  raw_ptr<MockTransferBuffer> transfer_buffer_;
  raw_ptr<GLES2Implementation> gl_;
  raw_ptr<CommandBufferEntry> commands_;
};

void GLES2ImplementationTest::SetUp() {
  ContextInitOptions init_options;
  ASSERT_TRUE(Initialize(init_options));
}

void GLES2ImplementationTest::TearDown() {
  gl_ = nullptr;
  for (int i = 0; i < kNumTestContexts; i++)
    test_contexts_[i].TearDown();
}

class GLES2ImplementationManualInitTest : public GLES2ImplementationTest {
 protected:
  void SetUp() override {}
};

class GLES2ImplementationStrictSharedTest : public GLES2ImplementationTest {
 protected:
  void SetUp() override;

  template <class ResApi>
  void FlushGenerationTest() {
    GLuint id1, id2, id3;

    // Generate valid id.
    ResApi::Gen(gl_, 1, &id1);
    EXPECT_NE(id1, 0u);

    // Delete id1 and generate id2.  id1 should not be reused.
    ResApi::Delete(gl_, 1, &id1);
    ResApi::Gen(gl_, 1, &id2);
    EXPECT_NE(id2, 0u);
    EXPECT_NE(id2, id1);

    // Expect id1 reuse after Flush.
    gl_->Flush();
    ResApi::Gen(gl_, 1, &id3);
    EXPECT_EQ(id3, id1);
  }

  // Ids should not be reused unless the |Deleting| context does a Flush()
  // AND triggers a lazy release after that.
  template <class ResApi>
  void CrossContextGenerationTest() {
    GLES2Implementation* gl1 = test_contexts_[0].gl_.get();
    GLES2Implementation* gl2 = test_contexts_[1].gl_.get();
    GLuint id1, id2, id3;

    // Delete, no flush on context 1.  No reuse.
    ResApi::Gen(gl1, 1, &id1);
    ResApi::Delete(gl1, 1, &id1);
    ResApi::Gen(gl1, 1, &id2);
    EXPECT_NE(id1, id2);

    // Flush context 2.  Still no reuse.
    gl2->Flush();
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id2, id3);

    // Flush on context 1, but no lazy release.  Still no reuse.
    gl1->Flush();
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_NE(id1, id3);

    // Lazy release triggered by another Delete.  Should reuse id1.
    ResApi::Delete(gl1, 1, &id2);
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_EQ(id1, id3);
  }

  // Same as CrossContextGenerationTest(), but triggers an Auto Flush on
  // the Delete().  Tests an edge case regression.
  template <class ResApi>
  void CrossContextGenerationAutoFlushTest() {
    GLES2Implementation* gl1 = test_contexts_[0].gl_.get();
    GLES2Implementation* gl2 = test_contexts_[1].gl_.get();
    GLuint id1, id2, id3;

    // Delete, no flush on context 1.  No reuse.
    // By half filling the buffer, an internal flush is forced on the Delete().
    ResApi::Gen(gl1, 1, &id1);
    gl1->helper()->Noop(kNumCommandEntries / 2);
    ResApi::Delete(gl1, 1, &id1);
    ResApi::Gen(gl1, 1, &id2);
    EXPECT_NE(id1, id2);

    // Flush context 2.  Still no reuse.
    gl2->Flush();
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id2, id3);

    // Flush on context 1, but no lazy release.  Still no reuse.
    gl1->Flush();
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_NE(id1, id3);

    // Lazy release triggered by another Delete.  Should reuse id1.
    ResApi::Delete(gl1, 1, &id2);
    ResApi::Gen(gl2, 1, &id3);
    EXPECT_EQ(id1, id3);
  }

  // Require that deleting definitely-invalid IDs produces an error.
  template <class ResApi>
  void DeletingInvalidIdGeneratesError() {
    GLES2Implementation* gl1 = test_contexts_[0].gl_.get();
    GLuint id1;
    ResApi::Gen(gl1, 1, &id1);
    const GLuint kDefinitelyInvalidId = 0xBEEF;
    EXPECT_EQ(GL_NO_ERROR, CheckError());
    ResApi::Delete(gl1, 1, &kDefinitelyInvalidId);
    EXPECT_EQ(GL_INVALID_VALUE, CheckError());
  }

  // Require that double-deleting IDs produces an error.
  template <class ResApi>
  void DoubleDeletingIdGeneratesError() {
    GLES2Implementation* gl1 = test_contexts_[0].gl_.get();
    GLuint id1;
    ResApi::Gen(gl1, 1, &id1);
    ResApi::Delete(gl1, 1, &id1);
    EXPECT_EQ(GL_NO_ERROR, CheckError());
    ResApi::Delete(gl1, 1, &id1);
    EXPECT_EQ(GL_INVALID_VALUE, CheckError());
  }
};

void GLES2ImplementationStrictSharedTest::SetUp() {
  ContextInitOptions init_options;
  init_options.bind_generates_resource_client = false;
  init_options.bind_generates_resource_service = false;
  ASSERT_TRUE(Initialize(init_options));
}

class GLES3ImplementationTest : public GLES2ImplementationTest {
 protected:
  void SetUp() override;
};

void GLES3ImplementationTest::SetUp() {
  ContextInitOptions init_options;
  init_options.major_version = 3;
  init_options.minor_version = 0;
  ASSERT_TRUE(Initialize(init_options));
}

const uint8_t GLES2ImplementationTest::kInitialValue;
const int32_t GLES2ImplementationTest::kNumCommandEntries;
const int32_t GLES2ImplementationTest::kCommandBufferSizeBytes;
const size_t GLES2ImplementationTest::kTransferBufferSize;
const GLint GLES2ImplementationTest::kMaxCombinedTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxCubeMapTextureSize;
const GLint GLES2ImplementationTest::kMaxFragmentUniformVectors;
const GLint GLES2ImplementationTest::kMaxRenderbufferSize;
const GLint GLES2ImplementationTest::kMaxTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxTextureSize;
const GLint GLES2ImplementationTest::kMaxVaryingVectors;
const GLint GLES2ImplementationTest::kMaxVertexAttribs;
const GLint GLES2ImplementationTest::kMaxVertexTextureImageUnits;
const GLint GLES2ImplementationTest::kMaxVertexUniformVectors;
const GLint GLES2ImplementationTest::kNumCompressedTextureFormats;
const GLint GLES2ImplementationTest::kNumShaderBinaryFormats;
const GLuint GLES2ImplementationTest::kStartId;
const GLuint GLES2ImplementationTest::kBuffersStartId;
const GLuint GLES2ImplementationTest::kFramebuffersStartId;
const GLuint GLES2ImplementationTest::kProgramsAndShadersStartId;
const GLuint GLES2ImplementationTest::kRenderbuffersStartId;
const GLuint GLES2ImplementationTest::kSamplersStartId;
const GLuint GLES2ImplementationTest::kTexturesStartId;
const GLuint GLES2ImplementationTest::kTransformFeedbacksStartId;
const GLuint GLES2ImplementationTest::kQueriesStartId;
const GLuint GLES2ImplementationTest::kVertexArraysStartId;

TEST_F(GLES2ImplementationTest, Basic) {
  EXPECT_TRUE(gl_->share_group());
}

TEST_F(GLES2ImplementationTest, GetBucketContents) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const uint32_t kTestSize = MaxTransferBufferSize() + 32;

  auto buf = base::HeapArray<uint8_t>::Uninit(kTestSize);
  uint8_t* expected_data = buf.data();
  for (uint32_t ii = 0; ii < kTestSize; ++ii) {
    expected_data[ii] = ii * 3;
  }

  struct Cmds {
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token2;
    cmd::SetBucketSize set_bucket_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(sizeof(uint32_t));
  ExpectedMemoryInfo mem2 = GetExpectedMemory(
      kTestSize - MaxTransferBufferSize());

  Cmds expected;
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.get_bucket_data.Init(
      kBucketId, MaxTransferBufferSize(),
      kTestSize - MaxTransferBufferSize(), mem2.id, mem2.offset);
  expected.set_bucket_size2.Init(kBucketId, 0);
  expected.set_token2.Init(GetNextToken());

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(
          SetMemory(result1.ptr, kTestSize),
          SetMemoryFromArray(
              mem1.ptr, expected_data, MaxTransferBufferSize())))
      .WillOnce(SetMemoryFromArray(
          mem2.ptr, expected_data + MaxTransferBufferSize(),
          kTestSize - MaxTransferBufferSize()))
      .RetiresOnSaturation();

  std::vector<int8_t> data;
  GetBucketContents(kBucketId, &data);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ASSERT_EQ(kTestSize, data.size());
  EXPECT_EQ(0, memcmp(expected_data, &data[0], data.size()));
}

TEST_F(GLES2ImplementationTest, GetShaderPrecisionFormat) {
  struct Cmds {
    cmds::GetShaderPrecisionFormat cmd;
  };
  typedef cmds::GetShaderPrecisionFormat::Result Result;
  const unsigned kDummyType1 = 3;
  const unsigned kDummyType2 = 4;

  // The first call for dummy type 1 should trigger a command buffer request.
  GLint range1[2] = {0, 0};
  GLint precision1 = 0;
  Cmds expected1;
  ExpectedMemoryInfo client_result1 = GetExpectedResultMemory(4);
  expected1.cmd.Init(GL_FRAGMENT_SHADER, kDummyType1, client_result1.id,
                     client_result1.offset);
  Result server_result1 = {true, 14, 14, 10};
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(client_result1.ptr, server_result1))
      .RetiresOnSaturation();
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, kDummyType1, range1,
                                &precision1);
  const void* commands2 = GetPut();
  EXPECT_NE(commands_, commands2);
  EXPECT_EQ(0, memcmp(&expected1, commands_, sizeof(expected1)));
  EXPECT_EQ(range1[0], 14);
  EXPECT_EQ(range1[1], 14);
  EXPECT_EQ(precision1, 10);

  // The second call for dummy type 1 should use the cached value and avoid
  // triggering a command buffer request, so we do not expect a call to
  // OnFlush() here. We do expect the results to be correct though.
  GLint range2[2] = {0, 0};
  GLint precision2 = 0;
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, kDummyType1, range2,
                                &precision2);
  const void* commands3 = GetPut();
  EXPECT_EQ(commands2, commands3);
  EXPECT_EQ(range2[0], 14);
  EXPECT_EQ(range2[1], 14);
  EXPECT_EQ(precision2, 10);

  // If we then make a request for dummy type 2, we should get another command
  // buffer request since it hasn't been cached yet.
  GLint range3[2] = {0, 0};
  GLint precision3 = 0;
  Cmds expected3;
  ExpectedMemoryInfo result3 = GetExpectedResultMemory(4);
  expected3.cmd.Init(GL_FRAGMENT_SHADER, kDummyType2, result3.id,
                     result3.offset);
  Result result3_source = {true, 62, 62, 16};
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result3.ptr, result3_source))
      .RetiresOnSaturation();
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, kDummyType2, range3,
                                &precision3);
  const void* commands4 = GetPut();
  EXPECT_NE(commands3, commands4);
  EXPECT_EQ(0, memcmp(&expected3, commands3, sizeof(expected3)));
  EXPECT_EQ(range3[0], 62);
  EXPECT_EQ(range3[1], 62);
  EXPECT_EQ(precision3, 16);

  // Any call for predefined types should use the cached value from the
  // Capabilities  and avoid triggering a command buffer request, so we do not
  // expect a call to OnFlush() here. We do expect the results to be correct
  // though.
  GLint range4[2] = {0, 0};
  GLint precision4 = 0;
  gl_->GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range4,
                                &precision4);
  const void* commands5 = GetPut();
  EXPECT_EQ(commands4, commands5);
  EXPECT_EQ(range4[0], 3);
  EXPECT_EQ(range4[1], 5);
  EXPECT_EQ(precision4, 7);
}

TEST_F(GLES2ImplementationTest, GetShaderSource) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kShaderId = 456;
  const Str7 kString = {"foobar"};
  const char kBad = 0x12;
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetShaderSource get_shader_source;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 = GetExpectedResultMemory(sizeof(uint32_t));

  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_shader_source.Init(kShaderId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  char buf[sizeof(kString) + 1];
  memset(buf, kBad, sizeof(buf));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .RetiresOnSaturation();

  GLsizei length = 0;
  gl_->GetShaderSource(kShaderId, sizeof(buf), &length, buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(sizeof(kString) - 1, static_cast<size_t>(length));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, DrawArraysClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawArrays draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLint kFirst = 1;
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      std::size(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kFirst, kCount);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawArrays(GL_POINTS, kFirst, kCount);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawArraysInstancedANGLEClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::VertexAttribDivisorANGLE divisor;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawArraysInstancedANGLE draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLint kFirst = 1;
  const GLsizei kCount = 2;
  const GLuint kDivisor = 1;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      1 * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.divisor.Init(kAttribIndex2, kDivisor);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kFirst, kCount, 1);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(
      kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribDivisorANGLE(kAttribIndex2, kDivisor);
  gl_->DrawArraysInstancedANGLE(GL_POINTS, kFirst, kCount, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint16_t indices[] = {
      1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      std::size(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, 0);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT, indices);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffersIndexUint) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint32_t indices[] = {
      1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      std::size(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_INT, 0);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_INT, indices);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffersInvalidIndexUint) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint32_t indices[] = {1, 0x90000000};

  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;

  EXPECT_CALL(*command_buffer(), OnFlush())
      .Times(1)
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_INT, indices);

  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
}

TEST_F(GLES2ImplementationTest,
       DrawElementsClientSideBuffersServiceSideIndices) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::BindBuffer bind_to_index;
    cmds::GetMaxValueInBufferCHROMIUM get_max;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElements draw;
    cmds::BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kClientIndexBufferId = 0x789;
  const GLuint kIndexOffset = 0x40;
  const GLuint kMaxIndex = 2;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      std::size(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedResultMemory(sizeof(uint32_t));
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);


  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index.Init(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  expected.get_max.Init(kClientIndexBufferId, kCount, GL_UNSIGNED_SHORT,
                        kIndexOffset, mem1.id, mem1.offset);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, kIndexOffset);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(mem1.ptr,kMaxIndex))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT,
                    reinterpret_cast<const void*>(kIndexOffset));
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsInstancedANGLEClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint16_t indices[] = {
      1, 2,
  };
  struct Cmds {
    cmds::EnableVertexAttribArray enable1;
    cmds::EnableVertexAttribArray enable2;
    cmds::VertexAttribDivisorANGLE divisor;
    cmds::BindBuffer bind_to_index_emu;
    cmds::BufferData set_index_size;
    cmds::BufferSubData copy_data0;
    cmd::SetToken set_token0;
    cmds::BindBuffer bind_to_emu;
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::VertexAttribPointer set_pointer1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
    cmds::VertexAttribPointer set_pointer2;
    cmds::DrawElementsInstancedANGLE draw;
    cmds::BindBuffer restore;
    cmds::BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      std::size(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      1 * kNumComponents2 * sizeof(verts[0][0]);
  const GLuint kDivisor = 1;
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;
  const GLsizei kTotalSize = kSize1 + kSize2;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kIndexSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kSize1);
  ExpectedMemoryInfo mem3 = GetExpectedMemory(kSize2);

  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.divisor.Init(kAttribIndex2, kDivisor);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, mem1.id, mem1.offset);
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, mem2.id, mem2.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, mem3.id, mem3.offset);
  expected.set_token2.Init(GetNextToken());
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, 0, 1);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribDivisorANGLE(kAttribIndex2, kDivisor);
  gl_->DrawElementsInstancedANGLE(
      GL_POINTS, kCount, GL_UNSIGNED_SHORT, indices, 1);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetVertexBufferPointerv) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // It's all cached on the client side so no get commands are issued.
  struct Cmds {
    cmds::BindBuffer bind;
    cmds::VertexAttribPointer set_pointer;
  };

  Cmds expected;
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);

  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // now get them both.
  void* ptr1 = nullptr;
  void* ptr2 = nullptr;

  gl_->GetVertexAttribPointerv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr1);
  gl_->GetVertexAttribPointerv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr2);

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(static_cast<const void*>(&verts) == ptr1);
  EXPECT_TRUE(ptr2 == reinterpret_cast<void*>(kOffset2));
}

TEST_F(GLES2ImplementationTest, GetVertexAttrib) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // Only one set and one get because the client side buffer's info is stored
  // on the client side.
  struct Cmds {
    cmds::EnableVertexAttribArray enable;
    cmds::BindBuffer bind;
    cmds::VertexAttribPointer set_pointer;
    cmds::GetVertexAttribfv get2;  // for getting the value from attrib1
  };

  ExpectedMemoryInfo mem2 = GetExpectedResultMemory(16);

  Cmds expected;
  expected.enable.Init(kAttribIndex1);
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);
  expected.get2.Init(kAttribIndex1,
                     GL_CURRENT_VERTEX_ATTRIB,
                     mem2.id, mem2.offset);

  FourFloats current_attrib(1.2f, 3.4f, 5.6f, 7.8f);

  // One call to flush to wait for last call to GetVertexAttribiv
  // as others are all cached.
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(
          mem2.ptr, SizedResultHelper<FourFloats>(current_attrib)))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // first get the service side once to see that we make a command
  GLint buffer_id = 0;
  GLint enabled = 0;
  GLint size = 0;
  GLint stride = 0;
  GLint type = 0;
  GLint normalized = 1;
  float current[4] = { 0.0f, };

  gl_->GetVertexAttribiv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  EXPECT_EQ(kBufferId, static_cast<GLuint>(buffer_id));
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
  gl_->GetVertexAttribfv(
      kAttribIndex1, GL_CURRENT_VERTEX_ATTRIB, &current[0]);

  EXPECT_EQ(0, buffer_id);
  EXPECT_EQ(GL_TRUE, enabled);
  EXPECT_EQ(kNumComponents1, size);
  EXPECT_EQ(kStride1, stride);
  EXPECT_EQ(GL_FLOAT, type);
  EXPECT_EQ(GL_FALSE, normalized);
  EXPECT_EQ(0, memcmp(&current_attrib, &current, sizeof(current_attrib)));

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReservedIds) {
  // Only the get error command should be issued.
  struct Cmds {
    cmds::GetError get;
  };
  Cmds expected;

  ExpectedMemoryInfo mem1 = GetExpectedResultMemory(
      sizeof(cmds::GetError::Result));

  expected.get.Init(mem1.id, mem1.offset);

  // One call to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(mem1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideArrayId);
  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideElementArrayId);
  GLenum err = gl_->GetError();
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), err);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, ReadPixels2Reads) {
  struct Cmds {
    cmds::ReadPixels read1;
    cmd::SetToken set_token1;
    cmds::ReadPixels read2;
    cmd::SetToken set_token2;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth =
      (kTransferBufferSize - GLES2Implementation::kStartingOffset) /
      kBytesPerPixel;
  const GLint kHeight = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(kWidth * kHeight / 2 * kBytesPerPixel);
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));
  ExpectedMemoryInfo mem2 =
      GetExpectedMemory(kWidth * kHeight / 2 * kBytesPerPixel);
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));

  Cmds expected;
  expected.read1.Init(
      0, 0, kWidth, kHeight / 2, kFormat, kType,
      mem1.id, mem1.offset, result1.id, result1.offset,
      false);
  expected.set_token1.Init(GetNextToken());
  expected.read2.Init(
      0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      mem2.id, mem2.offset, result2.id, result2.offset, false);
  expected.set_token2.Init(GetNextToken());
  auto buffer =
      base::HeapArray<int8_t>::Uninit(kWidth * kHeight * kBytesPerPixel);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, static_cast<uint32_t>(1)))
      .WillOnce(SetMemory(result2.ptr, static_cast<uint32_t>(1)))
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.data());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReadPixelsBadFormatType) {
  struct Cmds {
    cmds::ReadPixels read;
    cmd::SetToken set_token;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth = 2;
  const GLint kHeight = 2;
  const GLenum kFormat = 0;
  const GLenum kType = 0;

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(kWidth * kHeight * kBytesPerPixel);
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::ReadPixels::Result));

  Cmds expected;
  expected.read.Init(
      0, 0, kWidth, kHeight, kFormat, kType,
      mem1.id, mem1.offset, result1.id, result1.offset, false);
  expected.set_token.Init(GetNextToken());
  auto buffer =
      base::HeapArray<int8_t>::Uninit(kWidth * kHeight * kBytesPerPixel);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .Times(1)
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.data());
}

TEST_F(GLES2ImplementationTest, FreeUnusedSharedMemory) {
  struct Cmds {
    cmds::BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kSize);

  Cmds expected;
  expected.buf.Init(
    kTarget, kOffset, kSize, mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != nullptr);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_CALL(*command_buffer(), DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  gl_->FreeUnusedSharedMemory();
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUM) {
  struct Cmds {
    cmds::BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  uint32_t offset = 0;
  Cmds expected;
  expected.buf.Init(
      kTarget, kOffset, kSize,
      command_buffer()->GetNextFreeTransferBufferId(), offset);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != nullptr);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUMBadArgs) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, -1, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, -1, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, kSize, GL_READ_ONLY);
  ASSERT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapBufferSubDataCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUM) {
  struct Cmds {
    cmds::TexSubImage2D tex;
    cmd::SetToken set_token;
  };
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  uint32_t offset = 0;
  Cmds expected;
  expected.tex.Init(
      GL_TEXTURE_2D, kLevel, kXOffset, kYOffset, kWidth, kHeight, kFormat,
      kType,
      command_buffer()->GetNextFreeTransferBufferId(), offset, GL_FALSE);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapTexSubImage2DCHROMIUM(
      GL_TEXTURE_2D,
      kLevel,
      kXOffset,
      kYOffset,
      kWidth,
      kHeight,
      kFormat,
      kType,
      GL_WRITE_ONLY);
  ASSERT_TRUE(mem != nullptr);
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUMBadArgs) {
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result5 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result6 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result7 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result5.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result6.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result7.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    -1,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    -1,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    -1,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    -1,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    -1,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_READ_ONLY);
  EXPECT_TRUE(mem == nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapTexSubImage2DCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMGoodArgs) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  const char kBad = 0x12;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  memset(buf, kBad, sizeof(buf));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
  EXPECT_EQ(sizeof(kString), static_cast<size_t>(size));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMBadArgs) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // try bufsize not big enough.
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, 6, &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
  ClearCommands();

  // try bad bufsize
  gl_->GetProgramInfoCHROMIUM(kProgramId, -1, &size, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  ClearCommands();
  // try no size ptr.
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), nullptr, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetUniformBlocksCHROMIUMGoodArgs) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  const char kBad = 0x12;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 =
      GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  memset(buf, kBad, sizeof(buf));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetUniformBlocksCHROMIUM get_uniform_blocks;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_uniform_blocks.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetUniformBlocksCHROMIUM(kProgramId, sizeof(buf), &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
  EXPECT_EQ(sizeof(kString), static_cast<size_t>(size));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

TEST_F(GLES2ImplementationTest, GetUniformBlocksCHROMIUMBadArgs) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const GLuint kProgramId = 123;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  ExpectedMemoryInfo result2 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result3 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  ExpectedMemoryInfo result4 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .WillOnce(SetMemory(result2.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result3.ptr, GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(result4.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // try bufsize not big enough.
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetUniformBlocksCHROMIUM get_uniform_blocks;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_uniform_blocks.Init(kProgramId, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetUniformBlocksCHROMIUM(kProgramId, 6, &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
  ClearCommands();

  // try bad bufsize
  gl_->GetUniformBlocksCHROMIUM(kProgramId, -1, &size, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  ClearCommands();
  // try no size ptr.
  gl_->GetUniformBlocksCHROMIUM(kProgramId, sizeof(buf), nullptr, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

// Test that things are cached
TEST_F(GLES2ImplementationTest, GetIntegerCacheRead) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  const PNameValue pairs[] = {
      {GL_ACTIVE_TEXTURE, GL_TEXTURE0, },
      {GL_TEXTURE_BINDING_2D, 0, },
      {GL_TEXTURE_BINDING_CUBE_MAP, 0, },
      {GL_TEXTURE_BINDING_EXTERNAL_OES, 0, },
      {GL_FRAMEBUFFER_BINDING, 0, },
      {GL_RENDERBUFFER_BINDING, 0, },
      {GL_ARRAY_BUFFER_BINDING, 0, },
      {GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, },
      {GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kMaxCombinedTextureImageUnits, },
      {GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMaxCubeMapTextureSize, },
      {GL_MAX_FRAGMENT_UNIFORM_VECTORS, kMaxFragmentUniformVectors, },
      {GL_MAX_RENDERBUFFER_SIZE, kMaxRenderbufferSize, },
      {GL_MAX_TEXTURE_IMAGE_UNITS, kMaxTextureImageUnits, },
      {GL_MAX_TEXTURE_SIZE, kMaxTextureSize, },
      {GL_MAX_VARYING_VECTORS, kMaxVaryingVectors, },
      {GL_MAX_VERTEX_ATTRIBS, kMaxVertexAttribs, },
      {GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, kMaxVertexTextureImageUnits, },
      {GL_MAX_VERTEX_UNIFORM_VECTORS, kMaxVertexUniformVectors, },
      {GL_NUM_COMPRESSED_TEXTURE_FORMATS, kNumCompressedTextureFormats, },
      {GL_NUM_SHADER_BINARY_FORMATS, kNumShaderBinaryFormats, }, };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_TRUE(NoCommandsWritten());
    EXPECT_EQ(pv.expected, v);
  }

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetIntegerDisjointValue) {
  ExpectedMemoryInfo mem = GetExpectedMappedMemory(sizeof(DisjointValueSync));
  gl_->SetDisjointValueSyncCHROMIUM();
  ASSERT_EQ(mem.id, GetQueryTracker()->DisjointCountSyncShmID());
  ASSERT_EQ(mem.offset, GetQueryTracker()->DisjointCountSyncShmOffset());
  DisjointValueSync* disjoint_sync =
      reinterpret_cast<DisjointValueSync*>(mem.ptr);

  ClearCommands();
  GLint disjoint_value = -1;
  gl_->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0, disjoint_value);

  // After setting disjoint, it should be true.
  disjoint_value = -1;
  disjoint_sync->SetDisjointCount(1);
  gl_->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(1, disjoint_value);

  // After checking disjoint, it should be false again.
  disjoint_value = -1;
  gl_->GetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0, disjoint_value);

  // Check for errors.
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetIntegerCacheWrite) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  gl_->ActiveTexture(GL_TEXTURE4);
  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 3);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 4);
  gl_->BindRenderbuffer(GL_RENDERBUFFER, 5);
  gl_->BindTexture(GL_TEXTURE_2D, 6);
  gl_->BindTexture(GL_TEXTURE_CUBE_MAP, 7);
  gl_->BindTexture(GL_TEXTURE_EXTERNAL_OES, 8);

  const PNameValue pairs[] = {{GL_ACTIVE_TEXTURE, GL_TEXTURE4, },
                              {GL_ARRAY_BUFFER_BINDING, 2, },
                              {GL_ELEMENT_ARRAY_BUFFER_BINDING, 3, },
                              {GL_FRAMEBUFFER_BINDING, 4, },
                              {GL_RENDERBUFFER_BINDING, 5, },
                              {GL_TEXTURE_BINDING_2D, 6, },
                              {GL_TEXTURE_BINDING_CUBE_MAP, 7, },
                              {GL_TEXTURE_BINDING_EXTERNAL_OES, 8, }, };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_EQ(pv.expected, v);
  }

  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

static bool CheckRect(int width,
                      int height,
                      GLenum format,
                      GLenum type,
                      int alignment,
                      const uint8_t* r1,
                      const uint8_t* r2) {
  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, 1, format, type, alignment, &size, &unpadded_row_size,
      &padded_row_size)) {
    return false;
  }

  int r2_stride = static_cast<int>(padded_row_size);

  for (int y = 0; y < height; ++y) {
    if (memcmp(r1, r2, unpadded_row_size) != 0) {
      return false;
    }
    r1 += padded_row_size;
    r2 += r2_stride;
  }
  return true;
}

ACTION_P7(CheckRectAction, width, height, format, type, alignment, r1, r2) {
  EXPECT_TRUE(CheckRect(
      width, height, format, type, alignment, r1, r2));
}

TEST_F(GLES2ImplementationTest, TexImage2D) {
  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  struct Cmds2 {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 4;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  static uint8_t pixels[] = {
      11, 12, 13, 13,  14,  15,  15,  16,  17,  101, 102, 103, 21, 22, 23,
      23, 24, 25, 25,  26,  27,  201, 202, 203, 31,  32,  33,  33, 34, 35,
      35, 36, 37, 123, 124, 125, 41,  42,  43,  43,  44,  45,  45, 46, 47,
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(sizeof(pixels));

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kFormat, kType,
      mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment,
      pixels, mem1.ptr));
}

TEST_F(GLES2ImplementationTest, TexImage2DViaMappedMem) {
  if (!AllowExtraTransferBufferSize()) {
    LOG(WARNING) << "Low memory device do not support MappedMem. Skipping test";
    return;
  }

  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLsizei kWidth = 3;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  const GLsizei kMaxHeight = (MaxTransferBufferSize() / padded_row_size) * 2;
  const GLsizei kHeight = kMaxHeight * 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));

  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  ExpectedMemoryInfo mem1 = GetExpectedMappedMemory(size);

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kFormat, kType,
      mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());
  gl_->TexImage2D(kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat,
                  kType, pixels.data());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(kWidth, kHeight, kFormat, kType,
                        kPixelStoreUnpackAlignment, pixels.data(), mem1.ptr));
}

// Test TexImage2D with 2 writes
TEST_F(GLES2ImplementationTest, TexImage2DViaTexSubImage2D) {
  // Set limit to 1 to effectively disable mapped memory.
  SetMappedMemoryLimit(1);

  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmds::TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    cmds::TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  const GLsizei kHeight = (MaxTransferBufferSize() / padded_row_size) * 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, kFormat, kType, kPixelStoreUnpackAlignment, &size,
      nullptr, nullptr));
  uint32_t half_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &half_size, nullptr, nullptr));

  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  ExpectedMemoryInfo mem1 = GetExpectedMemory(half_size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(half_size);

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, 0, kWidth, kHeight / 2, kFormat, kType,
      mem1.id, mem1.offset, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, 0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      mem2.id, mem2.offset, true);
  expected.set_token2.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer(), OnFlush())
  //     .WillOnce(CheckRectAction(
  //         kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
  //         false, pixels.data(),
  //         GetExpectedTransferAddressFromOffsetAs<uint8_t>(offset1,
  //         half_size)))
  //     .RetiresOnSaturation();

  gl_->TexImage2D(kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat,
                  kType, pixels.data());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(
      CheckRect(kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
                pixels.data() + kHeight / 2 * padded_row_size, mem2.ptr));
}

TEST_F(GLES2ImplementationTest, SubImage2DUnpack) {
  static const GLint unpack_alignments[] = { 1, 2, 4, 8 };

  static const GLenum kFormat = GL_RGB;
  static const GLenum kType = GL_UNSIGNED_BYTE;
  static const GLint kLevel = 0;
  static const GLint kBorder = 0;
  // We're testing using the unpack params to pull a subimage out of a larger
  // source of pixels. Here we specify the subimage by its border rows /
  // columns.
  static const GLint kSrcWidth = 33;
  static const GLint kSrcSubImageX0 = 11;
  static const GLint kSrcSubImageX1 = 20;
  static const GLint kSrcSubImageY0 = 18;
  static const GLint kSrcSubImageY1 = 23;
  static const GLint kSrcSubImageWidth = kSrcSubImageX1 - kSrcSubImageX0;
  static const GLint kSrcSubImageHeight = kSrcSubImageY1 - kSrcSubImageY0;

  // these are only used in the texsubimage tests
  static const GLint kTexWidth = 1023;
  static const GLint kTexHeight = 511;
  static const GLint kTexSubXOffset = 419;
  static const GLint kTexSubYOffset = 103;

  struct {
    cmds::PixelStorei pixel_store_i;
    cmds::TexImage2D tex_image_2d;
  } texImageExpected;

  struct  {
    cmds::PixelStorei pixel_store_i;
    cmds::TexImage2D tex_image_2d;
    cmds::TexSubImage2D tex_sub_image_2d;
  } texSubImageExpected;

  uint32_t pixel_size;
  PixelStoreParams pixel_params;
  // Makes sure the pixels size is large enough for all tests.
  pixel_params.alignment = 8;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
      kSrcWidth, kSrcSubImageY1, 1, kFormat, kType,
      pixel_params, &pixel_size, nullptr, nullptr, nullptr, nullptr));
  auto src_pixels = base::HeapArray<uint8_t>::WithSize(pixel_size);
  for (size_t i = 0; i < pixel_size; ++i) {
    src_pixels[i] = static_cast<uint8_t>(i % 255);
  }

  for (int sub = 0; sub < 2; ++sub) {
    for (size_t a = 0; a < std::size(unpack_alignments); ++a) {
      const void* commands = GetPut();

      GLint alignment = unpack_alignments[a];
      gl_->PixelStorei(GL_UNPACK_ALIGNMENT, alignment);
      gl_->PixelStorei(GL_UNPACK_ROW_LENGTH, kSrcWidth);
      gl_->PixelStorei(GL_UNPACK_SKIP_PIXELS, kSrcSubImageX0);
      gl_->PixelStorei(GL_UNPACK_SKIP_ROWS, kSrcSubImageY0);

      uint32_t client_size;
      uint32_t client_unpadded_row_size;
      uint32_t client_padded_row_size;
      uint32_t client_skip_size;
      {
        PixelStoreParams params;
        params.alignment = alignment;
        params.row_length = kSrcWidth;
        params.skip_pixels = kSrcSubImageX0;
        params.skip_rows = kSrcSubImageY0;
        ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
            kSrcSubImageWidth, kSrcSubImageHeight, 1, kFormat, kType, params,
            &client_size, &client_unpadded_row_size, &client_padded_row_size,
            &client_skip_size, nullptr));
        ASSERT_TRUE(client_size + client_skip_size <= pixel_size);
      }

      uint32_t service_size;
      uint32_t service_unpadded_row_size;
      uint32_t service_padded_row_size;
      uint32_t service_skip_size;
      {
        PixelStoreParams params;
        // For pixels we send to service side, we already applied all unpack
        // parameters except for UNPACK_ALIGNMENT.
        params.alignment = alignment;
        ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
            kSrcSubImageWidth, kSrcSubImageHeight, 1, kFormat, kType, params,
            &service_size, &service_unpadded_row_size, &service_padded_row_size,
            &service_skip_size, nullptr));
        ASSERT_TRUE(service_size <= MaxTransferBufferSize());
        ASSERT_TRUE(service_skip_size == 0);
        ASSERT_TRUE(client_unpadded_row_size == service_unpadded_row_size);
      }

      ExpectedMemoryInfo mem = GetExpectedMemory(service_size);
      if (sub) {
        gl_->TexImage2D(
            GL_TEXTURE_2D, kLevel, kFormat, kTexWidth, kTexHeight, kBorder,
            kFormat, kType, nullptr);
        gl_->TexSubImage2D(GL_TEXTURE_2D, kLevel, kTexSubXOffset,
                           kTexSubYOffset, kSrcSubImageWidth,
                           kSrcSubImageHeight, kFormat, kType,
                           src_pixels.data());
        texSubImageExpected.pixel_store_i.Init(GL_UNPACK_ALIGNMENT, alignment);
        texSubImageExpected.tex_image_2d.Init(
            GL_TEXTURE_2D, kLevel, kFormat, kTexWidth, kTexHeight,
            kFormat, kType, 0, 0);
        texSubImageExpected.tex_sub_image_2d.Init(
            GL_TEXTURE_2D, kLevel, kTexSubXOffset, kTexSubYOffset,
            kSrcSubImageWidth, kSrcSubImageHeight, kFormat, kType, mem.id,
            mem.offset, GL_FALSE);
        EXPECT_EQ(0, memcmp(&texSubImageExpected, commands,
                            sizeof(texSubImageExpected)));
      } else {
        gl_->TexImage2D(GL_TEXTURE_2D, kLevel, kFormat, kSrcSubImageWidth,
                        kSrcSubImageHeight, kBorder, kFormat, kType,
                        src_pixels.data());
        texImageExpected.pixel_store_i.Init(GL_UNPACK_ALIGNMENT, alignment);
        texImageExpected.tex_image_2d.Init(
            GL_TEXTURE_2D, kLevel, kFormat, kSrcSubImageWidth,
            kSrcSubImageHeight, kFormat, kType, mem.id, mem.offset);
        EXPECT_EQ(0, memcmp(&texImageExpected, commands,
                            sizeof(texImageExpected)));
      }
      for (int y = 0; y < kSrcSubImageHeight; ++y) {
        const uint8_t* src_row =
            src_pixels.data() + client_skip_size + y * client_padded_row_size;
        const uint8_t* dst_row = mem.ptr + y * service_padded_row_size;
        EXPECT_EQ(0, memcmp(src_row, dst_row, service_unpadded_row_size));
      }
      ClearCommands();
    }
  }
}

TEST_F(GLES3ImplementationTest, SubImage3DUnpack) {
  static const GLint unpack_alignments[] = { 1, 2, 4, 8 };

  static const GLenum kFormat = GL_RGB;
  static const GLenum kType = GL_UNSIGNED_BYTE;
  static const GLint kLevel = 0;
  static const GLint kBorder = 0;
  // We're testing using the unpack params to pull a subimage out of a larger
  // source of pixels. Here we specify the subimage by its border rows /
  // columns.
  static const GLint kSrcWidth = 23;
  static const GLint kSrcHeight = 7;
  static const GLint kSrcSubImageX0 = 11;
  static const GLint kSrcSubImageX1 = 16;
  static const GLint kSrcSubImageY0 = 1;
  static const GLint kSrcSubImageY1 = 4;
  static const GLint kSrcSubImageZ0 = 2;
  static const GLint kSrcSubImageZ1 = 5;
  static const GLint kSrcSubImageWidth = kSrcSubImageX1 - kSrcSubImageX0;
  static const GLint kSrcSubImageHeight = kSrcSubImageY1 - kSrcSubImageY0;
  static const GLint kSrcSubImageDepth = kSrcSubImageZ1 - kSrcSubImageZ0;

  // these are only used in the texsubimage tests
  static const GLint kTexWidth = 255;
  static const GLint kTexHeight = 127;
  static const GLint kTexDepth = 11;
  static const GLint kTexSubXOffset = 119;
  static const GLint kTexSubYOffset = 63;
  static const GLint kTexSubZOffset = 1;

  struct {
    cmds::PixelStorei pixel_store_i[3];
    cmds::TexImage3D tex_image_3d;
  } texImageExpected;

  struct  {
    cmds::PixelStorei pixel_store_i[3];
    cmds::TexImage3D tex_image_3d;
    cmds::TexSubImage3D tex_sub_image_3d;
  } texSubImageExpected;

  uint32_t pixel_size;
  PixelStoreParams pixel_params;
  // Makes sure the pixels size is large enough for all tests.
  pixel_params.alignment = 8;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
      kSrcWidth, kSrcHeight, kSrcSubImageZ1, kFormat, kType,
      pixel_params, &pixel_size, nullptr, nullptr, nullptr, nullptr));
  auto src_pixels = base::HeapArray<uint8_t>::WithSize(pixel_size);
  for (size_t i = 0; i < pixel_size; ++i) {
    src_pixels[i] = static_cast<uint8_t>(i % 255);
  }

  for (int sub = 0; sub < 2; ++sub) {
    for (size_t a = 0; a < std::size(unpack_alignments); ++a) {
      const void* commands = GetPut();

      GLint alignment = unpack_alignments[a];
      gl_->PixelStorei(GL_UNPACK_ALIGNMENT, alignment);
      gl_->PixelStorei(GL_UNPACK_ROW_LENGTH, kSrcWidth);
      gl_->PixelStorei(GL_UNPACK_IMAGE_HEIGHT, kSrcHeight);
      gl_->PixelStorei(GL_UNPACK_SKIP_PIXELS, kSrcSubImageX0);
      gl_->PixelStorei(GL_UNPACK_SKIP_ROWS, kSrcSubImageY0);
      gl_->PixelStorei(GL_UNPACK_SKIP_IMAGES, kSrcSubImageZ0);

      uint32_t client_size;
      uint32_t client_unpadded_row_size;
      uint32_t client_padded_row_size;
      uint32_t client_skip_size;
      {
        PixelStoreParams params;
        params.alignment = alignment;
        params.row_length = kSrcWidth;
        params.image_height = kSrcHeight;
        params.skip_pixels = kSrcSubImageX0;
        params.skip_rows = kSrcSubImageY0;
        params.skip_images = kSrcSubImageZ0;
        ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
            kSrcSubImageWidth, kSrcSubImageHeight, kSrcSubImageDepth,
            kFormat, kType, params,
            &client_size, &client_unpadded_row_size, &client_padded_row_size,
            &client_skip_size, nullptr));
        ASSERT_TRUE(client_size + client_skip_size <= pixel_size);
      }

      uint32_t service_size;
      uint32_t service_unpadded_row_size;
      uint32_t service_padded_row_size;
      uint32_t service_skip_size;
      {
        PixelStoreParams params;
        // For pixels we send to service side, we already applied all unpack
        // parameters except for UNPACK_ALIGNMENT.
        params.alignment = alignment;
        ASSERT_TRUE(GLES2Util::ComputeImageDataSizesES3(
            kSrcSubImageWidth, kSrcSubImageHeight, kSrcSubImageDepth,
            kFormat, kType, params,
            &service_size, &service_unpadded_row_size, &service_padded_row_size,
            &service_skip_size, nullptr));
        ASSERT_TRUE(service_size <= MaxTransferBufferSize());
        ASSERT_TRUE(service_skip_size == 0);
        ASSERT_TRUE(client_unpadded_row_size == service_unpadded_row_size);
      }

      ExpectedMemoryInfo mem = GetExpectedMemory(service_size);
      if (sub) {
        gl_->TexImage3D(
            GL_TEXTURE_3D, kLevel, kFormat, kTexWidth, kTexHeight, kTexDepth,
            kBorder, kFormat, kType, nullptr);
        gl_->TexSubImage3D(GL_TEXTURE_3D, kLevel, kTexSubXOffset,
                           kTexSubYOffset, kTexSubZOffset, kSrcSubImageWidth,
                           kSrcSubImageHeight, kSrcSubImageDepth, kFormat,
                           kType, src_pixels.data());
        texSubImageExpected.pixel_store_i[0].Init(
            GL_UNPACK_ALIGNMENT, alignment);
        texSubImageExpected.pixel_store_i[1].Init(
            GL_UNPACK_ROW_LENGTH, kSrcWidth);
        texSubImageExpected.pixel_store_i[2].Init(
            GL_UNPACK_IMAGE_HEIGHT, kSrcHeight);
        texSubImageExpected.tex_image_3d.Init(
            GL_TEXTURE_3D, kLevel, kFormat, kTexWidth, kTexHeight, kTexDepth,
            kFormat, kType, 0, 0);
        texSubImageExpected.tex_sub_image_3d.Init(
            GL_TEXTURE_3D, kLevel,
            kTexSubXOffset, kTexSubYOffset, kTexSubZOffset,
            kSrcSubImageWidth, kSrcSubImageHeight, kSrcSubImageDepth,
            kFormat, kType, mem.id, mem.offset, GL_FALSE);
        EXPECT_EQ(0, memcmp(&texSubImageExpected, commands,
                            sizeof(texSubImageExpected)));
      } else {
        gl_->TexImage3D(GL_TEXTURE_3D, kLevel, kFormat, kSrcSubImageWidth,
                        kSrcSubImageHeight, kSrcSubImageDepth, kBorder, kFormat,
                        kType, src_pixels.data());
        texImageExpected.pixel_store_i[0].Init(GL_UNPACK_ALIGNMENT, alignment);
        texImageExpected.pixel_store_i[1].Init(
            GL_UNPACK_ROW_LENGTH, kSrcWidth);
        texImageExpected.pixel_store_i[2].Init(
            GL_UNPACK_IMAGE_HEIGHT, kSrcHeight);
        texImageExpected.tex_image_3d.Init(
            GL_TEXTURE_3D, kLevel, kFormat,
            kSrcSubImageWidth, kSrcSubImageHeight, kSrcSubImageDepth,
            kFormat, kType, mem.id, mem.offset);
        EXPECT_EQ(0, memcmp(&texImageExpected, commands,
                            sizeof(texImageExpected)));
      }
      for (int z = 0; z < kSrcSubImageDepth; ++z) {
        for (int y = 0; y < kSrcSubImageHeight; ++y) {
          const uint8_t* src_row =
              src_pixels.data() + client_skip_size +
              (kSrcHeight * z + y) * client_padded_row_size;
          const uint8_t* dst_row = mem.ptr +
              (kSrcSubImageHeight * z + y) * service_padded_row_size;
          EXPECT_EQ(0, memcmp(src_row, dst_row, service_unpadded_row_size));
        }
      }
      ClearCommands();
    }
  }
}

// Test texture related calls with invalid arguments.
TEST_F(GLES2ImplementationTest, TextureInvalidArguments) {
  struct Cmds {
    cmds::TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 4;
  const GLint kBorder = 0;
  const GLint kInvalidBorder = 1;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  static uint8_t pixels[] = {
      11, 12, 13, 13,  14,  15,  15,  16,  17,  101, 102, 103, 21, 22, 23,
      23, 24, 25, 25,  26,  27,  201, 202, 203, 31,  32,  33,  33, 34, 35,
      35, 36, 37, 123, 124, 125, 41,  42,  43,  43,  44,  45,  45, 46, 47,
  };

  // Verify that something works.

  ExpectedMemoryInfo mem1 = GetExpectedMemory(sizeof(pixels));

  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kFormat, kType,
      mem1.id, mem1.offset);
  expected.set_token.Init(GetNextToken());
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment,
      pixels, mem1.ptr));

  ClearCommands();

  // Use invalid border.
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kInvalidBorder, kFormat, kType,
      pixels);

  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());

  ClearCommands();

  // Checking for CompressedTexImage2D argument validation is a bit tricky due
  // to (runtime-detected) compression formats. Try to infer the error with an
  // aux check.
  const GLenum kCompressedFormat = GL_ETC1_RGB8_OES;
  gl_->CompressedTexImage2D(kTarget, kLevel, kCompressedFormat, kWidth, kHeight,
                            kBorder, std::size(pixels), pixels);

  // In the above, kCompressedFormat and std::size(pixels) are possibly wrong
  // values. First ensure that these do not cause failures at the client. If
  // this check ever fails, it probably means that client checks more than at
  // the time of writing of this test. In this case, more code needs to be
  // written for this test.
  EXPECT_FALSE(NoCommandsWritten());

  ClearCommands();

  // Changing border to invalid border should make the call fail at the client
  // checks.
  gl_->CompressedTexImage2D(kTarget, kLevel, kCompressedFormat, kWidth, kHeight,
                            kInvalidBorder, std::size(pixels), pixels);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());
}

TEST_F(GLES2ImplementationTest, TexImage3DSingleCommand) {
  struct Cmds {
    cmds::TexImage3D tex_image_3d;
  };
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;
  const GLsizei kDepth = 2;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, kDepth, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  // Makes sure we can just send over the data in one command.
  const GLsizei kHeight = MaxTransferBufferSize() / padded_row_size / kDepth;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, nullptr, nullptr));

  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  ExpectedMemoryInfo mem = GetExpectedMemory(size);

  Cmds expected;
  expected.tex_image_3d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kDepth,
      kFormat, kType, mem.id, mem.offset);

  gl_->TexImage3D(kTarget, kLevel, kFormat, kWidth, kHeight, kDepth, kBorder,
                  kFormat, kType, pixels.data());

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(kWidth, kHeight * kDepth, kFormat, kType,
                        kPixelStoreUnpackAlignment,
                        reinterpret_cast<uint8_t*>(pixels.data()), mem.ptr));
}

TEST_F(GLES2ImplementationTest, TexImage3DViaMappedMem) {
  if (!AllowExtraTransferBufferSize()) {
    LOG(WARNING) << "Low memory device do not support MappedMem. Skipping test";
    return;
  }

  struct Cmds {
    cmds::TexImage3D tex_image_3d;
  };
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;
  const GLsizei kDepth = 2;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, kDepth, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  // Makes sure we can just send over the data in one command.
  const GLsizei kMaxHeight = MaxTransferBufferSize() / padded_row_size / kDepth;
  const GLsizei kHeight = kMaxHeight * 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, nullptr, nullptr));

  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  ExpectedMemoryInfo mem = GetExpectedMappedMemory(size);

  Cmds expected;
  expected.tex_image_3d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kDepth,
      kFormat, kType, mem.id, mem.offset);

  gl_->TexImage3D(kTarget, kLevel, kFormat, kWidth, kHeight, kDepth, kBorder,
                  kFormat, kType, pixels.data());

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(kWidth, kHeight * kDepth, kFormat, kType,
                        kPixelStoreUnpackAlignment,
                        reinterpret_cast<uint8_t*>(pixels.data()), mem.ptr));
}

TEST_F(GLES2ImplementationTest, TexImage3DViaTexSubImage3D) {
  // Set limit to 1 to effectively disable mapped memory.
  SetMappedMemoryLimit(1);

  struct Cmds {
    cmds::TexImage3D tex_image_3d;
    cmds::TexSubImage3D tex_sub_image_3d1;
    cmd::SetToken set_token;
    cmds::TexSubImage3D tex_sub_image_3d2;
  };
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  // Makes sure the data is more than one command can hold.
  const GLsizei kHeight = MaxTransferBufferSize() / padded_row_size + 3;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, 1, kFormat, kType, kPixelStoreUnpackAlignment, &size,
      nullptr, nullptr));
  uint32_t first_size = padded_row_size * (kHeight - 3);
  uint32_t second_size =
      padded_row_size * 3 - (padded_row_size - unpadded_row_size);
  EXPECT_EQ(size, first_size + second_size);
  ExpectedMemoryInfo mem1 = GetExpectedMemory(first_size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(second_size);
  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  Cmds expected;
  expected.tex_image_3d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, 1, kFormat, kType, 0, 0);
  expected.tex_sub_image_3d1.Init(
      kTarget, kLevel, 0, 0, 0, kWidth, kHeight - 3, 1, kFormat, kType,
      mem1.id, mem1.offset, GL_TRUE);
  expected.tex_sub_image_3d2.Init(
      kTarget, kLevel, 0, kHeight - 3, 0, kWidth, 3, 1, kFormat, kType,
      mem2.id, mem2.offset, GL_TRUE);
  expected.set_token.Init(GetNextToken());

  gl_->TexImage3D(kTarget, kLevel, kFormat, kWidth, kHeight, 1, kBorder,
                  kFormat, kType, pixels.data());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

// Test TexSubImage3D with 4 writes
TEST_F(GLES2ImplementationTest, TexSubImage3D4Writes) {
  struct Cmds {
    cmds::TexSubImage3D tex_sub_image_3d1_1;
    cmd::SetToken set_token1;
    cmds::TexSubImage3D tex_sub_image_3d1_2;
    cmd::SetToken set_token2;
    cmds::TexSubImage3D tex_sub_image_3d2_1;
    cmd::SetToken set_token3;
    cmds::TexSubImage3D tex_sub_image_3d2_2;
  };
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kXOffset = 0;
  const GLint kYOffset = 0;
  const GLint kZOffset = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;
  const GLsizei kDepth = 2;

  uint32_t size = 0;
  uint32_t unpadded_row_size = 0;
  uint32_t padded_row_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, 2, 1, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  const GLsizei kHeight = MaxTransferBufferSize() / padded_row_size + 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSizes(
      kWidth, kHeight, kDepth, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, nullptr, nullptr));
  uint32_t first_size = (kHeight - 2) * padded_row_size;
  uint32_t second_size = 2 * padded_row_size;
  uint32_t third_size = first_size;
  uint32_t fourth_size = second_size - (padded_row_size - unpadded_row_size);
  EXPECT_EQ(size, first_size + second_size + third_size + fourth_size);

  auto pixels = base::HeapArray<uint8_t>::Uninit(size);
  for (uint32_t ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8_t>(ii);
  }

  ExpectedMemoryInfo mem1_1 = GetExpectedMemory(first_size);
  ExpectedMemoryInfo mem1_2 = GetExpectedMemory(second_size);
  ExpectedMemoryInfo mem2_1 = GetExpectedMemory(third_size);
  ExpectedMemoryInfo mem2_2 = GetExpectedMemory(fourth_size);

  Cmds expected;
  expected.tex_sub_image_3d1_1.Init(
      kTarget, kLevel, kXOffset, kYOffset, kZOffset,
      kWidth, kHeight - 2, 1, kFormat, kType,
      mem1_1.id, mem1_1.offset, GL_FALSE);
  expected.tex_sub_image_3d1_2.Init(
      kTarget, kLevel, kXOffset, kYOffset + kHeight - 2, kZOffset,
      kWidth, 2, 1, kFormat, kType, mem1_2.id, mem1_2.offset, GL_FALSE);
  expected.tex_sub_image_3d2_1.Init(
      kTarget, kLevel, kXOffset, kYOffset, kZOffset + 1,
      kWidth, kHeight - 2, 1, kFormat, kType,
      mem2_1.id, mem2_1.offset, GL_FALSE);
  expected.tex_sub_image_3d2_2.Init(
      kTarget, kLevel, kXOffset, kYOffset + kHeight - 2, kZOffset + 1,
      kWidth, 2, 1, kFormat, kType, mem2_2.id, mem2_2.offset, GL_FALSE);
  expected.set_token1.Init(GetNextToken());
  expected.set_token2.Init(GetNextToken());
  expected.set_token3.Init(GetNextToken());

  gl_->TexSubImage3D(kTarget, kLevel, kXOffset, kYOffset, kZOffset, kWidth,
                     kHeight, kDepth, kFormat, kType, pixels.data());

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  uint32_t offset_to_last = first_size + second_size + third_size;
  EXPECT_TRUE(CheckRect(
      kWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment,
      reinterpret_cast<uint8_t*>(pixels.data()) + offset_to_last, mem2_2.ptr));
}

// glGen* Ids must not be reused until glDelete* commands have been
// flushed by glFlush.
TEST_F(GLES2ImplementationStrictSharedTest, FlushGenerationTestBuffers) {
  FlushGenerationTest<GenBuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest, FlushGenerationTestRenderbuffers) {
  FlushGenerationTest<GenRenderbuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest, FlushGenerationTestTextures) {
  FlushGenerationTest<GenTexturesAPI>();
}

// glGen* Ids must not be reused cross-context until glDelete* commands are
// flushed by glFlush, and the Ids are lazily freed after.
TEST_F(GLES2ImplementationStrictSharedTest, CrossContextGenerationTestBuffers) {
  CrossContextGenerationTest<GenBuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       CrossContextGenerationTestRenderbuffers) {
  CrossContextGenerationTest<GenRenderbuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       CrossContextGenerationTestTextures) {
  CrossContextGenerationTest<GenTexturesAPI>();
}

// Test Delete which causes auto flush.  Tests a regression case that occurred
// in testing.
TEST_F(GLES2ImplementationStrictSharedTest,
       CrossContextGenerationAutoFlushTestBuffers) {
  CrossContextGenerationAutoFlushTest<GenBuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       CrossContextGenerationAutoFlushTestRenderbuffers) {
  CrossContextGenerationAutoFlushTest<GenRenderbuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       CrossContextGenerationAutoFlushTestTextures) {
  CrossContextGenerationAutoFlushTest<GenTexturesAPI>();
}

// Test deleting an invalid ID.
TEST_F(GLES2ImplementationStrictSharedTest,
       DeletingInvalidIdGeneratesErrorBuffers) {
  DeletingInvalidIdGeneratesError<GenBuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       DeletingInvalidIdGeneratesErrorRenderbuffers) {
  DeletingInvalidIdGeneratesError<GenRenderbuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       DeletingInvalidIdGeneratesErrorTextures) {
  DeletingInvalidIdGeneratesError<GenTexturesAPI>();
}

// Test double-deleting the same ID.
TEST_F(GLES2ImplementationStrictSharedTest,
       DoubleDeletingIdGeneratesErrorBuffers) {
  DoubleDeletingIdGeneratesError<GenBuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       DoubleDeletingIdGeneratesErrorRenderbuffers) {
  DoubleDeletingIdGeneratesError<GenRenderbuffersAPI>();
}
TEST_F(GLES2ImplementationStrictSharedTest,
       DoubleDeletingIdGeneratesErrorTextures) {
  DoubleDeletingIdGeneratesError<GenTexturesAPI>();
}

TEST_F(GLES2ImplementationTest, GetString) {
  const uint32_t kBucketId = GLES2Implementation::kResultBucketId;
  const Str7 kString = {"foobar"};
  // GL_CHROMIUM_map_sub is hard coded into GLES2Implementation.
  const char* expected_str =
      "foobar "
      "GL_CHROMIUM_map_sub "
      "GL_CHROMIUM_ordering_barrier "
      "GL_CHROMIUM_sync_point "
      "GL_EXT_unpack_subimage";
  const char kBad = 0x12;
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    cmds::GetString get_string;
    cmd::GetBucketStart get_bucket_start;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  ExpectedMemoryInfo mem1 = GetExpectedMemory(MaxTransferBufferSize());
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmd::GetBucketStart::Result));
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_string.Init(GL_EXTENSIONS, kBucketId);
  expected.get_bucket_start.Init(
      kBucketId, result1.id, result1.offset,
      MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  char buf[sizeof(kString) + 1];
  memset(buf, kBad, sizeof(buf));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(SetMemory(result1.ptr, uint32_t(sizeof(kString))),
                      SetMemory(mem1.ptr, kString)))
      .RetiresOnSaturation();

  const GLubyte* result = gl_->GetString(GL_EXTENSIONS);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_STREQ(expected_str, reinterpret_cast<const char*>(result));
}

TEST_F(GLES2ImplementationTest, CreateProgram) {
  struct Cmds {
    cmds::CreateProgram cmd;
  };

  Cmds expected;
  expected.cmd.Init(kProgramsAndShadersStartId);
  GLuint id = gl_->CreateProgram();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kProgramsAndShadersStartId, id);
}

TEST_F(GLES2ImplementationTest, BufferDataLargerThanTransferBuffer) {
  struct Cmds {
    cmds::BufferData set_size;
    cmds::BufferSubData copy_data1;
    cmd::SetToken set_token1;
    cmds::BufferSubData copy_data2;
    cmd::SetToken set_token2;
  };
  const unsigned kUsableSize =
      kTransferBufferSize - GLES2Implementation::kStartingOffset;
  uint8_t buf[kUsableSize * 2] = {
      0,
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kUsableSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kUsableSize);

  Cmds expected;
  expected.set_size.Init(GL_ARRAY_BUFFER, std::size(buf), 0, 0,
                         GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, 0, kUsableSize, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kUsableSize, kUsableSize, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  gl_->BufferData(GL_ARRAY_BUFFER, std::size(buf), buf, GL_DYNAMIC_DRAW);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MultiDrawArraysWEBGLLargerThanTransferBuffer) {
  struct Cmds {
    cmds::MultiDrawBeginCHROMIUM begin;
    cmds::MultiDrawArraysCHROMIUM draw1;
    cmd::SetToken set_token1;
    cmds::MultiDrawArraysCHROMIUM draw2;
    cmd::SetToken set_token2;
    cmds::MultiDrawEndCHROMIUM end;
  };
  const unsigned kUsableSize =
      kTransferBufferSize - GLES2Implementation::kStartingOffset;
  const unsigned kDrawCount = kUsableSize / sizeof(int);
  const unsigned kChunkDrawCount = kDrawCount / 2;
  const unsigned kCountsOffset = kChunkDrawCount * sizeof(int);
  GLint firsts[kDrawCount] = {0};
  GLsizei counts[kDrawCount] = {0};

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kUsableSize);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kUsableSize);

  Cmds expected;
  expected.begin.Init(kDrawCount);
  expected.draw1.Init(GL_TRIANGLES, mem1.id, mem1.offset, mem1.id,
                      mem1.offset + kCountsOffset, kChunkDrawCount);
  expected.set_token1.Init(GetNextToken());
  expected.draw2.Init(GL_TRIANGLES, mem2.id, mem2.offset, mem2.id,
                      mem2.offset + kCountsOffset, kChunkDrawCount);
  expected.set_token2.Init(GetNextToken());
  expected.end.Init();
  gl_->MultiDrawArraysWEBGL(GL_TRIANGLES, firsts, counts, kDrawCount);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, CapabilitiesAreCached) {
  static const GLenum kStates[] = {
    GL_DITHER,
    GL_BLEND,
    GL_CULL_FACE,
    GL_DEPTH_TEST,
    GL_POLYGON_OFFSET_FILL,
    GL_SAMPLE_ALPHA_TO_COVERAGE,
    GL_SAMPLE_COVERAGE,
    GL_SCISSOR_TEST,
    GL_STENCIL_TEST,
  };
  struct Cmds {
    cmds::Enable enable_cmd;
  };
  Cmds expected;

  for (size_t ii = 0; ii < std::size(kStates); ++ii) {
    GLenum state = kStates[ii];
    expected.enable_cmd.Init(state);
    GLboolean result = gl_->IsEnabled(state);
    EXPECT_EQ(static_cast<GLboolean>(ii == 0), result);
    EXPECT_TRUE(NoCommandsWritten());
    const void* commands = GetPut();
    if (!result) {
      gl_->Enable(state);
      EXPECT_EQ(0, memcmp(&expected, commands, sizeof(expected)));
    }
    ClearCommands();
    result = gl_->IsEnabled(state);
    EXPECT_TRUE(result);
    EXPECT_TRUE(NoCommandsWritten());
  }
}

TEST_F(GLES2ImplementationTest, BindVertexArrayOES) {
  GLuint id = 0;
  gl_->GenVertexArraysOES(1, &id);
  ClearCommands();

  struct Cmds {
    cmds::BindVertexArrayOES cmd;
  };
  Cmds expected;
  expected.cmd.Init(id);

  const void* commands = GetPut();
  gl_->BindVertexArrayOES(id);
  EXPECT_EQ(0, memcmp(&expected, commands, sizeof(expected)));
  ClearCommands();
  gl_->BindVertexArrayOES(id);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, BeginEndQueryEXT) {
  // Test GetQueryivEXT returns 0 if no current query.
  GLint param = -1;
  gl_->GetQueryivEXT(GL_ANY_SAMPLES_PASSED_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(0, param);

  GLuint expected_ids[2] = { 1, 2 }; // These must match what's actually genned.
  struct GenCmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  GenCmds expected_gen_cmds;
  expected_gen_cmds.gen.Init(std::size(expected_ids), &expected_ids[0]);
  GLuint ids[std::size(expected_ids)] = {
      0,
  };
  gl_->GenQueriesEXT(std::size(expected_ids), &ids[0]);
  EXPECT_EQ(0, memcmp(
      &expected_gen_cmds, commands_, sizeof(expected_gen_cmds)));
  GLuint id1 = ids[0];
  GLuint id2 = ids[1];
  ClearCommands();

  // Test BeginQueryEXT fails if id = 0.
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, 0);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test BeginQueryEXT inserts command.
  struct BeginCmds {
    cmds::BeginQueryEXT begin_query;
  };
  BeginCmds expected_begin_cmds;
  const void* commands = GetPut();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id1);
  QueryTracker::Query* query = GetQuery(id1);
  ASSERT_TRUE(query != nullptr);
  expected_begin_cmds.begin_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, id1, query->shm_id(), query->shm_offset());
  EXPECT_EQ(0, memcmp(
      &expected_begin_cmds, commands, sizeof(expected_begin_cmds)));
  ClearCommands();

  // Test GetQueryivEXT returns id.
  param = -1;
  gl_->GetQueryivEXT(GL_ANY_SAMPLES_PASSED_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(id1, static_cast<GLuint>(param));
  gl_->GetQueryivEXT(
      GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, GL_CURRENT_QUERY_EXT, &param);
  EXPECT_EQ(0, param);

  // Test BeginQueryEXT fails if between Begin/End.
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT fails if target not same as current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT sends command
  struct EndCmds {
    cmds::EndQueryEXT end_query;
  };
  commands = GetPut();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EndCmds expected_end_cmds;
  expected_end_cmds.end_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, query->submit_count());
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));

  // Test EndQueryEXT fails if no current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test 2nd Begin/End increments count.
  base::subtle::Atomic32 old_submit_count = query->submit_count();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, id1);
  EXPECT_EQ(old_submit_count, query->submit_count());
  commands = GetPut();
  gl_->EndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  EXPECT_NE(old_submit_count, query->submit_count());
  expected_end_cmds.end_query.Init(
      GL_ANY_SAMPLES_PASSED_EXT, query->submit_count());
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));

  // Test BeginQueryEXT fails if target changed.
  ClearCommands();
  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, id1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT fails if unused id
  GLuint available = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id2, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT fails if bad id
  ClearCommands();
  gl_->GetQueryObjectuivEXT(4567, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectuivEXT CheckResultsAvailable
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_EQ(0u, available);

  // Test GetQueryObjectui64vEXT fails if unused id
  GLuint64 available2 = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectui64vEXT(id2, GL_QUERY_RESULT_AVAILABLE_EXT, &available2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available2);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectui64vEXT fails if bad id
  ClearCommands();
  gl_->GetQueryObjectui64vEXT(4567, GL_QUERY_RESULT_AVAILABLE_EXT, &available2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0xBDu, available2);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test GetQueryObjectui64vEXT CheckResultsAvailable
  ClearCommands();
  gl_->GetQueryObjectui64vEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available2);
  EXPECT_EQ(0u, available2);
}

TEST_F(GLES2ImplementationManualInitTest, BadQueryTargets) {
  ContextInitOptions init_options;
  init_options.sync_query = false;
  init_options.occlusion_query_boolean = false;
  init_options.timer_queries = false;
  ASSERT_TRUE(Initialize(init_options));

  GLuint id = 0;
  gl_->GenQueriesEXT(1, &id);
  ClearCommands();

  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(GL_TIME_ELAPSED_EXT, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(0x123, id);
  EXPECT_EQ(GL_INVALID_ENUM, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->QueryCounterEXT(id, GL_TIMESTAMP_EXT);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->QueryCounterEXT(id, 0x123);
  EXPECT_EQ(GL_INVALID_ENUM, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));
}

TEST_F(GLES2ImplementationTest, SetDisjointSync) {
  struct SetDisjointSyncCmd {
    cmds::SetDisjointValueSyncCHROMIUM disjoint_sync;
  };
  SetDisjointSyncCmd expected_disjoint_sync_cmd;
  const void* commands = GetPut();
  gl_->SetDisjointValueSyncCHROMIUM();
  expected_disjoint_sync_cmd.disjoint_sync.Init(
      GetQueryTracker()->DisjointCountSyncShmID(),
      GetQueryTracker()->DisjointCountSyncShmOffset());

  EXPECT_EQ(0, memcmp(&expected_disjoint_sync_cmd, commands,
                      sizeof(expected_disjoint_sync_cmd)));
}

TEST_F(GLES2ImplementationTest, QueryCounterEXT) {
  // These must match what's actually genned.
  GLuint expected_ids[3] = {1, 2, 3};
  struct GenCmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[3];
  };
  GenCmds expected_gen_cmds;
  expected_gen_cmds.gen.Init(std::size(expected_ids), &expected_ids[0]);
  GLuint ids[std::size(expected_ids)] = {
      0,
  };
  gl_->GenQueriesEXT(std::size(expected_ids), &ids[0]);
  EXPECT_EQ(0, memcmp(
      &expected_gen_cmds, commands_, sizeof(expected_gen_cmds)));
  GLuint id1 = ids[0];
  GLuint id2 = ids[1];
  GLuint id3 = ids[2];
  ClearCommands();

  // Make sure disjoint value is synchronized already.
  gl_->SetDisjointValueSyncCHROMIUM();
  ClearCommands();

  // Test QueryCounterEXT fails if id = 0.
  gl_->QueryCounterEXT(0, GL_TIMESTAMP_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test QueryCounterEXT fails if target is unknown.
  ClearCommands();
  gl_->QueryCounterEXT(id1, GL_TIME_ELAPSED_EXT);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_ENUM, CheckError());

  // Test QueryCounterEXT inserts command.
  struct QueryCounterCmds {
    cmds::QueryCounterEXT query_counter;
  };
  QueryCounterCmds expected_query_counter_cmds;
  const void* commands = GetPut();
  gl_->QueryCounterEXT(id1, GL_TIMESTAMP_EXT);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  QueryTracker::Query* query = GetQuery(id1);
  ASSERT_TRUE(query != nullptr);
  expected_query_counter_cmds.query_counter.Init(
      id1, GL_TIMESTAMP_EXT, query->shm_id(), query->shm_offset(),
      query->submit_count());
  EXPECT_EQ(0, memcmp(&expected_query_counter_cmds, commands,
                      sizeof(expected_query_counter_cmds)));

  // Test QueryCounterEXT fails if id is reused with different target.
  ClearCommands();
  gl_->QueryCounterEXT(id1, GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test 2nd QueryCounterEXT succeeds.
  commands = GetPut();
  gl_->QueryCounterEXT(id2, GL_TIMESTAMP_EXT);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  QueryTracker::Query* query2 = GetQuery(id2);
  ASSERT_TRUE(query2 != nullptr);
  expected_query_counter_cmds.query_counter.Init(
      id2, GL_TIMESTAMP_EXT, query2->shm_id(), query2->shm_offset(),
      query2->submit_count());
  EXPECT_EQ(0, memcmp(&expected_query_counter_cmds, commands,
                      sizeof(expected_query_counter_cmds)));
  ClearCommands();

  // Test 3rd QueryCounterEXT succeeds.
  commands = GetPut();
  gl_->QueryCounterEXT(id3, GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  QueryTracker::Query* query3 = GetQuery(id3);
  ASSERT_TRUE(query3 != nullptr);
  expected_query_counter_cmds.query_counter.Init(
      id3, GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM, query3->shm_id(),
      query3->shm_offset(), query3->submit_count());
  EXPECT_EQ(0, memcmp(&expected_query_counter_cmds, commands,
                      sizeof(expected_query_counter_cmds)));
  ClearCommands();

  // Test QueryCounterEXT increments count.
  base::subtle::Atomic32 old_submit_count = query->submit_count();
  commands = GetPut();
  gl_->QueryCounterEXT(id1, GL_TIMESTAMP_EXT);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  EXPECT_NE(old_submit_count, query->submit_count());
  expected_query_counter_cmds.query_counter.Init(
      id1, GL_TIMESTAMP_EXT, query->shm_id(), query->shm_offset(),
      query->submit_count());
  EXPECT_EQ(0, memcmp(&expected_query_counter_cmds, commands,
                      sizeof(expected_query_counter_cmds)));
  ClearCommands();

  // Test GetQueryObjectuivEXT CheckResultsAvailable.
  GLuint available = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_EQ(0u, available);

  available = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectuivEXT(id3, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_EQ(0u, available);

  // Test GetQueryObjectui64vEXT CheckResultsAvailable.
  GLuint64 available2 = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectui64vEXT(id1, GL_QUERY_RESULT_AVAILABLE_EXT, &available2);
  EXPECT_EQ(0u, available2);

  available2 = 0xBDu;
  ClearCommands();
  gl_->GetQueryObjectui64vEXT(id3, GL_QUERY_RESULT_AVAILABLE_EXT, &available2);
  EXPECT_EQ(0u, available2);
}

TEST_F(GLES2ImplementationTest, ErrorQuery) {
  GLuint id = 0;
  gl_->GenQueriesEXT(1, &id);
  ClearCommands();

  // Test BeginQueryEXT does NOT insert commands.
  gl_->BeginQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM, id);
  EXPECT_TRUE(NoCommandsWritten());
  QueryTracker::Query* query = GetQuery(id);
  ASSERT_TRUE(query != nullptr);

  // Test EndQueryEXT sends both begin and end command
  struct EndCmds {
    cmds::BeginQueryEXT begin_query;
    cmds::EndQueryEXT end_query;
  };
  const void* commands = GetPut();
  gl_->EndQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM);
  EndCmds expected_end_cmds;
  expected_end_cmds.begin_query.Init(
      GL_GET_ERROR_QUERY_CHROMIUM, id, query->shm_id(), query->shm_offset());
  expected_end_cmds.end_query.Init(
      GL_GET_ERROR_QUERY_CHROMIUM, query->submit_count());
  EXPECT_EQ(0, memcmp(
      &expected_end_cmds, commands, sizeof(expected_end_cmds)));
  ClearCommands();

  // Check result is not yet available.
  GLuint available = 0xBDu;
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(0u, available);

  // Test no commands are sent if there is a client side error.

  // Generate a client side error
  gl_->ActiveTexture(GL_TEXTURE0 - 1);

  gl_->BeginQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM, id);
  gl_->EndQueryEXT(GL_GET_ERROR_QUERY_CHROMIUM);
  EXPECT_TRUE(NoCommandsWritten());

  // Check result is available.
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_NE(0u, available);

  // Check result.
  GLuint result = 0xBDu;
  gl_->GetQueryObjectuivEXT(id, GL_QUERY_RESULT_EXT, &result);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLuint>(GL_INVALID_ENUM), result);
}

#if !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
TEST_F(GLES2ImplementationTest, VertexArrays) {
  const GLuint kAttribIndex1 = 1;
  const GLint kNumComponents1 = 3;
  const GLsizei kClientStride = 12;

  GLuint id = 0;
  gl_->GenVertexArraysOES(1, &id);
  ClearCommands();

  gl_->BindVertexArrayOES(id);

  // Test that VertexAttribPointer cannot be called with a bound buffer of 0
  // unless the offset is nullptr
  gl_->BindBuffer(GL_ARRAY_BUFFER, 0);

  gl_->VertexAttribPointer(
      kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE, kClientStride,
      reinterpret_cast<const void*>(4));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1, GL_FLOAT, GL_FALSE,
                           kClientStride, nullptr);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
}
#endif

TEST_F(GLES2ImplementationTest, Disable) {
  struct Cmds {
    cmds::Disable cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_DITHER);  // Note: DITHER defaults to enabled.

  gl_->Disable(GL_DITHER);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  // Check it's cached and not called again.
  ClearCommands();
  gl_->Disable(GL_DITHER);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, Enable) {
  struct Cmds {
    cmds::Enable cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_BLEND);  // Note: BLEND defaults to disabled.

  gl_->Enable(GL_BLEND);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  // Check it's cached and not called again.
  ClearCommands();
  gl_->Enable(GL_BLEND);
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, CreateAndTexStorage2DSharedImageCHROMIUM) {
  struct Cmds {
    cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate cmd;
    GLbyte data[GL_MAILBOX_SIZE_CHROMIUM];
  };

  Mailbox mailbox = Mailbox::Generate();
  Cmds expected;
  expected.cmd.Init(kTexturesStartId, mailbox.name);
  GLuint id = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kTexturesStartId, id);
}

TEST_F(GLES2ImplementationTest, LimitSizeAndOffsetTo32Bit) {
  GLsizeiptr size;
  GLintptr offset;
  if (sizeof(size) <= 4 || sizeof(offset) <= 4)
    return;
  // The below two casts should be no-op, as we return early if
  // it's 32-bit system.
  int64_t value64 = 0x100000000;
  size = static_cast<GLsizeiptr>(value64);
  offset = static_cast<GLintptr>(value64);

  const char kSizeOverflowMessage[] = "size more than 32-bit";
  const char kOffsetOverflowMessage[] = "offset more than 32-bit";

  const GLfloat buf[] = { 1.0, 1.0, 1.0, 1.0 };
  const GLubyte indices[] = { 0 };

  const GLuint kClientArrayBufferId = 0x789;
  const GLuint kClientElementArrayBufferId = 0x790;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kClientArrayBufferId);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, kClientElementArrayBufferId);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // Call BufferData() should succeed with legal paramaters.
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(buf), buf, GL_DYNAMIC_DRAW);
  gl_->BufferData(
      GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // BufferData: size
  gl_->BufferData(GL_ARRAY_BUFFER, size, buf, GL_DYNAMIC_DRAW);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kSizeOverflowMessage, GetLastError().c_str());

  // Call BufferSubData() should succeed with legal paramaters.
  gl_->BufferSubData(GL_ARRAY_BUFFER, 0, sizeof(buf[0]), buf);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // BufferSubData: offset
  gl_->BufferSubData(GL_ARRAY_BUFFER, offset, 1, buf);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kOffsetOverflowMessage, GetLastError().c_str());

  // BufferSubData: size
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  gl_->BufferSubData(GL_ARRAY_BUFFER, 0, size, buf);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kSizeOverflowMessage, GetLastError().c_str());

  // Call MapBufferSubDataCHROMIUM() should succeed with legal paramaters.
  void* mem =
      gl_->MapBufferSubDataCHROMIUM(GL_ARRAY_BUFFER, 0, 1, GL_WRITE_ONLY);
  EXPECT_TRUE(nullptr != mem);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  gl_->UnmapBufferSubDataCHROMIUM(mem);

  // MapBufferSubDataCHROMIUM: offset
  EXPECT_TRUE(nullptr == gl_->MapBufferSubDataCHROMIUM(GL_ARRAY_BUFFER, offset,
                                                       1, GL_WRITE_ONLY));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kOffsetOverflowMessage, GetLastError().c_str());

  // MapBufferSubDataCHROMIUM: size
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  EXPECT_TRUE(nullptr == gl_->MapBufferSubDataCHROMIUM(GL_ARRAY_BUFFER, 0, size,
                                                       GL_WRITE_ONLY));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kSizeOverflowMessage, GetLastError().c_str());

  // Call DrawElements() should succeed with legal paramaters.
  gl_->DrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, nullptr);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // DrawElements: offset
  gl_->DrawElements(
      GL_POINTS, 1, GL_UNSIGNED_BYTE, reinterpret_cast<void*>(offset));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kOffsetOverflowMessage, GetLastError().c_str());

  // Call DrawElementsInstancedANGLE() should succeed with legal paramaters.
  gl_->DrawElementsInstancedANGLE(GL_POINTS, 1, GL_UNSIGNED_BYTE, nullptr, 1);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // DrawElementsInstancedANGLE: offset
  gl_->DrawElementsInstancedANGLE(
      GL_POINTS, 1, GL_UNSIGNED_BYTE, reinterpret_cast<void*>(offset), 1);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kOffsetOverflowMessage, GetLastError().c_str());

  // Call VertexAttribPointer() should succeed with legal paramaters.
  const GLuint kAttribIndex = 1;
  const GLsizei kStride = 4;
  gl_->VertexAttribPointer(kAttribIndex, 1, GL_FLOAT, GL_FALSE, kStride,
                           nullptr);
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // VertexAttribPointer: offset
  gl_->VertexAttribPointer(
      kAttribIndex, 1, GL_FLOAT, GL_FALSE, kStride,
      reinterpret_cast<void*>(offset));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_STREQ(kOffsetOverflowMessage, GetLastError().c_str());
}

TEST_F(GLES2ImplementationTest, TraceBeginCHROMIUM) {
  const uint32_t kCategoryBucketId = GLES2Implementation::kResultBucketId;
  const uint32_t kNameBucketId = GLES2Implementation::kResultBucketId + 1;
  const std::string category_name = "test category";
  const std::string trace_name = "test trace";
  const size_t kPaddedString1Size =
      transfer_buffer_->RoundToAlignment(category_name.size() + 1);
  const size_t kPaddedString2Size =
      transfer_buffer_->RoundToAlignment(trace_name.size() + 1);

  gl_->TraceBeginCHROMIUM(category_name.c_str(), trace_name.c_str());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  struct Cmds {
    cmd::SetBucketSize category_size1;
    cmd::SetBucketData category_data;
    cmd::SetToken set_token1;
    cmd::SetBucketSize name_size1;
    cmd::SetBucketData name_data;
    cmd::SetToken set_token2;
    cmds::TraceBeginCHROMIUM trace_call_begin;
    cmd::SetBucketSize category_size2;
    cmd::SetBucketSize name_size2;
  };

  ExpectedMemoryInfo mem1 = GetExpectedMemory(kPaddedString1Size);
  ExpectedMemoryInfo mem2 = GetExpectedMemory(kPaddedString2Size);

  ASSERT_STREQ(category_name.c_str(), reinterpret_cast<char*>(mem1.ptr));
  ASSERT_STREQ(trace_name.c_str(), reinterpret_cast<char*>(mem2.ptr));

  Cmds expected;
  expected.category_size1.Init(kCategoryBucketId, category_name.size() + 1);
  expected.category_data.Init(
      kCategoryBucketId, 0, category_name.size() + 1, mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.name_size1.Init(kNameBucketId, trace_name.size() + 1);
  expected.name_data.Init(
      kNameBucketId, 0, trace_name.size() + 1, mem2.id, mem2.offset);
  expected.set_token2.Init(GetNextToken());
  expected.trace_call_begin.Init(kCategoryBucketId, kNameBucketId);
  expected.category_size2.Init(kCategoryBucketId, 0);
  expected.name_size2.Init(kNameBucketId, 0);

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, SetActiveURLCHROMIUM) {
  const uint32_t kURLBucketId = GLES2Implementation::kResultBucketId;
  const std::string url = "chrome://test";
  const size_t kPaddedStringSize =
      transfer_buffer_->RoundToAlignment(url.size());

  gl_->SetActiveURLCHROMIUM(url.c_str());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  struct Cmds {
    cmd::SetBucketSize url_size;
    cmd::SetBucketData url_data;
    cmd::SetToken set_token;
    cmds::SetActiveURLCHROMIUM set_url_call;
    cmd::SetBucketSize url_size_end;
  };

  ExpectedMemoryInfo mem = GetExpectedMemory(kPaddedStringSize);
  EXPECT_EQ(0,
            memcmp(url.c_str(), reinterpret_cast<char*>(mem.ptr), url.size()));

  Cmds expected;
  expected.url_size.Init(kURLBucketId, url.size());
  expected.url_data.Init(kURLBucketId, 0, url.size(), mem.id, mem.offset);
  expected.set_token.Init(GetNextToken());
  expected.set_url_call.Init(kURLBucketId);
  expected.url_size_end.Init(kURLBucketId, 0);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));

  // Same URL shouldn't make any commands.
  EXPECT_FALSE(NoCommandsWritten());
  ClearCommands();
  gl_->SetActiveURLCHROMIUM(url.c_str());
  EXPECT_TRUE(NoCommandsWritten());
}

TEST_F(GLES2ImplementationTest, AllowNestedTracesCHROMIUM) {
  const std::string category1_name = "test category 1";
  const std::string trace1_name = "test trace 1";
  const std::string category2_name = "test category 2";
  const std::string trace2_name = "test trace 2";

  gl_->TraceBeginCHROMIUM(category1_name.c_str(), trace1_name.c_str());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  gl_->TraceBeginCHROMIUM(category2_name.c_str(), trace2_name.c_str());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  gl_->TraceEndCHROMIUM();
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  gl_->TraceEndCHROMIUM();
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  // No more corresponding begin tracer marker should error.
  gl_->TraceEndCHROMIUM();
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
}

TEST_F(GLES2ImplementationTest, GenSyncTokenCHROMIUM) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 123u;
  SyncToken sync_token;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  gl_->GenSyncTokenCHROMIUM(nullptr);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());

  const void* commands = GetPut();
  cmd::InsertFenceSync insert_fence_sync;
  insert_fence_sync.Init(kFenceSync);

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gl_->GenSyncTokenCHROMIUM(sync_token.GetData());
  EXPECT_EQ(0, memcmp(&insert_fence_sync, commands, sizeof(insert_fence_sync)));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token.verified_flush());
  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
}

TEST_F(GLES2ImplementationTest, GenUnverifiedSyncTokenCHROMIUM) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 123u;
  SyncToken sync_token;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  gl_->GenUnverifiedSyncTokenCHROMIUM(nullptr);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());

  const void* commands = GetPut();
  cmd::InsertFenceSync insert_fence_sync;
  insert_fence_sync.Init(kFenceSync);

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  EXPECT_EQ(0, memcmp(&insert_fence_sync, commands, sizeof(insert_fence_sync)));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_FALSE(sync_token.verified_flush());
  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
}

TEST_F(GLES2ImplementationTest, VerifySyncTokensCHROMIUM) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 123u;
  gpu::SyncToken sync_token;
  GLbyte* sync_token_datas[] = {sync_token.GetData()};

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  EXPECT_TRUE(sync_token.HasData());
  EXPECT_FALSE(sync_token.verified_flush());

  ClearCommands();
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(false));
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_FALSE(sync_token.verified_flush());

  ClearCommands();
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
  EXPECT_TRUE(sync_token.verified_flush());
}

TEST_F(GLES2ImplementationTest, VerifySyncTokensCHROMIUM_Sequence) {
  // To verify sync tokens, the sync tokens must all be verified after
  // CanWaitUnverifiedSyncTokens() are called. This test ensures the right
  // sequence.
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync1 = 123u;
  const GLuint64 kFenceSync2 = 234u;
  gpu::SyncToken sync_token1;
  gpu::SyncToken sync_token2;
  GLbyte* sync_token_datas[] = {sync_token1.GetData(), sync_token2.GetData()};

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  // Generate sync token 1.
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync1));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token1.GetData());
  EXPECT_TRUE(sync_token1.HasData());
  EXPECT_FALSE(sync_token1.verified_flush());

  // Generate sync token 2.
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync2));
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token2.GetData());
  EXPECT_TRUE(sync_token2.HasData());
  EXPECT_FALSE(sync_token2.verified_flush());

  // Ensure proper sequence of checking and validating.
  Sequence sequence;
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token1))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token2))
      .InSequence(sequence)
      .WillOnce(Return(true));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible()).InSequence(sequence);
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(GLES2ImplementationTest, VerifySyncTokensCHROMIUM_EmptySyncToken) {
  // To verify sync tokens, the sync tokens must all be verified after
  // CanWaitUnverifiedSyncTokens() are called. This test ensures the right
  // sequence.
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gpu::SyncToken sync_token1, sync_token2;
  GLbyte* sync_token_datas[] = {sync_token1.GetData(), sync_token2.GetData()};

  // Ensure proper sequence of checking and validating.
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(_)).Times(0);
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible()).Times(0);
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, std::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(GLES2ImplementationTest, WaitSyncTokenCHROMIUM) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId =
      CommandBufferId::FromUnsafeValue(234u);
  const GLuint64 kFenceSync = 456u;

  gpu::SyncToken sync_token;
  GLbyte* sync_token_data = sync_token.GetData();

  struct Cmds {
    cmd::InsertFenceSync insert_fence_sync;
  };
  Cmds expected;
  expected.insert_fence_sync.Init(kFenceSync);

  EXPECT_CALL(*gpu_control_, GetNamespaceID()).WillOnce(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillOnce(Return(kCommandBufferId));
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gl_->GenSyncTokenCHROMIUM(sync_token_data);

  EXPECT_CALL(*gpu_control_, WaitSyncToken(sync_token));
  gl_->WaitSyncTokenCHROMIUM(sync_token_data);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, WaitSyncTokenCHROMIUMErrors) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::GetError::Result));
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillRepeatedly(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // Empty sync tokens should be produce no error and be a nop.
  ClearCommands();
  gl_->WaitSyncTokenCHROMIUM(nullptr);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());

  // Invalid sync tokens should produce no error and be a nop.
  ClearCommands();
  gpu::SyncToken invalid_sync_token;
  gl_->WaitSyncTokenCHROMIUM(invalid_sync_token.GetConstData());
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());

  // Unverified sync token should produce INVALID_OPERATION.
  ClearCommands();
  gpu::SyncToken unverified_sync_token(CommandBufferNamespace::GPU_IO,
                                       gpu::CommandBufferId(), 0);
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(unverified_sync_token))
      .WillOnce(Return(false));
  gl_->WaitSyncTokenCHROMIUM(unverified_sync_token.GetConstData());
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, IsEnabled) {
  // If we use a valid enum, its state is cached on client side, so no command
  // is actually generated, and this test will fail.
  // TODO(zmo): it seems we never need the command. Maybe remove it.
  GLenum kCap = 1;
  struct Cmds {
    cmds::IsEnabled cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::IsEnabled::Result));
  expected.cmd.Init(kCap, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_TRUE)))
      .RetiresOnSaturation();

  GLboolean result = gl_->IsEnabled(kCap);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(result);
}

TEST_F(GLES2ImplementationTest, ClientWaitSync) {
  const GLuint client_sync_id = 36;
  struct Cmds {
    cmds::ClientWaitSync cmd;
  };

  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(cmds::ClientWaitSync::Result));
  const GLuint64 kTimeout = 0xABCDEF0123456789;
  expected.cmd.Init(client_sync_id, GL_SYNC_FLUSH_COMMANDS_BIT,
                    kTimeout, result1.id, result1.offset);

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, uint32_t(GL_CONDITION_SATISFIED)))
      .RetiresOnSaturation();

  GLenum result = gl_->ClientWaitSync(
      reinterpret_cast<GLsync>(client_sync_id), GL_SYNC_FLUSH_COMMANDS_BIT,
      kTimeout);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_CONDITION_SATISFIED), result);
}

TEST_F(GLES2ImplementationTest, WaitSync) {
  const GLuint kClientSyncId = 36;
  struct Cmds {
    cmds::WaitSync cmd;
  };
  Cmds expected;
  const GLuint64 kTimeout = GL_TIMEOUT_IGNORED;
  expected.cmd.Init(kClientSyncId, 0, kTimeout);

  gl_->WaitSync(reinterpret_cast<GLsync>(kClientSyncId), 0, kTimeout);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapBufferRangeUnmapBufferWrite) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(1)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_WRITE_BIT);
  EXPECT_TRUE(mem != nullptr);

  EXPECT_TRUE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
}

TEST_F(GLES2ImplementationTest, MapBufferRangeWriteWithInvalidateBit) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(1)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  GLsizeiptr kSize = 64;
  void* mem = gl_->MapBufferRange(
      GL_ARRAY_BUFFER, 10, kSize,
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
  EXPECT_TRUE(mem != nullptr);
  std::vector<int8_t> zero(kSize);
  memset(&zero[0], 0, kSize);
  EXPECT_EQ(0, memcmp(mem, &zero[0], kSize));
}

TEST_F(GLES2ImplementationTest, MapBufferRangeWriteWithGLError) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  // Return a result of 0 to indicate an GL error.
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(0)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_WRITE_BIT);
  EXPECT_TRUE(mem == nullptr);
}

TEST_F(GLES2ImplementationTest, MapBufferRangeUnmapBufferRead) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(1)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_READ_BIT);
  EXPECT_TRUE(mem != nullptr);

  EXPECT_TRUE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
}

TEST_F(GLES2ImplementationTest, MapBufferRangeReadWithGLError) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  // Return a result of 0 to indicate an GL error.
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(0)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_READ_BIT);
  EXPECT_TRUE(mem == nullptr);
}

TEST_F(GLES2ImplementationTest, UnmapBufferFails) {
  // No bound buffer.
  EXPECT_FALSE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  // Buffer is unmapped.
  EXPECT_FALSE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
}

TEST_F(GLES2ImplementationTest, BufferDataUnmapsDataStore) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(1)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_WRITE_BIT);
  EXPECT_TRUE(mem != nullptr);

  std::vector<uint8_t> data(16);
  // BufferData unmaps the data store.
  gl_->BufferData(GL_ARRAY_BUFFER, 16, &data[0], GL_STREAM_DRAW);

  EXPECT_FALSE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
}

TEST_F(GLES2ImplementationTest, DeleteBuffersUnmapsDataStore) {
  ExpectedMemoryInfo result =
      GetExpectedResultMemory(sizeof(cmds::MapBufferRange::Result));

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result.ptr, uint32_t(1)))
      .RetiresOnSaturation();

  const GLuint kBufferId = 123;
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);

  void* mem = gl_->MapBufferRange(GL_ARRAY_BUFFER, 10, 64, GL_MAP_WRITE_BIT);
  EXPECT_TRUE(mem != nullptr);

  std::vector<uint8_t> data(16);
  // DeleteBuffers unmaps the data store.
  gl_->DeleteBuffers(1, &kBufferId);

  EXPECT_FALSE(gl_->UnmapBuffer(GL_ARRAY_BUFFER));
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
}

TEST_F(GLES2ImplementationTest, GetInternalformativ) {
  const GLint kNumSampleCounts = 8;
  struct Cmds {
    cmds::GetInternalformativ cmd;
  };
  typedef cmds::GetInternalformativ::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, GL_RGBA8, GL_NUM_SAMPLE_COUNTS,
                    result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr,
                          SizedResultHelper<ResultType>(kNumSampleCounts)))
      .RetiresOnSaturation();
  gl_->GetInternalformativ(123, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(kNumSampleCounts), result);
}

static void CountCallback(int* count) {
  (*count)++;
}

TEST_F(GLES2ImplementationTest, SignalSyncToken) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gpu::SyncToken sync_token;
  gl_->GenSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce(Invoke([&signal_closure](const SyncToken& sync_token,
                                         base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      }));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // When GpuControl runs the callback, the original callback we gave to
  // GLES2Implementation is run.
  std::move(signal_closure).Run();
  EXPECT_EQ(1, signaled_count);
}

TEST_F(GLES2ImplementationTest, SignalSyncTokenAfterContextLoss) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID()).WillOnce(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillOnce(Return(kCommandBufferId));
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  EXPECT_CALL(*gpu_control_, EnsureWorkVisible());
  gpu::SyncToken sync_token;
  gl_->GenSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce(Invoke([&signal_closure](const SyncToken& sync_token,
                                         base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      }));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // Inform the GLES2Implementation that the context is lost.
  GpuControlClient* gl_as_client = gl_;
  gl_as_client->OnGpuControlLostContext();

  // When GpuControl runs the callback, the original callback we gave to
  // GLES2Implementation is *not* run, since the context is lost and we
  // have already run the lost context callback.
  std::move(signal_closure).Run();
  EXPECT_EQ(0, signaled_count);
}

TEST_F(GLES2ImplementationTest, ReportLoss) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContext();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should be run when GLES2Implementation is
  // notified of the loss.
  EXPECT_EQ(1, lost_count);
}

TEST_F(GLES2ImplementationTest, ReportLossReentrant) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContextMaybeReentrant();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should not be run yet to avoid calling back into
  // clients re-entrantly, and having them re-enter GLES2Implementation.
  EXPECT_EQ(0, lost_count);
}

TEST_F(GLES2ImplementationManualInitTest, FailInitOnBGRMismatch1) {
  ContextInitOptions init_options;
  init_options.bind_generates_resource_client = false;
  init_options.bind_generates_resource_service = true;
  EXPECT_FALSE(Initialize(init_options));
}

TEST_F(GLES2ImplementationManualInitTest, FailInitOnBGRMismatch2) {
  ContextInitOptions init_options;
  init_options.bind_generates_resource_client = true;
  init_options.bind_generates_resource_service = false;
  EXPECT_FALSE(Initialize(init_options));
}

TEST_F(GLES2ImplementationManualInitTest, FailInitOnTransferBufferFail) {
  ContextInitOptions init_options;
  init_options.transfer_buffer_initialize_fail = true;
  EXPECT_FALSE(Initialize(init_options));
}

TEST_F(GLES2ImplementationTest, DiscardableMemoryDelete) {
  const GLuint texture_id = 1;
  EXPECT_FALSE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
  EXPECT_TRUE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));

  // Deleting a texture should clear its discardable entry.
  gl_->DeleteTextures(1, &texture_id);
  EXPECT_FALSE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));
}

TEST_F(GLES2ImplementationTest, DiscardableTextureLockFail) {
  const GLuint texture_id = 1;
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
  EXPECT_TRUE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));

  // Unlock the handle on the client side.
  gl_->UnlockDiscardableTextureCHROMIUM(texture_id);

  // Unlock and delete the handle on the service side.
  ClientDiscardableHandle client_handle =
      share_group_->discardable_texture_manager()->GetHandleForTesting(
          texture_id);
  ServiceDiscardableHandle service_handle(client_handle.BufferForTesting(),
                                          client_handle.byte_offset(),
                                          client_handle.shm_id());
  service_handle.Unlock();
  EXPECT_TRUE(service_handle.Delete());

  // Trying to re-lock the texture via GL should fail and delete the entry.
  EXPECT_FALSE(gl_->LockDiscardableTextureCHROMIUM(texture_id));
  EXPECT_FALSE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));
}

TEST_F(GLES2ImplementationTest, DiscardableTextureDoubleInitError) {
  const GLuint texture_id = 1;
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());
}

TEST_F(GLES2ImplementationTest, DiscardableTextureLockError) {
  const GLuint texture_id = 1;
  EXPECT_FALSE(gl_->LockDiscardableTextureCHROMIUM(texture_id));
  EXPECT_EQ(GL_INVALID_VALUE, CheckError());
}

TEST_F(GLES2ImplementationTest, DiscardableTextureLockCounting) {
  const GLint texture_id = 1;
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
  EXPECT_TRUE(
      share_group_->discardable_texture_manager()->TextureIsValid(texture_id));

  // Bind the texture.
  gl_->BindTexture(GL_TEXTURE_2D, texture_id);
  GLint bound_texture_id = 0;
  gl_->GetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture_id);
  EXPECT_EQ(texture_id, bound_texture_id);

  // Lock the texture 3 more times (for 4 locks total).
  for (int i = 0; i < 3; ++i) {
    gl_->LockDiscardableTextureCHROMIUM(texture_id);
  }

  // Unlock 4 times. Only after the last unlock should the texture be unbound.
  for (int i = 0; i < 4; ++i) {
    gl_->UnlockDiscardableTextureCHROMIUM(texture_id);
    bound_texture_id = 0;
    gl_->GetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture_id);
    if (i < 3) {
      EXPECT_EQ(texture_id, bound_texture_id);
    } else {
      EXPECT_EQ(0, bound_texture_id);
    }
  }
}

struct ErrorMessageCounter {
  explicit ErrorMessageCounter(GLES2Implementation* gl) : gl(gl) {}

  void Callback(const char* message, int32_t id) {
    if (++num_calls == 1)
      gl->ShaderBinary(-1, nullptr, 0, nullptr, 0);
  }

  raw_ptr<GLES2Implementation> gl;
  int32_t num_calls = 0;
};

TEST_F(GLES2ImplementationTest, ReentrantErrorCallbacksShouldNotCrash) {
  ErrorMessageCounter counter(gl_);
  gl_->SetErrorMessageCallback(base::BindRepeating(
      &ErrorMessageCounter::Callback, base::Unretained(&counter)));
  // Call any function which can easily provoke an error. See also the
  // callback above.
  gl_->ShaderBinary(-1, nullptr, 0, nullptr, 0);
  EXPECT_EQ(2, counter.num_calls);
  ResetErrorMessageCallback();
}

TEST_F(GLES2ImplementationTest, DeleteZero) {
  gl_->DeleteProgram(0);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  gl_->DeleteShader(0);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
  gl_->DeleteSync(0);
  EXPECT_EQ(GL_NO_ERROR, CheckError());
}

#include "gpu/command_buffer/client/gles2_implementation_unittest_autogen.h"

}  // namespace gles2
}  // namespace gpu
