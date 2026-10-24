// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_picture_buffer.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/base/win/d3d12_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace media {

class D3DPictureBufferTest : public ::testing::Test {
 public:
  D3DPictureBufferTest() {
    // Made-up size for the images.
    const gfx::Size size_{100, 200};
    auto wrapper = std::make_unique<DefaultD3DTextureWrapper>(
        size_, gfx::ColorSpace::CreateREC709(), viz::MultiPlaneFormat::kNV12,
        /*device=*/nullptr, gpu::GpuDriverBugWorkarounds());
    auto texture = MakeComPtr<NiceMock<D3D11Texture2DMock>>();
    picture_buffer_ = base::MakeRefCounted<D3D11PictureBufferImpl>(
        task_environment_.GetMainThreadTaskRunner(), std::move(texture), 0,
        std::move(wrapper), size_, 0);
    auto resource = MakeComPtr<NiceMock<D3D12ResourceMock>>();
    d3d12_picture_buffer_ = base::MakeRefCounted<D3D12PictureBufferImpl>(
        task_environment_.GetMainThreadTaskRunner(), std::move(resource), 0,
        /*texture_wrapper=*/nullptr, size_, 0);
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<D3DPictureBuffer> picture_buffer_;
  scoped_refptr<D3DPictureBuffer> d3d12_picture_buffer_;
};

TEST_F(D3DPictureBufferTest, AsMatchingImplementation) {
  EXPECT_EQ(picture_buffer_->As<D3D11PictureBufferImpl>(),
            picture_buffer_.get());
  EXPECT_EQ(d3d12_picture_buffer_->As<D3D12PictureBufferImpl>(),
            d3d12_picture_buffer_.get());
}

TEST_F(D3DPictureBufferTest, AsWrongImplementation) {
  EXPECT_CHECK_DEATH(picture_buffer_->As<D3D12PictureBufferImpl>());
  EXPECT_CHECK_DEATH(d3d12_picture_buffer_->As<D3D11PictureBufferImpl>());
}

// The processor proxy wraps the VideoDevice/VideoContext and stores some of the
// d3d11 types. Make sure that the arguments we give these methods are passed
// through correctly.
TEST_F(D3DPictureBufferTest, InClientUse) {
  EXPECT_FALSE(picture_buffer_->in_client_use());

  // Add two client refs.
  picture_buffer_->add_client_use();
  EXPECT_TRUE(picture_buffer_->in_client_use());
  picture_buffer_->add_client_use();
  EXPECT_TRUE(picture_buffer_->in_client_use());

  // Remove them.  Should still be in use by the client until the second one has
  // been removed.
  picture_buffer_->remove_client_use();
  EXPECT_TRUE(picture_buffer_->in_client_use());
  picture_buffer_->remove_client_use();
  EXPECT_FALSE(picture_buffer_->in_client_use());
}

}  // namespace media
