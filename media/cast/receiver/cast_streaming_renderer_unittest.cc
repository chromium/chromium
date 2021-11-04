// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer.h"

#include "base/test/bind.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class CastStreamingRendererTest : public testing::Test {
 public:
  CastStreamingRendererTest() {
    auto mock_renderer = std::make_unique<testing::StrictMock<MockRenderer>>();
    mock_renderer_ = mock_renderer.get();
    renderer_ =
        std::make_unique<CastStreamingRenderer>(std::move(mock_renderer));
  }

  ~CastStreamingRendererTest() override = default;

 protected:
  MockRenderer* mock_renderer_;
  std::unique_ptr<CastStreamingRenderer> renderer_;
};

TEST_F(CastStreamingRendererTest, RendererInitializeCallsDownstreamInitialize) {
  testing::StrictMock<MockMediaResource> mock_media_resource;
  testing::StrictMock<MockRendererClient> mock_renderer_client;

  auto init_cb =
      base::BindLambdaForTesting([](PipelineStatus status) { FAIL(); });

  EXPECT_CALL(*mock_renderer_, OnInitialize(&mock_media_resource,
                                            &mock_renderer_client, testing::_));
  renderer_->Initialize(&mock_media_resource, &mock_renderer_client,
                        std::move(init_cb));
}

}  // namespace cast
}  // namespace media
