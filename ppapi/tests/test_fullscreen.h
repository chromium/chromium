// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FULLSCREEN_H_
#define PPAPI_TESTS_TEST_FULLSCREEN_H_

#include <stdint.h>

#include <string>

#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

namespace pp {
class InputEvent;
}  // namespace pp

struct ColorPremul { uint32_t A, R, G, B; };  // Use premultipled Alpha.

class TestFullscreen : public TestCase {
 public:
  explicit TestFullscreen(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);
  virtual bool HandleInputEvent(const pp::InputEvent& event);
  virtual void DidChangeView(const pp::View& view);

  void CheckPluginPaint();

 private:
  std::string TestGetScreenSize();
  std::string TestNormalToFullscreenToNormal();

  void SimulateUserGesture();
  void FailFullscreenTest(const std::string& error);
  void FailNormalTest(const std::string& error);
  void PassFullscreenTest();
  void PassNormalTest();
  bool PaintPlugin(pp::Size size, ColorPremul color);

  bool GotError();
  std::string Error();

  std::string error_;

  pp::Fullscreen screen_mode_;
  pp::Size screen_size_;
  pp::Rect normal_position_;
  pp::Size painted_size_;
  uint32_t painted_color_;

  bool fullscreen_pending_;
  bool normal_pending_;
  pp::Graphics2D graphics2d_;

  NestedEvent fullscreen_event_;
  NestedEvent normal_event_;
};

#endif  // PPAPI_TESTS_TEST_FULLSCREEN_H_
