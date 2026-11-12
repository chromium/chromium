// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_command_allocator_provider.h"

#include <memory>

#include "media/base/win/d3d12_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoDefault;
using ::testing::NiceMock;
using ::testing::Return;

namespace media {

class D3D12CommandAllocatorProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    device_ = MakeComPtr<NiceMock<D3D12DeviceMock>>();
    command_queue_ = MakeComPtr<NiceMock<D3D12CommandQueueMock>>();
    command_allocator_ = MakeComPtr<NiceMock<D3D12CommandAllocatorMock>>();
    second_command_allocator_ =
        MakeComPtr<NiceMock<D3D12CommandAllocatorMock>>();
    fence_mock_ = MakeComPtr<NiceMock<D3D12FenceMock>>();

    ON_CALL(*device_.Get(), CreateFence(_, _, _, _))
        .WillByDefault(SetComPointeeAndReturnOk<3>(fence_mock_.Get()));
    // The first created allocator is |command_allocator_|, the second one is
    // |second_command_allocator_|.
    ON_CALL(*device_.Get(), CreateCommandAllocator(_, _, _))
        .WillByDefault(
            [this](D3D12_COMMAND_LIST_TYPE, REFIID riid, void** allocator) {
              auto& mock = num_created_allocators_++ == 0
                               ? command_allocator_
                               : second_command_allocator_;
              mock->AddRef();
              *allocator = mock.Get();
              return S_OK;
            });
    ON_CALL(*command_queue_.Get(), Signal(_, _)).WillByDefault(Return(S_OK));
    ON_CALL(*command_allocator_.Get(), Reset()).WillByDefault(Return(S_OK));
    ON_CALL(*second_command_allocator_.Get(), Reset())
        .WillByDefault(Return(S_OK));
    ON_CALL(*fence_mock_.Get(), GetCompletedValue()).WillByDefault([this]() {
      return completed_value_;
    });
    ON_CALL(*fence_mock_.Get(), SetEventOnCompletion(_, _))
        .WillByDefault([](UINT64, HANDLE event) {
          ::SetEvent(event);
          return S_OK;
        });

    fence_ = D3D12Fence::Create(device_.Get());
    ASSERT_TRUE(fence_);
    provider_ =
        std::make_unique<D3D12CommandAllocatorProvider>(device_, fence_);
  }

  // Records one frame: gets the command allocator and submits it.
  ID3D12CommandAllocator* RecordFrame() {
    ID3D12CommandAllocator* allocator =
        provider_->PrepareCommandAllocator().value_or(nullptr);
    if (allocator) {
      auto fence_value = fence_->Signal(*command_queue_.Get());
      EXPECT_TRUE(fence_value.has_value());
      provider_->OnSubmitted(fence_value.value());
    }
    return allocator;
  }

  // The fence value that the GPU has completed.
  UINT64 completed_value_ = 0;
  int num_created_allocators_ = 0;

  Microsoft::WRL::ComPtr<D3D12DeviceMock> device_;
  Microsoft::WRL::ComPtr<D3D12CommandQueueMock> command_queue_;
  Microsoft::WRL::ComPtr<D3D12CommandAllocatorMock> command_allocator_;
  Microsoft::WRL::ComPtr<D3D12CommandAllocatorMock> second_command_allocator_;
  Microsoft::WRL::ComPtr<D3D12FenceMock> fence_mock_;
  scoped_refptr<D3D12Fence> fence_;
  std::unique_ptr<D3D12CommandAllocatorProvider> provider_;
};

TEST_F(D3D12CommandAllocatorProviderTest, FirstCallCreatesAllocator) {
  EXPECT_CALL(*device_.Get(), CreateCommandAllocator(
                                  D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE, _, _))
      .Times(1);
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(0);

  EXPECT_EQ(provider_->PrepareCommandAllocator().value_or(nullptr),
            command_allocator_.Get());
}

TEST_F(D3D12CommandAllocatorProviderTest, DoesNotResetWhileGpuIsBehind) {
  EXPECT_CALL(*device_.Get(), CreateCommandAllocator(_, _, _)).Times(1);
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(0);

  // The GPU has not completed anything.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(RecordFrame(), command_allocator_.Get());
  }
}

TEST_F(D3D12CommandAllocatorProviderTest, ResetsWhenGpuCompletedAllCommands) {
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(1);

  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 1.
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 2.
  completed_value_ = 2;
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());
}

TEST_F(D3D12CommandAllocatorProviderTest,
       DoesNotResetWhenGpuCompletedOnlySomeCommands) {
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(0);

  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 1.
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 2.
  completed_value_ = 1;
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());
}

TEST_F(D3D12CommandAllocatorProviderTest, ResetsAgainAfterEachCatchUp) {
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(2);

  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 1.
  completed_value_ = 1;
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Reset, fence value 2.
  completed_value_ = 2;
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Reset.
}

TEST_F(D3D12CommandAllocatorProviderTest, SwitchesAllocatorAfterMaxFrames) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  EXPECT_CALL(*device_.Get(), CreateCommandAllocator(_, _, _)).Times(2);
  EXPECT_CALL(*fence_mock_.Get(), SetEventOnCompletion(_, _)).Times(0);
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(0);
  EXPECT_CALL(*second_command_allocator_.Get(), Reset()).Times(0);
  // The GPU never catches up while the frames are recorded.
  for (size_t i = 0; i < kMax; ++i) {
    EXPECT_EQ(RecordFrame(), command_allocator_.Get());
  }
  // Switch to a newly created allocator without waiting.
  for (size_t i = 0; i < kMax; ++i) {
    EXPECT_EQ(RecordFrame(), second_command_allocator_.Get());
  }
}
TEST_F(D3D12CommandAllocatorProviderTest,
       SwitchesBackAndResetsWithoutWaitingIfGpuCompletedTheOther) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  for (size_t i = 0; i < 2 * kMax; ++i) {
    RecordFrame();
  }
  // The GPU has completed the first allocator's commands but not the second's.
  completed_value_ = kMax;
  EXPECT_CALL(*fence_mock_.Get(), SetEventOnCompletion(_, _)).Times(0);
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(1);
  EXPECT_CALL(*second_command_allocator_.Get(), Reset()).Times(0);
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());
}
TEST_F(D3D12CommandAllocatorProviderTest, WaitsIfBothAllocatorsAreInUse) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  for (size_t i = 0; i < 2 * kMax; ++i) {
    RecordFrame();
  }
  // The GPU has not completed the first allocator's commands: wait for them,
  // then reset.
  testing::InSequence sequence;
  EXPECT_CALL(*fence_mock_.Get(), SetEventOnCompletion(kMax, _)).Times(1);
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(1);
  EXPECT_EQ(RecordFrame(), command_allocator_.Get());
}
TEST_F(D3D12CommandAllocatorProviderTest, ReturnsErrorIfCreateFails) {
  ON_CALL(*device_.Get(), CreateCommandAllocator(_, _, _))
      .WillByDefault(Return(E_FAIL));

  EXPECT_FALSE(provider_->PrepareCommandAllocator().has_value());
}

TEST_F(D3D12CommandAllocatorProviderTest,
       ReturnsErrorIfCreateOfNextAllocatorFails) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  for (size_t i = 0; i < kMax; ++i) {
    EXPECT_EQ(RecordFrame(), command_allocator_.Get());
  }
  // The first allocator is full and the GPU has not caught up, so the next
  // allocator is needed, but it cannot be created.
  EXPECT_CALL(*device_.Get(), CreateCommandAllocator(_, _, _))
      .WillOnce(Return(E_FAIL))
      .WillRepeatedly(DoDefault());
  EXPECT_FALSE(provider_->PrepareCommandAllocator().has_value());

  // The provider has not switched, and succeeds once creation works again.
  EXPECT_EQ(RecordFrame(), second_command_allocator_.Get());
}

TEST_F(D3D12CommandAllocatorProviderTest,
       ReturnsErrorIfResetOfNextAllocatorFails) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  for (size_t i = 0; i < 2 * kMax; ++i) {
    RecordFrame();
  }
  // The second allocator is full and the GPU has not caught up, so the first
  // allocator is needed, but it cannot be reset.
  ON_CALL(*command_allocator_.Get(), Reset()).WillByDefault(Return(E_FAIL));
  EXPECT_FALSE(provider_->PrepareCommandAllocator().has_value());

  // The provider has not switched: it keeps using the second allocator.
  completed_value_ = 2 * kMax;
  EXPECT_CALL(*second_command_allocator_.Get(), Reset()).Times(1);
  EXPECT_EQ(RecordFrame(), second_command_allocator_.Get());
}

TEST_F(D3D12CommandAllocatorProviderTest, ReturnsErrorIfResetFails) {
  ON_CALL(*command_allocator_.Get(), Reset()).WillByDefault(Return(E_FAIL));

  EXPECT_EQ(RecordFrame(), command_allocator_.Get());  // Fence value 1.
  completed_value_ = 1;
  EXPECT_FALSE(provider_->PrepareCommandAllocator().has_value());
}

TEST_F(D3D12CommandAllocatorProviderTest, ReturnsErrorIfWaitFails) {
  constexpr size_t kMax = D3D12CommandAllocatorProvider::kMaxFramesPerAllocator;
  for (size_t i = 0; i < 2 * kMax; ++i) {
    RecordFrame();
  }
  ON_CALL(*fence_mock_.Get(), SetEventOnCompletion(_, _))
      .WillByDefault(Return(E_FAIL));
  EXPECT_CALL(*command_allocator_.Get(), Reset()).Times(0);
  EXPECT_FALSE(provider_->PrepareCommandAllocator().has_value());
}

}  // namespace media
