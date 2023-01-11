// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class D3D11PictureBufferTest : public ::testing::Test {
 public:
  D3D11PictureBufferTest() {
    picture_buffer_ = base::MakeRefCounted<D3D11PictureBuffer>(
        task_environment_.GetMainThreadTaskRunner(), nullptr, 0, nullptr,
        gfx::Size(), 0);
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<D3D11PictureBuffer> picture_buffer_;
};

// The processor proxy wraps the VideoDevice/VideoContext and stores some of the
// d3d11 types. Make sure that the arguments we give these methods are passed
// through correctly.
TEST_F(D3D11PictureBufferTest, InClientUse) {
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