// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for RasterImplementation.

#include "gpu/command_buffer/client/raster_implementation.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/mock_transfer_buffer.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/ring_buffer.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using gpu::gles2::QueryTracker;
using testing::_;
using testing::AtLeast;
using testing::AnyNumber;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Sequence;
using testing::StrictMock;
using testing::Return;
using testing::ReturnRef;

namespace gpu {
namespace raster {

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
  explicit SizedResultHelper(T result) : size_(sizeof(result)) {
    memcpy(result_, &result, sizeof(T));
  }

 private:
  uint32_t size_;
  char result_[sizeof(T)];
};

class RasterImplementationTest : public testing::Test {
 protected:
  static const uint8_t kInitialValue = 0xBD;
  static const uint32_t kNumCommandEntries = 500;
  static const uint32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const uint32_t kTransferBufferSize = 512;

  static const GLint kMaxCombinedTextureImageUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxTextureSize = 128;
  static const GLint kNumCompressedTextureFormats = 0;
  static const GLuint kStartId = 1024;
  static const GLuint kBuffersStartId = 1;
  static const GLuint kTexturesStartId = 1;
  static const GLuint kQueriesStartId = 1;

  typedef MockTransferBuffer::ExpectedMemoryInfo ExpectedMemoryInfo;

  class TestContext {
   public:
    TestContext() : commands_(nullptr), token_(0) {}

    bool Initialize(bool bind_generates_resource_client,
                    bool bind_generates_resource_service,
                    bool lose_context_when_out_of_memory,
                    bool transfer_buffer_initialize_fail,
                    bool sync_query) {
      SharedMemoryLimits limits = SharedMemoryLimitsForTesting();
      command_buffer_.reset(new StrictMock<MockClientCommandBuffer>());

      transfer_buffer_.reset(new MockTransferBuffer(
          command_buffer_.get(), kTransferBufferSize,
          RasterImplementation::kStartingOffset,
          RasterImplementation::kAlignment, transfer_buffer_initialize_fail));

      helper_.reset(new RasterCmdHelper(command_buffer()));
      helper_->Initialize(limits.command_buffer_size);

      gpu_control_.reset(new StrictMock<MockClientGpuControl>());
      capabilities_.max_combined_texture_image_units =
          kMaxCombinedTextureImageUnits;
      capabilities_.max_texture_image_units = kMaxTextureImageUnits;
      capabilities_.max_texture_size = kMaxTextureSize;
      capabilities_.num_compressed_texture_formats =
          kNumCompressedTextureFormats;
      capabilities_.bind_generates_resource_chromium =
          bind_generates_resource_service ? 1 : 0;
      capabilities_.sync_query = sync_query;
      EXPECT_CALL(*gpu_control_, GetCapabilities())
          .WillOnce(ReturnRef(capabilities_));

      {
        InSequence sequence;

        gl_.reset(new RasterImplementation(
            helper_.get(), transfer_buffer_.get(),
            bind_generates_resource_client, lose_context_when_out_of_memory,
            gpu_control_.get(), nullptr /* image_decode_accelerator */));
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
    std::unique_ptr<RasterCmdHelper> helper_;
    std::unique_ptr<MockTransferBuffer> transfer_buffer_;
    std::unique_ptr<RasterImplementation> gl_;
    CommandBufferEntry* commands_;
    int token_;
    Capabilities capabilities_;
  };

  RasterImplementationTest() : commands_(nullptr) {}

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

  QueryTracker* GetQueryTracker() { return gl_->query_tracker_.get(); }

  struct ContextInitOptions {
    ContextInitOptions()
        : bind_generates_resource_client(true),
          bind_generates_resource_service(true),
          lose_context_when_out_of_memory(false),
          transfer_buffer_initialize_fail(false),
          sync_query(true) {}
    bool bind_generates_resource_client;
    bool bind_generates_resource_service;
    bool lose_context_when_out_of_memory;
    bool transfer_buffer_initialize_fail;
    bool sync_query;
  };

  bool Initialize(const ContextInitOptions& init_options) {
    bool success = true;
    if (!test_context_.Initialize(init_options.bind_generates_resource_client,
                                  init_options.bind_generates_resource_service,
                                  init_options.lose_context_when_out_of_memory,
                                  init_options.transfer_buffer_initialize_fail,
                                  init_options.sync_query)) {
      success = false;
    }

    // Default to test context 0.
    gpu_control_ = test_context_.gpu_control_.get();
    helper_ = test_context_.helper_.get();
    transfer_buffer_ = test_context_.transfer_buffer_.get();
    gl_ = test_context_.gl_.get();
    commands_ = test_context_.commands_;
    return success;
  }

  MockClientCommandBuffer* command_buffer() const {
    return test_context_.command_buffer_.get();
  }

  int GetNextToken() { return test_context_.GetNextToken(); }

  const void* GetPut() { return helper_->GetSpace(0); }

  void ClearCommands() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    memset(ring_buffer->memory(), kInitialValue, ring_buffer->size());
  }

  uint32_t MaxTransferBufferSize() {
    return transfer_buffer_->MaxTransferBufferSize();
  }

  void SetMappedMemoryLimit(size_t limit) {
    gl_->mapped_memory_->set_max_allocated_bytes(limit);
  }

  ExpectedMemoryInfo GetExpectedMemory(uint32_t size) {
    return transfer_buffer_->GetExpectedMemory(size);
  }

  ExpectedMemoryInfo GetExpectedResultMemory(uint32_t size) {
    return transfer_buffer_->GetExpectedResultMemory(size);
  }

  ExpectedMemoryInfo GetExpectedMappedMemory(uint32_t size) {
    ExpectedMemoryInfo mem;

    // Temporarily allocate memory and expect that memory block to be reused.
    mem.ptr = static_cast<uint8_t*>(
        gl_->mapped_memory_->Alloc(size, &mem.id, &mem.offset));
    gl_->mapped_memory_->Free(mem.ptr);

    return mem;
  }

  int CheckError() {
    ExpectedMemoryInfo result =
        GetExpectedResultMemory(sizeof(cmds::GetError::Result));
    EXPECT_CALL(*command_buffer(), OnFlush())
        .WillOnce(SetMemory(result.ptr, GLuint(GL_NO_ERROR)))
        .RetiresOnSaturation();
    return gl_->GetError();
  }

  const std::string& GetLastError() { return gl_->GetLastError(); }

  bool GetBucketContents(uint32_t bucket_id, std::vector<int8_t>* data) {
    return gl_->GetBucketContents(bucket_id, data);
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

  TestContext test_context_;

  MockClientGpuControl* gpu_control_;
  RasterCmdHelper* helper_;
  MockTransferBuffer* transfer_buffer_;
  RasterImplementation* gl_;
  CommandBufferEntry* commands_;
};

void RasterImplementationTest::SetUp() {
  ContextInitOptions init_options;
  ASSERT_TRUE(Initialize(init_options));
}

void RasterImplementationTest::TearDown() {
  test_context_.TearDown();
}

class RasterImplementationManualInitTest : public RasterImplementationTest {
 protected:
  void SetUp() override {}
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const uint8_t RasterImplementationTest::kInitialValue;
const uint32_t RasterImplementationTest::kNumCommandEntries;
const uint32_t RasterImplementationTest::kCommandBufferSizeBytes;
const uint32_t RasterImplementationTest::kTransferBufferSize;
const GLint RasterImplementationTest::kMaxCombinedTextureImageUnits;
const GLint RasterImplementationTest::kMaxTextureImageUnits;
const GLint RasterImplementationTest::kMaxTextureSize;
const GLint RasterImplementationTest::kNumCompressedTextureFormats;
const GLuint RasterImplementationTest::kStartId;
const GLuint RasterImplementationTest::kBuffersStartId;
const GLuint RasterImplementationTest::kTexturesStartId;
const GLuint RasterImplementationTest::kQueriesStartId;
#endif

TEST_F(RasterImplementationTest, GetBucketContents) {
  const uint32_t kBucketId = RasterImplementation::kResultBucketId;
  const uint32_t kTestSize = MaxTransferBufferSize() + 32;

  std::unique_ptr<uint8_t[]> buf(new uint8_t[kTestSize]);
  uint8_t* expected_data = buf.get();
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
  ExpectedMemoryInfo mem2 =
      GetExpectedMemory(kTestSize - MaxTransferBufferSize());

  Cmds expected;
  expected.get_bucket_start.Init(kBucketId, result1.id, result1.offset,
                                 MaxTransferBufferSize(), mem1.id, mem1.offset);
  expected.set_token1.Init(GetNextToken());
  expected.get_bucket_data.Init(kBucketId, MaxTransferBufferSize(),
                                kTestSize - MaxTransferBufferSize(), mem2.id,
                                mem2.offset);
  expected.set_bucket_size2.Init(kBucketId, 0);
  expected.set_token2.Init(GetNextToken());

  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(DoAll(
          SetMemory(result1.ptr, kTestSize),
          SetMemoryFromArray(mem1.ptr, expected_data, MaxTransferBufferSize())))
      .WillOnce(SetMemoryFromArray(mem2.ptr,
                                   expected_data + MaxTransferBufferSize(),
                                   kTestSize - MaxTransferBufferSize()))
      .RetiresOnSaturation();

  std::vector<int8_t> data;
  GetBucketContents(kBucketId, &data);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  ASSERT_EQ(kTestSize, data.size());
  EXPECT_EQ(0, memcmp(expected_data, &data[0], data.size()));
}

TEST_F(RasterImplementationTest, BeginEndQueryEXT) {
  //  GL_COMMANDS_COMPLETED_CHROMIUM,
  //  GL_CURRENT_QUERY_EXT

  GLuint expected_ids[2] = {1, 2};  // These must match what's actually genned.
  struct GenCmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  GenCmds expected_gen_cmds;
  expected_gen_cmds.gen.Init(base::size(expected_ids), &expected_ids[0]);
  GLuint ids[base::size(expected_ids)] = {
      0,
  };
  gl_->GenQueriesEXT(base::size(expected_ids), &ids[0]);
  EXPECT_EQ(0,
            memcmp(&expected_gen_cmds, commands_, sizeof(expected_gen_cmds)));
  GLuint id1 = ids[0];
  GLuint id2 = ids[1];
  ClearCommands();

  // Test BeginQueryEXT fails if id = 0.
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, 0);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test BeginQueryEXT inserts command.
  struct BeginCmds {
    cmds::BeginQueryEXT begin_query;
  };
  BeginCmds expected_begin_cmds;
  const void* commands = GetPut();
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id1);
  QueryTracker::Query* query = GetQuery(id1);
  ASSERT_TRUE(query != nullptr);
  expected_begin_cmds.begin_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM, id1,
                                       query->shm_id(), query->shm_offset());
  EXPECT_EQ(
      0, memcmp(&expected_begin_cmds, commands, sizeof(expected_begin_cmds)));
  ClearCommands();

  // Test BeginQueryEXT fails if between Begin/End.
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id2);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test EndQueryEXT sends command
  struct EndCmds {
    cmds::EndQueryEXT end_query;
  };
  commands = GetPut();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EndCmds expected_end_cmds;
  expected_end_cmds.end_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM,
                                   query->submit_count());
  EXPECT_EQ(0, memcmp(&expected_end_cmds, commands, sizeof(expected_end_cmds)));

  // Test EndQueryEXT fails if no current query.
  ClearCommands();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());

  // Test 2nd Begin/End increments count.
  base::subtle::Atomic32 old_submit_count = query->submit_count();
  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id1);
  EXPECT_EQ(old_submit_count, query->submit_count());
  commands = GetPut();
  gl_->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  EXPECT_NE(old_submit_count, query->submit_count());
  expected_end_cmds.end_query.Init(GL_COMMANDS_COMPLETED_CHROMIUM,
                                   query->submit_count());
  EXPECT_EQ(0, memcmp(&expected_end_cmds, commands, sizeof(expected_end_cmds)));

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
  available = 1u;
  gl_->GetQueryObjectuivEXT(
      id1, GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT, &available);
  EXPECT_EQ(0u, available);
}

TEST_F(RasterImplementationManualInitTest, BadQueryTargets) {
  ContextInitOptions init_options;
  init_options.sync_query = false;
  ASSERT_TRUE(Initialize(init_options));

  GLuint id = 0;
  gl_->GenQueriesEXT(1, &id);
  ClearCommands();

  gl_->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, id);
  EXPECT_EQ(GL_INVALID_OPERATION, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));

  gl_->BeginQueryEXT(0x123, id);
  EXPECT_EQ(GL_INVALID_ENUM, CheckError());
  EXPECT_EQ(nullptr, GetQuery(id));
}

TEST_F(RasterImplementationTest, GenUnverifiedSyncTokenCHROMIUM) {
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

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM) {
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
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, base::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_EQ(kNamespaceId, sync_token.namespace_id());
  EXPECT_EQ(kCommandBufferId, sync_token.command_buffer_id());
  EXPECT_EQ(kFenceSync, sync_token.release_count());
  EXPECT_TRUE(sync_token.verified_flush());
}

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM_Sequence) {
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
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, base::size(sync_token_datas));
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(RasterImplementationTest, VerifySyncTokensCHROMIUM_EmptySyncToken) {
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
  gl_->VerifySyncTokensCHROMIUM(sync_token_datas, base::size(sync_token_datas));
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(GL_NO_ERROR, CheckError());

  EXPECT_TRUE(sync_token1.verified_flush());
  EXPECT_TRUE(sync_token2.verified_flush());
}

TEST_F(RasterImplementationTest, WaitSyncTokenCHROMIUM) {
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
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token_data);

  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gpu::SyncToken verified_sync_token = sync_token;
  verified_sync_token.SetVerifyFlush();
  EXPECT_CALL(*gpu_control_, WaitSyncToken(verified_sync_token));
  gl_->WaitSyncTokenCHROMIUM(sync_token_data);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, WaitSyncTokenCHROMIUMErrors) {
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

static void CountCallback(int* count) {
  (*count)++;
}

TEST_F(RasterImplementationTest, SignalSyncToken) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID())
      .WillRepeatedly(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillRepeatedly(Return(kCommandBufferId));

  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce(Invoke([&signal_closure](const SyncToken& sync_token,
                                         base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      }));
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // When GpuControl runs the callback, the original callback we gave to
  // RasterImplementation is run.
  std::move(signal_closure).Run();
  EXPECT_EQ(1, signaled_count);
}

TEST_F(RasterImplementationTest, SignalSyncTokenAfterContextLoss) {
  const CommandBufferNamespace kNamespaceId = CommandBufferNamespace::GPU_IO;
  const CommandBufferId kCommandBufferId = CommandBufferId::FromUnsafeValue(1);
  const uint64_t kFenceSync = 123u;

  EXPECT_CALL(*gpu_control_, GetNamespaceID()).WillOnce(Return(kNamespaceId));
  EXPECT_CALL(*gpu_control_, GetCommandBufferID())
      .WillOnce(Return(kCommandBufferId));
  EXPECT_CALL(*gpu_control_, GenerateFenceSyncRelease())
      .WillOnce(Return(kFenceSync));
  gpu::SyncToken sync_token;
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  int signaled_count = 0;

  // Request a signal sync token, which gives a callback to the GpuControl to
  // run when the sync token is reached.
  base::OnceClosure signal_closure;
  EXPECT_CALL(*gpu_control_, DoSignalSyncToken(_, _))
      .WillOnce(Invoke([&signal_closure](const SyncToken& sync_token,
                                         base::OnceClosure* callback) {
        signal_closure = std::move(*callback);
      }));
  EXPECT_CALL(*gpu_control_, CanWaitUnverifiedSyncToken(sync_token))
      .WillOnce(Return(true));
  gl_->SignalSyncToken(sync_token,
                       base::BindOnce(&CountCallback, &signaled_count));
  EXPECT_EQ(0, signaled_count);

  // Inform the RasterImplementation that the context is lost.
  GpuControlClient* gl_as_client = gl_;
  gl_as_client->OnGpuControlLostContext();

  // When GpuControl runs the callback, the original callback we gave to
  // RasterImplementation is *not* run, since the context is lost and we
  // have already run the lost context callback.
  std::move(signal_closure).Run();
  EXPECT_EQ(0, signaled_count);
}

TEST_F(RasterImplementationTest, ReportLoss) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContext();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should be run when RasterImplementation is
  // notified of the loss.
  EXPECT_EQ(1, lost_count);
}

TEST_F(RasterImplementationTest, ReportLossReentrant) {
  GpuControlClient* gl_as_client = gl_;
  int lost_count = 0;
  gl_->SetLostContextCallback(base::BindOnce(&CountCallback, &lost_count));
  EXPECT_EQ(0, lost_count);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  gl_as_client->OnGpuControlLostContextMaybeReentrant();
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), gl_->GetGraphicsResetStatusKHR());
  // The lost context callback should not be run yet to avoid calling back into
  // clients re-entrantly, and having them re-enter RasterImplementation.
  EXPECT_EQ(0, lost_count);
}

TEST_F(RasterImplementationManualInitTest, FailInitOnTransferBufferFail) {
  ContextInitOptions init_options;
  init_options.transfer_buffer_initialize_fail = true;
  EXPECT_FALSE(Initialize(init_options));
}

TEST_F(RasterImplementationTest, TransferCacheSerialization) {
  gl_->set_max_inlined_entry_size_for_testing(768u);
  uint32_t buffer_size = transfer_buffer_->MaxTransferBufferSize();
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);
  ASSERT_EQ(buffer.size(), buffer_size);

  char* buffer_start = reinterpret_cast<char*>(buffer.address());
  memset(buffer_start, 0, buffer_size);
  gl_->SetRasterMappedBufferForTesting(std::move(buffer));
  auto transfer_cache = gl_->CreateTransferCacheHelperForTesting();

  std::vector<uint8_t> data(buffer_size - 16u);
  char* memory = buffer_start + 8u;
  cc::ClientRawMemoryTransferCacheEntry inlined_entry(data);
  EXPECT_EQ(transfer_cache->CreateEntry(inlined_entry, memory), data.size());
  EXPECT_EQ(memcmp(data.data(), memory, data.size()), 0);

  data.resize(buffer_size + 16u);
  memory = buffer_start + 8u;
  cc::ClientRawMemoryTransferCacheEntry non_inlined_entry(data);
  EXPECT_EQ(transfer_cache->CreateEntry(non_inlined_entry, memory), 0u);
}

TEST_F(RasterImplementationTest, SetActiveURLCHROMIUM) {
  const uint32_t kURLBucketId = RasterImplementation::kResultBucketId;
  const std::string url = "chrome://test";
  const uint32_t kPaddedStringSize =
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

#include "base/macros.h"
#include "gpu/command_buffer/client/raster_implementation_unittest_autogen.h"

}  // namespace raster
}  // namespace gpu
