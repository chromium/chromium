// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <map>
#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/mock_media_session_service_impl.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;

using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionImageType;

namespace content {

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

static const int kPlayerId = 0;

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  explicit MockMediaSessionPlayerObserver(RenderFrameHost* rfh)
      : render_frame_host_(rfh) {}

  ~MockMediaSessionPlayerObserver() override = default;

  MOCK_METHOD1(OnSuspend, void(int player_id));
  MOCK_METHOD1(OnResume, void(int player_id));
  MOCK_METHOD2(OnSeekForward, void(int player_id, base::TimeDelta seek_time));
  MOCK_METHOD2(OnSeekBackward, void(int player_id, base::TimeDelta seek_time));
  MOCK_METHOD2(OnSetVolumeMultiplier,
               void(int player_id, double volume_multiplier));

  base::Optional<media_session::MediaPosition> GetPosition(
      int player_id) const override {
    return position_;
  }

  void SetPosition(
      const base::Optional<media_session::MediaPosition>& position) {
    position_ = position;
  }

  RenderFrameHost* render_frame_host() const override {
    return render_frame_host_;
  }

 private:
  RenderFrameHost* render_frame_host_;

  base::Optional<media_session::MediaPosition> position_;
};

}  // anonymous namespace

class MediaSessionImplServiceRoutingTest
    : public RenderViewHostImplTestHarness {
 public:
  MediaSessionImplServiceRoutingTest() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kStop);
  }

  ~MediaSessionImplServiceRoutingTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    test_service_manager_context_ =
        std::make_unique<content::TestServiceManagerContext>();

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    contents()->NavigateAndCommit(GURL("http://www.example.com"));

    main_frame_ = contents()->GetMainFrame();
    sub_frame_ = main_frame_->AppendChild("sub_frame");

    empty_metadata_.title = contents()->GetTitle();
    empty_metadata_.source_title = base::ASCIIToUTF16("example.com");
  }

  void TearDown() override {
    services_.clear();

    test_service_manager_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  void CreateServiceForFrame(TestRenderFrameHost* frame) {
    services_[frame] =
        std::make_unique<NiceMock<MockMediaSessionServiceImpl>>(frame);
  }

  void DestroyServiceForFrame(TestRenderFrameHost* frame) {
    services_.erase(frame);
  }

  MockMediaSessionClient* GetClientForFrame(TestRenderFrameHost* frame) {
    auto iter = services_.find(frame);
    return (iter != services_.end()) ? &iter->second.get()->mock_client()
                                     : nullptr;
  }

  void StartPlayerForFrame(TestRenderFrameHost* frame) {
    StartPlayerForFrame(frame, media::MediaContentType::Persistent);
  }

  void StartPlayerForFrame(TestRenderFrameHost* frame,
                           media::MediaContentType type) {
    players_[frame] =
        std::make_unique<NiceMock<MockMediaSessionPlayerObserver>>(frame);
    MediaSessionImpl::Get(contents())
        ->AddPlayer(players_[frame].get(), kPlayerId, type);
  }

  void ClearPlayersForFrame(TestRenderFrameHost* frame) {
    if (!players_.count(frame))
      return;

    MediaSessionImpl::Get(contents())
        ->RemovePlayer(players_[frame].get(), kPlayerId);
  }

  MockMediaSessionPlayerObserver* GetPlayerForFrame(
      TestRenderFrameHost* frame) {
    auto iter = players_.find(frame);
    return (iter != players_.end()) ? iter->second.get() : nullptr;
  }

  MediaSessionServiceImpl* ComputeServiceForRouting() {
    return MediaSessionImpl::Get(contents())->ComputeServiceForRouting();
  }

  MediaSessionImpl* GetMediaSession() {
    return MediaSessionImpl::Get(contents());
  }

  std::set<MediaSessionAction> GetDefaultActionsWithExtra(
      MediaSessionAction action) const {
    std::set<MediaSessionAction> actions(actions_.begin(), actions_.end());
    actions.insert(action);
    return actions;
  }

  const std::set<MediaSessionAction>& default_actions() const {
    return actions_;
  }

  const media_session::MediaMetadata& empty_metadata() const {
    return empty_metadata_;
  }

  const base::string16& GetSourceTitleForNonEmptyMetadata() const {
    return empty_metadata_.source_title;
  }

  TestRenderFrameHost* main_frame_;
  TestRenderFrameHost* sub_frame_;

  using ServiceMap = std::map<TestRenderFrameHost*,
                              std::unique_ptr<MockMediaSessionServiceImpl>>;
  ServiceMap services_;

  using PlayerMap = std::map<TestRenderFrameHost*,
                             std::unique_ptr<MockMediaSessionPlayerObserver>>;
  PlayerMap players_;

 private:
  media_session::MediaMetadata empty_metadata_;

  std::set<MediaSessionAction> actions_;

  std::unique_ptr<content::TestServiceManagerContext>
      test_service_manager_context_;
};

TEST_F(MediaSessionImplServiceRoutingTest, NoFrameProducesAudio) {
  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);
  DestroyServiceForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(sub_frame_);
  StartPlayerForFrame(sub_frame_);
  DestroyServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(sub_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       BothFrameProducesAudioButOnlySubFrameHasService) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest, PreferTopMostFrame) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       RoutedServiceUpdatedAfterRemovingPlayer) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ClearPlayersForFrame(main_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DontNotifyMetadataAndActionsChangeWhenUncontrollable) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(nullptr);
  services_[main_frame_]->EnableAction(MediaSessionAction::kPlay);

  observer.WaitForEmptyActions();
  observer.WaitForEmptyMetadata();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");
  expected_metadata.source_title = GetSourceTitleForNonEmptyMetadata();

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    observer.WaitForExpectedActions(default_actions());
    observer.WaitForExpectedMetadata(empty_metadata());
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    blink::mojom::SpecMediaMetadataPtr spec_metadata(
        blink::mojom::SpecMediaMetadata::New());
    spec_metadata->title = base::ASCIIToUTF16("title");
    spec_metadata->artist = base::ASCIIToUTF16("artist");
    spec_metadata->album = base::ASCIIToUTF16("album");

    services_[main_frame_]->SetMetadata(std::move(spec_metadata));
    services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);

    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForExpectedActions(
        GetDefaultActionsWithExtra(MediaSessionAction::kSeekForward));
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenTurningControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");
  expected_metadata.source_title = GetSourceTitleForNonEmptyMetadata();

  CreateServiceForFrame(main_frame_);

  {
    blink::mojom::SpecMediaMetadataPtr spec_metadata(
        blink::mojom::SpecMediaMetadata::New());
    spec_metadata->title = base::ASCIIToUTF16("title");
    spec_metadata->artist = base::ASCIIToUTF16("artist");
    spec_metadata->album = base::ASCIIToUTF16("album");

    services_[main_frame_]->SetMetadata(std::move(spec_metadata));
  }

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    observer.WaitForEmptyActions();
    observer.WaitForExpectedMetadata(empty_metadata());
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    StartPlayerForFrame(main_frame_);

    observer.WaitForExpectedMetadata(expected_metadata);
    observer.WaitForExpectedActions(
        GetDefaultActionsWithExtra(MediaSessionAction::kSeekForward));
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyActionsAndMetadataChangeWhenTurningUncontrollable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");
  expected_metadata.source_title = GetSourceTitleForNonEmptyMetadata();

  CreateServiceForFrame(main_frame_);

  {
    blink::mojom::SpecMediaMetadataPtr spec_metadata(
        blink::mojom::SpecMediaMetadata::New());
    spec_metadata->title = base::ASCIIToUTF16("title");
    spec_metadata->artist = base::ASCIIToUTF16("artist");
    spec_metadata->album = base::ASCIIToUTF16("album");

    services_[main_frame_]->SetMetadata(std::move(spec_metadata));
  }

  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    observer.WaitForExpectedActions(default_actions());
    observer.WaitForExpectedMetadata(expected_metadata);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    ClearPlayersForFrame(main_frame_);

    observer.WaitForEmptyActions();
    observer.WaitForExpectedMetadata(empty_metadata());
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPauseBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(sub_frame_), OnSuspend(_));
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kPause, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kPause);

  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(MediaSessionAction::kPause);

  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPauseBehaviorWhenSubFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(sub_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSuspend(_));
  EXPECT_CALL(*GetClientForFrame(sub_frame_),
              DidReceiveAction(MediaSessionAction::kPause, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[sub_frame_]->EnableAction(MediaSessionAction::kPause);

  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(MediaSessionAction::kPause);

  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestReceivingPauseActionWhenNoServiceRouted) {
  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  // This should not crash.
  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(MediaSessionAction::kPause);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPreviousTrackBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kPreviousTrack, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kPreviousTrack);

  MediaSessionImpl::Get(contents())->PreviousTrack();
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestNextTrackBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kNextTrack, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kNextTrack);

  MediaSessionImpl::Get(contents())->NextTrack();
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, TestSeekBackwardBehaviourDefault) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_),
              OnSeekBackward(_, kDefaultSeekTime))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekBackward, _))
      .Times(0);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime * -1);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestSeekBackwardBehaviourWhenActionEnabled) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSeekBackward(_, _)).Times(0);
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekBackward, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekBackward);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime * -1);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, TestSeekForwardBehaviourDefault) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_),
              OnSeekForward(_, kDefaultSeekTime))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekForward, _))
      .Times(0);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestSeekForwardBehaviourWhenActionEnabled) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSeekForward(_, _)).Times(0);
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekForward, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestSeekToBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(10);

  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekTo, _))
      .WillOnce([&run_loop, &seek_time](auto a, auto details) {
        EXPECT_EQ(seek_time, details->get_seek_to()->seek_time);
        EXPECT_FALSE(details->get_seek_to()->fast_seek);
        run_loop.Quit();
      });

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekTo);

  MediaSessionImpl::Get(contents())->SeekTo(seek_time);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestScrubToBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(10);

  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kSeekTo, _))
      .WillOnce([&run_loop, &seek_time](auto a, auto details) {
        EXPECT_EQ(seek_time, details->get_seek_to()->seek_time);
        EXPECT_TRUE(details->get_seek_to()->fast_seek);
        run_loop.Quit();
      });

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekTo);

  MediaSessionImpl::Get(contents())->ScrubTo(seek_time);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, SeekToActionEnablesScrubTo) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  std::set<MediaSessionAction> expected_actions(default_actions().begin(),
                                                default_actions().end());
  expected_actions.insert(MediaSessionAction::kSeekTo);
  expected_actions.insert(MediaSessionAction::kScrubTo);

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekTo);

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(expected_actions);

  services_[main_frame_]->DisableAction(MediaSessionAction::kSeekTo);
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverMetadataWhenControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");
  expected_metadata.source_title = GetSourceTitleForNonEmptyMetadata();

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    blink::mojom::SpecMediaMetadataPtr spec_metadata(
        blink::mojom::SpecMediaMetadata::New());
    spec_metadata->title = base::ASCIIToUTF16("title");
    spec_metadata->artist = base::ASCIIToUTF16("artist");
    spec_metadata->album = base::ASCIIToUTF16("album");

    services_[main_frame_]->SetMetadata(std::move(spec_metadata));

    observer.WaitForExpectedMetadata(expected_metadata);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverMetadataEmptyWhenControllable) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    services_[main_frame_]->SetMetadata(nullptr);

    // When the session becomes controllable we should receive default
    // metadata. The |is_controllable| boolean will also become true.
    observer.WaitForExpectedMetadata(empty_metadata());
    EXPECT_TRUE(observer.session_info()->is_controllable);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWhenTurningUncontrollable) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    ClearPlayersForFrame(main_frame_);

    // When the session becomes inactive it will also become uncontrollable so
    // we should check the |is_controllable| boolean.
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    EXPECT_FALSE(observer.session_info()->is_controllable);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest, NotifyObserverWhenActionsChange) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(
      GetDefaultActionsWithExtra(MediaSessionAction::kSeekForward));

  services_[main_frame_]->DisableAction(MediaSessionAction::kSeekForward);
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplServiceRoutingTest, DefaultActionsAlwaysSupported) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  services_[main_frame_]->EnableAction(MediaSessionAction::kPlay);

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());

  services_[main_frame_]->DisableAction(MediaSessionAction::kPlay);

  // This will cause the observer to be flushed with the latest actions and
  // kPlay should still be there even though we disabled it.
  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);
  observer.WaitForExpectedActions(
      GetDefaultActionsWithExtra(MediaSessionAction::kSeekForward));
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DefaultActionsRemovedIfUncontrollable) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_, media::MediaContentType::OneShot);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    observer.WaitForEmptyActions();
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    services_[main_frame_]->EnableAction(MediaSessionAction::kPlay);

    std::set<MediaSessionAction> expected_actions;
    expected_actions.insert(MediaSessionAction::kPlay);
    observer.WaitForExpectedActions(expected_actions);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest, NotifyObserverOnNavigation) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  contents()->NavigateAndCommit(GURL("http://www.google.com/test"));

  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = contents()->GetTitle();
  expected_metadata.source_title = base::ASCIIToUTF16("google.com");
  observer.WaitForExpectedMetadata(expected_metadata);
}

TEST_F(MediaSessionImplServiceRoutingTest, NotifyObserverOnTitleChange) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());

  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("new title");
  expected_metadata.source_title = GetSourceTitleForNonEmptyMetadata();

  contents()->UpdateTitle(contents()->GetMainFrame(), expected_metadata.title,
                          base::i18n::TextDirection::LEFT_TO_RIGHT);

  observer.WaitForExpectedMetadata(expected_metadata);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithActionsOnAddWhenServiceNotPresent) {
  StartPlayerForFrame(main_frame_);

  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithActionsOnAddWhenServicePresent) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithActionsOnAddWhenServiceDestroyed) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  services_[main_frame_]->EnableAction(MediaSessionAction::kSeekForward);

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());
  observer.WaitForExpectedActions(
      GetDefaultActionsWithExtra(MediaSessionAction::kSeekForward));

  DestroyServiceForFrame(main_frame_);

  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  observer.WaitForExpectedActions(default_actions());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithEmptyImagesWhenServiceNotPresent) {
  StartPlayerForFrame(main_frame_);
  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    std::vector<media_session::MediaImage> expected_images;
    observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                         expected_images);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithImagesWhenServicePresent) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  std::vector<media_session::MediaImage> expected_images;

  media_session::MediaImage test_image_1;
  test_image_1.src = GURL("https://www.google.com");
  expected_images.push_back(test_image_1);

  media_session::MediaImage test_image_2;
  test_image_2.src = GURL("https://www.example.org");
  expected_images.push_back(test_image_2);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    std::vector<media_session::MediaImage> empty_images;
    observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                         empty_images);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());

    blink::mojom::SpecMediaMetadataPtr spec_metadata(
        blink::mojom::SpecMediaMetadata::New());
    spec_metadata->artwork.push_back(test_image_1);
    spec_metadata->artwork.push_back(test_image_2);

    services_[main_frame_]->SetMetadata(std::move(spec_metadata));
    observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                         expected_images);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                         expected_images);
  }

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    ClearPlayersForFrame(main_frame_);

    std::vector<media_session::MediaImage> empty_images;
    observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                         empty_images);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyObserverWithImagesWhenMultipleServicesPresent) {
  CreateServiceForFrame(sub_frame_);
  StartPlayerForFrame(sub_frame_);

  EXPECT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());

  media_session::MediaImage test_image;
  test_image.src = GURL("https://www.google.com");

  blink::mojom::SpecMediaMetadataPtr spec_metadata(
      blink::mojom::SpecMediaMetadata::New());
  spec_metadata->artwork.push_back(test_image);
  services_[sub_frame_]->SetMetadata(std::move(spec_metadata));

  // Since |sub_frame_| is the routed service then we should see the artwork
  // from that service.
  std::vector<media_session::MediaImage> expected_images;
  expected_images.push_back(test_image);
  observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                       expected_images);

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  // Now that |main_frame_| is routed then only artwork from that frame should
  // be used.
  std::vector<media_session::MediaImage> empty_images;
  observer.WaitForExpectedImagesOfType(MediaSessionImageType::kArtwork,
                                       empty_images);
}

TEST_F(MediaSessionImplServiceRoutingTest, StopBehaviourDefault) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSuspend(_))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kStop, _))
      .Times(0);

  MediaSessionImpl::Get(contents())->Stop(MediaSession::SuspendType::kUI);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, StopBehaviourWhenActionEnabled) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSuspend(_));
  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(MediaSessionAction::kStop, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(MediaSessionAction::kStop);

  MediaSessionImpl::Get(contents())->Stop(MediaSession::SuspendType::kUI);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       PositionFromServiceShouldOverridePlayer) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  media_session::MediaPosition player_position(
      0.0, base::TimeDelta::FromSeconds(20), base::TimeDelta());

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());

  players_[main_frame_].get()->SetPosition(player_position);
  GetMediaSession()->RebuildAndNotifyMediaPositionChanged();

  observer.WaitForExpectedPosition(player_position);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  media_session::MediaPosition expected_position(
      0.0, base::TimeDelta::FromSeconds(10), base::TimeDelta());

  services_[main_frame_]->SetPositionState(expected_position);

  observer.WaitForExpectedPosition(expected_position);

  DestroyServiceForFrame(main_frame_);

  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  observer.WaitForExpectedPosition(player_position);
}

TEST_F(MediaSessionImplServiceRoutingTest, PositionFromServiceCanBeReset) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  media_session::MediaPosition player_position(
      0.0, base::TimeDelta::FromSeconds(20), base::TimeDelta());

  media_session::test::MockMediaSessionMojoObserver observer(
      *GetMediaSession());

  players_[main_frame_].get()->SetPosition(player_position);
  GetMediaSession()->RebuildAndNotifyMediaPositionChanged();

  observer.WaitForExpectedPosition(player_position);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  media_session::MediaPosition expected_position(
      0.0, base::TimeDelta::FromSeconds(10), base::TimeDelta());

  services_[main_frame_]->SetPositionState(expected_position);

  observer.WaitForExpectedPosition(expected_position);

  services_[main_frame_]->SetPositionState(base::nullopt);

  EXPECT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());

  observer.WaitForExpectedPosition(player_position);
}

}  // namespace content
