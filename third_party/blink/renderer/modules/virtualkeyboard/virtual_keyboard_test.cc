// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/virtualkeyboard/virtual_keyboard.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class VirtualKeyboardTest : public testing::Test {
 protected:
  VirtualKeyboardTest()
      : holder_(DummyPageHolder::CreateAndCommitNavigation(
            KURL("https://example.com"),
            gfx::Size(411, 777))) {}

  VirtualKeyboard& GetVirtualKeyboard() {
    return *VirtualKeyboard::virtualKeyboard(
        *holder_->GetFrame().DomWindow()->navigator());
  }

  int ViewportWidth() const {
    return holder_->GetFrame().DomWindow()->innerWidth();
  }
  int ViewportHeight() const {
    return holder_->GetFrame().DomWindow()->innerHeight();
  }

  String EnvValue(UADefinedVariable variable) {
    DocumentStyleEnvironmentVariables& vars =
        holder_->GetDocument().GetStyleEngine().EnsureEnvironmentVariables();
    CSSVariableData* data = vars.ResolveVariable(
        StyleEnvironmentVariables::GetVariableName(variable, nullptr), {});
    EXPECT_TRUE(data);
    return data ? data->Serialize() : String();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> holder_;
};

TEST_F(VirtualKeyboardTest,
       KeyboardInsetEnvironmentVariablesUseViewportInsets) {
  const int viewport_width = ViewportWidth();
  const int viewport_height = ViewportHeight();
  ASSERT_GT(viewport_width, 0);
  ASSERT_GT(viewport_height, 0);

  const int keyboard_height = 343;
  const int keyboard_top = viewport_height - keyboard_height;
  ASSERT_GT(keyboard_top, 0);

  VirtualKeyboard& keyboard = GetVirtualKeyboard();
  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(
      gfx::Rect(0, keyboard_top, viewport_width, keyboard_height));

  EXPECT_EQ(0, keyboard.boundingRect()->x());
  EXPECT_EQ(keyboard_top, keyboard.boundingRect()->y());
  EXPECT_EQ(viewport_width, keyboard.boundingRect()->width());
  EXPECT_EQ(keyboard_height, keyboard.boundingRect()->height());
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_top),
            EnvValue(UADefinedVariable::kKeyboardInsetTop));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetLeft));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetBottom));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetRight));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(viewport_width),
            EnvValue(UADefinedVariable::kKeyboardInsetWidth));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_height),
            EnvValue(UADefinedVariable::kKeyboardInsetHeight));
}

TEST_F(VirtualKeyboardTest,
       KeyboardInsetEnvironmentVariablesTreatAnyNonEmptyRectAsVisible) {
  const int viewport_width = ViewportWidth();
  const int viewport_height = ViewportHeight();
  ASSERT_GT(viewport_width, 8);
  ASSERT_GT(viewport_height, 12);

  const gfx::Rect keyboard_rect(7, 11, 1, 1);
  VirtualKeyboard& keyboard = GetVirtualKeyboard();
  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(keyboard_rect);

  EXPECT_EQ(keyboard_rect.x(), keyboard.boundingRect()->x());
  EXPECT_EQ(keyboard_rect.y(), keyboard.boundingRect()->y());
  EXPECT_EQ(keyboard_rect.width(), keyboard.boundingRect()->width());
  EXPECT_EQ(keyboard_rect.height(), keyboard.boundingRect()->height());
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_rect.y()),
            EnvValue(UADefinedVariable::kKeyboardInsetTop));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_rect.x()),
            EnvValue(UADefinedVariable::kKeyboardInsetLeft));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(viewport_height -
                                                keyboard_rect.bottom()),
            EnvValue(UADefinedVariable::kKeyboardInsetBottom));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(viewport_width -
                                                keyboard_rect.right()),
            EnvValue(UADefinedVariable::kKeyboardInsetRight));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_rect.width()),
            EnvValue(UADefinedVariable::kKeyboardInsetWidth));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(keyboard_rect.height()),
            EnvValue(UADefinedVariable::kKeyboardInsetHeight));
}

TEST_F(VirtualKeyboardTest,
       KeyboardInsetEnvironmentVariablesAreZeroWhenKeyboardHidden) {
  const int viewport_width = ViewportWidth();
  ASSERT_GT(viewport_width, 0);

  VirtualKeyboard& keyboard = GetVirtualKeyboard();
  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(
      gfx::Rect(0, 10, viewport_width, 100));
  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(
      gfx::Rect(10, 20, 100, 0));

  EXPECT_EQ(0, keyboard.boundingRect()->x());
  EXPECT_EQ(0, keyboard.boundingRect()->y());
  EXPECT_EQ(0, keyboard.boundingRect()->width());
  EXPECT_EQ(0, keyboard.boundingRect()->height());
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetTop));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetLeft));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetBottom));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetRight));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetWidth));
  EXPECT_EQ(StyleEnvironmentVariables::FormatPx(0),
            EnvValue(UADefinedVariable::kKeyboardInsetHeight));
}

TEST_F(VirtualKeyboardTest, GeometryUpdateDoesNotForceLifecycleUpdate) {
  holder_->GetPage().GetSettings().SetViewportEnabled(true);
  holder_->GetDocument().documentElement()->setAttribute(
      html_names::kStyleAttr, AtomicString("color: pink"));
  ASSERT_TRUE(holder_->GetDocument().NeedsLayoutTreeUpdate());

  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(
      gfx::Rect(0, 500, 411, 277));

  EXPECT_TRUE(holder_->GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(VirtualKeyboardTest, GeometryIsAvailableWhenApiIsCreatedAfterUpdate) {
  const gfx::Rect keyboard_rect(10, 400, 380, 300);
  holder_->GetFrame().SetVirtualKeyboardOverlayGeometry(keyboard_rect);

  DOMRect* bounding_rect = GetVirtualKeyboard().boundingRect();
  ASSERT_TRUE(bounding_rect);
  EXPECT_EQ(keyboard_rect.x(), bounding_rect->x());
  EXPECT_EQ(keyboard_rect.y(), bounding_rect->y());
  EXPECT_EQ(keyboard_rect.width(), bounding_rect->width());
  EXPECT_EQ(keyboard_rect.height(), bounding_rect->height());
}

}  // namespace blink
