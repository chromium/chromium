// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_fullscreen_module.h"

#include "base/task/bind_post_task.h"
#import "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
namespace {
MATCHER_P(MatchesScWindow, ID, "") {
  *result_listener << "with ID " << arg.windowID;
  return arg.windowID == ID;
}

static NSString* const kEmptyString = @"";

static NSString* const kApplicationNameFooBar = @"FooBar";
static NSString* const kApplicationNameKeynote = @"Keynote";
static NSString* const kApplicationNameLibreOffice = @"LibreOffice";
static NSString* const kApplicationNameOpenOffice = @"OpenOffice";
static NSString* const kApplicationNamePowerPoint = @"Microsoft PowerPoint";

constexpr gfx::Rect kDisplayPrimary(0, 0, 1920, 1080);
constexpr gfx::Rect kDisplaySecondary(-1920, 10, 1920, 1080);

constexpr gfx::Rect kFrameOther(140, 170, 600, 400);
constexpr gfx::Rect kFrameEditor(10, 20, 1280, 720);
constexpr gfx::Rect kFrameSlideshow = kDisplayPrimary;
constexpr gfx::Rect kFramePresentersView = kDisplaySecondary;

enum class Application {
  kFoobar = 0,
  kPowerPoint = 1,
  kKeynote = 2,
  kOpenOffice = 3,
  kLibreOffice = 4,
  kSize
};

enum class Mode {
  kOther = 0,
  kEditor = 1,
  kPresentersView = 2,
  kSlideshow = 3,
};

class WindowConfig {
 public:
  WindowConfig(Application application,
               Mode mode,
               int doc_index,
               bool on_screen)
      : application_(application),
        mode_(mode),
        doc_index_(doc_index),
        on_screen_(on_screen) {}

  pid_t process_id() const {
    return static_cast<int>(application_) * 11 + 1234;
  }

  NSString* application_name() const {
    switch (application_) {
      case Application::kFoobar:
        return kApplicationNameFooBar;
      case Application::kPowerPoint:
        return kApplicationNamePowerPoint;
      case Application::kKeynote:
        return kApplicationNameKeynote;
      case Application::kOpenOffice:
        return kApplicationNameOpenOffice;
      case Application::kLibreOffice:
        return kApplicationNameLibreOffice;
      default:
        return kEmptyString;
    }
  }

  NSString* window_title() const {
    switch (application_) {
      case Application::kFoobar:
        return kEmptyString;
      case Application::kPowerPoint: {
        NSString* doc_name =
            [NSString stringWithFormat:@"Presentation%d", doc_index_];
        switch (mode_) {
          case Mode::kEditor:
            return doc_name;
          case Mode::kPresentersView:
            return [NSString
                stringWithFormat:@"PowerPoint Presenter View - [%@]", doc_name];
          case Mode::kSlideshow:
            return [NSString
                stringWithFormat:@"PowerPoint Slide Show - [%@]", doc_name];
          default:
            return kEmptyString;
        }
      }
      case Application::kKeynote: {
        NSString* doc_name =
            [NSString stringWithFormat:@"Untitled%d", doc_index_];
        return doc_name;
      }
      case Application::kOpenOffice: {
        NSString* doc_name =
            [NSString stringWithFormat:@"Untitled %d", doc_index_];
        switch (mode_) {
          case Mode::kEditor:
          case Mode::kPresentersView:
            return [NSString
                stringWithFormat:@"%@ - OpenOffice Impress", doc_name];
          case Mode::kSlideshow:
          default:
            return kEmptyString;
        }
      }
      default:
        return kEmptyString;
    }
  }

  int window_layer() const { return static_cast<int>(mode_); }

  bool on_screen() const { return on_screen_; }
  void set_on_screen(bool on_screen) { on_screen_ = on_screen; }

  CGRect frame() const {
    switch (mode_) {
      case Mode::kOther:
        return kFrameOther.ToCGRect();
      case Mode::kEditor:
        return kFrameEditor.ToCGRect();
      case Mode::kPresentersView:
        return kFramePresentersView.ToCGRect();
      case Mode::kSlideshow:
        return kFrameSlideshow.ToCGRect();
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

 private:
  Application application_;
  Mode mode_;
  int doc_index_;
  bool on_screen_;
  bool active_ = true;
};

SCDisplay* API_AVAILABLE(macos(12.3)) CreateSCDisplay(CGRect frame) {
  id display = OCMClassMock([SCDisplay class]);
  OCMStub([display frame]).andReturn(frame);
  return display;
}

}  // namespace

class API_AVAILABLE(macos(12.3)) MockResetStreamInterface
    : public ScreenCaptureKitResetStreamInterface {
 public:
  MOCK_METHOD1(ResetStreamTo, void(SCWindow* window));
};

class SCKFullscreenModuleTest : public testing::Test {
 public:
  SCKFullscreenModuleTest() = default;

  void SetUp() override {}
  SCWindow* API_AVAILABLE(macos(12.3)) AddWindow(WindowConfig window_config) {
    windows_.push_back(window_config);
    return CreateSCWindow(window_config, windows_.size());
  }

  void API_AVAILABLE(macos(12.3)) getShareableContentMock(
      ScreenCaptureKitFullscreenModule::ContentHandler handler) {
    NSArray* windows = [NSArray array];
    for (size_t i = 0; i < windows_.size(); ++i) {
      if (windows_[i].active()) {
        windows =
            [windows arrayByAddingObject:CreateSCWindow(windows_[i], i + 1)];
      }
    }

    NSArray* displays = @[
      CreateSCDisplay(kDisplayPrimary.ToCGRect()),
      CreateSCDisplay(kDisplaySecondary.ToCGRect())
    ];

    id content = OCMClassMock([SCShareableContent class]);

    OCMStub([content windows]).andReturn(windows);
    OCMStub([content displays]).andReturn(displays);

    std::move(handler).Run(content);
  }

 protected:
  SCWindow* API_AVAILABLE(macos(12.3))
      CreateSCWindow(WindowConfig config, CGWindowID window_id) const {
    id window = OCMClassMock([SCWindow class]);
    id owning_application = OCMClassMock([SCRunningApplication class]);

    OCMStub([owning_application applicationName])
        .andReturn(config.application_name());
    OCMStub([owning_application processID]).andReturn(config.process_id());
    OCMStub([window owningApplication]).andReturn(owning_application);
    OCMStub([window title]).andReturn(config.window_title());
    OCMStub([window windowID]).andReturn(window_id);
    OCMStub([window windowLayer]).andReturn(config.window_layer());
    OCMStub([window frame]).andReturn(config.frame());
    OCMStub([window isOnScreen]).andReturn(config.on_screen());
    return window;
  }

  void SetWindowOnScreen(CGWindowID id, bool on_screen) {
    windows_[id - 1].set_on_screen(on_screen);
  }

  void SetWindowActive(CGWindowID id, bool active) {
    windows_[id - 1].set_active(active);
  }

  void StepForward(base::TimeDelta delta, int steps) {
    for (int i = 0; i < steps; ++i) {
      task_environment_.FastForwardBy(delta);
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<WindowConfig> windows_;
};

TEST_F(SCKFullscreenModuleTest, PowerPointInitialization) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    SCWindow* editor_window = AddWindow(
        {Application::kPowerPoint, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kPowerPoint);
  }
}

TEST_F(SCKFullscreenModuleTest, OpenOfficeInitialization) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    SCWindow* editor_window = AddWindow(
        {Application::kOpenOffice, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kOpenOffice);
  }
}

TEST_F(SCKFullscreenModuleTest, KeynoteInitialization) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    SCWindow* editor_window = AddWindow(
        {Application::kKeynote, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kKeynote);
  }
}

TEST_F(SCKFullscreenModuleTest, LibreOfficeInitializationFails) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    SCWindow* editor_window = AddWindow(
        {Application::kLibreOffice, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_FALSE(fullscreen_module);
  }
}

TEST_F(SCKFullscreenModuleTest, FooBarInitializationFails) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    SCWindow* editor_window =
        AddWindow({Application::kFoobar, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_FALSE(fullscreen_module);
  }
}

// Test that the callback is run.
// Add content when calling the callback handler.
TEST_F(SCKFullscreenModuleTest, DetectFullscreenWindowPowerPoint) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    // Add other application window as first window.
    AddWindow({Application::kFoobar, Mode::kEditor, 0, /*on_screen=*/true});
    SCWindow* editor_window = AddWindow(
        {Application::kPowerPoint, Mode::kEditor, 0, /*on_screen=*/true});
    // Add some more applications.
    AddWindow({Application::kKeynote, Mode::kEditor, 0, /*on_screen=*/true});
    AddWindow({Application::kOpenOffice, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kPowerPoint);

    // Unretained since the test fixture must outlive the test.
    fullscreen_module->set_get_sharable_content_for_test(
        base::BindRepeating(&SCKFullscreenModuleTest::getShareableContentMock,
                            base::Unretained(this)));
    // Start process of checking for new fullscreen windows and run for a few
    // seconds.
    fullscreen_module->Start();
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());

    // Change to fullscreen mode.
    SCWindow* slideshow_window = AddWindow(
        {Application::kPowerPoint, Mode::kSlideshow, 0, /*on_screen=*/true});
    AddWindow({Application::kPowerPoint, Mode::kPresentersView, 0,
               /*on_screen=*/true});
    EXPECT_CALL(reset_stream_interface,
                ResetStreamTo(MatchesScWindow(slideshow_window.windowID)))
        .Times(1);
    StepForward(base::Seconds(1), /*steps=*/1);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());
    // For PowerPoint, the fullscreen window is closed once the slideshow stops.
    // The stream will then be reset to the editor window by
    // ScreenCaptureKitDeviceMac.
  }
}

TEST_F(SCKFullscreenModuleTest, DetectFullscreenWindowKeynote) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    // Add other application window as first window.
    AddWindow({Application::kFoobar, Mode::kEditor, 0, /*on_screen=*/true});
    SCWindow* editor_window = AddWindow(
        {Application::kKeynote, Mode::kEditor, 0, /*on_screen=*/true});
    // Add some more applications.
    AddWindow({Application::kPowerPoint, Mode::kEditor, 0, /*on_screen=*/true});
    AddWindow({Application::kOpenOffice, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kKeynote);

    // Unretained since the test fixture must outlive the test.
    fullscreen_module->set_get_sharable_content_for_test(
        base::BindRepeating(&SCKFullscreenModuleTest::getShareableContentMock,
                            base::Unretained(this)));
    // Start process of checking for new fullscreen windows and run for a few
    // seconds.
    fullscreen_module->Start();
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());

    // Hide editor and change to fullscreen mode.
    SetWindowOnScreen(editor_window.windowID, false);
    SCWindow* slideshow_window = AddWindow(
        {Application::kKeynote, Mode::kSlideshow, 0, /*on_screen=*/true});
    SCWindow* presenters_view_window =
        AddWindow({Application::kKeynote, Mode::kPresentersView, 0,
                   /*on_screen=*/true});
    EXPECT_CALL(reset_stream_interface,
                ResetStreamTo(MatchesScWindow(slideshow_window.windowID)))
        .Times(1);

    StepForward(base::Seconds(1), /*steps=*/1);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());

    // Hide fullscreen windows and restore editor window.
    SetWindowOnScreen(editor_window.windowID, true);
    SetWindowOnScreen(slideshow_window.windowID, false);
    SetWindowOnScreen(presenters_view_window.windowID, false);

    EXPECT_CALL(reset_stream_interface,
                ResetStreamTo(MatchesScWindow(editor_window.windowID)))
        .Times(1);
    task_environment_.FastForwardBy(base::Seconds(1));
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());
  }
}

TEST_F(SCKFullscreenModuleTest, DetectFullscreenWindowOpenOfficeImpress) {
  if (@available(macOS 13.1, *)) {
    MockResetStreamInterface reset_stream_interface;

    // Add other application window as first window.
    AddWindow({Application::kFoobar, Mode::kEditor, 0, /*on_screen=*/true});
    SCWindow* editor_window = AddWindow(
        {Application::kOpenOffice, Mode::kEditor, 0, /*on_screen=*/true});
    SCWindow* second_editor_window = AddWindow(
        {Application::kOpenOffice, Mode::kEditor, 1, /*on_screen=*/true});
    // Add some more applications.
    AddWindow({Application::kPowerPoint, Mode::kEditor, 0, /*on_screen=*/true});
    AddWindow({Application::kKeynote, Mode::kEditor, 0, /*on_screen=*/true});

    std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module =
        MaybeCreateScreenCaptureKitFullscreenModule(
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            reset_stream_interface, editor_window);
    EXPECT_TRUE(fullscreen_module);
    EXPECT_EQ(fullscreen_module->get_mode(),
              ScreenCaptureKitFullscreenModule::Mode::kOpenOffice);

    // Unretained since the test fixture must outlive the test.
    fullscreen_module->set_get_sharable_content_for_test(
        base::BindRepeating(&SCKFullscreenModuleTest::getShareableContentMock,
                            base::Unretained(this)));
    // Start process of checking for new fullscreen windows and run for a few
    // seconds.
    fullscreen_module->Start();
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());

    // Change to fullscreen mode.
    SCWindow* slideshow_window = AddWindow(
        {Application::kOpenOffice, Mode::kSlideshow, 0, /*on_screen=*/true});
    SCWindow* presenters_view_window =
        AddWindow({Application::kOpenOffice, Mode::kPresentersView, 0,
                   /*on_screen=*/true});
    EXPECT_CALL(reset_stream_interface,
                ResetStreamTo(MatchesScWindow(slideshow_window.windowID)))
        .Times(1);

    // Ignore fullscreen window since there are two presentations open and we
    // cannot determine which presentation is in full screen.
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());

    // Close the second document window and verify that the fullscreen window is
    // detected.
    SetWindowOnScreen(second_editor_window.windowID, false);
    StepForward(base::Seconds(1), /*steps=*/1);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());
    StepForward(base::Seconds(1), /*steps=*/4);
    EXPECT_TRUE(fullscreen_module->is_fullscreen_window_active());

    // Hide fullscreen windows and restore editor window.
    SetWindowOnScreen(editor_window.windowID, true);
    SetWindowOnScreen(slideshow_window.windowID, false);
    SetWindowOnScreen(presenters_view_window.windowID, false);

    EXPECT_CALL(reset_stream_interface,
                ResetStreamTo(MatchesScWindow(editor_window.windowID)))
        .Times(1);
    task_environment_.FastForwardBy(base::Seconds(1));
    EXPECT_FALSE(fullscreen_module->is_fullscreen_window_active());
  }
}

}  // namespace content
