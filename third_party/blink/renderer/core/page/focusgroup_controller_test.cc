// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

using utils = FocusgroupControllerUtils;
using NoCellFoundAtIndexBehavior =
    GridFocusgroupStructureInfo::NoCellFoundAtIndexBehavior;

class FocusgroupControllerTest : public PageTestBase {
 public:
  KeyboardEvent* KeyDownEvent(
      int dom_key,
      Element* target = nullptr,
      WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers) {
    WebKeyboardEvent web_event = {WebInputEvent::Type::kRawKeyDown, modifiers,
                                  WebInputEvent::GetStaticTimeStampForTests()};
    web_event.dom_key = dom_key;
    auto* event = KeyboardEvent::Create(web_event, nullptr);
    if (target)
      event->SetTarget(target);

    return event;
  }

  void SendEvent(KeyboardEvent* event) {
    GetDocument().GetFrame()->GetEventHandler().DefaultKeyboardEventHandler(
        event);
  }

 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  ScopedFocusgroupForTest focusgroup_enabled{true};
};

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventValid) {
  // Arrow right should be forward and inline.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardInline);

  // Arrow down should be forward and block.
  event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardBlock);

  // Arrow left should be backward and inline.
  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardInline);

  // Arrow up should be backward and block.
  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardBlock);

  // When the shift key is pressed, even when combined with a valid arrow key,
  // it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kShiftKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the ctrl key is pressed, even when combined with a valid arrow key, it
  // should return kNone.
  event =
      KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kControlKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the meta key (e.g.: CMD on mac) is pressed, even when combined with a
  // valid arrow key, it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kMetaKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // Any other key than an arrow key should return kNone.
  event = KeyDownEvent(ui::DomKey::TAB);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);
}

TEST_F(FocusgroupControllerTest, IsDirectionBackward) {
  ASSERT_FALSE(utils::IsDirectionBackward(FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsDirectionBackward(FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::IsDirectionBackward(FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::IsDirectionBackward(FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::IsDirectionBackward(FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, IsDirectionForward) {
  ASSERT_FALSE(utils::IsDirectionForward(FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::IsDirectionForward(FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::IsDirectionForward(FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::IsDirectionForward(FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::IsDirectionForward(FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, IsDirectionInline) {
  ASSERT_FALSE(utils::IsDirectionInline(FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsDirectionInline(FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::IsDirectionInline(FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::IsDirectionInline(FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::IsDirectionInline(FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, IsDirectionBlock) {
  ASSERT_FALSE(utils::IsDirectionBlock(FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::IsDirectionBlock(FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::IsDirectionBlock(FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::IsDirectionBlock(FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::IsDirectionBlock(FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, IsAxisSupported) {
  FocusgroupFlags flags_inline_only = FocusgroupFlags::kInline;
  ASSERT_FALSE(
      utils::IsAxisSupported(flags_inline_only, FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsAxisSupported(flags_inline_only,
                                     FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::IsAxisSupported(flags_inline_only,
                                      FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::IsAxisSupported(flags_inline_only,
                                     FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::IsAxisSupported(flags_inline_only,
                                      FocusgroupDirection::kForwardBlock));

  FocusgroupFlags flags_block_only = FocusgroupFlags::kBlock;
  ASSERT_FALSE(
      utils::IsAxisSupported(flags_block_only, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::IsAxisSupported(flags_block_only,
                                      FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::IsAxisSupported(flags_block_only,
                                     FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::IsAxisSupported(flags_block_only,
                                      FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::IsAxisSupported(flags_block_only,
                                     FocusgroupDirection::kForwardBlock));

  FocusgroupFlags flags_both_directions =
      FocusgroupFlags::kInline | FocusgroupFlags::kBlock;
  ASSERT_FALSE(utils::IsAxisSupported(flags_both_directions,
                                      FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, WrapsInDirection) {
  FocusgroupFlags flags_no_wrap = FocusgroupFlags::kNone;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_no_wrap, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kForwardBlock));

  FocusgroupFlags flags_wrap_inline = FocusgroupFlags::kWrapInline;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_wrap_inline, FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_inline,
                                      FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_inline,
                                       FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_inline,
                                      FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_inline,
                                       FocusgroupDirection::kForwardBlock));

  FocusgroupFlags flags_wrap_block = FocusgroupFlags::kWrapBlock;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_wrap_block, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_block,
                                       FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_block,
                                      FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_block,
                                       FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_block,
                                      FocusgroupDirection::kForwardBlock));

  FocusgroupFlags flags_wrap_both =
      FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_wrap_both, FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, FocusgroupExtendsInAxis) {
  FocusgroupFlags focusgroup = FocusgroupFlags::kNone;
  FocusgroupFlags extending_focusgroup = FocusgroupFlags::kNone;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  focusgroup |= FocusgroupFlags::kInline | FocusgroupFlags::kBlock;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  extending_focusgroup |= FocusgroupFlags::kInline | FocusgroupFlags::kBlock;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  extending_focusgroup = FocusgroupFlags::kExtend;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  extending_focusgroup |= FocusgroupFlags::kInline;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  extending_focusgroup |= FocusgroupFlags::kBlock;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  focusgroup = FocusgroupFlags::kNone;
  extending_focusgroup = FocusgroupFlags::kExtend | FocusgroupFlags::kInline |
                         FocusgroupFlags::kBlock;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  focusgroup |= FocusgroupFlags::kBlock;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));

  focusgroup |= FocusgroupFlags::kInline;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kBackwardBlock));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardInline));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardBlock));
}

TEST_F(FocusgroupControllerTest, FindNearestFocusgroupAncestor) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div>
      <span id=item1 tabindex=0></span>
    </div>
    <div id=fg1 focusgroup>
      <span id=item2 tabindex=-1></span>
      <div>
        <div id=fg2 focusgroup=extend>
          <span id=item3 tabindex=-1></span>
          <div>
            <span id=item4></span>
          </div>
          <table id=fg3 focusgroup=grid>
            <tr>
              <td id=item5 tabindex=-1>
                <!-- The following is an error. -->
                <div id=fg4 focusgroup=grid>
                  <span id=item6 tabindex=-1></span>
                  <div id=fg5 focusgroup>
                    <span id=item7 tabindex=-1></span>
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div id=fg6-container>
            <template shadowrootmode=open>
              <div id=fg6 focusgroup=extend>
                <span id=item8 tabindex=-1></span>
              </div>
            </template>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg6_container = GetElementById("fg6-container");
  ASSERT_TRUE(fg6_container);

  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  auto* item5 = GetElementById("item5");
  auto* item6 = GetElementById("item6");
  auto* item7 = GetElementById("item7");
  auto* item8 =
      fg6_container->GetShadowRoot()->getElementById(AtomicString("item8"));
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* fg3 = GetElementById("fg3");
  auto* fg4 = GetElementById("fg4");
  auto* fg5 = GetElementById("fg5");
  auto* fg6 =
      fg6_container->GetShadowRoot()->getElementById(AtomicString("fg6"));
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);
  ASSERT_TRUE(item6);
  ASSERT_TRUE(item7);
  ASSERT_TRUE(item8);
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(fg3);
  ASSERT_TRUE(fg4);
  ASSERT_TRUE(fg5);
  ASSERT_TRUE(fg6);

  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item1, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item1, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item2, FocusgroupType::kLinear),
      fg1);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item2, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item3, FocusgroupType::kLinear),
      fg2);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item3, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item4, FocusgroupType::kLinear),
      fg2);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item4, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item5, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item5, FocusgroupType::kGrid),
            fg3);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item6, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item6, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item7, FocusgroupType::kLinear),
      fg5);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item7, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item8, FocusgroupType::kLinear),
      fg6);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item8, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(fg6, FocusgroupType::kLinear),
            fg2);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(fg6, FocusgroupType::kGrid),
            nullptr);
}

TEST_F(FocusgroupControllerTest, NextElement) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
      <span id=item3 tabindex=-1></span>
    </div>
    <div id=fg3 focusgroup>
        <template shadowrootmode=open>
          <span id=item4 tabindex=-1></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* fg3 = GetElementById("fg3");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(fg3);

  auto* item1 = GetElementById("item1");
  auto* item4 = fg3->GetShadowRoot()->getElementById(AtomicString("item4"));
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);

  ASSERT_EQ(utils::NextElement(fg1, /* skip_subtree */ false), item1);
  ASSERT_EQ(utils::NextElement(fg1, /* skip_subtree */ true), fg2);
  ASSERT_EQ(utils::NextElement(fg3, /* skip_subtree */ false), item4);
  ASSERT_EQ(utils::NextElement(item4, /* skip_subtree */ false), item5);
}

TEST_F(FocusgroupControllerTest, PreviousElement) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
      <span id=item3 tabindex=-1></span>
    </div>
    <div id=fg3 focusgroup>
        <template shadowrootmode=open>
          <span id=item4 tabindex=-1></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg3 = GetElementById("fg3");
  ASSERT_TRUE(fg3);

  auto* item3 = GetElementById("item3");
  auto* item4 = fg3->GetShadowRoot()->getElementById(AtomicString("item4"));
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);

  ASSERT_EQ(utils::PreviousElement(item5), item4);
  ASSERT_EQ(utils::PreviousElement(item4), fg3);
  ASSERT_EQ(utils::PreviousElement(fg3), item3);
}

TEST_F(FocusgroupControllerTest, LastElementWithin) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
        <template shadowrootmode=open>
          <span id=item3 tabindex=-1></span>
          <span id=item4></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);

  auto* item2 = GetElementById("item2");
  auto* item4 = fg2->GetShadowRoot()->getElementById(AtomicString("item4"));
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item4);

  ASSERT_EQ(utils::LastElementWithin(fg1), item2);
  ASSERT_EQ(utils::LastElementWithin(fg2), item4);
  ASSERT_EQ(utils::LastElementWithin(item4), nullptr);
}

TEST_F(FocusgroupControllerTest, IsFocusgroupItem) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2></span>
      <div id=fg2 focusgroup=extend>
        <span tabindex=-1></span>
        <div id=non-fg1 tabindex=-1>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <button id=button1></button>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* non_fg1 = GetElementById("non-fg1");
  auto* button1 = GetElementById("button1");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(non_fg1);
  ASSERT_TRUE(button1);

  ASSERT_TRUE(utils::IsFocusgroupItem(item1));
  ASSERT_FALSE(utils::IsFocusgroupItem(item2));
  ASSERT_FALSE(utils::IsFocusgroupItem(item3));
  ASSERT_FALSE(utils::IsFocusgroupItem(fg1));
  ASSERT_FALSE(utils::IsFocusgroupItem(fg2));
  ASSERT_TRUE(utils::IsFocusgroupItem(non_fg1));
  ASSERT_TRUE(utils::IsFocusgroupItem(button1));
}

TEST_F(FocusgroupControllerTest, CellAtIndexInRowBehaviorOnNoCellFound) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <table id=table focusgroup=grid>
      <tr>
        <td id=r1c1></td>
        <td id=r1c2></td>
        <td id=r1c3 rowspan=2></td>
      </tr>
      <tr id=row2>
        <td id=r2c1></td>
        <!-- r2c2 doesn't exist, but r2c3 exists because of the rowspan on the
             previous row. -->
      </tr>
      <tr>
        <td id=r3c1></td>
        <td id=r3c2></td>
        <td id=r3c3></td>
      </tr>
    </table>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* table = GetElementById("table");
  auto* row2 = GetElementById("row2");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c1 = GetElementById("r2c1");
  auto* r3c2 = GetElementById("r3c2");
  ASSERT_TRUE(table);
  ASSERT_TRUE(row2);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r3c2);

  ASSERT_TRUE(table->GetFocusgroupFlags() & FocusgroupFlags::kGrid);
  auto* helper = utils::CreateGridFocusgroupStructureInfoForGridRoot(table);

  // The first column starts at index 0.
  unsigned no_cell_index = 1;

  EXPECT_EQ(helper->CellAtIndexInRow(no_cell_index, row2,
                                     NoCellFoundAtIndexBehavior::kReturn),
            nullptr);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindPreviousCellInRow),
            r2c1);
  EXPECT_EQ(
      helper->CellAtIndexInRow(no_cell_index, row2,
                               NoCellFoundAtIndexBehavior::kFindNextCellInRow),
      r1c3);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindPreviousCellInColumn),
            r1c2);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindNextCellInColumn),
            r3c2);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenNoFocusedElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  ASSERT_EQ(GetDocument().FocusedElement(), nullptr);

  // Since there are no focused element, the arrow down event shouldn't move the
  // focus.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), nullptr);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenModifierKeyIsSet) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  // 1. Set the focus on an item of the focusgroup.
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // 2. Send an "ArrowDown" event from that element.
  auto* event =
      KeyDownEvent(ui::DomKey::ARROW_DOWN, item1, WebInputEvent::kShiftKey);
  SendEvent(event);

  // 3. The focus shouldn't have moved because of the shift key.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenItAlreadyMoved) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  // 1. Set the focus on an item of the focusgroup.
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item2);
  item2->Focus();

  // 2. Create the "ArrowDown" event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, item2);

  // 3. Move the focus to a different element before we send the event.
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // 4. Pass the event we created earlier to our FocusgroupController. The
  // controller shouldn't even try to move the focus since the focus isn't on
  // the element that triggered the arrow key press event.
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

}  // namespace blink
