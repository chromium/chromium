// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_chromeos.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check_deref.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/ozone/public/system_input_injector.h"

namespace remoting {

namespace {

// The index used by `DisplayManager` to distinguish the displays, which is
// simply a zero-based index and *not* the `display.id()` field!
enum DisplayIndex {
  kFirstDisplay = 0,
  kSecondDisplay = 1,
};

protocol::MouseEvent MakeMouseMoveEvent(int x, int y) {
  protocol::MouseEvent result;
  result.set_x(x);
  result.set_y(y);
  return result;
}

protocol::MouseEvent MakeLeftClickEvent(int x, int y) {
  protocol::MouseEvent result;
  result.set_x(x);
  result.set_y(y);
  result.set_button_down(true);
  result.set_button(protocol::MouseEvent::BUTTON_LEFT);
  return result;
}

gfx::PointF PointFWithOffset(float x, float y, gfx::Point offset) {
  gfx::PointF result{x, y};
  result.Offset(offset.x(), offset.y());
  return result;
}

gfx::Point PointWithOffset(int x, int y, gfx::Point offset) {
  gfx::Point result{x, y};
  result.Offset(offset.x(), offset.y());
  return result;
}

class FakeSystemInputInjector : public ui::SystemInputInjector {
 public:
  FakeSystemInputInjector() = default;
  FakeSystemInputInjector(const FakeSystemInputInjector&) = delete;
  FakeSystemInputInjector& operator=(const FakeSystemInputInjector&) = delete;
  ~FakeSystemInputInjector() override = default;

  // `SystemInputInjector` implementation:
  void SetDeviceId(int device_id) override {
    device_id_future_.SetValue(device_id);
  }
  void MoveCursorTo(const gfx::PointF& location) override {
    cursor_moves_.SetValue(location);
  }
  void InjectMouseButton(ui::EventFlags button, bool down) override {
    mouse_button_event_.SetValue(button);
  }
  void InjectMouseWheel(int delta_x, int delta_y) override {}
  void InjectKeyEvent(ui::DomCode physical_key,
                      bool down,
                      bool suppress_auto_repeat) override {}

  int WaitForDeviceId() { return device_id_future_.Get(); }

  gfx::PointF NextCursorMove() { return cursor_moves_.Take(); }

  ui::EventFlags WaitForMouseButtonEvent() {
    return mouse_button_event_.Take();
  }

 private:
  base::test::TestFuture<int> device_id_future_;
  base::test::TestFuture<gfx::PointF> cursor_moves_;
  base::test::TestFuture<ui::EventFlags> mouse_button_event_;
};

}  // namespace

class InputInjectorChromeosTest : public ash::AshTestBase {
 public:
  InputInjectorChromeosTest() = default;
  InputInjectorChromeosTest(const InputInjectorChromeosTest&) = delete;
  InputInjectorChromeosTest& operator=(const InputInjectorChromeosTest&) =
      delete;
  ~InputInjectorChromeosTest() override = default;

  void SetUp() override {
    // Unset the resource bundle set by our own test suite...
    ui::ResourceBundle::CleanupSharedInstance();
    // ... since Ash requires that we load their resource bundle.
    ash::AshTestSuite::LoadTestResources();
    ash::AshTestBase::SetUp();

    input_injector_ = std::make_unique<InputInjectorChromeos>(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    auto system_input_unique_ptr = std::make_unique<FakeSystemInputInjector>();
    delegate_ = system_input_unique_ptr.get();
    input_injector_->StartForTesting(
        std::move(system_input_unique_ptr),
        std::make_unique<protocol::MockClipboardStub>());
  }

  void TearDown() override {
    delegate_ = nullptr;
    input_injector_.reset();
    ash::AshTestBase::TearDown();
  }

  void CreateSingleDisplay(const std::string& display_specs) {
    display::test::DisplayManagerTestApi(&display_manager())
        .UpdateDisplay(display_specs);
  }

  void CreateMultipleDisplays(const std::string& first_display_specs,
                              const std::string& second_display_specs) {
    display::test::DisplayManagerTestApi(&display_manager())
        .UpdateDisplay(first_display_specs + "," + second_display_specs);
  }

  void InjectMouseMoveEvent(int x, int y) {
    input_injector().InjectMouseEvent(MakeMouseMoveEvent(x, y));
  }

  void InjectMouseEvent(const protocol::MouseEvent& event) {
    input_injector().InjectMouseEvent(event);
  }

  void EnableUnifiedDesktop() {
    display_manager().SetUnifiedDesktopEnabled(true);
    EXPECT_EQ(display_manager().GetNumDisplays(), 1ull);
    EXPECT_EQ(ash::Shell::Get()->GetAllRootWindows().size(), 1ull);
  }

  // Even if a display has origin (0,0) in our DPI screen coordinates,
  // its origin is likely not (0,0) in the Pixel screen coordinates.
  gfx::Point get_origin_in_pixel_coordinates(DisplayIndex display) {
    return ash::Shell::GetRootWindowForDisplayId(
               display_manager().GetDisplayAt(display).id())
        ->GetHost()
        ->GetBoundsInPixels()
        .origin();
  }

  gfx::Point get_origin_in_screen_coordinates(DisplayIndex display) {
    return display_manager().GetDisplayAt(display).bounds().origin();
  }

  // Convert the given relative location (in screen coordinates) to the
  // absolute location (still in screen coordinates).
  // In practice this means we have to add in the origin of the display.
  gfx::Point PointInScreenIn(DisplayIndex display, int x, int y) {
    return PointWithOffset(x, y, get_origin_in_pixel_coordinates(display));
  }

  // Convert the given relative location (in pixel coordinates) to the
  // absolute location (still in pixel coordinates).
  // In practice this means we have to add in the origin of the display.
  gfx::PointF PointFInPixelIn(DisplayIndex display, int x, int y) {
    return PointFWithOffset(x, y, get_origin_in_pixel_coordinates(display));
  }

  display::DisplayManager& display_manager() {
    return CHECK_DEREF(ash::Shell::Get()->display_manager());
  }

  FakeSystemInputInjector& delegate() { return CHECK_DEREF(delegate_.get()); }

  InputInjectorChromeos& input_injector() {
    return CHECK_DEREF(input_injector_.get());
  }

 private:
  raw_ptr<FakeSystemInputInjector> delegate_;
  std::unique_ptr<InputInjectorChromeos> input_injector_;
};

TEST_F(InputInjectorChromeosTest, ShouldUseRemoteInputDeviceId) {
  EXPECT_EQ(delegate().WaitForDeviceId(), ui::ED_REMOTE_INPUT_DEVICE);
}

TEST_F(InputInjectorChromeosTest, ShouldForwardMouseEvents) {
  CreateSingleDisplay("2000x1000");

  InjectMouseMoveEvent(100, 200);

  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 100, 200));
}

TEST_F(InputInjectorChromeosTest, ShouldForwardMouseEventsOnSecondScreen) {
  CreateMultipleDisplays("750x250", "2000x1000");

  InjectMouseMoveEvent(750 + 500, 200);

  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 500, 200));
}

TEST_F(InputInjectorChromeosTest, ShouldForwardMouseEventsOnScreenEdges) {
  CreateMultipleDisplays("1080x720", "2000x1000");

  InjectMouseMoveEvent(0, 0);
  EXPECT_EQ(delegate().NextCursorMove(), PointFInPixelIn(kFirstDisplay, 0, 0));

  InjectMouseMoveEvent(1079, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 1079, 0));

  InjectMouseMoveEvent(0, 719);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 0, 719));

  InjectMouseMoveEvent(1079, 719);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 1079, 719));
}

TEST_F(InputInjectorChromeosTest,
       ShouldForwardMouseEventsOnScreenEdgesOfSecondaryDisplay) {
  CreateMultipleDisplays("1000x500", "1920x1080");

  const int left_display_width = 1000;

  InjectMouseMoveEvent(left_display_width + 0, 0);
  EXPECT_EQ(delegate().NextCursorMove(),  //
            PointFInPixelIn(kSecondDisplay, 0, 0));

  InjectMouseMoveEvent(left_display_width + 1919, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 1919, 0));

  InjectMouseMoveEvent(left_display_width + 0, 1079);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 0, 1079));

  InjectMouseMoveEvent(left_display_width + 1919, 1079);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 1919, 1079));
}

TEST_F(InputInjectorChromeosTest, ShouldUseCorrectPositionForMouseClick) {
  CreateSingleDisplay("2000x1000");

  InjectMouseEvent(MakeLeftClickEvent(100, 200));

  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 100, 200));

  EXPECT_EQ(delegate().WaitForMouseButtonEvent(), ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(InputInjectorChromeosTest, ShouldSupportLeftRotation) {
  // `/l` rotates 90 degrees to the left.
  CreateSingleDisplay("2000x1000/l");

  // The input display has dimensions 1000x2000 after the rotation.
  // On the other hand, the output coordinates are in pixel coordinates so
  // they do *not* take rotation into account, meaning the output still
  // needs to use the 2000x1000 resolution.

  InjectMouseMoveEvent(0, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 0, 1000));

  InjectMouseMoveEvent(1000, 0);
  EXPECT_EQ(delegate().NextCursorMove(), PointFInPixelIn(kFirstDisplay, 0, 0));

  InjectMouseMoveEvent(0, 2000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 1000));

  InjectMouseMoveEvent(1000, 2000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 0));
}

TEST_F(InputInjectorChromeosTest, ShouldSupportRightRotation) {
  // `/r` rotates 90 degrees to the right.
  CreateSingleDisplay("2000x1000/r");

  // The input display has dimensions 1000x2000 after the rotation.
  // On the other hand, the output coordinates are in pixel coordinates so
  // they do *not* take rotation into account, meaning the output still
  // needs to use the 2000x1000 resolution.

  InjectMouseMoveEvent(0, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 0));

  InjectMouseMoveEvent(1000, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 1000));

  InjectMouseMoveEvent(0, 2000);
  EXPECT_EQ(delegate().NextCursorMove(), PointFInPixelIn(kFirstDisplay, 0, 0));

  InjectMouseMoveEvent(1000, 2000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 0, 1000));
}

TEST_F(InputInjectorChromeosTest, ShouldSupportUpsideDownRotation) {
  // `/u` rotates 180 degrees.
  CreateSingleDisplay("2000x1000/u");

  // The input display has dimensions 2000x1000 after the rotation.

  InjectMouseMoveEvent(0, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 1000));

  InjectMouseMoveEvent(2000, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 0, 1000));

  InjectMouseMoveEvent(0, 1000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 2000, 0));

  InjectMouseMoveEvent(2000, 1000);
  EXPECT_EQ(delegate().NextCursorMove(), PointFInPixelIn(kFirstDisplay, 0, 0));
}

TEST_F(InputInjectorChromeosTest,
       ShouldSupportDisplayRotationOnSecondaryScreen) {
  // `/l` rotates 90 degrees to the left.
  CreateMultipleDisplays("5000x500", "2000x1000/l");

  constexpr int left_display_width = 5000;

  // The input display has dimensions 1000x2000 after the rotation.

  InjectMouseMoveEvent(left_display_width + 0, 0);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 0, 1000));

  InjectMouseMoveEvent(left_display_width + 1000, 0);
  EXPECT_EQ(delegate().NextCursorMove(), PointFInPixelIn(kSecondDisplay, 0, 0));

  InjectMouseMoveEvent(left_display_width + 0, 2000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 2000, 1000));

  InjectMouseMoveEvent(left_display_width + 1000, 2000);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, 2000, 0));
}

TEST_F(InputInjectorChromeosTest, ShouldSupportScaleFactor) {
  // `@3` adds a scale factor of 3.
  CreateSingleDisplay("3000x1500@3");

  // The input display has dimensions 1000x500 after applying the scale factor.

  InjectMouseMoveEvent(300, 40);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kFirstDisplay, 300 * 3, 40 * 3));
}

TEST_F(InputInjectorChromeosTest, ShouldSupportScaleFactorWithRotation) {
  CreateMultipleDisplays("3000x1500/l@3", "4000x2000/r@2");

  // The first input display has dimensions 500x1000 after applying the scale
  // factor and rotation.
  // The second input display has dimensions 1000x2000 after applying the scale
  // factor and rotation.

  constexpr int left_display_width = 500;

  InjectMouseMoveEvent(left_display_width + 60, 123);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFInPixelIn(kSecondDisplay, (2000 - 123) * 2, 60 * 2));
}

TEST_F(InputInjectorChromeosTest, ShouldSupportUnifiedDesktop) {
  CreateMultipleDisplays("3000x1500", "3000x1500");

  // Collect display offsets now because the APIs we use lose access to the
  // real displays once unified desktop is enabled.
  gfx::Point first_display_pixel_offset =
      get_origin_in_pixel_coordinates(kFirstDisplay);
  gfx::Point second_display_pixel_offset =
      get_origin_in_pixel_coordinates(kSecondDisplay);

  EnableUnifiedDesktop();

  InjectMouseMoveEvent(48, 127);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFWithOffset(48, 127, first_display_pixel_offset));

  InjectMouseMoveEvent(3000 + 48, 127);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFWithOffset(48, 127, second_display_pixel_offset));
}

TEST_F(InputInjectorChromeosTest,
       ShouldSupportUnifiedDesktopWithSingleDisplay) {
  CreateSingleDisplay("3000x1500");

  // Collect display offsets now because the APIs we use lose access to the
  // real displays once unified desktop is enabled.
  gfx::Point display_pixel_offset =
      get_origin_in_pixel_coordinates(kFirstDisplay);

  EnableUnifiedDesktop();

  InjectMouseMoveEvent(48, 127);
  EXPECT_EQ(delegate().NextCursorMove(),
            PointFWithOffset(48, 127, display_pixel_offset));
}

}  // namespace remoting
