// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/cast/receiver/mojom/cast_streaming_renderer_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class CastStreamingRendererTest : public testing::Test {
 public:
  CastStreamingRendererTest() {
    auto mock_renderer = std::make_unique<testing::StrictMock<MockRenderer>>();
    mock_renderer_ = mock_renderer.get();
    renderer_ = std::make_unique<CastStreamingRenderer>(
        std::move(mock_renderer), task_environment_.GetMainThreadTaskRunner(),
        remote_.BindNewPipeAndPassReceiver());
  }

  ~CastStreamingRendererTest() override = default;

  MOCK_METHOD1(OnInitializationComplete, void(PipelineStatus));

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  mojo::Remote<media::mojom::Renderer> remote_;

  MockRenderer* mock_renderer_;
  std::unique_ptr<CastStreamingRenderer> renderer_;
};

TEST_F(CastStreamingRendererTest, RendererInitializeInitializesMojoPipe) {
  testing::StrictMock<MockMediaResource> mock_media_resource;
  testing::StrictMock<MockRendererClient> mock_renderer_client;

  auto init_cb =
      base::BindOnce(&CastStreamingRendererTest::OnInitializationComplete,
                     base::Unretained(this));

  EXPECT_CALL(*mock_renderer_, OnInitialize(&mock_media_resource,
                                            &mock_renderer_client, testing::_))
      .WillOnce([this](MediaResource* media_resource, RendererClient* client,
                       PipelineStatusCallback& init_cb) {
        auto result =
            base::BindOnce(std::move(init_cb), PipelineStatus::PIPELINE_OK);
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, std::move(result));
      });

  remote_->SetPlaybackRate(1.0);
  remote_->StartPlayingFrom(base::TimeDelta{});
  task_environment_.RunUntilIdle();

  renderer_->Initialize(&mock_media_resource, &mock_renderer_client,
                        std::move(init_cb));

  EXPECT_CALL(*this, OnInitializationComplete(PipelineStatus::PIPELINE_OK));
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(1.0));
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(testing::_));
  task_environment_.RunUntilIdle();
}

}  // namespace cast
}  // namespace media
