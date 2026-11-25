// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_copy_command_list_wrapper.h"

#include <memory>

#include "media/base/win/d3d12_mocks.h"
#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

class D3D12CopyCommandQueueWrapperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    wrapper_ = std::make_unique<D3D12CopyCommandQueueWrapper>(
        command_queue_, command_allocator_, command_list_, fence_);
  }

  // Reports the work submitted by Execute() as in flight for the first query
  // only, so that the wait in the wrapper's destructor does not block.
  void ExpectWorkInFlightOnce() {
    EXPECT_CALL(*fence_.Get(), GetCompletedValue())
        .WillOnce(Return(0))
        .WillRepeatedly(Return(1));
  }

  Microsoft::WRL::ComPtr<D3D12CommandQueueMock> command_queue_ =
      MakeComPtr<NiceMock<D3D12CommandQueueMock>>();
  Microsoft::WRL::ComPtr<D3D12CommandAllocatorMock> command_allocator_ =
      MakeComPtr<NiceMock<D3D12CommandAllocatorMock>>();
  Microsoft::WRL::ComPtr<D3D12GraphicsCommandListMock> command_list_ =
      MakeComPtr<NiceMock<D3D12GraphicsCommandListMock>>();
  Microsoft::WRL::ComPtr<D3D12FenceMock> fence_ =
      MakeComPtr<NiceMock<D3D12FenceMock>>();
  std::unique_ptr<D3D12CopyCommandQueueWrapper> wrapper_;
};

TEST_F(D3D12CopyCommandQueueWrapperTest, WaitQueuesGpuWaitOnFence) {
  auto input_fence = MakeComPtr<NiceMock<D3D12FenceMock>>();
  EXPECT_CALL(*command_queue_.Get(), Wait(input_fence.Get(), 42u))
      .WillOnce(Return(S_OK));
  EXPECT_TRUE(wrapper_->Wait({input_fence.Get(), 42}));
}

TEST_F(D3D12CopyCommandQueueWrapperTest, WaitReturnsFalseOnFailure) {
  auto input_fence = MakeComPtr<NiceMock<D3D12FenceMock>>();
  EXPECT_CALL(*command_queue_.Get(), Wait(input_fence.Get(), 42u))
      .WillOnce(Return(E_FAIL));
  EXPECT_FALSE(wrapper_->Wait({input_fence.Get(), 42}));
}

TEST_F(D3D12CopyCommandQueueWrapperTest, WaitSyncWithoutSubmittedWork) {
  EXPECT_CALL(*fence_.Get(), SetEventOnCompletion).Times(0);
  EXPECT_TRUE(wrapper_->WaitSync().is_ok());
}

TEST_F(D3D12CopyCommandQueueWrapperTest, WaitSyncWaitsForSubmittedWork) {
  ExpectWorkInFlightOnce();
  EXPECT_CALL(*fence_.Get(), SetEventOnCompletion(1, _))
      .WillOnce([](UINT64, HANDLE event) {
        ::SetEvent(event);
        return S_OK;
      });
  ASSERT_TRUE(wrapper_->Execute().first);
  EXPECT_TRUE(wrapper_->WaitSync().is_ok());
}

TEST_F(D3D12CopyCommandQueueWrapperTest, WaitSyncReturnsErrorWhenWaitFails) {
  ExpectWorkInFlightOnce();
  EXPECT_CALL(*fence_.Get(), SetEventOnCompletion(1, _))
      .WillOnce(Return(E_FAIL));
  ASSERT_TRUE(wrapper_->Execute().first);
  EXPECT_FALSE(wrapper_->WaitSync().is_ok());
}

}  // namespace media
