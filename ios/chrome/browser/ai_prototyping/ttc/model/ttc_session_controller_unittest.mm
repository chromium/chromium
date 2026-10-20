// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <utility>

#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/test_utils.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Not;

#pragma mark - Fake Audio Controller

@interface FakeTTCSessionAudioController : NSObject <TTCAudioController>

@property(nonatomic, weak) id<TTCAudioControllerDelegate> delegate;
@property(nonatomic, assign, getter=isCapturing) BOOL capturing;
@property(nonatomic, assign, getter=isPlaying) BOOL playing;
@property(nonatomic, assign, getter=isLoopbackEnabled) BOOL loopbackEnabled;
@property(nonatomic, assign, getter=isOutputRoutedToSpeaker)
    BOOL outputRoutedToSpeaker;
@property(nonatomic, assign) TTCJournal* journal;
@property(nonatomic, assign) BOOL didStopCapture;
@property(nonatomic, assign) BOOL didStopPlayback;

@end

@implementation FakeTTCSessionAudioController

- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion {
  self.capturing = YES;
  if (completion) {
    completion(YES, nil);
  }
}

- (void)stopCapture {
  self.capturing = NO;
  self.didStopCapture = YES;
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  self.playing = YES;
}

- (void)clearPlaybackQueue {
}

- (void)stopPlayback {
  self.playing = NO;
  self.didStopPlayback = YES;
}

- (void)playTestTone {
}

- (void)stopTestTone {
}

- (void)disconnect {
}

@end

#pragma mark - Fake Session Observer

namespace {

// Fake observer implementing `TtcSessionControllerObserver` for unit testing.
class FakeTtcSessionControllerObserver : public TtcSessionControllerObserver {
 public:
  FakeTtcSessionControllerObserver() = default;
  ~FakeTtcSessionControllerObserver() override = default;

  void OnSessionLifecycleChanged(TtcSessionController* controller,
                                 TTCSessionLifecycle lifecycle) override {
    lifecycle_change_count_++;
    last_lifecycle_ = lifecycle;
  }

  void OnUserAudioLevelUpdated(TtcSessionController* controller,
                               float audio_level) override {
    audio_level_update_count_++;
    last_audio_level_ = audio_level;
  }

  void OnSessionError(TtcSessionController* controller,
                      ttc::ErrorCode error) override {
    error_count_++;
    last_error_ = error;
  }

  int lifecycle_change_count() const { return lifecycle_change_count_; }
  TTCSessionLifecycle last_lifecycle() const { return last_lifecycle_; }
  int audio_level_update_count() const { return audio_level_update_count_; }
  float last_audio_level() const { return last_audio_level_; }
  int error_count() const { return error_count_; }
  ttc::ErrorCode last_error() const { return last_error_; }

 private:
  int lifecycle_change_count_ = 0;
  TTCSessionLifecycle last_lifecycle_ = TTCSessionLifecycle::kInitializing;
  int audio_level_update_count_ = 0;
  float last_audio_level_ = 0.0f;
  int error_count_ = 0;
  ttc::ErrorCode last_error_ = ttc::ErrorCode::kUnknown;
};

}  // namespace

// Test fixture for `TtcSessionController`.
class TtcSessionControllerTest : public PlatformTest {
 protected:
  // Inserts and activates a realized fake `WebState` with `profile` and `url`
  // into `browser`, returning a non-owning pointer to the created tab.
  web::FakeWebState* InsertActiveEligibleTab(Browser& browser,
                                             ProfileIOS* profile,
                                             const GURL& url) {
    auto tab = std::make_unique<web::FakeWebState>();
    tab->SetBrowserState(profile);
    tab->SetIsRealized(true);
    tab->SetCurrentURL(url);
    tab->SetContentsMimeType("text/html");
    for (web::ContentWorld world : {web::ContentWorld::kIsolatedWorld,
                                    web::ContentWorld::kPageContentWorld}) {
      tab->SetWebFramesManager(world,
                               std::make_unique<web::FakeWebFramesManager>());
    }
    web::FakeWebState* tab_ptr = tab.get();
    browser.GetWebStateList()->InsertWebState(
        std::move(tab), WebStateList::InsertionParams::Automatic().Activate());
    return tab_ptr;
  }

  // Simulates completion of an asynchronous page context extraction for
  // `controller`'s active monitor using `url` and `title`.
  void SimulateMonitorExtraction(TtcSessionController& controller,
                                 const GURL& url,
                                 const std::string& title) {
    ASSERT_NE(controller.page_context_monitor_, nullptr);
    auto page_context =
        std::make_unique<optimization_guide::proto::PageContext>();
    page_context->set_url(url.spec());
    page_context->set_title(title);
    auto* apc = page_context->mutable_annotated_page_content();
    apc->mutable_main_frame_data()->set_url(url.spec());
    apc->mutable_main_frame_data()->set_title(title);
    controller.page_context_monitor_->OnPageContextExtracted(
        url, base::ok(std::move(page_context)));
  }

  web::WebTaskEnvironment task_environment_;
};

namespace {

// Tests that a controller initializes in `kInitializing` state.
TEST_F(TtcSessionControllerTest, TestInitialLifecycleIsInitializing) {
  TtcSessionController controller;
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kInitializing);
  controller.Disconnect();
}

// Tests that `OnConversationInitialized` transitions the lifecycle to `kLive`
// and notifies observers.
TEST_F(TtcSessionControllerTest, TestSessionInitializedTransitionsToLive) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  controller.OnConversationInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kLive);
  controller.Disconnect();
}

// Tests that `StopSession` transitions the lifecycle to `kFinished`.
TEST_F(TtcSessionControllerTest, TestStopSessionTransitionsToFinished) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnConversationInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  controller.StopSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycle_change_count(), 2);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
  controller.Disconnect();
}

// Tests that `Disconnect` transitions lifecycle to `kFinished`, notifies
// observers, and invalidates weak pointers.
TEST_F(TtcSessionControllerTest, TestObserverNotifiedOnDisconnect) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);
  base::WeakPtr<TtcSessionController> weak_controller = controller.GetWeakPtr();
  EXPECT_TRUE(weak_controller);

  controller.StartSession();
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  controller.Disconnect();
  EXPECT_FALSE(weak_controller);
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
}

// Tests that `UIApplicationDidEnterBackgroundNotification` stops the session,
// moves lifecycle to `kFinished`, and notifies observers.
TEST_F(TtcSessionControllerTest, TestBackgroundNotificationStopsSession) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnConversationInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);

  controller.Disconnect();
}

// Tests that `OnAudioEnergyUpdated` forwards the level to observers.
TEST_F(TtcSessionControllerTest, TestAudioLevelUpdateForwardedToObserver) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.OnAudioEnergyUpdated(0.65f);
  EXPECT_EQ(observer.audio_level_update_count(), 1);
  EXPECT_FLOAT_EQ(observer.last_audio_level(), 0.65f);
  controller.Disconnect();
}

// Tests that `OnConversationError` forwards the error to observers.
TEST_F(TtcSessionControllerTest, TestOnConversationErrorForwardedToObserver) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.OnConversationError(ttc::ErrorCode::kInternalBackendError);
  EXPECT_EQ(observer.error_count(), 1);
  EXPECT_EQ(observer.last_error(), ttc::ErrorCode::kInternalBackendError);
  controller.Disconnect();
}

// Tests that removing an observer stops notifications.
TEST_F(TtcSessionControllerTest, TestRemoveObserverStopsNotifications) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);
  controller.RemoveObserver(&observer);

  controller.OnConversationInitialized();
  controller.OnAudioEnergyUpdated(0.5f);
  EXPECT_EQ(observer.lifecycle_change_count(), 0);
  EXPECT_EQ(observer.audio_level_update_count(), 0);
  controller.Disconnect();
}

// Tests that `Disconnect` cleanly unregisters background notifications.
TEST_F(TtcSessionControllerTest,
       TestDisconnectCleanlyUnregistersBackgroundNotification) {
  TtcSessionController controller;
  controller.StartSession();
  controller.Disconnect();

  // Posting background notification after disconnect should not crash.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
}

// Tests that adding or removing a `nullptr` observer is a safe no-op and does
// not crash.
TEST_F(TtcSessionControllerTest, TestAddObserverNullDoesNotCrash) {
  TtcSessionController controller;
  controller.AddObserver(nullptr);
  controller.RemoveObserver(nullptr);
  controller.Disconnect();
}

// Tests that `OnConversationInitialized` does not resurrect a session that has
// already finished.
TEST_F(TtcSessionControllerTest,
       TestLateOnConversationInitializedDoesNotResurrectFinishedSession) {
  TtcSessionController controller;
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.StopSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);

  // Late initialization after session termination must not resurrect to
  // `kLive`.
  controller.OnConversationInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);

  controller.Disconnect();
}

// Tests that `TtcSessionController` initializes with an injected conversation
// and assigns itself as delegate.
TEST_F(TtcSessionControllerTest, TestCustomConversationInjection) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));

  EXPECT_EQ(controller.conversation(), conversation_ptr);
  EXPECT_EQ(conversation_ptr->delegate(), &controller);
  controller.Disconnect();
}

// Tests that constructing `TtcSessionController` with a `ttc::TtcBackend`
// creates a conversation backed by that backend and assigns itself as delegate.
TEST_F(TtcSessionControllerTest, TestInitWithBackend) {
  auto backend = std::make_unique<testing::NiceMock<ttc::MockTtcBackend>>();
  ttc::TtcBackend* backend_ptr = backend.get();
  TtcSessionController controller(std::move(backend));

  ASSERT_NE(controller.conversation(), nullptr);
  EXPECT_EQ(controller.conversation()->backend(), backend_ptr);
  EXPECT_EQ(controller.conversation()->delegate(), &controller);
  controller.Disconnect();
}

// Tests that `StartSession` starts the underlying conversation and capture.
TEST_F(TtcSessionControllerTest, TestStartSessionDrivesConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcSessionController controller(std::move(conversation));

  EXPECT_FALSE(fake_audio.isCapturing);
  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.Disconnect();
}

// Tests that `StopSession` stops the underlying conversation and capture.
TEST_F(TtcSessionControllerTest, TestStopSessionDrivesConversationStop) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcSessionController controller(std::move(conversation));

  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.StopSession();
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_TRUE(fake_audio.didStopPlayback);

  controller.Disconnect();
}

// Tests that conversation audio energy updates are forwarded to session
// observers.
TEST_F(TtcSessionControllerTest, TestConversationEnergyForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  ASSERT_NE(conversation_ptr->delegate(), nullptr);

  conversation_ptr->delegate()->OnAudioEnergyUpdated(0.75f);

  EXPECT_EQ(observer.audio_level_update_count(), 1);
  EXPECT_FLOAT_EQ(observer.last_audio_level(), 0.75f);

  controller.Disconnect();
}

// Tests that conversation errors are forwarded to session observers and stop
// the session when fatal.
TEST_F(TtcSessionControllerTest, TestConversationErrorForwardedToObserver) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  conversation_ptr->delegate()->OnConversationError(
      ttc::ErrorCode::kInternalBackendError);

  EXPECT_EQ(observer.error_count(), 1);
  EXPECT_EQ(observer.last_error(), ttc::ErrorCode::kInternalBackendError);
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_TRUE(fake_audio.didStopCapture);

  // Subsequent fatal errors must be ignored once a fatal error is reported.
  conversation_ptr->delegate()->OnConversationError(
      ttc::ErrorCode::kSessionExpired);
  EXPECT_EQ(observer.error_count(), 1);

  controller.Disconnect();
}

// Tests that `OnConversationClosed` stops the session and transitions lifecycle
// to `kFinished`.
TEST_F(TtcSessionControllerTest, TestConversationDidCloseStopsSession) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  controller.OnConversationInitialized();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);

  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  conversation_ptr->delegate()->OnConversationClosed();

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kFinished);
  EXPECT_TRUE(fake_audio.didStopCapture);

  controller.Disconnect();
}

// Tests that `TtcSessionController` `Disconnect` calls `Disconnect` on the
// underlying conversation.
TEST_F(TtcSessionControllerTest, TestDisconnectCleansUpConversation) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto conversation = std::make_unique<TtcConversation>(fake_audio, nullptr);
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));

  controller.StartSession();
  EXPECT_TRUE(fake_audio.isCapturing);

  controller.Disconnect();
  EXPECT_TRUE(fake_audio.didStopCapture);
  EXPECT_EQ(conversation_ptr->delegate(), nullptr);
  EXPECT_EQ(fake_audio.delegate, nil);
}

// Tests that `OnConversationInitialized` sends the default tool definitions to
// the backend, transitions the lifecycle to `kLive`, and notifies observers.
TEST_F(TtcSessionControllerTest,
       TestConversationDidInitializeTransitionsToLive) {
  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto backend = std::make_unique<testing::NiceMock<ttc::MockTtcBackend>>();
  testing::NiceMock<ttc::MockTtcBackend>* mock_backend = backend.get();
  auto conversation =
      std::make_unique<TtcConversation>(fake_audio, std::move(backend));
  TtcConversation* conversation_ptr = conversation.get();
  TtcSessionController controller(std::move(conversation));
  FakeTtcSessionControllerObserver observer;
  controller.AddObserver(&observer);

  controller.StartSession();
  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kInitializing);
  EXPECT_EQ(observer.lifecycle_change_count(), 0);

  ASSERT_NE(conversation_ptr->delegate(), nullptr);
  EXPECT_CALL(*mock_backend, SendToolSetUpdate(Not(IsEmpty()))).Times(1);
  conversation_ptr->delegate()->OnConversationInitialized();

  EXPECT_EQ(controller.lifecycle(), TTCSessionLifecycle::kLive);
  EXPECT_EQ(observer.lifecycle_change_count(), 1);
  EXPECT_EQ(observer.last_lifecycle(), TTCSessionLifecycle::kLive);

  controller.Disconnect();
}

// Tests that `TtcSessionController` tracks active `WebState` changes within the
// active `Browser` and forwards extracted `AnnotatedPageContent` updates to the
// backend.
TEST_F(TtcSessionControllerTest,
       TestActiveWebStateContextForwardedToBackendOnTabSwitch) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile.get());
  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.activationLevel = SceneActivationLevelForegroundActive;
  TestBrowser browser(profile.get(), scene_state);
  browser_list->AddBrowser(&browser);

  const GURL first_url("https://example.com/first");
  web::FakeWebState* first_tab_ptr =
      InsertActiveEligibleTab(browser, profile.get(), first_url);

  FakeTTCSessionAudioController* fake_audio =
      [[FakeTTCSessionAudioController alloc] init];
  auto mock_backend =
      std::make_unique<testing::NiceMock<ttc::MockTtcBackend>>();
  ttc::MockTtcBackend* mock_backend_ptr = mock_backend.get();
  auto conversation =
      std::make_unique<TtcConversation>(fake_audio, std::move(mock_backend));
  TtcSessionController controller(
      std::move(conversation),
      std::make_unique<TtcVoiceFocusedWebStateTracker>(browser_list));

  EXPECT_EQ(controller.GetVoiceFocusedWebState(), first_tab_ptr);
  controller.StartSession();
  EXPECT_EQ(controller.GetVoiceFocusedWebState(), first_tab_ptr);

  EXPECT_CALL(*mock_backend_ptr,
              SendContextUpdate(first_url, "First Tab", testing::_))
      .Times(1);
  SimulateMonitorExtraction(controller, first_url, "First Tab");

  // Inserts and activates a second tab; the monitor must switch to the new tab
  // and forward its `AnnotatedPageContent` to the backend.
  const GURL second_url("https://example.com/second");
  web::FakeWebState* second_tab_ptr =
      InsertActiveEligibleTab(browser, profile.get(), second_url);
  EXPECT_EQ(controller.GetVoiceFocusedWebState(), second_tab_ptr);

  EXPECT_CALL(*mock_backend_ptr,
              SendContextUpdate(second_url, "Second Tab", testing::_))
      .Times(1);
  SimulateMonitorExtraction(controller, second_url, "Second Tab");

  // Error responses and responses without `AnnotatedPageContent` must not
  // forward context updates to the backend.
  EXPECT_CALL(*mock_backend_ptr, SendContextUpdate).Times(0);
  controller.OnPageContextFetched(
      base::unexpected(PageContextWrapperError::kGenericError));
  auto empty_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  controller.OnPageContextFetched(base::ok(std::move(empty_context)));

  // Falls back to `PageContext` URL and title when `main_frame_data` fields are
  // empty.
  const GURL fallback_url("https://example.com/fallback");
  auto fallback_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  fallback_context->set_url(fallback_url.spec());
  fallback_context->set_title("Fallback Title");
  fallback_context->mutable_annotated_page_content();
  EXPECT_CALL(*mock_backend_ptr,
              SendContextUpdate(fallback_url, "Fallback Title", testing::_))
      .Times(1);
  controller.OnPageContextFetched(base::ok(std::move(fallback_context)));

  // Closing all tabs clears the controller's focused `WebState`.
  CloseAllWebStates(*browser.GetWebStateList(),
                    WebStateList::ClosingReason::kDefault);
  EXPECT_EQ(controller.GetVoiceFocusedWebState(), nullptr);

  controller.Disconnect();
  EXPECT_EQ(controller.GetVoiceFocusedWebState(), nullptr);
  browser_list->RemoveBrowser(&browser);
}

}  // namespace
