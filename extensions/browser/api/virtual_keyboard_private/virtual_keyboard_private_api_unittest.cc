// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <memory>

#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {

namespace {

class MockVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  MockVirtualKeyboardDelegate() {}
  ~MockVirtualKeyboardDelegate() override = default;

  // VirtualKeyboardDelegate impl:
  void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) override {}
  void OnKeyboardConfigChanged() override {}
  bool HideKeyboard() override { return false; }
  bool InsertText(const base::string16& text) override { return false; }
  bool OnKeyboardLoaded() override { return false; }
  void SetHotrodKeyboard(bool enable) override {}
  bool LockKeyboard(bool state) override { return false; }
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override {
    return false;
  }
  bool ShowLanguageSettings() override { return false; }
  bool IsLanguageSettingsEnabled() override { return false; }
  bool SetVirtualKeyboardMode(int mode_enum,
                              base::Optional<gfx::Rect> target_bounds,
                              OnSetModeCallback on_set_mode_callback) override {
    return false;
  }
  bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& rect) override {
    return false;
  }
  bool SetRequestedKeyboardState(int state_enum) override { return false; }

  bool SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override {
    occluded_bounds_ = bounds;
    return true;
  }
  const std::vector<gfx::Rect>& GetOccludedBounds() { return occluded_bounds_; }

  bool SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override {
    hit_test_bounds_ = bounds;
    return true;
  }
  const std::vector<gfx::Rect>& GetHitTestBounds() { return hit_test_bounds_; }

  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override {
    area_to_remain_on_screen_ = bounds;
    return true;
  }
  const gfx::Rect& GetAreaToRemainOnScreen() {
    return area_to_remain_on_screen_;
  }

  api::virtual_keyboard::FeatureRestrictions RestrictFeatures(
      const api::virtual_keyboard::RestrictFeatures::Params& params) override {
    return api::virtual_keyboard::FeatureRestrictions();
  }

 private:
  std::vector<gfx::Rect> occluded_bounds_;
  std::vector<gfx::Rect> hit_test_bounds_;
  gfx::Rect area_to_remain_on_screen_;

  DISALLOW_COPY_AND_ASSIGN(MockVirtualKeyboardDelegate);
};

class TestVirtualKeyboardExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  TestVirtualKeyboardExtensionsAPIClient() {}
  ~TestVirtualKeyboardExtensionsAPIClient() override {}

  // ExtensionsAPIClient implementation.
  std::unique_ptr<VirtualKeyboardDelegate> CreateVirtualKeyboardDelegate(
      content::BrowserContext* browser_context) const override {
    auto delegate = std::make_unique<MockVirtualKeyboardDelegate>();
    delegates_[browser_context] = delegate.get();
    return std::move(delegate);
  }

  MockVirtualKeyboardDelegate* GetDelegateForBrowserContext(
      content::BrowserContext* browser_context) const {
    return delegates_[browser_context];
  }

 private:
  // Points to the last mock delegate created for each browser context. Does not
  // own the delegates.
  mutable std::map<content::BrowserContext*, MockVirtualKeyboardDelegate*>
      delegates_;

  DISALLOW_COPY_AND_ASSIGN(TestVirtualKeyboardExtensionsAPIClient);
};

}  // namespace

class VirtualKeyboardPrivateApiUnittest : public ApiUnitTest {
 public:
  VirtualKeyboardPrivateApiUnittest() {}
  ~VirtualKeyboardPrivateApiUnittest() override {}

  const TestVirtualKeyboardExtensionsAPIClient& client() const {
    return extensions_api_client_;
  }

 protected:
  TestVirtualKeyboardExtensionsAPIClient extensions_api_client_;
};

TEST_F(VirtualKeyboardPrivateApiUnittest, SetOccludedBoundsWithNoBounds) {
  RunFunction(new VirtualKeyboardPrivateSetOccludedBoundsFunction(), "[[]]");

  const auto bounds = client()
                          .GetDelegateForBrowserContext(browser_context())
                          ->GetOccludedBounds();
  EXPECT_EQ(0U, bounds.size());
}

TEST_F(VirtualKeyboardPrivateApiUnittest, SetOccludedBoundsWithOneBound) {
  RunFunction(new VirtualKeyboardPrivateSetOccludedBoundsFunction(),
              R"([[{ "left": 0, "top": 10, "width": 20, "height": 30 }]])");

  const auto bounds = client()
                          .GetDelegateForBrowserContext(browser_context())
                          ->GetOccludedBounds();
  ASSERT_EQ(1U, bounds.size());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 30), bounds[0]);
}

TEST_F(VirtualKeyboardPrivateApiUnittest, SetOccludedBoundsWithTwoBounds) {
  RunFunction(new VirtualKeyboardPrivateSetOccludedBoundsFunction(),
              R"([[{ "left": 0, "top": 10, "width": 20, "height": 30 },
      { "left": 10, "top": 20, "width": 30, "height": 40 }]])");

  const auto bounds = client()
                          .GetDelegateForBrowserContext(browser_context())
                          ->GetOccludedBounds();
  ASSERT_EQ(2U, bounds.size());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 30), bounds[0]);
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), bounds[1]);
}

TEST_F(VirtualKeyboardPrivateApiUnittest, SetHitTestBoundsWithNoBounds) {
  RunFunction(new VirtualKeyboardPrivateSetHitTestBoundsFunction(), "[[]]");

  const auto bounds = client()
                          .GetDelegateForBrowserContext(browser_context())
                          ->GetHitTestBounds();
  EXPECT_EQ(0U, bounds.size());
}

TEST_F(VirtualKeyboardPrivateApiUnittest, SetHitTestBoundsWithMultipleBounds) {
  RunFunction(new VirtualKeyboardPrivateSetHitTestBoundsFunction(),
              R"([[{ "left": 0, "top": 10, "width": 20, "height": 30 },
      { "left": 10, "top": 20, "width": 30, "height": 40 }]])");

  const auto bounds = client()
                          .GetDelegateForBrowserContext(browser_context())
                          ->GetHitTestBounds();
  ASSERT_EQ(2U, bounds.size());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 30), bounds[0]);
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), bounds[1]);
}

TEST_F(VirtualKeyboardPrivateApiUnittest, SetAreaToRemainOnScreenWithBounds) {
  RunFunction(new VirtualKeyboardPrivateSetAreaToRemainOnScreenFunction(),
              R"([{ "left": 0, "top": 0, "width": 10, "height": 20 }])");

  const gfx::Rect bounds = client()
                               .GetDelegateForBrowserContext(browser_context())
                               ->GetAreaToRemainOnScreen();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), bounds);
}

}  // namespace extensions
