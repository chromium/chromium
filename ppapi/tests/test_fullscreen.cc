// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_fullscreen.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Fullscreen);

namespace {

const ColorPremul kSheerBlue = { 0x88, 0x00, 0x00, 0x88 };
const ColorPremul kOpaqueYellow = { 0xFF, 0xFF, 0xFF, 0x00 };
const int kBytesPerPixel = sizeof(uint32_t);  // 4 bytes for BGRA or RGBA.

uint32_t FormatColor(PP_ImageDataFormat format, ColorPremul color) {
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return (color.A << 24) | (color.R << 16) | (color.G << 8) | (color.B);
  else if (format == PP_IMAGEDATAFORMAT_RGBA_PREMUL)
    return (color.A << 24) | (color.B << 16) | (color.G << 8) | (color.R);
  else
    return 0;
}

bool HasMidScreen(const pp::Rect& position, const pp::Size& screen_size) {
  static int32_t mid_x = screen_size.width() / 2;
  static int32_t mid_y = screen_size.height() / 2;
  return (position.Contains(mid_x, mid_y));
}

void FlushCallbackCheckImageData(void* data, int32_t result) {
  static_cast<TestFullscreen*>(data)->CheckPluginPaint();
}

}  // namespace

TestFullscreen::TestFullscreen(TestingInstance* instance)
    : TestCase(instance),
      error_(),
      screen_mode_(instance),
      painted_color_(0),
      fullscreen_pending_(false),
      normal_pending_(false),
      fullscreen_event_(instance->pp_instance()),
      normal_event_(instance->pp_instance()) {
  screen_mode_.GetScreenSize(&screen_size_);
}

bool TestFullscreen::Init() {
  if (screen_size_.IsEmpty()) {
    instance_->AppendError("Failed to initialize screen_size_");
    return false;
  }
  graphics2d_ = pp::Graphics2D(instance_, screen_size_, true);
  if (!instance_->BindGraphics(graphics2d_)) {
    instance_->AppendError("Failed to initialize graphics2d_");
    return false;
  }
  return CheckTestingInterface();
}

void TestFullscreen::RunTests(const std::string& filter) {
  RUN_TEST(GetScreenSize, filter);
  RUN_TEST(NormalToFullscreenToNormal, filter);
}

bool TestFullscreen::GotError() {
  return !error_.empty();
}

std::string TestFullscreen::Error() {
  std::string last_error = error_;
  error_.clear();
  return last_error;
}

// TODO(polina): consider adding custom logic to JS for this test to
// get screen.width and screen.height and postMessage those to this code,
// so the dimensions can be checked exactly.
std::string TestFullscreen::TestGetScreenSize() {
  if (screen_size_.width() < 320 || screen_size_.width() > 2560)
    return ReportError("screen_size.width()", screen_size_.width());
  if (screen_size_.height() < 200 || screen_size_.height() > 2048)
    return ReportError("screen_size.height()", screen_size_.height());
  PASS();
}

std::string TestFullscreen::TestNormalToFullscreenToNormal() {
  // 0. Start in normal mode.
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() at start", true);

  // 1. Switch to fullscreen.
  // This is only allowed within a context of a user gesture (e.g. mouse click).
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) outside of user gesture", true);
  // Trigger another call to SetFullscreen(true) from HandleInputEvent().
  // The transition is asynchronous and ends at the next DidChangeView().
  instance_->RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  SimulateUserGesture();
  // DidChangeView() will call the callback once in fullscreen mode.
  fullscreen_event_.Wait();
  if (GotError())
    return Error();
  if (fullscreen_pending_)
    return "fullscreen_pending_ has not been reset";
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen", false);

  // 2. Stay in fullscreen. No change.
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in fullscreen", true);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen^2", false);

  // 3. Switch to normal.
  // The transition is asynchronous and ends at DidChangeView().
  // No graphics devices can be bound while in transition.
  normal_pending_ = true;
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in fullscreen", false);
  // DidChangeView() will signal once out of fullscreen mode.
  normal_event_.Wait();
  if (GotError())
    return Error();
  if (normal_pending_)
    return "normal_pending_ has not been reset";
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal", true);

  // 4. Stay in normal. No change.
  if (screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in normal", true);
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal^2", true);

  PASS();
}

void TestFullscreen::SimulateUserGesture() {
  pp::Point plugin_center(
      normal_position_.x() + normal_position_.width() / 2,
      normal_position_.y() + normal_position_.height() / 2);
  pp::Point mouse_movement;
  pp::MouseInputEvent input_event(
      instance_,
      PP_INPUTEVENT_TYPE_MOUSEDOWN,
      NowInTimeTicks(),  // time_stamp
      0,  // modifiers
      PP_INPUTEVENT_MOUSEBUTTON_LEFT,
      plugin_center,
      1,  // click_count
      mouse_movement);

  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
}

void TestFullscreen::FailFullscreenTest(const std::string& error) {
  error_ = error;
  fullscreen_event_.Signal();
}

void TestFullscreen::FailNormalTest(const std::string& error) {
  error_ = error;
  normal_event_.Signal();
}

void TestFullscreen::PassFullscreenTest() {
  fullscreen_event_.Signal();
}

void TestFullscreen::PassNormalTest() {
  normal_event_.Signal();
}

// Transition to fullscreen can only happen when processing a user gesture.
bool TestFullscreen::HandleInputEvent(const pp::InputEvent& event) {
  // We only let mouse events through and only mouse clicks count.
  if (event.GetType() != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event.GetType() != PP_INPUTEVENT_TYPE_MOUSEUP)
    return false;
  // We got the gesture. No need to handle any more events.
  instance_->ClearInputEventRequest(PP_INPUTEVENT_CLASS_MOUSE);
  if (screen_mode_.IsFullscreen()) {
    FailFullscreenTest(
        ReportError("IsFullscreen() before fullscreen transition", true));
    return false;
  }
  fullscreen_pending_ = true;
  if (!screen_mode_.SetFullscreen(true)) {
    FailFullscreenTest(ReportError("SetFullscreen(true) in normal", false));
    return false;
  }
  // DidChangeView() will complete the transition to fullscreen.
  return false;
}

bool TestFullscreen::PaintPlugin(pp::Size size, ColorPremul color) {
  painted_size_ = size;
  PP_ImageDataFormat image_format = pp::ImageData::GetNativeImageDataFormat();
  painted_color_ = FormatColor(image_format, color);
  if (painted_color_ == 0)
    return false;
  pp::Point origin(0, 0);

  pp::ImageData image(instance_, image_format, size, false);
  if (image.is_null())
    return false;
  uint32_t* pixels = static_cast<uint32_t*>(image.data());
  int num_pixels = image.stride() / kBytesPerPixel * image.size().height();
  for (int i = 0; i < num_pixels; i++)
    pixels[i] = painted_color_;
  graphics2d_.PaintImageData(image, origin);
  pp::CompletionCallback cc(FlushCallbackCheckImageData, this);
  if (graphics2d_.Flush(cc) != PP_OK_COMPLETIONPENDING)
    return false;

  return true;
}

void TestFullscreen::CheckPluginPaint() {
  PP_ImageDataFormat image_format = pp::ImageData::GetNativeImageDataFormat();
  pp::ImageData readback(instance_, image_format, painted_size_, false);
  pp::Point origin(0, 0);
  if (readback.is_null() ||
      PP_TRUE != testing_interface_->ReadImageData(graphics2d_.pp_resource(),
                                                   readback.pp_resource(),
                                                   &origin.pp_point())) {
    error_ = "Can't read plugin image";
    return;
  }
  for (int y = 0; y < painted_size_.height(); y++) {
    for (int x = 0; x < painted_size_.width(); x++) {
      uint32_t* readback_color = readback.GetAddr32(pp::Point(x, y));
      if (painted_color_ != *readback_color) {
        error_ = "Plugin image contains incorrect pixel value";
        return;
      }
    }
  }
  if (screen_mode_.IsFullscreen())
    PassFullscreenTest();
  else
    PassNormalTest();
}

// Transitions to/from fullscreen is asynchronous ending at DidChangeView.
// The number of calls to DidChangeView during fullscreen / normal transitions
// isn't specified by the API. The test waits until it the screen has
// transitioned to the desired state.
//
// WebKit does not change the plugin size, but Pepper does explicitly set
// it to screen width and height when SetFullscreen(true) is called and
// resets it back when ViewChanged is received indicating that we exited
// fullscreen.
//
// NOTE: The number of DidChangeView calls for <object> might be different.
// TODO(bbudge) Figure out how to test that the plugin positon eventually
// changes to normal_position_.
void TestFullscreen::DidChangeView(const pp::View& view) {
  pp::Rect position = view.GetRect();
  pp::Rect clip = view.GetClipRect();

  if (normal_position_.IsEmpty())
    normal_position_ = position;

  bool is_fullscreen = screen_mode_.IsFullscreen();
  if (fullscreen_pending_ && is_fullscreen) {
    fullscreen_pending_ = false;
    if (!HasMidScreen(position, screen_size_))
      FailFullscreenTest("DidChangeView is not in the middle of the screen");
    else if (position.size() != screen_size_)
      FailFullscreenTest("DidChangeView does not have screen size");
    // NOTE: we cannot reliably test for clip size being equal to the screen
    // because it might be affected by JS console, info bars, etc.
    else if (!instance_->BindGraphics(graphics2d_))
      FailFullscreenTest("Failed to BindGraphics() in fullscreen");
    else if (!PaintPlugin(position.size(), kOpaqueYellow))
      FailFullscreenTest("Failed to paint plugin image in fullscreen");
  } else if (normal_pending_ && !is_fullscreen) {
    normal_pending_ = false;
    if (screen_mode_.IsFullscreen())
      FailNormalTest("DidChangeview is in fullscreen");
    else if (!instance_->BindGraphics(graphics2d_))
      FailNormalTest("Failed to BindGraphics() in normal");
    else if (!PaintPlugin(position.size(), kSheerBlue))
      FailNormalTest("Failed to paint plugin image in normal");
  }
}
