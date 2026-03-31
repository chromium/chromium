// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/color_chooser_popup_ui_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class TestPagePopup final : public PagePopup {
 public:
  TestPagePopup() = default;
  ~TestPagePopup() override = default;

  AXObject* RootAXObject(Element* popup_owner) override { return nullptr; }
  void SetWindowRect(const gfx::Rect&) override {}
  void PostMessageToPopup(const String& message) override {
    last_message_ = message;
  }
  void Update() override {}

  const String& last_message() const { return last_message_; }

 private:
  String last_message_;
};

class TestChromeClient final : public EmptyChromeClient {
 public:
  TestChromeClient() = default;
  ~TestChromeClient() override = default;

  PagePopup* OpenPagePopup(PagePopupClient*) override {
    popup_opened_ = true;
    return &popup_;
  }

  bool popup_opened() const { return popup_opened_; }
  const String& popup_message() const { return popup_.last_message(); }

 private:
  TestPagePopup popup_;
  bool popup_opened_ = false;
};

class FakeColorChooserClient final
    : public GarbageCollected<FakeColorChooserClient>,
      public ColorChooserClient {
 public:
  explicit FakeColorChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeColorChooserClient() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(owner_element_);
    ColorChooserClient::Trace(visitor);
  }

  void DidChooseColor(const Color&) override {}
  void DidEndChooser() override {}
  Element& OwnerElement() const override { return *owner_element_; }
  gfx::Rect ElementRectRelativeToLocalRoot() const override {
    return gfx::Rect();
  }
  Color CurrentColor() override { return Color(); }
  bool ShouldShowSuggestions() const override { return false; }
  Vector<mojom::blink::ColorSuggestionPtr> Suggestions() const override {
    return {};
  }

 private:
  Member<Element> owner_element_;
};

class ColorChooserPopupUIControllerTest : public PageTestBase {
 protected:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<TestChromeClient>();
    SetupPageWithClients(chrome_client_);
    SetHtmlInnerHTML("<input id='color' type='color'>");
    Element* owner = GetElementById("color");
    ASSERT_TRUE(owner);
    client_ = MakeGarbageCollected<FakeColorChooserClient>(owner);
    controller_ = MakeGarbageCollected<ColorChooserPopupUIController>(
        &GetFrame(), chrome_client_.Get(), client_.Get());
  }

  Persistent<TestChromeClient> chrome_client_;
  Persistent<FakeColorChooserClient> client_;
  Persistent<ColorChooserPopupUIController> controller_;
};

TEST_F(ColorChooserPopupUIControllerTest,
       EyeDropperResponseColorIsSerializedAsOpaque) {
  controller_->OpenUI();
  ASSERT_TRUE(chrome_client_->popup_opened());

  // 0x00ffffff has alpha=0. EyeDropperResponseHandler() calls MakeOpaque()
  // before serializing, so the popup should receive an opaque rgb() color.
  constexpr uint32_t kFullyTransparentWhite = 0x00ffffff;
  const String expected_popup_message =
      "window.updateData = {\n"
      "success: true,\n"
      "color: \"rgb(255, 255, 255)\",\n"
      "}\n";

  controller_->EyeDropperResponseHandler(/*success=*/true,
                                         kFullyTransparentWhite);

  EXPECT_EQ(chrome_client_->popup_message(), expected_popup_message);
  controller_->DidClosePopup();
}

}  // namespace blink
