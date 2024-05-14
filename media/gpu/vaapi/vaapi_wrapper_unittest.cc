// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {
VaapiWrapper::VABufferDescriptor CreateVABufferDescriptor() {
  constexpr static char kData[] = "vaBufferData";
  return VaapiWrapper::VABufferDescriptor{VAProcPipelineParameterBufferType,
                                          sizeof(kData), kData};
}

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper() : VaapiWrapper(VADisplayStateHandle(), kVideoProcess) {}
  MOCK_METHOD1(SubmitBuffer_Locked, bool(const VABufferDescriptor&));
  MOCK_METHOD0(DestroyPendingBuffers_Locked, void());

 protected:
  ~MockVaapiWrapper() override = default;
};
}  // namespace

class VaapiWrapperTest : public testing::Test {
 public:
  VaapiWrapperTest() = default;

  void SetUp() override {
    // Create a VaapiWrapper for testing.
    mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>();
    ASSERT_TRUE(mock_vaapi_wrapper_);

    ON_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked)
        .WillByDefault(
            Invoke(this, &VaapiWrapperTest::DefaultSubmitBuffer_Locked));
    ON_CALL(*mock_vaapi_wrapper_, DestroyPendingBuffers_Locked)
        .WillByDefault(Invoke(
            this, &VaapiWrapperTest::DefaultDestroyPendingBuffers_Locked));
  }
  void TearDown() override {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(
        mock_vaapi_wrapper_->sequence_checker_);
    // The VaapiWrapper destructor calls DestroyPendingBuffers_Locked(). Since
    // MockVaapiWrapper is a derived class,
    // MockVaapiWrapper::DestroyPendingBuffers_Locked() won't get called during
    // destruction even though it's a virtual function. Instead,
    // VaapiWrapper::DestroyPendingBuffers_Locked() will get called. Therefore,
    // we need to clear |pending_va_buffers_| before this happens so that
    // VaapiWrapper::DestroyPendingBuffers_Locked() doesn't call
    // vaDestroyBuffer().
    mock_vaapi_wrapper_->pending_va_buffers_.clear();
    mock_vaapi_wrapper_.reset();
  }

  bool DefaultSubmitBuffer_Locked(
      const VaapiWrapper::VABufferDescriptor& va_buffer)
      EXCLUSIVE_LOCKS_REQUIRED(mock_vaapi_wrapper_->va_lock_) {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(
        mock_vaapi_wrapper_->sequence_checker_);
    if (va_buffer.data) {
      constexpr VABufferID kFakeBufferId = 1234;
      mock_vaapi_wrapper_->pending_va_buffers_.push_back(kFakeBufferId);
      return true;
    }
    // When |va_buffer|.data is null, the base method should return false and
    // no libva calls should be made.
    const bool submit_buffer_res =
        (*mock_vaapi_wrapper_).VaapiWrapper::SubmitBuffer_Locked(va_buffer);
    if (submit_buffer_res)
      ADD_FAILURE();
    return false;
  }

  void DefaultDestroyPendingBuffers_Locked()
      EXCLUSIVE_LOCKS_REQUIRED(mock_vaapi_wrapper_->va_lock_) {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(
        mock_vaapi_wrapper_->sequence_checker_);
    mock_vaapi_wrapper_->pending_va_buffers_.clear();
  }

  size_t GetPendingBuffersSize() const {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(
        mock_vaapi_wrapper_->sequence_checker_);
    return mock_vaapi_wrapper_->pending_va_buffers_.size();
  }

 protected:
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
};

// This test ensures SubmitBuffer() calls SubmitBuffer_Locked().
TEST_F(VaapiWrapperTest, SubmitBuffer) {
  constexpr size_t kNumBuffers = 3;
  auto va_buffer = CreateVABufferDescriptor();

  EXPECT_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked(_)).Times(kNumBuffers);
  for (size_t i = 0; i < kNumBuffers; ++i) {
    EXPECT_TRUE(mock_vaapi_wrapper_->SubmitBuffer(
        va_buffer.type, va_buffer.size, va_buffer.data));
  }
  EXPECT_EQ(GetPendingBuffersSize(), kNumBuffers);
}

// This test ensures SubmitBuffers() calls SubmitBuffer_Locked() as many times
// as the number of passed buffers.
TEST_F(VaapiWrapperTest, SubmitBuffers) {
  constexpr size_t kNumBuffers = 3;
  auto va_buffer = CreateVABufferDescriptor();
  std::vector<VaapiWrapper::VABufferDescriptor> buffers(kNumBuffers, va_buffer);

  EXPECT_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked(_)).Times(kNumBuffers);
  EXPECT_TRUE(mock_vaapi_wrapper_->SubmitBuffers(buffers));
  EXPECT_EQ(GetPendingBuffersSize(), kNumBuffers);
}

// This test ensures DestroyPendingBuffers_Locked() is executed on a failure of
// SubmitBuffer().
TEST_F(VaapiWrapperTest, FailOnSubmitBuffer) {
  auto va_buffer = CreateVABufferDescriptor();

  ::testing::InSequence s;
  EXPECT_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked(_)).Times(2);
  EXPECT_CALL(*mock_vaapi_wrapper_, DestroyPendingBuffers_Locked);
  EXPECT_TRUE(mock_vaapi_wrapper_->SubmitBuffer(va_buffer.type, va_buffer.size,
                                                va_buffer.data));
  EXPECT_FALSE(mock_vaapi_wrapper_->SubmitBuffer(va_buffer.type, va_buffer.size,
                                                 /*data=*/nullptr));
  EXPECT_EQ(GetPendingBuffersSize(), 0u);
}

// This test ensures DestroyPendingBuffers_Locked() is executed on a failure of
// SubmitBuffers().
TEST_F(VaapiWrapperTest, FailOnSubmitBuffers) {
  constexpr size_t kNumBuffers = 3;
  auto va_buffer = CreateVABufferDescriptor();
  std::vector<VaapiWrapper::VABufferDescriptor> buffers(kNumBuffers, va_buffer);
  // Set data to nullptr so that VaapiWrapper::SubmitBuffer_Locked() fails.
  buffers[1].data = nullptr;

  ::testing::InSequence s;
  EXPECT_CALL(*mock_vaapi_wrapper_, SubmitBuffer_Locked(_))
      .Times(kNumBuffers - 1);
  EXPECT_CALL(*mock_vaapi_wrapper_, DestroyPendingBuffers_Locked);
  EXPECT_FALSE(mock_vaapi_wrapper_->SubmitBuffers(buffers));
  EXPECT_EQ(GetPendingBuffersSize(), 0u);
}
}  // namespace media
