// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
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

const uint32_t kTestClientId = 1;

const viz::FrameSinkId kTestVideoSinkId1(kTestClientId, 2);
const viz::FrameSinkId kTestVideoSinkId2(kTestClientId, 3);
const viz::FrameSinkId kTestVideoSinkId3(kTestClientId, 4);

class MockBeginFrameObserver : public VideoFrameSinkBundle::BeginFrameObserver {
 public:
  MOCK_METHOD(void, OnBeginFrameCompletion, (), (override));
  MOCK_METHOD(void, OnBeginFrameCompletionEnabled, (bool), (override));
};

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
      WTF::HashMap<uint32_t, viz::FrameTimingDetails>(),
      /*frame_ack=*/false, WTF::Vector<viz::ReturnedResource>());
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
    base::UnguessableToken::CreateForTesting(1, 2));

class VideoFrameSinkBundleTest : public testing::Test {
 public:
  VideoFrameSinkBundleTest() {
    VideoFrameSinkBundle::SetFrameSinkProviderForTesting(
        &frame_sink_provider());
  }

  ~VideoFrameSinkBundleTest() override {
    VideoFrameSinkBundle::SetFrameSinkProviderForTesting(nullptr);
    VideoFrameSinkBundle::DestroySharedInstanceForTesting();
  }

  void CreateTestBundle() {
    EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_).Times(1);
    test_bundle();
  }

  VideoFrameSinkBundle& test_bundle() {
    return VideoFrameSinkBundle::GetOrCreateSharedInstance(kTestClientId);
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
      VideoFrameSinkBundle::GetOrCreateSharedInstance(kTestClientId);

  // And that acquiring an instance with the same client ID reuses the existing
  // instance.
  VideoFrameSinkBundle& other_bundle =
      VideoFrameSinkBundle::GetOrCreateSharedInstance(kTestClientId);
  EXPECT_EQ(&other_bundle, &bundle);
}

TEST_F(VideoFrameSinkBundleTest, Reconnect) {
  // Verifies that VideoFrameSinkBundle is destroyed and recreated if
  // disconnected, reestablishing a connection to Viz as a result.
  CreateTestBundle();
  viz::FrameSinkBundleId first_bundle_id = test_bundle().bundle_id();

  base::RunLoop loop;
  test_bundle().set_disconnect_handler_for_testing(loop.QuitClosure());
  mock_frame_sink_bundle().Disconnect();
  loop.Run();

  EXPECT_CALL(frame_sink_provider(), CreateFrameSinkBundle_)
      .Times(1)
      .WillOnce([&] { loop.Quit(); });

  viz::FrameSinkBundleId second_bundle_id = test_bundle().bundle_id();
  EXPECT_EQ(first_bundle_id.client_id(), second_bundle_id.client_id());
  EXPECT_NE(first_bundle_id.bundle_id(), second_bundle_id.bundle_id());
}

TEST_F(VideoFrameSinkBundleTest, PassThrough) {
  // Verifies that as a safe default, VideoFrameSinkBundle passes frame
  // submissions through to Viz without any batching.
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  bundle.SubmitCompositorFrame(
      2, kTestSurfaceId, viz::MakeDefaultCompositorFrame(), std::nullopt, 0);
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
  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> provider;
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink1;
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink2;
  mojo::Remote<viz::mojom::blink::CompositorFrameSink> sink3;
  mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient> receiver1{
      &mock_client1};
  mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient> receiver2{
      &mock_client2};
  mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient> receiver3{
      &mock_client3};
  std::ignore = provider.BindNewPipeAndPassReceiver();
  bundle.AddClient(kTestVideoSinkId1, &mock_client1, provider, receiver1,
                   sink1);
  bundle.AddClient(kTestVideoSinkId2, &mock_client2, provider, receiver2,
                   sink2);
  bundle.AddClient(kTestVideoSinkId3, &mock_client3, provider, receiver3,
                   sink3);

  // All clients will submit a frame synchronously within OnBeginFrame.
  EXPECT_CALL(mock_client1, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.SubmitCompositorFrame(kTestVideoSinkId1.sink_id(), kTestSurfaceId,
                                 viz::MakeDefaultCompositorFrame(),
                                 std::nullopt, 0);
  });
  EXPECT_CALL(mock_client2, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.DidNotProduceFrame(kTestVideoSinkId2.sink_id(),
                              viz::BeginFrameAck(1, 1, false));
  });
  EXPECT_CALL(mock_client3, OnBeginFrame).Times(1).WillOnce([&] {
    bundle.SubmitCompositorFrame(kTestVideoSinkId3.sink_id(), kTestSurfaceId,
                                 viz::MakeDefaultCompositorFrame(),
                                 std::nullopt, 0);
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

TEST_F(VideoFrameSinkBundleTest,
       DeliversBeginFramesDisabledWithoutSinksOnRegistration) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  auto observer = std::make_unique<MockBeginFrameObserver>();
  EXPECT_CALL(*observer, OnBeginFrameCompletionEnabled(false));
  bundle.SetBeginFrameObserver(std::move(observer));
}

TEST_F(VideoFrameSinkBundleTest,
       DeliversBeginFramesEnabledWithSinkOnRegistration) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  auto observer = std::make_unique<MockBeginFrameObserver>();
  EXPECT_CALL(*observer, OnBeginFrameCompletionEnabled(true));
  bundle.SetNeedsBeginFrame(kTestVideoSinkId1.sink_id(), true);
  bundle.SetBeginFrameObserver(std::move(observer));
}

TEST_F(VideoFrameSinkBundleTest, DeliversBeginFramesDisabledOnSinksDisabled) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  bundle.SetNeedsBeginFrame(kTestVideoSinkId1.sink_id(), true);
  auto observer = std::make_unique<MockBeginFrameObserver>();
  MockBeginFrameObserver* observer_ptr = observer.get();
  bundle.SetBeginFrameObserver(std::move(observer));
  EXPECT_CALL(*observer_ptr, OnBeginFrameCompletionEnabled(false));
  bundle.SetNeedsBeginFrame(kTestVideoSinkId1.sink_id(), false);
}

TEST_F(VideoFrameSinkBundleTest, DeliversBeginFramesEnabledOnSinkAdded) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  auto observer = std::make_unique<MockBeginFrameObserver>();
  MockBeginFrameObserver* observer_ptr = observer.get();
  bundle.SetBeginFrameObserver(std::move(observer));
  EXPECT_CALL(*observer_ptr, OnBeginFrameCompletionEnabled(true));
  bundle.SetNeedsBeginFrame(kTestVideoSinkId1.sink_id(), true);
}

TEST_F(VideoFrameSinkBundleTest,
       DeliversBeginFrameCompletionOnFlushWithBeginFrames) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();

  auto make_begin_frames = [] {
    WTF::Vector<viz::mojom::blink::BeginFrameInfoPtr> begin_frames;
    begin_frames.push_back(MakeBeginFrameInfo(kTestVideoSinkId1.sink_id()));
    return begin_frames;
  };

  auto observer = std::make_unique<MockBeginFrameObserver>();
  EXPECT_CALL(*observer, OnBeginFrameCompletion).Times(2);
  bundle.SetBeginFrameObserver(std::move(observer));
  bundle.FlushNotifications({}, make_begin_frames(), {});
  bundle.FlushNotifications({}, make_begin_frames(), {});
}

TEST_F(VideoFrameSinkBundleTest,
       OmitsBeginFrameCompletionOnceOnFlushWithoutBeginFrames) {
  CreateTestBundle();
  VideoFrameSinkBundle& bundle = test_bundle();
  auto observer = std::make_unique<MockBeginFrameObserver>();
  EXPECT_CALL(*observer, OnBeginFrameCompletion).Times(0);
  bundle.SetBeginFrameObserver(std::move(observer));
  bundle.FlushNotifications({}, {}, {});
}

}  // namespace
}  // namespace blink
