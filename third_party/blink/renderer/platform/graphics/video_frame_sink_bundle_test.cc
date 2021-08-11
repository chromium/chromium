// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_compositor_frame_sink_client.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_embedded_frame_sink_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/mock_frame_sink_bundle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

const viz::FrameSinkId kTestParentFrameSinkId(1, 1);
const viz::FrameSinkId kTestDifferentParentFrameSinkId(1, 42);

const viz::FrameSinkId kTestVideoSinkId1(1, 2);
const viz::FrameSinkId kTestVideoSinkId2(1, 3);
const viz::FrameSinkId kTestVideoSinkId3(1, 4);

MATCHER(IsEmpty, "") {
  return arg.IsEmpty();
}
MATCHER(IsFrame, "") {
  return arg->data->which() ==
         viz::mojom::blink::BundledFrameSubmissionData::Tag::kFrame;
}
MATCHER(IsDidNotProduceFrame, "") {
  return arg->data->which() == viz::mojom::blink::BundledFrameSubmissionData::
                                   Tag::kDidNotProduceFrame;
}
MATCHER_P(ForSink, sink_id, "") {
  return arg->sink_id == sink_id;
}

viz::mojom::blink::BeginFrameInfoPtr MakeBeginFrameInfo(uint32_t sink_id) {
  return viz::mojom::blink::BeginFrameInfo::New(
      sink_id,
      viz::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, 1, 1, base::TimeTicks(),
                                  base::TimeTicks(), base::TimeDelta(),
                                  viz::BeginFrameArgs::NORMAL),
      WTF::HashMap<uint32_t, viz::FrameTimingDetails>());
}

class MockFrameSinkBundleClient
    : public viz::mojom::blink::FrameSinkBundleClient {
 public:
  ~MockFrameSinkBundleClient() override = default;

  // viz::mojom::blink::FrameSinkBundleClient implementation:
  MOCK_METHOD3(
      FlushNotifications,
      void(WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr> acks,
           WTF::Vector<viz::mojom::blink::BeginFrameInfoPtr> begin_frames,
           WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr>
               reclaimed_resources));
  MOCK_METHOD2(OnBeginFramePausedChanged, void(uint32_t sink_id, bool paused));
  MOCK_METHOD2(OnCompositorFrameTransitionDirectiveProcessed,
               void(uint32_t sink_id, uint32_t sequence_id));
};

const viz::LocalSurfaceId kTestSurfaceId(
    1,
    base::UnguessableToken::Deserialize(1, 2));

class VideoFrameSinkBundleTest : public testing::Test {
 public:
  VideoFrameSinkBundleTest() = default;

  ~VideoFrameSinkBundleTest() override {
    VideoFrameSinkBundle::DestroySharedInstancesForTesting();
  }

  void CreateTestBundle() {
    EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
    test_bundle();
  }

  VideoFrameSinkBundle& test_bundle() {
    return VideoFrameSinkBundle::GetOrCreateSharedInstance(
        frame_sink_provider(), kTestParentFrameSinkId,
        /*for_media_streams=*/true);
  }

  MockEmbeddedFrameSinkProvider& frame_sink_provider() {
    return mock_frame_sink_provider_;
  }

  MockFrameSinkBundleClient& mock_bundle_client() {
    return mock_bundle_client_;
  }

  MockFrameSinkBundle& mock_frame_sink_bundle() {
    return frame_sink_provider().mock_frame_sink_bundle();
  }

 private:
  MockEmbeddedFrameSinkProvider mock_frame_sink_provider_;
  MockFrameSinkBundleClient mock_bundle_client_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(VideoFrameSinkBundleTest, GetOrCreateSharedInstance) {
  // Verify that GetOrCreateSharedInstance lazily initializes an instance.
  EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
  VideoFrameSinkBundle& bundle =
      VideoFrameSinkBundle::GetOrCreateSharedInstance(
          frame_sink_provider(), kTestParentFrameSinkId,
          /*for_media_streams=*/true);
  EXPECT_FALSE(bundle.is_context_lost());

  // And that acquiring an instance with the same parameters reuses the existing
  // instance.
  VideoFrameSinkBundle& other_bundle =
      VideoFrameSinkBundle::GetOrCreateSharedInstance(
          frame_sink_provider(), kTestParentFrameSinkId,
          /*for_media_streams=*/true);
  EXPECT_EQ(&other_bundle, &bundle);

  // And finally that acquiring an instance with either a different parent frame
  // sink ID or a different frame type will create a new instance.
  EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
  VideoFrameSinkBundle& yet_another_bundle =
      VideoFrameSinkBundle::GetOrCreateSharedInstance(
          frame_sink_provider(), kTestDifferentParentFrameSinkId,
          /*for_media_streams=*/true);
  EXPECT_NE(&yet_another_bundle, &bundle);

  EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
  VideoFrameSinkBundle::GetOrCreateSharedInstance(
      frame_sink_provider(), kTestDifferentParentFrameSinkId,
      /*for_media_streams=*/false);
}

TEST_F(VideoFrameSinkBundleTest, Reconnect) {
  // Verifies that a VideoFrameSinkBundle reestablishes a connection to Viz
  // when acquired again after disconnection.
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  bundle.OnContextLost(bundle.bundle_id());

  EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
  VideoFrameSinkBundle& another_bundle = test_bundle();
  EXPECT_EQ(&another_bundle, &bundle);
}

TEST_F(VideoFrameSinkBundleTest, Cleanup) {
  // Verifies that shared instances clean up after themselves when their last
  // client is removed.
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink;
  ignore_result(sink.BindNewPipeAndPassReceiver());
  MockCompositorFrameSinkClient client;
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  bundle.AddClient(kTestVideoSinkId1, &client, sink);
  bundle.AddClient(kTestVideoSinkId2, &client, sink);

  EXPECT_EQ(&bundle, VideoFrameSinkBundle::GetSharedInstanceForTesting(
                         kTestParentFrameSinkId, true));
  bundle.RemoveClient(kTestVideoSinkId1);
  EXPECT_EQ(&bundle, VideoFrameSinkBundle::GetSharedInstanceForTesting(
                         kTestParentFrameSinkId, true));
  bundle.RemoveClient(kTestVideoSinkId2);
  EXPECT_EQ(nullptr, VideoFrameSinkBundle::GetSharedInstanceForTesting(
                         kTestParentFrameSinkId, true));
}

TEST_F(VideoFrameSinkBundleTest, PassThrough) {
  // Verifies that as a safe default, VideoFrameSinkBundle passes frame
  // submissions through to Viz without any batching.
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  bundle.SubmitCompositorFrame(
      2, kTestSurfaceId, viz::MakeDefaultCompositorFrame(), absl::nullopt, 0);
  EXPECT_CALL(mock_frame_sink_bundle(),
              Submit(ElementsAre(AllOf(IsFrame(), ForSink(2u)))))
      .Times(1);
  mock_frame_sink_bundle().FlushReceiver();

  bundle.DidNotProduceFrame(3, viz::BeginFrameAck(1, 2, false));
  EXPECT_CALL(mock_frame_sink_bundle(),
              Submit(ElementsAre(AllOf(IsDidNotProduceFrame(), ForSink(3u)))));
  mock_frame_sink_bundle().FlushReceiver();
}

TEST_F(VideoFrameSinkBundleTest, BatchSubmissionsDuringOnBeginFrame) {
  // Verifies that submitted compositor frames (or DidNotProduceFrames) are
  // batched when submitted during an OnBeginFrame handler, and flushed
  // afterwords.
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();

  MockCompositorFrameSinkClient mock_client1;
  MockCompositorFrameSinkClient mock_client2;
  MockCompositorFrameSinkClient mock_client3;
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink;
  ignore_result(sink.BindNewPipeAndPassReceiver());
  bundle.AddClient(kTestVideoSinkId1, &mock_client1, sink);
  bundle.AddClient(kTestVideoSinkId2, &mock_client2, sink);
  bundle.AddClient(kTestVideoSinkId3, &mock_client3, sink);

  // All clients will submit a frame synchronously within OnBeginFrame.
  EXPECT_CALL(mock_client1, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.SubmitCompositorFrame(kTestVideoSinkId1.sink_id(), kTestSurfaceId,
                                 viz::MakeDefaultCompositorFrame(),
                                 absl::nullopt, 0);
  });
  EXPECT_CALL(mock_client2, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.DidNotProduceFrame(kTestVideoSinkId2.sink_id(),
                              viz::BeginFrameAck(1, 1, false));
  });
  EXPECT_CALL(mock_client3, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.SubmitCompositorFrame(kTestVideoSinkId3.sink_id(), kTestSurfaceId,
                                 viz::MakeDefaultCompositorFrame(),
                                 absl::nullopt, 0);
  });

  WTF::Vector<viz::mojom::blink::BeginFrameInfoPtr> begin_frames;
  begin_frames.push_back(MakeBeginFrameInfo(kTestVideoSinkId1.sink_id()));
  begin_frames.push_back(MakeBeginFrameInfo(kTestVideoSinkId2.sink_id()));
  begin_frames.push_back(MakeBeginFrameInfo(kTestVideoSinkId3.sink_id()));
  bundle.FlushNotifications({}, std::move(begin_frames), {});

  EXPECT_CALL(
      mock_frame_sink_bundle(),
      Submit(UnorderedElementsAre(
          AllOf(IsFrame(), ForSink(kTestVideoSinkId1.sink_id())),
          AllOf(IsDidNotProduceFrame(), ForSink(kTestVideoSinkId2.sink_id())),
          AllOf(IsFrame(), ForSink(kTestVideoSinkId3.sink_id())))))
      .Times(1);
  mock_frame_sink_bundle().FlushReceiver();
}

}  // namespace
}  // namespace blink
