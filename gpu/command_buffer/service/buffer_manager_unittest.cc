// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/buffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class BufferManagerTestBase : public GpuServiceTest {
 protected:
  void SetUpBase(
      MemoryTracker* memory_tracker,
      FeatureInfo* feature_info,
      const char* extensions) {
    GpuServiceTest::SetUp();
    if (feature_info) {
      TestHelper::SetupFeatureInfoInitExpectations(gl_.get(), extensions);
      feature_info->InitializeForTesting();
    }
    error_state_ = std::make_unique<MockErrorState>();
    manager_ = std::make_unique<BufferManager>(memory_tracker, feature_info);
  }

  void TearDown() override {
    manager_->MarkContextLost();
    manager_->Destroy();
    manager_.reset();
    error_state_.reset();
    GpuServiceTest::TearDown();
  }

  GLenum GetInitialTarget(const Buffer* buffer) const {
    return buffer->initial_target();
  }

  void DoBufferData(
      Buffer* buffer, GLenum target, GLsizeiptr size, GLenum usage,
      const GLvoid* data, GLenum error) {
    TestHelper::DoBufferData(
        gl_.get(), error_state_.get(), manager_.get(),
        buffer, target, size, usage, data, error);
  }

  bool DoBufferSubData(
      Buffer* buffer, GLenum target, GLintptr offset, GLsizeiptr size,
      const GLvoid* data) {
    if (!buffer->CheckRange(offset, size)) {
      return false;
    }
    if (!buffer->IsClientSideArray()) {
      EXPECT_CALL(*gl_, BufferSubData(target, offset, size, _))
          .Times(1)
          .RetiresOnSaturation();
    }
    manager_->DoBufferSubData(buffer, target, offset, size, data);
    return true;
  }

  void RunGetMaxValueForRangeUint8Test(bool enable_primitive_restart)
  {
    const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
    const GLuint kClientBufferId = 1;
    const GLuint kServiceBufferId = 11;
    const uint8_t data[] = {10, 9, 8, 7, 6, 0xFFu, 4, 3, 2, 1};
    const uint8_t new_data[] = {100, 120, 110};
    manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
    Buffer* buffer = manager_->GetBuffer(kClientBufferId);
    ASSERT_TRUE(buffer != nullptr);
    manager_->SetTarget(buffer, kTarget);
    DoBufferData(buffer, kTarget, sizeof(data), GL_STATIC_DRAW, nullptr,
                 GL_NO_ERROR);
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 0, sizeof(data), data));
    GLuint max_value;
    // Check entire range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(10u, max_value);
    } else {
      EXPECT_EQ(0xFFu, max_value);
    }
    // Check sub range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        4, 3, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(6u, max_value);
    } else {
      EXPECT_EQ(0xFFu, max_value);
    }
    // Check changing sub range succeeds.
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 4, sizeof(new_data),
                                new_data));
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        4, 3, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    max_value = 0;
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    // Check out of range fails.
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        0, 11, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        10, 1, GL_UNSIGNED_BYTE, enable_primitive_restart, &max_value));
  }

  void RunGetMaxValueForRangeUint16Test(bool enable_primitive_restart) {
    const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
    const GLuint kClientBufferId = 1;
    const GLuint kServiceBufferId = 11;
    const uint16_t data[] = {10, 9, 8, 7, 6, 0xFFFF, 4, 3, 2, 1};
    const uint16_t new_data[] = {100, 120, 110};
    manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
    Buffer* buffer = manager_->GetBuffer(kClientBufferId);
    ASSERT_TRUE(buffer != nullptr);
    manager_->SetTarget(buffer, kTarget);
    DoBufferData(buffer, kTarget, sizeof(data), GL_STATIC_DRAW, nullptr,
                 GL_NO_ERROR);
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 0, sizeof(data), data));
    GLuint max_value;
    // Check entire range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(10u, max_value);
    } else {
      EXPECT_EQ(0xFFFFu, max_value);
    }
    // Check odd offset fails for GL_UNSIGNED_SHORT.
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        1, 10, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    // Check sub range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        8, 3, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(6u, max_value);
    } else {
      EXPECT_EQ(0xFFFFu, max_value);
    }
    // Check changing sub range succeeds.
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 8, sizeof(new_data),
                                new_data));
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        8, 3, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    max_value = 0;
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    // Check out of range fails.
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        0, 11, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        20, 1, GL_UNSIGNED_SHORT, enable_primitive_restart, &max_value));
  }

  void RunGetMaxValueForRangeUint32Test(bool enable_primitive_restart) {
    const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
    const GLuint kClientBufferId = 1;
    const GLuint kServiceBufferId = 11;
    const uint32_t data[] = {10, 9, 8, 7, 6, 0xFFFFFFFFu, 4, 3, 2, 1};
    const uint32_t new_data[] = {100, 120, 110};
    manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
    Buffer* buffer = manager_->GetBuffer(kClientBufferId);
    ASSERT_TRUE(buffer != nullptr);
    manager_->SetTarget(buffer, kTarget);
    DoBufferData(buffer, kTarget, sizeof(data), GL_STATIC_DRAW, nullptr,
                 GL_NO_ERROR);
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 0, sizeof(data), data));
    GLuint max_value;
    // Check entire range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(10u, max_value);
    } else {
      EXPECT_EQ(0xFFFFFFFFu, max_value);
    }
    // Check non aligned offsets fails for GL_UNSIGNED_INT.
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        1, 10, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        2, 10, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        3, 10, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    // Check sub range succeeds.
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        16, 3, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    if (enable_primitive_restart) {
      EXPECT_EQ(6u, max_value);
    } else {
      EXPECT_EQ(0xFFFFFFFFu, max_value);
    }
    // Check changing sub range succeeds.
    EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 16, sizeof(new_data),
                                new_data));
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        16, 3, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    max_value = 0;
    EXPECT_TRUE(buffer->GetMaxValueForRange(
        0, 10, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    EXPECT_EQ(120u, max_value);
    // Check out of range fails.
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        0, 11, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
    EXPECT_FALSE(buffer->GetMaxValueForRange(
        40, 1, GL_UNSIGNED_INT, enable_primitive_restart, &max_value));
  }

  std::unique_ptr<BufferManager> manager_;
  std::unique_ptr<MockErrorState> error_state_;
};

class BufferManagerTest : public BufferManagerTestBase {
 protected:
  void SetUp() override { SetUpBase(nullptr, nullptr, ""); }
};

class BufferManagerMemoryTrackerTest : public BufferManagerTestBase {
 protected:
  void SetUp() override { SetUpBase(&mock_memory_tracker_, nullptr, ""); }

  StrictMock<MockMemoryTracker> mock_memory_tracker_;
};

class BufferManagerClientSideArraysTest : public BufferManagerTestBase {
 protected:
  void SetUp() override {
    GpuDriverBugWorkarounds gpu_driver_bug_workarounds;
    gpu_driver_bug_workarounds.use_client_side_arrays_for_stream_buffers = true;
    GpuFeatureInfo gpu_feature_info;
    feature_info_ =
        new FeatureInfo(gpu_driver_bug_workarounds, gpu_feature_info);
    SetUpBase(nullptr, feature_info_.get(), "");
  }

  scoped_refptr<FeatureInfo> feature_info_;
};

#define EXPECT_MEMORY_ALLOCATION_CHANGE(old_size, new_size)    \
  EXPECT_CALL(mock_memory_tracker_,                            \
              TrackMemoryAllocatedChange(new_size - old_size)) \
      .Times(1)                                                \
      .RetiresOnSaturation()

TEST_F(BufferManagerTest, Basic) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLuint kClientBuffer1Id = 1;
  const GLuint kServiceBuffer1Id = 11;
  const GLsizeiptr kBuffer1Size = 123;
  const GLuint kClientBuffer2Id = 2;
  // Check we can create buffer.
  manager_->CreateBuffer(kClientBuffer1Id, kServiceBuffer1Id);
  // Check buffer got created.
  Buffer* buffer1 = manager_->GetBuffer(kClientBuffer1Id);
  ASSERT_TRUE(buffer1 != nullptr);
  EXPECT_EQ(0u, GetInitialTarget(buffer1));
  EXPECT_EQ(0, buffer1->size());
  EXPECT_EQ(static_cast<GLenum>(GL_STATIC_DRAW), buffer1->usage());
  EXPECT_FALSE(buffer1->IsDeleted());
  EXPECT_FALSE(buffer1->IsClientSideArray());
  EXPECT_EQ(kServiceBuffer1Id, buffer1->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_->GetClientId(buffer1->service_id(), &client_id));
  EXPECT_EQ(kClientBuffer1Id, client_id);
  manager_->SetTarget(buffer1, kTarget);
  EXPECT_EQ(kTarget, GetInitialTarget(buffer1));
  // Check we and set its size.
  DoBufferData(buffer1, kTarget, kBuffer1Size, GL_DYNAMIC_DRAW, nullptr,
               GL_NO_ERROR);
  EXPECT_EQ(kBuffer1Size, buffer1->size());
  EXPECT_EQ(static_cast<GLenum>(GL_DYNAMIC_DRAW), buffer1->usage());
  // Check we get nothing for a non-existent buffer.
  EXPECT_TRUE(manager_->GetBuffer(kClientBuffer2Id) == nullptr);
  // Check trying to a remove non-existent buffers does not crash.
  manager_->RemoveBuffer(kClientBuffer2Id);
  // Check that it gets deleted when the last reference is released.
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, ::testing::Pointee(kServiceBuffer1Id)))
      .Times(1)
      .RetiresOnSaturation();
  // Check we can't get the buffer after we remove it.
  manager_->RemoveBuffer(kClientBuffer1Id);
  EXPECT_TRUE(manager_->GetBuffer(kClientBuffer1Id) == nullptr);
}

TEST_F(BufferManagerMemoryTrackerTest, Basic) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLuint kClientBuffer1Id = 1;
  const GLuint kServiceBuffer1Id = 11;
  const GLsizeiptr kBuffer1Size1 = 123;
  const GLsizeiptr kBuffer1Size2 = 456;
  // Check we can create buffer.
  manager_->CreateBuffer(kClientBuffer1Id, kServiceBuffer1Id);
  // Check buffer got created.
  Buffer* buffer1 = manager_->GetBuffer(kClientBuffer1Id);
  ASSERT_TRUE(buffer1 != nullptr);
  manager_->SetTarget(buffer1, kTarget);
  // Check we and set its size.
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, kBuffer1Size1);
  DoBufferData(buffer1, kTarget, kBuffer1Size1, GL_DYNAMIC_DRAW, nullptr,
               GL_NO_ERROR);
  EXPECT_MEMORY_ALLOCATION_CHANGE(kBuffer1Size1, 0);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, kBuffer1Size2);
  DoBufferData(buffer1, kTarget, kBuffer1Size2, GL_DYNAMIC_DRAW, nullptr,
               GL_NO_ERROR);
  // On delete it will get freed.
  EXPECT_MEMORY_ALLOCATION_CHANGE(kBuffer1Size2, 0);
}

TEST_F(BufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create buffer.
  manager_->CreateBuffer(kClient1Id, kService1Id);
  // Check buffer got created.
  Buffer* buffer1 = manager_->GetBuffer(kClient1Id);
  ASSERT_TRUE(buffer1 != nullptr);
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy();
  // Check the resources were released.
  buffer1 = manager_->GetBuffer(kClient1Id);
  ASSERT_TRUE(buffer1 == nullptr);
}

TEST_F(BufferManagerTest, DoBufferSubData) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLuint kClientBufferId = 1;
  const GLuint kServiceBufferId = 11;
  const uint8_t data[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
  Buffer* buffer = manager_->GetBuffer(kClientBufferId);
  ASSERT_TRUE(buffer != nullptr);
  manager_->SetTarget(buffer, kTarget);
  DoBufferData(buffer, kTarget, sizeof(data), GL_STATIC_DRAW, nullptr,
               GL_NO_ERROR);
  EXPECT_TRUE(DoBufferSubData(buffer, kTarget, 0, sizeof(data), data));
  EXPECT_TRUE(DoBufferSubData(buffer, kTarget, sizeof(data), 0, data));
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, sizeof(data), 1, data));
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, 0, sizeof(data) + 1, data));
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, -1, sizeof(data), data));
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, 0, -1, data));
  DoBufferData(buffer, kTarget, 1, GL_STATIC_DRAW, nullptr, GL_NO_ERROR);
  const int size = 0x20000;
  auto temp = base::HeapArray<uint8_t>::Uninit(size);
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, 0 - size, size, temp.data()));
  EXPECT_FALSE(DoBufferSubData(buffer, kTarget, 1, size / 2, temp.data()));
}

TEST_F(BufferManagerTest, GetRange) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLuint kClientBufferId = 1;
  const GLuint kServiceBufferId = 11;
  const GLsizeiptr kDataSize = 10;
  manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
  Buffer* buffer = manager_->GetBuffer(kClientBufferId);
  ASSERT_TRUE(buffer != nullptr);
  manager_->SetTarget(buffer, kTarget);
  DoBufferData(buffer, kTarget, kDataSize, GL_STATIC_DRAW, nullptr,
               GL_NO_ERROR);
  const char* buf =
      static_cast<const char*>(buffer->GetRange(0, kDataSize));
  ASSERT_TRUE(buf != nullptr);
  const char* buf1 =
      static_cast<const char*>(buffer->GetRange(1, kDataSize - 1));
  EXPECT_EQ(buf + 1, buf1);
  EXPECT_TRUE(buffer->GetRange(kDataSize, 1) == nullptr);
  EXPECT_TRUE(buffer->GetRange(0, kDataSize + 1) == nullptr);
  EXPECT_TRUE(buffer->GetRange(-1, kDataSize) == nullptr);
  EXPECT_TRUE(buffer->GetRange(-0, -1) == nullptr);
  const int size = 0x20000;
  DoBufferData(buffer, kTarget, size / 2, GL_STATIC_DRAW, nullptr, GL_NO_ERROR);
  EXPECT_TRUE(buffer->GetRange(0 - size, size) == nullptr);
  EXPECT_TRUE(buffer->GetRange(1, size / 2) == nullptr);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint8) {
  RunGetMaxValueForRangeUint8Test(false);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint8PrimitiveRestart) {
  RunGetMaxValueForRangeUint8Test(true);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint16) {
  RunGetMaxValueForRangeUint16Test(false);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint16PrimitiveRestart) {
  RunGetMaxValueForRangeUint16Test(true);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint32) {
  RunGetMaxValueForRangeUint32Test(false);
}

TEST_F(BufferManagerTest, GetMaxValueForRangeUint32PrimitiveRestart) {
  RunGetMaxValueForRangeUint32Test(true);
}

TEST_F(BufferManagerTest, UseDeletedBuffer) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLuint kClientBufferId = 1;
  const GLuint kServiceBufferId = 11;
  const GLsizeiptr kDataSize = 10;
  manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
  scoped_refptr<Buffer> buffer = manager_->GetBuffer(kClientBufferId);
  ASSERT_TRUE(buffer.get() != nullptr);
  manager_->SetTarget(buffer.get(), kTarget);
  // Remove buffer
  manager_->RemoveBuffer(kClientBufferId);
  // Use it after removing
  DoBufferData(buffer.get(), kTarget, kDataSize, GL_STATIC_DRAW, nullptr,
               GL_NO_ERROR);
  // Check that it gets deleted when the last reference is released.
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, ::testing::Pointee(kServiceBufferId)))
      .Times(1)
      .RetiresOnSaturation();
  buffer = nullptr;
}

// Test buffers get shadowed when they are supposed to be.
TEST_F(BufferManagerClientSideArraysTest, StreamBuffersAreShadowed) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLuint kClientBufferId = 1;
  const GLuint kServiceBufferId = 11;
  static const uint32_t data[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
  Buffer* buffer = manager_->GetBuffer(kClientBufferId);
  ASSERT_TRUE(buffer != nullptr);
  manager_->SetTarget(buffer, kTarget);
  DoBufferData(
      buffer, kTarget, sizeof(data), GL_STREAM_DRAW, data, GL_NO_ERROR);
  EXPECT_TRUE(buffer->IsClientSideArray());
  EXPECT_EQ(0, memcmp(data, buffer->GetRange(0, sizeof(data)), sizeof(data)));
  DoBufferData(
      buffer, kTarget, sizeof(data), GL_DYNAMIC_DRAW, data, GL_NO_ERROR);
  EXPECT_FALSE(buffer->IsClientSideArray());
}

TEST_F(BufferManagerTest, MaxValueCacheClearedCorrectly) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLuint kClientBufferId = 1;
  const GLuint kServiceBufferId = 11;
  const uint32_t data1[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  const uint32_t data2[] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
  const uint32_t data3[] = {30, 29, 28};
  manager_->CreateBuffer(kClientBufferId, kServiceBufferId);
  Buffer* buffer = manager_->GetBuffer(kClientBufferId);
  ASSERT_TRUE(buffer != nullptr);
  manager_->SetTarget(buffer, kTarget);
  GLuint max_value;
  // Load the buffer with some initial data, and then get the maximum value for
  // a range, which has the side effect of caching it.
  DoBufferData(
      buffer, kTarget, sizeof(data1), GL_STATIC_DRAW, data1, GL_NO_ERROR);
  EXPECT_TRUE(
      buffer->GetMaxValueForRange(0, 10, GL_UNSIGNED_INT, false, &max_value));
  EXPECT_EQ(10u, max_value);
  // Check that any cached values are invalidated if the buffer is reloaded
  // with the same amount of data (but different content)
  ASSERT_EQ(sizeof(data2), sizeof(data1));
  DoBufferData(
      buffer, kTarget, sizeof(data2), GL_STATIC_DRAW, data2, GL_NO_ERROR);
  EXPECT_TRUE(
      buffer->GetMaxValueForRange(0, 10, GL_UNSIGNED_INT, false, &max_value));
  EXPECT_EQ(20u, max_value);
  // Check that any cached values are invalidated if the buffer is reloaded
  // with entirely different content.
  ASSERT_NE(sizeof(data3), sizeof(data1));
  DoBufferData(
      buffer, kTarget, sizeof(data3), GL_STATIC_DRAW, data3, GL_NO_ERROR);
  EXPECT_TRUE(
      buffer->GetMaxValueForRange(0, 3, GL_UNSIGNED_INT, false, &max_value));
  EXPECT_EQ(30u, max_value);
}

TEST_F(BufferManagerTest, BindBufferConflicts) {
  manager_->set_allow_buffers_on_multiple_targets(false);
  GLuint client_id = 1;
  GLuint service_id = 101;

  {
    // Once a buffer is bound to ELEMENT_ARRAY_BUFFER, it can't be bound to
    // any other targets except for GL_COPY_READ/WRITE_BUFFER.
    manager_->CreateBuffer(client_id, service_id);
    Buffer* buffer = manager_->GetBuffer(client_id);
    ASSERT_TRUE(buffer != nullptr);
    EXPECT_TRUE(manager_->SetTarget(buffer, GL_ELEMENT_ARRAY_BUFFER));
    EXPECT_TRUE(manager_->SetTarget(buffer, GL_COPY_READ_BUFFER));
    EXPECT_TRUE(manager_->SetTarget(buffer, GL_COPY_WRITE_BUFFER));
    EXPECT_FALSE(manager_->SetTarget(buffer, GL_ARRAY_BUFFER));
    EXPECT_FALSE(manager_->SetTarget(buffer, GL_PIXEL_PACK_BUFFER));
    EXPECT_FALSE(manager_->SetTarget(buffer, GL_PIXEL_UNPACK_BUFFER));
    EXPECT_FALSE(manager_->SetTarget(buffer, GL_TRANSFORM_FEEDBACK_BUFFER));
    EXPECT_FALSE(manager_->SetTarget(buffer, GL_UNIFORM_BUFFER));
  }

  {
    // Except for ELEMENT_ARRAY_BUFFER, a buffer can switch to any targets.
    const GLenum kTargets[] = {
      GL_ARRAY_BUFFER,
      GL_COPY_READ_BUFFER,
      GL_COPY_WRITE_BUFFER,
      GL_PIXEL_PACK_BUFFER,
      GL_PIXEL_UNPACK_BUFFER,
      GL_TRANSFORM_FEEDBACK_BUFFER,
      GL_UNIFORM_BUFFER
    };
    for (size_t ii = 0; ii < std::size(kTargets); ++ii) {
      client_id++;
      service_id++;
      manager_->CreateBuffer(client_id, service_id);
      Buffer* buffer = manager_->GetBuffer(client_id);
      ASSERT_TRUE(buffer != nullptr);

      EXPECT_TRUE(manager_->SetTarget(buffer, kTargets[ii]));
      for (size_t jj = 0; jj < std::size(kTargets); ++jj) {
        EXPECT_TRUE(manager_->SetTarget(buffer, kTargets[jj]));
      }
      EXPECT_EQ(kTargets[ii], GetInitialTarget(buffer));
    }
  }

  {
    // Once a buffer is bound to non ELEMENT_ARRAY_BUFFER target, it can't be
    // bound to ELEMENT_ARRAY_BUFFER target.
    const GLenum kTargets[] = {
      GL_ARRAY_BUFFER,
      GL_COPY_READ_BUFFER,
      GL_COPY_WRITE_BUFFER,
      GL_PIXEL_PACK_BUFFER,
      GL_PIXEL_UNPACK_BUFFER,
      GL_TRANSFORM_FEEDBACK_BUFFER,
      GL_UNIFORM_BUFFER
    };
    for (size_t ii = 0; ii < std::size(kTargets); ++ii) {
      client_id++;
      service_id++;
      manager_->CreateBuffer(client_id, service_id);
      Buffer* buffer = manager_->GetBuffer(client_id);
      ASSERT_TRUE(buffer != nullptr);

      EXPECT_TRUE(manager_->SetTarget(buffer, kTargets[ii]));
      for (size_t jj = 0; jj < std::size(kTargets); ++jj) {
        EXPECT_TRUE(manager_->SetTarget(buffer, kTargets[jj]));
      }
    }
  }
}

TEST_F(BufferManagerTest, DeleteBufferAfterContextLost) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  manager_->CreateBuffer(kClient1Id, kService1Id);
  Buffer* buffer1 = manager_->GetBuffer(kClient1Id);
  ASSERT_TRUE(buffer1 != nullptr);
  manager_->MarkContextLost();
  // Removing buffers after MarkContextLost cause no GL calls.
  manager_->RemoveBuffer(kClient1Id);
  manager_->Destroy();
  // Check the resources were released.
  buffer1 = manager_->GetBuffer(kClient1Id);
  ASSERT_TRUE(buffer1 == nullptr);
}

}  // namespace gles2
}  // namespace gpu
