// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer_factory.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_video_renderer_sink.h"
#include "media/base/overlay_info.h"
#include "media/base/video_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {
namespace cast {
namespace {

class MockOverlayInfoCbHandler {
 public:
  MOCK_METHOD2(Call, void(bool, ProvideOverlayInfoCB));
};

}  // namespace

class CastStreamingRendererFactoryTest : public testing::Test {
 public:
  CastStreamingRendererFactoryTest()
      : mock_factory_(std::make_unique<StrictMock<MockRendererFactory>>()),
        mock_factory_ptr_(mock_factory_.get()),
        factory_(std::move(mock_factory_)) {}

  ~CastStreamingRendererFactoryTest() override = default;

 protected:
  std::unique_ptr<MockRendererFactory> mock_factory_;
  MockRendererFactory* mock_factory_ptr_;
  CastStreamingRendererFactory factory_;
};

TEST_F(CastStreamingRendererFactoryTest,
       FactoryCreationCallsOtherFactoryCreate) {
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner(
      new base::TestSimpleTaskRunner());
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner(
      new base::TestSimpleTaskRunner());
  auto audio_sink = base::MakeRefCounted<StrictMock<MockAudioRendererSink>>();
  StrictMock<MockVideoRendererSink> video_sink;
  StrictMock<MockOverlayInfoCbHandler> cb_handler;
  RequestOverlayInfoCB mock_cb = base::BindRepeating(
      &MockOverlayInfoCbHandler::Call, base::Unretained(&cb_handler));
  gfx::ColorSpace color_space;

  EXPECT_CALL(*mock_factory_ptr_,
              CreateRenderer(Eq(ByRef(media_task_runner)),
                             Eq(ByRef(worker_task_runner)), audio_sink.get(),
                             &video_sink, testing::_, Eq(ByRef(color_space))))
      .WillOnce(Return(ByMove(std::make_unique<MockRenderer>())));
  factory_.CreateRenderer(media_task_runner, worker_task_runner,
                          audio_sink.get(), &video_sink, std::move(mock_cb),
                          color_space);
}

}  // namespace cast
}  // namespace media
