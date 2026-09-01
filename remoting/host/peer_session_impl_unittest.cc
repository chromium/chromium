// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/peer_session_impl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/base/errors.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/fake_terminal_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/peer_session.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/security_key/security_key_data_channel_handler.h"

#if BUILDFLAG(IS_POSIX)
#include "remoting/host/security_key/security_key_auth_handler_posix.h"
#endif

#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

using protocol::FakeSession;
using protocol::MockClientStub;
using protocol::MockHostStub;
using protocol::MockInputStub;
using protocol::MockVideoStub;
using protocol::test::EqualsClipboardEvent;
using protocol::test::EqualsKeyEvent;
using protocol::test::EqualsMouseButtonEvent;
using protocol::test::EqualsMouseMoveEvent;

using testing::_;
using testing::AtLeast;
using testing::Eq;
using testing::Not;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;

namespace {

constexpr char kTestClientJid[] = "host1@gmail.com/chromoting123";
constexpr char kTestDataChannelCallbackName[] = "test_channel_name";

// Use large fake screen-ids on 64-bit systems, to detect errors caused by
// inadvertent casts to 32-bits.
constexpr bool kUse64BitDisplayId = (sizeof(webrtc::ScreenId) >= 8);

// Matches a `protocol::Capabilities` argument against a list of capabilities
// formatted as a space-separated string.
MATCHER_P(IncludesCapabilities, expected_capabilities, "") {
  if (!arg.has_capabilities()) {
    return false;
  }

  std::vector<std::string> words_args =
      base::SplitString(arg.capabilities(), " ", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> words_expected =
      base::SplitString(expected_capabilities, " ", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  for (const auto& word : words_expected) {
    if (!std::ranges::contains(words_args, word)) {
      return false;
    }
  }
  return true;
}

MATCHER_P(ScreenIdMatches, expected_id, "") {
  return arg.screen_id() == expected_id;
}

protocol::KeyEvent MakeKeyEvent(bool pressed, std::uint32_t keycode) {
  protocol::KeyEvent result;
  result.set_pressed(pressed);
  result.set_usb_keycode(keycode);
  return result;
}

protocol::ClipboardEvent MakeClipboardEvent(const std::string& text) {
  protocol::ClipboardEvent result;
  result.set_mime_type(kMimeTypeTextUtf8);
  result.set_data(text);
  return result;
}

protocol::MouseEvent MakeFractionalMouseMoveEvent(int x,
                                                  int y,
                                                  int64_t screen_id,
                                                  int width,
                                                  int height) {
  protocol::MouseEvent result;
  auto* fractional = result.mutable_fractional_coordinate();
  fractional->set_screen_id(screen_id);
  fractional->set_x(static_cast<float>(x) / width);
  fractional->set_y(static_cast<float>(y) / height);
  return result;
}

class MockPeerSessionEventHandler : public PeerSession::EventHandler {
 public:
  MockPeerSessionEventHandler() = default;
  ~MockPeerSessionEventHandler() override = default;

  MOCK_METHOD(void, OnSessionChannelsConnected, (), (override));
  MOCK_METHOD(void,
              OnSessionClosed,
              (protocol::ErrorCode error,
               const std::string& error_details,
               const SourceLocation& error_location),
              (override));
  MOCK_METHOD(void,
              OnSessionRouteChange,
              (const std::string& channel_name,
               const protocol::TransportRoute& route),
              (override));
};

}  // namespace

class PeerSessionImplTest : public testing::Test {
 public:
  PeerSessionImplTest() = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Fake multi-monitor setup.
  static const int kDisplay1Width =
      protocol::FakeDesktopCapturer::kWidth;  // 800
  static const int kDisplay1Height =
      protocol::FakeDesktopCapturer::kHeight;  // 600
  static const std::int64_t kDisplay1Id =
      kUse64BitDisplayId ? 1111111111111111 : 11111111;
  static const int kDisplay2Width = 1024;
  static const int kDisplay2Height = 768;
  static const int kDisplay2YOffset = 35;
  static const std::int64_t kDisplay2Id =
      kUse64BitDisplayId ? 2222222222222222 : 22222222;

  // Creates the peer session from a FakeSession instance.
  void CreatePeerSession(std::unique_ptr<protocol::FakeSession> session);

  // Creates the peer session.
  void CreatePeerSession();

  // Starts the peer session.
  void StartPeerSession(const SessionPolicies& session_policies = {},
                        const SessionOptions& session_options = {});

  // Notifies the peer session that channels have been connected. This
  // effectively enables the input pipeline and starts video capturing.
  void ConnectPeerSession(const SessionPolicies& session_policies = {},
                          const SessionOptions& session_options = {});

  // Add a fake display to the layout list. Used in conjunction with
  // NotifyDesktopDisplaySize.
  void AddDisplayToLayout(protocol::VideoLayout* displays,
                          int x,
                          int y,
                          int width,
                          int height,
                          int dpi_x,
                          int dpi_y,
                          std::int64_t display_id);

  // Fakes desktop display size notification from Webrtc.
  void NotifyDesktopDisplaySize(
      std::unique_ptr<protocol::VideoLayout> displays);

  // Convenience methods to setup the display configuration.
  void ResetDisplayInfo();

  // Set up a single display (default size).
  void SetupSingleDisplay();

  // Geometry info for displays being tested.
  DesktopDisplayInfo displays_;
  int curr_display_;

  // Task environment that will process all PeerSession tasks.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // AutoThreadTaskRunner on which `peer_session_` will be run.
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Used to run `task_environment_` after each test, until no objects remain
  // that require it.
  base::RunLoop run_loop_;

  // Vectors of events to bind to `peer_session_`, must outlive it.
  std::vector<protocol::KeyEvent> key_events_;
  std::vector<protocol::MouseEvent> mouse_events_;
  std::vector<protocol::ClipboardEvent> clipboard_events_;

  // PeerSession instance under test.
  std::unique_ptr<PeerSessionImpl> peer_session_;

  // PeerSession::EventHandler mock for use in tests.
  MockPeerSessionEventHandler session_event_handler_;

  // Stubs returned to `peer_session_` components by `connection_`.
  MockClientStub client_stub_;

  // PeerSessionImpl owns `connection_` but tests need it to inject fake events.
  base::WeakPtr<protocol::FakeConnectionToClient> connection_;

  std::unique_ptr<FakeDesktopEnvironmentFactory> desktop_environment_factory_;
  DesktopEnvironmentOptions desktop_environment_options_;

  bool is_connected() const {
    return connection_ && connection_->is_connected();
  }

  void OnRequestPairing(
      const std::string& client_name,
      PeerSessionImpl::RequestPairingResponseCallback response_cb) {
    requested_client_name_ = client_name;
    pairing_response_cb_ = std::move(response_cb);
  }

  std::string requested_client_name_;
  PeerSessionImpl::RequestPairingResponseCallback pairing_response_cb_;
};

void PeerSessionImplTest::SetUp() {
  // Arrange to run `task_environment_` until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());

  desktop_environment_factory_ =
      std::make_unique<FakeDesktopEnvironmentFactory>(
          task_environment_.GetMainThreadTaskRunner());
  desktop_environment_options_ = DesktopEnvironmentOptions::CreateDefault();

  // Suppress spammy "uninteresting call" logs.
  EXPECT_CALL(client_stub_, SetCursorShape(_)).Times(testing::AnyNumber());
}

void PeerSessionImplTest::TearDown() {
  if (peer_session_) {
    if (is_connected()) {
      peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
    }
    peer_session_.reset();
    desktop_environment_factory_.reset();
  }

  // Clear out `task_runner_` reference so the loop can quit, and run it until
  // it does.
  task_runner_ = nullptr;
  run_loop_.Run();
}

void PeerSessionImplTest::CreatePeerSession() {
  std::unique_ptr<protocol::FakeConnectionToClient> connection(
      new protocol::FakeConnectionToClient());
  connection->set_client_stub(&client_stub_);
  connection_ = connection->GetWeakPtrForTest();

  peer_session_ = std::make_unique<PeerSessionImpl>(
      std::move(connection), desktop_environment_factory_.get(),
      base::BindRepeating(&PeerSessionImplTest::OnRequestPairing,
                          base::Unretained(this)));
}

void PeerSessionImplTest::StartPeerSession(
    const SessionPolicies& session_policies,
    const SessionOptions& session_options) {
  if (!peer_session_) {
    CreatePeerSession();
  }
  peer_session_->Start(&session_event_handler_, kTestClientJid,
                       desktop_environment_options_, session_policies,
                       session_options);
}

void PeerSessionImplTest::ConnectPeerSession(
    const SessionPolicies& session_policies,
    const SessionOptions& session_options) {
  if (!peer_session_) {
    CreatePeerSession();
  }

  // Stubs should not be set before connection is authenticated.
  EXPECT_FALSE(connection_->clipboard_stub());
  EXPECT_FALSE(connection_->input_stub());

  base::test::TestFuture<void> future;
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected())
      .WillOnce([&future] { future.SetValue(); });

  StartPeerSession(session_policies, session_options);
  peer_session_->CreateMediaStreams();
  peer_session_->OnConnectionChannelsConnected();
  future.Get();

  // Stubs should be set only after connection is authenticated.
  EXPECT_TRUE(connection_->clipboard_stub());
  EXPECT_TRUE(connection_->input_stub());
}

void PeerSessionImplTest::AddDisplayToLayout(protocol::VideoLayout* displays,
                                             int x,
                                             int y,
                                             int width,
                                             int height,
                                             int dpi_x,
                                             int dpi_y,
                                             std::int64_t display_id) {
  protocol::VideoTrackLayout* video_track = displays->add_video_track();
  video_track->set_position_x(x);
  video_track->set_position_y(y);
  video_track->set_width(width);
  video_track->set_height(height);
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);
  video_track->set_screen_id(display_id);
  displays_.AddDisplayFrom(*video_track);
}

void PeerSessionImplTest::NotifyDesktopDisplaySize(
    std::unique_ptr<protocol::VideoLayout> displays) {
  peer_session_->OnDesktopDisplayChanged(std::move(displays));
}

void PeerSessionImplTest::ResetDisplayInfo() {
  displays_.Reset();
  curr_display_ = webrtc::kInvalidScreenId;
}

// Set up a single display (default size).
void PeerSessionImplTest::SetupSingleDisplay() {
  ResetDisplayInfo();
  auto displays = std::make_unique<protocol::VideoLayout>();
  AddDisplayToLayout(displays.get(), 0, 0, kDisplay1Width, kDisplay1Height,
                     kDefaultDpi, kDefaultDpi, kDisplay1Id);
  NotifyDesktopDisplaySize(std::move(displays));
}

// TODO(lambroslambrou): Re-implement the deleted MultiMonMouseMove
// and MultiMonMouseMove_SameSize tests in a way that makes sense for
// multi-stream mode.

TEST_F(PeerSessionImplTest, DisableInputs) {
  ConnectPeerSession();
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  input_injector->set_key_events(&key_events_);
  input_injector->set_mouse_events(&mouse_events_);
  input_injector->set_clipboard_events(&clipboard_events_);

  // Inject test events that are expected to be injected.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("a"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      100, 101, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  // Disable input.
  peer_session_->SetDisableInputs(true);

  // These events shouldn't get though to the input injector.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("b"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(false, 2));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      200, 201, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  // Enable input again.
  peer_session_->SetDisableInputs(false);
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("c"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 3));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      300, 301, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();

  EXPECT_EQ(mouse_events_.size(), 2U);
  EXPECT_THAT(mouse_events_[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events_[1], EqualsMouseMoveEvent(300, 301));

  EXPECT_EQ(key_events_.size(), 4U);
  EXPECT_THAT(key_events_[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events_[1], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events_[2], EqualsKeyEvent(3, true));
  EXPECT_THAT(key_events_[3], EqualsKeyEvent(3, false));

  EXPECT_EQ(clipboard_events_.size(), 2U);
  EXPECT_THAT(clipboard_events_[0],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "a"));
  EXPECT_THAT(clipboard_events_[1],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "c"));
}

TEST_F(PeerSessionImplTest, InputAllowedFromRemotePolicy) {
  SessionPolicies remote_policies;
  remote_policies.allow_remote_input = true;

  ConnectPeerSession(remote_policies);
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  input_injector->set_key_events(&key_events_);
  input_injector->set_mouse_events(&mouse_events_);
  input_injector->set_clipboard_events(&clipboard_events_);

  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("a"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      100, 101, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();

  EXPECT_EQ(mouse_events_.size(), 1U);
  EXPECT_THAT(mouse_events_[0], EqualsMouseMoveEvent(100, 101));

  EXPECT_EQ(key_events_.size(), 2U);
  EXPECT_THAT(key_events_[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events_[1], EqualsKeyEvent(1, false));

  EXPECT_EQ(clipboard_events_.size(), 1U);
  EXPECT_THAT(clipboard_events_[0],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "a"));
}

TEST_F(PeerSessionImplTest, InputDisabledFromRemotePolicy) {
  SessionPolicies remote_policies;
  remote_policies.allow_remote_input = false;

  ConnectPeerSession(remote_policies);
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  input_injector->set_key_events(&key_events_);
  input_injector->set_mouse_events(&mouse_events_);
  input_injector->set_clipboard_events(&clipboard_events_);

  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("a"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      100, 101, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();

  EXPECT_EQ(mouse_events_.size(), 0U);
  EXPECT_EQ(key_events_.size(), 0U);
  EXPECT_EQ(clipboard_events_.size(), 0U);
}

TEST_F(PeerSessionImplTest, LocalInputTest) {
  ConnectPeerSession();
  SetupSingleDisplay();

  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events_);

  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      100, 101, kDisplay1Id, kDisplay1Width, kDisplay1Height));

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  // The OS echoes the injected event back.
  peer_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                     ui::EventType::kMouseMoved);
#endif  // !BUILDFLAG(IS_WIN)

  // This one should get through as well.
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      200, 201, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  // Now this is a genuine local event.
  peer_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                     ui::EventType::kMouseMoved);

  // This one should be blocked because of the previous local input event.
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      300, 301, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  // Verify that we've received correct set of mouse events.
  ASSERT_EQ(mouse_events_.size(), 2U);
  EXPECT_THAT(mouse_events_[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events_[1], EqualsMouseMoveEvent(200, 201));

  // Verify that we're still connected.
  EXPECT_TRUE(connection_->is_connected());

  // TODO(jamiewalch): Verify that remote inputs are re-enabled
  // eventually (via dependency injection, not sleep!)
}

TEST_F(PeerSessionImplTest, DisconnectOnLocalInputTest) {
  desktop_environment_options_.set_terminate_upon_input(true);
  ConnectPeerSession();
  SetupSingleDisplay();

  peer_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                     ui::EventType::kMouseMoved);
  EXPECT_FALSE(connection_->is_connected());
}

TEST_F(PeerSessionImplTest, RestoreEventState) {
  ConnectPeerSession();
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  input_injector->set_key_events(&key_events_);
  input_injector->set_mouse_events(&mouse_events_);

  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);
  connection_->input_stub()->InjectMouseEvent(mousedown);

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  EXPECT_EQ(mouse_events_.size(), 2U);
  EXPECT_THAT(mouse_events_[0],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, true));
  EXPECT_THAT(mouse_events_[1],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, false));

  EXPECT_EQ(key_events_.size(), 4U);
  EXPECT_THAT(key_events_[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events_[1], EqualsKeyEvent(2, true));
  EXPECT_THAT(key_events_[2], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events_[3], EqualsKeyEvent(2, false));
}

TEST_F(PeerSessionImplTest, ClampMouseEvents) {
  ConnectPeerSession();
  SetupSingleDisplay();

  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events_);

  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      0, 0, kDisplay1Id, kDisplay1Width, kDisplay1Height));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      -10, -10, kDisplay1Id, kDisplay1Width, kDisplay1Height));

  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      kDisplay1Width - 1, kDisplay1Height - 1, kDisplay1Id, kDisplay1Width,
      kDisplay1Height));
  connection_->input_stub()->InjectMouseEvent(MakeFractionalMouseMoveEvent(
      kDisplay1Width + 10, kDisplay1Height + 10, kDisplay1Id, kDisplay1Width,
      kDisplay1Height));

  ASSERT_EQ(mouse_events_.size(), 4U);
  EXPECT_THAT(mouse_events_[0], EqualsMouseMoveEvent(0, 0));
  EXPECT_THAT(mouse_events_[1], EqualsMouseMoveEvent(0, 0));
  EXPECT_THAT(mouse_events_[2],
              EqualsMouseMoveEvent(kDisplay1Width - 1, kDisplay1Height - 1));
  EXPECT_THAT(mouse_events_[3],
              EqualsMouseMoveEvent(kDisplay1Width - 1, kDisplay1Height - 1));
}



TEST_F(PeerSessionImplTest, DataChannelCallbackIsCalled) {
  ConnectPeerSession();

  bool callback_called = false;
  peer_session_->RegisterCreateHandlerCallbackForTesting(
      kTestDataChannelCallbackName,
      base::BindRepeating([](bool* callback_was_called, const std::string& name,
                             std::unique_ptr<protocol::MessagePipe> pipe)
                              -> void { *callback_was_called = true; },
                          &callback_called));

  std::unique_ptr<protocol::MessagePipe> pipe =
      base::WrapUnique(new protocol::FakeMessagePipe(false));

  peer_session_->OnIncomingDataChannel(kTestDataChannelCallbackName,
                                       std::move(pipe));

  ASSERT_TRUE(callback_called);
}

TEST_F(PeerSessionImplTest, OnSessionServicesClientConnected) {
  ConnectPeerSession();

  mojo::Remote<mojom::ChromotingSessionServices> remote;
  peer_session_->OnSessionServicesClientConnected(
      remote.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(remote.is_connected());
}

TEST_F(PeerSessionImplTest, ActiveDisplayMessageSent) {
  EXPECT_CALL(client_stub_, SetActiveDisplay(ScreenIdMatches(kDisplay1Id)));

  // The ActiveDisplayMonitor only gets created after negotiating this
  // capability with the client.
  desktop_environment_factory_->set_capabilities(
      protocol::kMultiStreamCapability);
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities client_capabilities;
  client_capabilities.set_capabilities(protocol::kMultiStreamCapability);
  peer_session_->SetCapabilities(client_capabilities);

  auto monitor = desktop_environment_factory_->last_desktop_environment()
                     ->last_active_display_monitor();
  ASSERT_TRUE(monitor);
  monitor->SetActiveDisplay(static_cast<webrtc::ScreenId>(kDisplay1Id));
}

TEST_F(PeerSessionImplTest, NotifyClientResolution_Bad) {
  CreatePeerSession();
  ConnectPeerSession();
  SetupSingleDisplay();

  FakeScreenControls* screen_controls =
      desktop_environment_factory_->last_desktop_environment()
          ->last_screen_controls()
          .get();
  ASSERT_TRUE(screen_controls);

  // Send invalid resolution with negative width.
  protocol::ClientResolution invalid_resolution;
  invalid_resolution.set_width_pixels(-800);
  invalid_resolution.set_height_pixels(600);
  invalid_resolution.set_x_dpi(96);
  invalid_resolution.set_y_dpi(96);
  peer_session_->NotifyClientResolution(invalid_resolution);
  EXPECT_FALSE(screen_controls->set_resolution_called());

  // Reset state on mock controls.
  screen_controls->reset();

  // Send invalid resolution with negative height.
  invalid_resolution.set_width_pixels(800);
  invalid_resolution.set_height_pixels(-600);
  peer_session_->NotifyClientResolution(invalid_resolution);
  EXPECT_FALSE(screen_controls->set_resolution_called());
}

TEST_F(PeerSessionImplTest, SetVideoLayout_Bad) {
  CreatePeerSession();
  ConnectPeerSession();
  SetupSingleDisplay();

  FakeScreenControls* screen_controls =
      desktop_environment_factory_->last_desktop_environment()
          ->last_screen_controls()
          .get();
  ASSERT_TRUE(screen_controls);

  // Send layout with negative track width.
  protocol::VideoLayout invalid_layout_width;
  protocol::VideoTrackLayout* invalid_track_width =
      invalid_layout_width.add_video_track();
  invalid_track_width->set_width(-800);
  invalid_track_width->set_height(600);
  invalid_track_width->set_x_dpi(96);
  invalid_track_width->set_y_dpi(96);
  invalid_track_width->set_screen_id(kDisplay1Id);
  peer_session_->SetVideoLayout(invalid_layout_width);
  EXPECT_FALSE(screen_controls->set_video_layout_called());

  // Reset state.
  screen_controls->reset();

  // Send layout with negative track height.
  protocol::VideoLayout invalid_layout_height;
  protocol::VideoTrackLayout* invalid_track_height =
      invalid_layout_height.add_video_track();
  invalid_track_height->set_width(800);
  invalid_track_height->set_height(-600);
  invalid_track_height->set_x_dpi(96);
  invalid_track_height->set_y_dpi(96);
  invalid_track_height->set_screen_id(kDisplay1Id);
  peer_session_->SetVideoLayout(invalid_layout_height);
  EXPECT_FALSE(screen_controls->set_video_layout_called());
}

TEST_F(PeerSessionImplTest, ControlTerminal_CreateTerminal) {
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(protocol::kTerminalModeCapability);
  peer_session_->SetCapabilities(capabilities);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();

  // Expect client_stub to receive the create response.
  protocol::TerminalControl create_response;
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_))
      .WillOnce([&create_response](const protocol::TerminalControl& control) {
        create_response = control;
      });

  protocol::TerminalControl create_req;
  create_req.mutable_create_request();
  peer_session_->ControlTerminal(create_req);

  // We should have a valid terminal ID returned.
  ASSERT_TRUE(create_response.has_create_response());
  EXPECT_EQ(create_response.create_response().terminal_id(), 1);

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();
}

TEST_F(PeerSessionImplTest, ControlTerminal_InputAndResize) {
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(protocol::kTerminalModeCapability);
  peer_session_->SetCapabilities(capabilities);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();

  // Create a terminal
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_)).Times(1);
  protocol::TerminalControl create_req;
  create_req.mutable_create_request();
  peer_session_->ControlTerminal(create_req);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_NE(sessions[0], nullptr);
  EXPECT_EQ(sessions[0]->id(), 1);

  // Send input
  protocol::TerminalControl input_req;
  auto* input = input_req.mutable_terminal_input();
  input->set_terminal_id(1);
  input->set_input("hello");
  peer_session_->ControlTerminal(input_req);

  EXPECT_EQ(sessions[0]->inputs().size(), 1u);
  EXPECT_EQ(sessions[0]->inputs()[0], "hello");

  // Send resize
  protocol::TerminalControl resize_req;
  auto* resize = resize_req.mutable_resize_terminal();
  resize->set_terminal_id(1);
  resize->set_width(80);
  resize->set_height(24);
  peer_session_->ControlTerminal(resize_req);

  EXPECT_EQ(sessions[0]->resizes().size(), 1u);
  EXPECT_EQ(sessions[0]->resizes()[0].first, 80u);
  EXPECT_EQ(sessions[0]->resizes()[0].second, 24u);

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();
}

TEST_F(PeerSessionImplTest, ControlTerminal_OutputAndExit) {
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(protocol::kTerminalModeCapability);
  peer_session_->SetCapabilities(capabilities);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();

  // Create a terminal
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_)).Times(1);
  protocol::TerminalControl create_req;
  create_req.mutable_create_request();
  peer_session_->ControlTerminal(create_req);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);

  // Trigger output from the terminal
  protocol::TerminalControl output_received;
  base::test::TestFuture<void> output_future;
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_))
      .WillOnce([&output_received,
                 &output_future](const protocol::TerminalControl& control) {
        output_received = control;
        output_future.SetValue();
      });

  sessions[0]->TriggerOutput("world");
  output_future.Get();

  ASSERT_TRUE(output_received.has_terminal_output());
  EXPECT_EQ(output_received.terminal_output().terminal_id(), 1);
  EXPECT_EQ(output_received.terminal_output().output(), "world");

  // Trigger terminal exit
  protocol::TerminalControl close_received;
  base::test::TestFuture<void> exit_future;
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_))
      .WillOnce([&close_received,
                 &exit_future](const protocol::TerminalControl& control) {
        close_received = control;
        exit_future.SetValue();
      });

  sessions[0]->TriggerExit();
  exit_future.Get();

  ASSERT_TRUE(close_received.has_close_terminal());
  EXPECT_EQ(close_received.close_terminal().terminal_id(), 1);

  // Task to delete the terminal in TerminalSessionManager runs asynchronously.
  EXPECT_TRUE(base::test::RunUntil([this] {
    return peer_session_->terminal_session_manager_for_tests()
        ->GetTerminalSessionIds()
        .empty();
  }));
}

TEST_F(PeerSessionImplTest, ControlTerminal_ProcessInfo) {
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(protocol::kTerminalModeCapability);
  peer_session_->SetCapabilities(capabilities);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::test::TestFuture<void> future;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        future.GetCallback());
  future.Get();

  // Create a terminal
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_)).Times(1);
  protocol::TerminalControl create_req;
  create_req.mutable_create_request();
  peer_session_->ControlTerminal(create_req);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);

  // Trigger ProcessInfo from the terminal
  base::test::TestFuture<protocol::TerminalControl> info_future;
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_))
      .WillOnce([&info_future](const protocol::TerminalControl& control) {
        info_future.SetValue(control);
      });

  sessions[0]->TriggerProcessInfo(true);
  protocol::TerminalControl info_received = info_future.Get();

  ASSERT_TRUE(info_received.has_process_info());
  EXPECT_EQ(info_received.process_info().terminal_id(), 1);
  EXPECT_TRUE(info_received.process_info().is_active());
  EXPECT_EQ(info_received.process_info().process_name(), "test-process");

  base::test::TestFuture<protocol::TerminalControl> info_future2;
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_))
      .WillOnce([&info_future2](const protocol::TerminalControl& control) {
        info_future2.SetValue(control);
      });

  sessions[0]->TriggerProcessInfo(true, "/var/log");
  info_received = info_future2.Get();

  ASSERT_TRUE(info_received.has_process_info());
  EXPECT_EQ(info_received.process_info().terminal_id(), 1);
  EXPECT_TRUE(info_received.process_info().is_active());
  EXPECT_EQ(info_received.process_info().process_name(), "/var/log");
}

TEST_F(PeerSessionImplTest, ControlTerminal_RemoveRequest) {
  CreatePeerSession();
  ConnectPeerSession();

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(protocol::kTerminalModeCapability);
  peer_session_->SetCapabilities(capabilities);
  base::ThreadPoolInstance::Get()->FlushForTesting();
  task_environment_.RunUntilIdle();

  // Create two terminals
  EXPECT_CALL(client_stub_, DeliverTerminalControl(_)).Times(2);
  protocol::TerminalControl create_req;
  create_req.mutable_create_request();
  peer_session_->ControlTerminal(create_req);
  peer_session_->ControlTerminal(create_req);

  auto sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 2u);

  // Send remove_request for terminal 1
  protocol::TerminalControl remove_req;
  remove_req.mutable_remove_request()->set_terminal_id(1);
  peer_session_->ControlTerminal(remove_req);

  sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0]->id(), 2);

  // Send remove_request for terminal 2
  remove_req.mutable_remove_request()->set_terminal_id(2);
  peer_session_->ControlTerminal(remove_req);

  sessions = FakeTerminalSession::GetActiveSessions();
  ASSERT_EQ(sessions.size(), 0u);

  peer_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
  peer_session_.reset();
}

TEST_F(PeerSessionImplTest, RequestPairing) {
  ConnectPeerSession();

  protocol::PairingRequest request;
  request.set_client_name("test_client");
  peer_session_->RequestPairing(request);

  EXPECT_EQ(requested_client_name_, "test_client");
  ASSERT_TRUE(pairing_response_cb_);

  protocol::PairingResponse response;
  response.set_client_id("client_id_123");
  response.set_shared_secret("secret_456");

  base::test::TestFuture<protocol::PairingResponse> future;
  EXPECT_CALL(client_stub_, SetPairingResponse(_))
      .WillOnce([&future](const protocol::PairingResponse& r) {
        future.SetValue(r);
      });
  std::move(pairing_response_cb_).Run(response);

  protocol::PairingResponse received_response = future.Take();
  EXPECT_EQ(received_response.client_id(), "client_id_123");
  EXPECT_EQ(received_response.shared_secret(), "secret_456");
}

TEST_F(PeerSessionImplTest, RequestPairing_InvalidOrNullCallback) {
  ConnectPeerSession();

  EXPECT_CALL(client_stub_, SetPairingResponse(_)).Times(0);

  protocol::PairingRequest empty_name_request;
  peer_session_->RequestPairing(empty_name_request);
  EXPECT_TRUE(requested_client_name_.empty());

  protocol::PairingRequest long_name_request;
  long_name_request.set_client_name(
      std::string(PeerSessionImpl::kMaxClientNameLength + 1, 'a'));
  peer_session_->RequestPairing(long_name_request);
  EXPECT_TRUE(requested_client_name_.empty());

  peer_session_->SetRequestPairingCallbackForTesting(base::NullCallback());
  protocol::PairingRequest valid_request;
  valid_request.set_client_name("test_client");
  peer_session_->RequestPairing(valid_request);
  EXPECT_TRUE(requested_client_name_.empty());
}

TEST_F(PeerSessionImplTest, RequestPairing_EmptyResponseIgnored) {
  ConnectPeerSession();

  protocol::PairingRequest request;
  request.set_client_name("test_client");
  peer_session_->RequestPairing(request);
  ASSERT_TRUE(pairing_response_cb_);

  EXPECT_CALL(client_stub_, SetPairingResponse(_)).Times(0);
  std::move(pairing_response_cb_).Run(std::nullopt);

  base::test::TestFuture<void> after_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, after_future.GetCallback());
  EXPECT_TRUE(after_future.Wait());
}

TEST_F(PeerSessionImplTest, RequestPairing_BufferedBeforeChannelsConnected) {
  CreatePeerSession();
  StartPeerSession();
  EXPECT_FALSE(peer_session_->channels_connected());

  protocol::PairingRequest request;
  request.set_client_name("test_client");
  peer_session_->RequestPairing(request);
  ASSERT_TRUE(pairing_response_cb_);

  protocol::PairingResponse response;
  response.set_client_id("client_id_123");
  response.set_shared_secret("secret_456");

  EXPECT_CALL(client_stub_, SetPairingResponse(_)).Times(0);
  std::move(pairing_response_cb_).Run(response);

  base::test::TestFuture<void> after_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, after_future.GetCallback());
  EXPECT_TRUE(after_future.Wait());

  EXPECT_CALL(client_stub_, SetPairingResponse(_))
      .WillOnce([](const protocol::PairingResponse& r) {
        EXPECT_EQ(r.client_id(), "client_id_123");
        EXPECT_EQ(r.shared_secret(), "secret_456");
      });
  base::test::TestFuture<void> future;
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected())
      .WillOnce([&future] { future.SetValue(); });
  peer_session_->CreateMediaStreams();
  peer_session_->OnConnectionChannelsConnected();
  future.Get();
}

TEST_F(PeerSessionImplTest, RequestPairing_SubsequentRequestsIgnored) {
  ConnectPeerSession();

  protocol::PairingRequest request1;
  request1.set_client_name("client1");
  peer_session_->RequestPairing(request1);
  EXPECT_EQ(requested_client_name_, "client1");
  ASSERT_TRUE(pairing_response_cb_);

  requested_client_name_.clear();
  protocol::PairingRequest request2;
  request2.set_client_name("client2");
  peer_session_->RequestPairing(request2);
  EXPECT_TRUE(requested_client_name_.empty());

  EXPECT_CALL(client_stub_, SetPairingResponse(_));
  protocol::PairingResponse response;
  response.set_client_id("id1");
  response.set_shared_secret("secret1");
  std::move(pairing_response_cb_).Run(response);

  base::test::TestFuture<void> after_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, after_future.GetCallback());
  EXPECT_TRUE(after_future.Wait());

  protocol::PairingRequest request3;
  request3.set_client_name("client3");
  peer_session_->RequestPairing(request3);
  EXPECT_TRUE(requested_client_name_.empty());
}

TEST_F(PeerSessionImplTest, RequestPairing_EmptyClientIdOrSecretIgnored) {
  ConnectPeerSession();

  protocol::PairingRequest request;
  request.set_client_name("test_client");
  peer_session_->RequestPairing(request);
  ASSERT_TRUE(pairing_response_cb_);

  EXPECT_CALL(client_stub_, SetPairingResponse(_)).Times(0);
  protocol::PairingResponse bad_response;
  bad_response.set_client_id("id_only");
  std::move(pairing_response_cb_).Run(bad_response);

  base::test::TestFuture<void> after_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, after_future.GetCallback());
  EXPECT_TRUE(after_future.Wait());
}

class PeerSessionSecurityKeyTest : public PeerSessionImplTest {
 public:
  PeerSessionSecurityKeyTest() {
    // Bind the factory override to return the mock handler.
    SecurityKeyAuthHandler::SetCreateHandlerCallbackForTesting(
        base::BindRepeating(&PeerSessionSecurityKeyTest::CreateMockHandler,
                            base::Unretained(this)));
  }

  ~PeerSessionSecurityKeyTest() override {
    SecurityKeyAuthHandler::SetCreateHandlerCallbackForTesting(
        base::NullCallback());
  }

  void SetUp() override {
    PeerSessionImplTest::SetUp();
    desktop_environment_options_.set_enable_security_key(true);
  }

  void TearDown() override {
    mock_handler_ = nullptr;
    PeerSessionImplTest::TearDown();
  }

 protected:
  std::unique_ptr<SecurityKeyAuthHandler> CreateMockHandler() {
    auto mock =
        std::make_unique<testing::NiceMock<MockSecurityKeyAuthHandler>>();
    mock_handler_ = mock.get();
    return mock;
  }

  raw_ptr<testing::NiceMock<MockSecurityKeyAuthHandler>> mock_handler_ =
      nullptr;
};

// Verifies that the security key capability is advertised.
TEST_F(PeerSessionSecurityKeyTest, AdvertisesCapabilities) {
  // Expect that the client stub gets the V2 capability advertised.
  EXPECT_CALL(client_stub_, SetCapabilities(IncludesCapabilities(
                                protocol::kSecurityKeyV2Capability)));

  CreatePeerSession();
  ConnectPeerSession();
}

// Verifies that when the WebRTC data channel connects, the handler binds
// to the auth handler.
TEST_F(PeerSessionSecurityKeyTest, DataChannelConnects) {
  CreatePeerSession();
  ConnectPeerSession();

  // Negotiate capabilities.
  protocol::Capabilities capabilities_message;
  capabilities_message.set_capabilities(protocol::kSecurityKeyV2Capability);
  peer_session_->SetCapabilities(capabilities_message);

  EXPECT_CALL(*mock_handler_, CreateSecurityKeyConnection()).Times(1);

  auto pipe = std::make_unique<protocol::FakeMessagePipe>(true);
  peer_session_->OnIncomingDataChannel(
      SecurityKeyDataChannelHandler::kChannelName, pipe->Wrap());

  // Open the pipe to trigger OnConnected().
  pipe->OpenPipe();

  // Close the pipe to clean up the handler and avoid dangling pointers.
  pipe->ClosePipe();
  ASSERT_TRUE(base::test::RunUntil([&]() { return !pipe->HasWrappers(); }));
}

}  // namespace remoting
