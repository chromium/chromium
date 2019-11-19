// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/fake_host_extension.h"
#include "remoting/host/fake_mouse_cursor_monitor.h"
#include "remoting/host/host_extension.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

// TODO(crbug.com/961064): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_LocalInputTest DISABLED_LocalInputTest
#define MAYBE_ClampMouseEvents DISABLED_ClampMouseEvents
#define MAYBE_DisableInputs DISABLED_DisableInputs
#define MAYBE_MultiMonMouseMove_SameSize DISABLED_MultiMonMouseMove_SameSize
#define MAYBE_RestoreEventState DISABLED_RestoreEventState
#define MAYBE_MultiMonMouseMove DISABLED_MultiMonMouseMove
#define MAYBE_DisconnectOnLocalInputTest DISABLED_DisconnectOnLocalInputTest
#else
#define MAYBE_LocalInputTest LocalInputTest
#define MAYBE_ClampMouseEvents ClampMouseEvents
#define MAYBE_DisableInputs DisableInputs
#define MAYBE_MultiMonMouseMove_SameSize MultiMonMouseMove_SameSize
#define MAYBE_RestoreEventState RestoreEventState
#define MAYBE_MultiMonMouseMove MultiMonMouseMove
#define MAYBE_DisconnectOnLocalInputTest DisconnectOnLocalInputTest
#endif

namespace remoting {

using protocol::FakeSession;
using protocol::MockClientStub;
using protocol::MockHostStub;
using protocol::MockInputStub;
using protocol::MockVideoStub;
using protocol::SessionConfig;
using protocol::test::EqualsClipboardEvent;
using protocol::test::EqualsMouseButtonEvent;
using protocol::test::EqualsMouseMoveEvent;
using protocol::test::EqualsKeyEvent;

using testing::_;
using testing::AtLeast;
using testing::ReturnRef;
using testing::StrictMock;

namespace {

constexpr char kTestDataChannelCallbackName[] = "test_channel_name";

// Matches a |protocol::Capabilities| argument against a list of capabilities
// formatted as a space-separated string.
MATCHER_P(EqCapabilities, expected_capabilities, "") {
  if (!arg.has_capabilities())
    return false;

  std::vector<std::string> words_args = base::SplitString(
      arg.capabilities(), " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> words_expected = base::SplitString(
      expected_capabilities, " ", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  std::sort(words_args.begin(), words_args.end());
  std::sort(words_expected.begin(), words_expected.end());
  return words_args == words_expected;
}

protocol::MouseEvent MakeMouseMoveEvent(int x, int y) {
  protocol::MouseEvent result;
  result.set_x(x);
  result.set_y(y);
  return result;
}

protocol::KeyEvent MakeKeyEvent(bool pressed, uint32_t keycode) {
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

}  // namespace

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Fake multi-monitor setup.
  static const int kDisplay1Width =
      protocol::FakeDesktopCapturer::kWidth;  // 800
  static const int kDisplay1Height =
      protocol::FakeDesktopCapturer::kHeight;  // 600
  static const int kDisplay2Width = 1024;
  static const int kDisplay2Height = 768;
  static const int kDisplay2YOffset = 35;
  static const int kInvalidDisplayIndex = -2;

  // Creates the client session from a FakeSession instance.
  void CreateClientSession(std::unique_ptr<protocol::FakeSession> session);

  // Creates the client session.
  void CreateClientSession();

  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected. This effectively enables
  // the input pipe line and starts video capturing.
  void ConnectClientSession();

  // Fakes video size notification from the VideoStream.
  void SendOnVideoSizeChanged(int width, int height, int dpi_x, int dpi_y);
  void NotifyVideoSize(int id);
  void NotifyVideoSizeAll();

  // Add a fake display to the layout list. Used in conjunction with
  // NotifyDesktopDisplaySize.
  void AddDisplayToLayout(protocol::VideoLayout* displays,
                          int x,
                          int y,
                          int width,
                          int height,
                          int dpi_x,
                          int dpi_y);

  // Fakes desktop display size notification from Webrtc.
  void NotifyDesktopDisplaySize(
      std::unique_ptr<protocol::VideoLayout> displays);

  // Fakes display select request from user.
  void NotifySelectDesktopDisplay(std::string id);

  // Convenience methods to setup a single- or double-monitor setup.
  void ResetDisplayInfo();
  void SetupSingleDisplay();
  void SetupMultiDisplay();
  void SetupMultiDisplay_SameSize();

  // When using a multi-mon setup, this fakes the user selecting which display
  // to show.
  void MultiMon_SelectFirstDisplay();
  void MultiMon_SelectSecondDisplay();
  void MultiMon_SelectAllDisplays();

  // Geometry info for displays being tested.
  DesktopDisplayInfo displays_;
  int curr_display_;

  // Message loop that will process all ClientSession tasks.
  base::test::SingleThreadTaskEnvironment task_environment_;

  // AutoThreadTaskRunner on which |client_session_| will be run.
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Used to run |message_loop_| after each test, until no objects remain that
  // require it.
  base::RunLoop run_loop_;

  // HostExtensions to pass when creating the ClientSession. Caller retains
  // ownership of the HostExtensions themselves.
  std::vector<HostExtension*> extensions_;

  // ClientSession instance under test.
  std::unique_ptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  // Stubs returned to |client_session_| components by |connection_|.
  MockClientStub client_stub_;

  // ClientSession owns |connection_| but tests need it to inject fake events.
  protocol::FakeConnectionToClient* connection_;

  std::unique_ptr<FakeDesktopEnvironmentFactory> desktop_environment_factory_;

  DesktopEnvironmentOptions desktop_environment_options_;
};

void ClientSessionTest::SetUp() {
  // Arrange to run |task_environment_| until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());

  desktop_environment_factory_.reset(new FakeDesktopEnvironmentFactory(
      task_environment_.GetMainThreadTaskRunner()));
  desktop_environment_options_ = DesktopEnvironmentOptions::CreateDefault();
}

void ClientSessionTest::TearDown() {
  if (client_session_) {
    if (connection_->is_connected())
      client_session_->DisconnectSession(protocol::OK);
    client_session_.reset();
    desktop_environment_factory_.reset();
  }

  // Clear out |task_runner_| reference so the loop can quit, and run it until
  // it does.
  task_runner_ = nullptr;
  run_loop_.Run();
}

void ClientSessionTest::CreateClientSession(
    std::unique_ptr<protocol::FakeSession> session) {
  DCHECK(session);

  // Mock protocol::ConnectionToClient APIs called directly by ClientSession.
  // HostStub is not touched by ClientSession, so we can safely pass nullptr.
  std::unique_ptr<protocol::FakeConnectionToClient> connection(
      new protocol::FakeConnectionToClient(std::move(session)));
  connection->set_client_stub(&client_stub_);
  connection_ = connection.get();

  client_session_.reset(new ClientSession(
      &session_event_handler_, std::move(connection),
      desktop_environment_factory_.get(), desktop_environment_options_,
      base::TimeDelta(), nullptr, extensions_));
}

void ClientSessionTest::CreateClientSession() {
  CreateClientSession(std::make_unique<protocol::FakeSession>());
}

void ClientSessionTest::ConnectClientSession() {
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_));

  // Stubs should be set only after connection is authenticated.
  EXPECT_FALSE(connection_->clipboard_stub());
  EXPECT_FALSE(connection_->input_stub());

  client_session_->OnConnectionAuthenticated();

  EXPECT_TRUE(connection_->clipboard_stub());
  EXPECT_TRUE(connection_->input_stub());

  client_session_->CreateMediaStreams();
  client_session_->OnConnectionChannelsConnected();
}

void ClientSessionTest::SendOnVideoSizeChanged(int width,
                                               int height,
                                               int dpi_x,
                                               int dpi_y) {
  connection_->last_video_stream()->observer()->OnVideoSizeChanged(
      connection_->last_video_stream().get(),
      webrtc::DesktopSize(width, height), webrtc::DesktopVector(dpi_x, dpi_y));
}

void ClientSessionTest::NotifyVideoSize(int display_index) {
  const DisplayGeometry* oldDisp = displays_.GetDisplayInfo(curr_display_);
  const DisplayGeometry* disp = displays_.GetDisplayInfo(display_index);

  curr_display_ = display_index;

  // The OnVideoSizeChanged message is sent only if the size has actually
  // changed.
  if (oldDisp == nullptr || disp == nullptr || oldDisp->width != disp->width ||
      oldDisp->height != disp->height) {
    SendOnVideoSizeChanged(disp->width, disp->height, disp->dpi, disp->dpi);
  } else {
    client_session_->UpdateMouseClampingFilterOffset();
  }
}

void ClientSessionTest::NotifyVideoSizeAll() {
  if (curr_display_ == webrtc::kFullDesktopScreenId) {
    return;
  }
  curr_display_ = webrtc::kFullDesktopScreenId;

  int x_min, x_max, y_min, y_max;
  bool initialized = false;
  for (DisplayGeometry disp : displays_.displays()) {
    int disp_x_max = disp.x + disp.width;
    int disp_y_max = disp.y + disp.height;
    if (!initialized) {
      x_min = disp.x;
      x_max = disp_x_max;
      y_min = disp.y;
      y_max = disp_y_max;
      initialized = true;
    } else {
      if (disp.x < x_min)
        x_min = disp.x;
      if (disp_x_max > x_max)
        x_max = disp_x_max;
      if (disp.y < y_min)
        y_min = disp.y;
      if (disp_y_max > y_max)
        y_max = disp_y_max;
    }
  }
  int width = x_max - x_min;
  int height = y_max - y_min;
  SendOnVideoSizeChanged(width, height, kDefaultDpi, kDefaultDpi);
}

void ClientSessionTest::AddDisplayToLayout(protocol::VideoLayout* displays,
                                           int x,
                                           int y,
                                           int width,
                                           int height,
                                           int dpi_x,
                                           int dpi_y) {
  protocol::VideoTrackLayout* video_track = displays->add_video_track();
  video_track->set_position_x(x);
  video_track->set_position_y(y);
  video_track->set_width(width);
  video_track->set_height(height);
  video_track->set_x_dpi(dpi_x);
  video_track->set_y_dpi(dpi_y);
  displays_.AddDisplayFrom(*video_track);
}

void ClientSessionTest::NotifyDesktopDisplaySize(
    std::unique_ptr<protocol::VideoLayout> displays) {
  client_session_->OnDesktopDisplayChanged(std::move(displays));
}

void ClientSessionTest::NotifySelectDesktopDisplay(std::string id) {
  protocol::SelectDesktopDisplayRequest req;
  req.set_id(id);
  client_session_->SelectDesktopDisplay(req);
}

void ClientSessionTest::ResetDisplayInfo() {
  displays_.Reset();
  curr_display_ = kInvalidDisplayIndex;
}

// Set up a single display (default size).
void ClientSessionTest::SetupSingleDisplay() {
  ResetDisplayInfo();
  auto displays = std::make_unique<protocol::VideoLayout>();
  AddDisplayToLayout(displays.get(), 0, 0, kDisplay1Width, kDisplay1Height,
                     kDefaultDpi, kDefaultDpi);
  NotifyDesktopDisplaySize(std::move(displays));
  NotifyVideoSizeAll();
}

// Set up multiple displays:
// +-----------+
// |  800x600  |---------------+
// |     0     |   1024x768    |
// +-----------+       1       |
//             |               |
//             +---------------+
void ClientSessionTest::SetupMultiDisplay() {
  ResetDisplayInfo();
  auto displays = std::make_unique<protocol::VideoLayout>();
  AddDisplayToLayout(displays.get(), 0, 0, kDisplay1Width, kDisplay1Height,
                     kDefaultDpi, kDefaultDpi);
  AddDisplayToLayout(displays.get(), kDisplay1Width, kDisplay2YOffset,
                     kDisplay2Width, kDisplay2Height, kDefaultDpi, kDefaultDpi);
  NotifyDesktopDisplaySize(std::move(displays));
  NotifyVideoSizeAll();
}

// Set up multiple displays that are the same size:
// +-----------+
// |  800x600  |-----------+
// |     0     |  800x600  |
// +-----------+     1     |
//             +-----------+
void ClientSessionTest::SetupMultiDisplay_SameSize() {
  ResetDisplayInfo();
  auto displays = std::make_unique<protocol::VideoLayout>();
  AddDisplayToLayout(displays.get(), 0, 0, kDisplay1Width, kDisplay1Height,
                     kDefaultDpi, kDefaultDpi);
  AddDisplayToLayout(displays.get(), kDisplay1Width, kDisplay2YOffset,
                     kDisplay1Width, kDisplay1Height, kDefaultDpi, kDefaultDpi);
  NotifyDesktopDisplaySize(std::move(displays));
  NotifyVideoSizeAll();
}

void ClientSessionTest::MultiMon_SelectFirstDisplay() {
  NotifySelectDesktopDisplay("0");
  NotifyVideoSize(0);
}

void ClientSessionTest::MultiMon_SelectSecondDisplay() {
  NotifySelectDesktopDisplay("1");
  NotifyVideoSize(1);
}

void ClientSessionTest::MultiMon_SelectAllDisplays() {
  NotifySelectDesktopDisplay("all");
  NotifyVideoSizeAll();
}

TEST_F(ClientSessionTest, MAYBE_MultiMonMouseMove) {
  CreateClientSession();
  ConnectClientSession();
  SetupMultiDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);

  // These mouse events are in global (full desktop) coordinates.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(70, 50));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(1000, 650));

  // Select second display: origin: 800,35 ; size: 1024x768
  MultiMon_SelectSecondDisplay();
  // This mouse event is injected relative to the second display.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(1005, 625));
  // Events should clamp to the selected display.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(2000, 700));

  // Select first display: origin: 0,0 ; size: 800x600
  MultiMon_SelectFirstDisplay();
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(80, 60));
  // Events should clamp to the selected display (800,600).
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(1000, 640));

  // Select entire desktop again: origin: 0,0 ; size: 1824x768
  MultiMon_SelectAllDisplays();
  // Events should clamp to the entire desktop (800+1024, 35+768).
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(2000, 1000));

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(7U, mouse_events.size());
  // Full desktop.
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(70, 50));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(1000, 650));
  // Second display.
  EXPECT_THAT(mouse_events[2], EqualsMouseMoveEvent(1005 + kDisplay1Width,
                                                    625 + kDisplay2YOffset));
  EXPECT_THAT(mouse_events[3],
              EqualsMouseMoveEvent(kDisplay1Width + kDisplay2Width - 1,
                                   700 + kDisplay2YOffset));
  // First display.
  EXPECT_THAT(mouse_events[4], EqualsMouseMoveEvent(80, 60));
  EXPECT_THAT(mouse_events[5],
              EqualsMouseMoveEvent(kDisplay1Width - 1, kDisplay1Height - 1));
  // Full desktop.
  EXPECT_THAT(mouse_events[6],
              EqualsMouseMoveEvent(kDisplay1Width + kDisplay2Width - 1,
                                   kDisplay2Height + kDisplay2YOffset - 1));
}

TEST_F(ClientSessionTest, MAYBE_MultiMonMouseMove_SameSize) {
  CreateClientSession();
  ConnectClientSession();
  SetupMultiDisplay_SameSize();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);

  // These mouse events are in global (full desktop) coordinates.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(70, 50));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(1000, 550));

  // Select second display: origin: 800,35 ; size: 800x600
  MultiMon_SelectSecondDisplay();
  // This mouse event is injected relative to the second display.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(705, 525));
  // Events should clamp to the selected display.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(2000, 500));

  // Select first display: origin: 0,0 ; size: 800x600
  MultiMon_SelectFirstDisplay();
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(80, 60));
  // Events should clamp to the selected display (800,600).
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(1000, 640));

  // Select entire desktop again: origin: 0,0 ; size: 1600x635
  MultiMon_SelectAllDisplays();
  // Events should clamp to the entire desktop (800+800, 35+600).
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(2000, 1000));

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(7U, mouse_events.size());
  // Full desktop.
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(70, 50));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(1000, 550));
  // Second display.
  EXPECT_THAT(mouse_events[2], EqualsMouseMoveEvent(705 + kDisplay1Width,
                                                    525 + kDisplay2YOffset));
  EXPECT_THAT(mouse_events[3], EqualsMouseMoveEvent(2 * kDisplay1Width - 1,
                                                    500 + kDisplay2YOffset));
  // First display.
  EXPECT_THAT(mouse_events[4], EqualsMouseMoveEvent(80, 60));
  EXPECT_THAT(mouse_events[5],
              EqualsMouseMoveEvent(kDisplay1Width - 1, kDisplay1Height - 1));
  // Full desktop.
  EXPECT_THAT(mouse_events[6],
              EqualsMouseMoveEvent(2 * kDisplay1Width - 1,
                                   kDisplay1Height + kDisplay2YOffset - 1));
}

TEST_F(ClientSessionTest, MAYBE_DisableInputs) {
  CreateClientSession();
  ConnectClientSession();
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::KeyEvent> key_events;
  input_injector->set_key_events(&key_events);
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);
  std::vector<protocol::ClipboardEvent> clipboard_events;
  input_injector->set_clipboard_events(&clipboard_events);

  // Inject test events that are expected to be injected.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("a"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(100, 101));

  // Disable input.
  client_session_->SetDisableInputs(true);

  // These event shouldn't get though to the input injector.
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("b"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(false, 2));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(200, 201));

  // Enable input again.
  client_session_->SetDisableInputs(false);
  connection_->clipboard_stub()->InjectClipboardEvent(MakeClipboardEvent("c"));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 3));
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(300, 301));

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(300, 301));

  EXPECT_EQ(4U, key_events.size());
  EXPECT_THAT(key_events[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events[1], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events[2], EqualsKeyEvent(3, true));
  EXPECT_THAT(key_events[3], EqualsKeyEvent(3, false));

  EXPECT_EQ(2U, clipboard_events.size());
  EXPECT_THAT(clipboard_events[0],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "a"));
  EXPECT_THAT(clipboard_events[1],
              EqualsClipboardEvent(kMimeTypeTextUtf8, "c"));
}

TEST_F(ClientSessionTest, MAYBE_LocalInputTest) {
  CreateClientSession();
  ConnectClientSession();
  SetupSingleDisplay();

  std::vector<protocol::MouseEvent> mouse_events;
  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events);

  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(100, 101));

#if !defined(OS_WIN)
  // The OS echoes the injected event back.
  client_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                       ui::ET_MOUSE_MOVED);
#endif  // !defined(OS_WIN)

  // This one should get throught as well.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(200, 201));

  // Now this is a genuine local event.
  client_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                       ui::ET_MOUSE_MOVED);

  // This one should be blocked because of the previous local input event.
  connection_->input_stub()->InjectMouseEvent(MakeMouseMoveEvent(300, 301));

  // Verify that we've received correct set of mouse events.
  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0], EqualsMouseMoveEvent(100, 101));
  EXPECT_THAT(mouse_events[1], EqualsMouseMoveEvent(200, 201));

  // Verify that we're still connected.
  EXPECT_TRUE(connection_->is_connected());

  // TODO(jamiewalch): Verify that remote inputs are re-enabled
  // eventually (via dependency injection, not sleep!)
}

TEST_F(ClientSessionTest, MAYBE_DisconnectOnLocalInputTest) {
  desktop_environment_options_.set_terminate_upon_input(true);
  CreateClientSession();
  ConnectClientSession();
  SetupSingleDisplay();

  client_session_->OnLocalPointerMoved(webrtc::DesktopVector(100, 101),
                                       ui::ET_MOUSE_MOVED);
  EXPECT_FALSE(connection_->is_connected());
}

TEST_F(ClientSessionTest, MAYBE_RestoreEventState) {
  CreateClientSession();
  ConnectClientSession();
  SetupSingleDisplay();

  FakeInputInjector* input_injector =
      desktop_environment_factory_->last_desktop_environment()
          ->last_input_injector()
          .get();
  std::vector<protocol::KeyEvent> key_events;
  input_injector->set_key_events(&key_events);
  std::vector<protocol::MouseEvent> mouse_events;
  input_injector->set_mouse_events(&mouse_events);

  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 1));
  connection_->input_stub()->InjectKeyEvent(MakeKeyEvent(true, 2));

  protocol::MouseEvent mousedown;
  mousedown.set_button(protocol::MouseEvent::BUTTON_LEFT);
  mousedown.set_button_down(true);
  connection_->input_stub()->InjectMouseEvent(mousedown);

  client_session_->DisconnectSession(protocol::OK);
  client_session_.reset();

  EXPECT_EQ(2U, mouse_events.size());
  EXPECT_THAT(mouse_events[0],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, true));
  EXPECT_THAT(mouse_events[1],
              EqualsMouseButtonEvent(protocol::MouseEvent::BUTTON_LEFT, false));

  EXPECT_EQ(4U, key_events.size());
  EXPECT_THAT(key_events[0], EqualsKeyEvent(1, true));
  EXPECT_THAT(key_events[1], EqualsKeyEvent(2, true));
  EXPECT_THAT(key_events[2], EqualsKeyEvent(1, false));
  EXPECT_THAT(key_events[3], EqualsKeyEvent(2, false));
}

TEST_F(ClientSessionTest, MAYBE_ClampMouseEvents) {
  CreateClientSession();
  ConnectClientSession();
  SetupSingleDisplay();

  std::vector<protocol::MouseEvent> mouse_events;
  desktop_environment_factory_->last_desktop_environment()
      ->last_input_injector()
      ->set_mouse_events(&mouse_events);

  int input_x[3] = {-999, 100, 999};
  int expected_x[3] = {0, 100, protocol::FakeDesktopCapturer::kWidth - 1};
  int input_y[3] = {-999, 50, 999};
  int expected_y[3] = {0, 50, protocol::FakeDesktopCapturer::kHeight - 1};

  protocol::MouseEvent expected_event;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      mouse_events.clear();
      connection_->input_stub()->InjectMouseEvent(
          MakeMouseMoveEvent(input_x[i], input_y[j]));

      EXPECT_EQ(1U, mouse_events.size());
      EXPECT_THAT(mouse_events[0],
                  EqualsMouseMoveEvent(expected_x[i], expected_y[j]));
    }
  }
}

// Verifies that clients can have extensions registered, resulting in the
// correct capabilities being reported, and messages delivered correctly.
// The extension system is tested more extensively in the
// HostExtensionSessionManager unit-tests.
TEST_F(ClientSessionTest, Extensions) {
  // Configure fake extensions for testing.
  FakeExtension extension1("ext1", "cap1");
  extensions_.push_back(&extension1);
  FakeExtension extension2("ext2", "");
  extensions_.push_back(&extension2);
  FakeExtension extension3("ext3", "cap3");
  extensions_.push_back(&extension3);

  // Verify that the ClientSession reports the correct capabilities.
  EXPECT_CALL(client_stub_, SetCapabilities(EqCapabilities("cap1 cap3")));

  CreateClientSession();
  ConnectClientSession();

  testing::Mock::VerifyAndClearExpectations(&client_stub_);

  // Mimic the client reporting an overlapping set of capabilities.
  protocol::Capabilities capabilities_message;
  capabilities_message.set_capabilities("cap1 cap4 default");
  client_session_->SetCapabilities(capabilities_message);

  // Verify that the correct extension messages are delivered, and dropped.
  protocol::ExtensionMessage message1;
  message1.set_type("ext1");
  message1.set_data("data");
  client_session_->DeliverClientMessage(message1);
  protocol::ExtensionMessage message3;
  message3.set_type("ext3");
  message3.set_data("data");
  client_session_->DeliverClientMessage(message3);
  protocol::ExtensionMessage message4;
  message4.set_type("ext4");
  message4.set_data("data");
  client_session_->DeliverClientMessage(message4);

  base::RunLoop().RunUntilIdle();

  // ext1 was instantiated and sent a message, and did not wrap anything.
  EXPECT_TRUE(extension1.was_instantiated());
  EXPECT_TRUE(extension1.has_handled_message());

  // ext2 was instantiated but not sent a message, and wrapped video encoder.
  EXPECT_TRUE(extension2.was_instantiated());
  EXPECT_FALSE(extension2.has_handled_message());

  // ext3 was sent a message but not instantiated.
  EXPECT_FALSE(extension3.was_instantiated());
}

TEST_F(ClientSessionTest, DataChannelCallbackIsCalled) {
  bool callback_called = false;

  CreateClientSession();
  client_session_->RegisterCreateHandlerCallbackForTesting(
      kTestDataChannelCallbackName,
      base::Bind([](bool* callback_was_called, const std::string& name,
                    std::unique_ptr<protocol::MessagePipe> pipe)
                     -> void { *callback_was_called = true; },
                 &callback_called));
  ConnectClientSession();

  std::unique_ptr<protocol::MessagePipe> pipe =
      base::WrapUnique(new protocol::FakeMessagePipe(false));

  client_session_->OnIncomingDataChannel(kTestDataChannelCallbackName,
                                         std::move(pipe));

  ASSERT_TRUE(callback_called);
}

TEST_F(ClientSessionTest, ForwardHostSessionOptions1) {
  auto session = std::make_unique<protocol::FakeSession>();
  auto configuration = std::make_unique<jingle_xmpp::XmlElement>(
      jingle_xmpp::QName(kChromotingXmlNamespace, "host-configuration"));
  configuration->SetBodyText("Detect-Updated-Region:true");
  session->SetAttachment(0, std::move(configuration));
  CreateClientSession(std::move(session));
  ConnectClientSession();
  ASSERT_TRUE(desktop_environment_factory_->last_desktop_environment()
                  ->options()
                  .desktop_capture_options()
                  ->detect_updated_region());
}

TEST_F(ClientSessionTest, ForwardHostSessionOptions2) {
  auto session = std::make_unique<protocol::FakeSession>();
  auto configuration = std::make_unique<jingle_xmpp::XmlElement>(
      jingle_xmpp::QName(kChromotingXmlNamespace, "host-configuration"));
  configuration->SetBodyText("Detect-Updated-Region:false");
  session->SetAttachment(0, std::move(configuration));
  CreateClientSession(std::move(session));
  ConnectClientSession();
  ASSERT_FALSE(desktop_environment_factory_->last_desktop_environment()
                   ->options()
                   .desktop_capture_options()
                   ->detect_updated_region());
}

#if defined(OS_WIN)
TEST_F(ClientSessionTest, ForwardDirectXHostSessionOptions1) {
  auto session = std::make_unique<protocol::FakeSession>();
  auto configuration = std::make_unique<jingle_xmpp::XmlElement>(
      jingle_xmpp::QName(kChromotingXmlNamespace, "host-configuration"));
  configuration->SetBodyText("DirectX-Capturer:true");
  session->SetAttachment(0, std::move(configuration));
  CreateClientSession(std::move(session));
  ConnectClientSession();
  ASSERT_TRUE(desktop_environment_factory_->last_desktop_environment()
                  ->options()
                  .desktop_capture_options()
                  ->allow_directx_capturer());
}

TEST_F(ClientSessionTest, ForwardDirectXHostSessionOptions2) {
  auto session = std::make_unique<protocol::FakeSession>();
  auto configuration = std::make_unique<jingle_xmpp::XmlElement>(
      jingle_xmpp::QName(kChromotingXmlNamespace, "host-configuration"));
  configuration->SetBodyText("DirectX-Capturer:false");
  session->SetAttachment(0, std::move(configuration));
  CreateClientSession(std::move(session));
  ConnectClientSession();
  ASSERT_FALSE(desktop_environment_factory_->last_desktop_environment()
                   ->options()
                   .desktop_capture_options()
                   ->allow_directx_capturer());
}
#endif

}  // namespace remoting
