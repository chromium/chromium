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

TEST_F(FocusgroupControllerTest,
       GridNavigationDisabledWithoutFocusgroupGridFlag) {
  // Explicitly disable FocusgroupGrid. Ensure arrow keys don't traverse a
  // grid when the feature is disabled.
  ScopedFocusgroupGridForTest grid_enabled{false};
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <table id=table focusgroup=grid>
      <tr>
        <td id=c1 tabindex=0>1</td>
        <td id=c2 tabindex=-1>2</td>
      </tr>
    </table>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* c1 = GetElementById("c1");
  auto* c2 = GetElementById("c2");
  ASSERT_TRUE(c1);
  ASSERT_TRUE(c2);
  c1->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), c1);

  // Send right arrow; with grid flag disabled, focus shouldn't move.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, c1);
  SendEvent(event);
  EXPECT_EQ(GetDocument().FocusedElement(), c1);
}

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

  FocusgroupFlags flags_both_directions = static_cast<FocusgroupFlags>(
      FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
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

  FocusgroupFlags flags_wrap_both = static_cast<FocusgroupFlags>(
      FocusgroupFlags::kWrapInline | FocusgroupFlags::kWrapBlock);
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

TEST_F(FocusgroupControllerTest, FindNearestFocusgroupAncestor) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div>
      <span id=item1 tabindex=0></span>
    </div>
    <div id=fg1 focusgroup="toolbar">
      <span id=item2 tabindex=-1></span>
      <div>
        <div id=fg2 focusgroup="toolbar">
          <span id=item3 tabindex=-1></span>
          <div>
            <span id=item4></span>
          </div>
          <table id=fg3 focusgroup="grid">
            <tr>
              <td id=item5 tabindex=-1>
                <!-- The following is an error. -->
                <div id=fg4 focusgroup="grid">
                  <span id=item6 tabindex=-1></span>
                  <div id=fg5 focusgroup="toolbar">
                    <span id=item7 tabindex=-1></span>
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div id=fg6-container>
            <template shadowrootmode=open>
              <div id=fg6 focusgroup="toolbar">
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
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
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
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
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

TEST_F(FocusgroupControllerTest, LastFocusgroupItemWithin) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup="toolbar">
        <template shadowrootmode=open>
          <span id=item3 tabindex=-1></span>
          <span id=item4 tabindex=-1></span>
          <span id=item5></span>
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

  EXPECT_EQ(utils::LastFocusgroupItemWithin(fg1), item2);
  EXPECT_EQ(utils::LastFocusgroupItemWithin(fg2), item4);
  EXPECT_EQ(utils::LastFocusgroupItemWithin(item4), nullptr);
}

TEST_F(FocusgroupControllerTest, FirstFocusgroupItemWithin) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup="toolbar">
        <template shadowrootmode=open>
          <span id=item3 tabindex=-1></span>
          <span id=item4 tabindex=-1></span>
          <span id=item5></span>
        </template>
    </div>
  )HTML");

  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);

  auto* item2 = GetElementById("item2");
  auto* item3 = fg2->GetShadowRoot()->getElementById(AtomicString("item3"));
  auto* item4 = fg2->GetShadowRoot()->getElementById(AtomicString("item4"));

  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);

  EXPECT_EQ(utils::FirstFocusgroupItemWithin(fg1), item2);
  EXPECT_EQ(utils::FirstFocusgroupItemWithin(fg2), item3);
  EXPECT_EQ(utils::FirstFocusgroupItemWithin(item4), nullptr);
}

TEST_F(FocusgroupControllerTest, IsFocusgroupItemWithOwner) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=outer_fg focusgroup="toolbar">
      <span id=outer_item1 tabindex=0></span>
      <div>
        <div id=inner_fg focusgroup="toolbar">
          <span id=inner_item1 tabindex=-1></span>
          <span id=inner_item2 tabindex=-1></span>
        </div>
      </div>
      <span id=outer_item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* outer_fg = GetElementById("outer_fg");
  auto* inner_fg = GetElementById("inner_fg");
  auto* outer_item1 = GetElementById("outer_item1");
  auto* outer_item2 = GetElementById("outer_item2");
  auto* inner_item1 = GetElementById("inner_item1");
  auto* inner_item2 = GetElementById("inner_item2");

  // Outer focusgroup items should belong to outer context.
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(outer_item1, outer_fg));
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(outer_item2, outer_fg));

  // Inner focusgroup items should NOT belong to outer context.
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(inner_item1, outer_fg));
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(inner_item2, outer_fg));

  // Inner focusgroup items should belong to inner context.
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(inner_item1, inner_fg));
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(inner_item2, inner_fg));

  // Outer focusgroup items should NOT belong to inner context.
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(outer_item1, inner_fg));
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(outer_item2, inner_fg));
}

TEST_F(FocusgroupControllerTest, CellAtIndexInRowBehaviorOnNoCellFound) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <table id=table focusgroup="grid">
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

  ASSERT_EQ(table->GetFocusgroupData().behavior, FocusgroupBehavior::kGrid);
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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

TEST_F(FocusgroupControllerTest, NestedFocusgroupsHaveSeparateScopes) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=outer focusgroup="toolbar">
      <button id=outer1 tabindex=0>Outer 1</button>
      <button id=outer2 tabindex=-1>Outer 2</button>

      <div id=inner focusgroup="menu">
        <button id=inner1 tabindex=-1>Inner 1</button>
        <button id=inner2 tabindex=-1>Inner 2</button>
        <button id=inner3 tabindex=-1>Inner 3</button>
      </div>

      <button id=outer3 tabindex=-1>Outer 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* outer = GetElementById("outer");
  auto* inner = GetElementById("inner");
  auto* outer1 = GetElementById("outer1");
  auto* outer2 = GetElementById("outer2");
  auto* outer3 = GetElementById("outer3");
  auto* inner1 = GetElementById("inner1");
  auto* inner2 = GetElementById("inner2");
  auto* inner3 = GetElementById("inner3");

  ASSERT_TRUE(outer);
  ASSERT_TRUE(inner);
  ASSERT_TRUE(outer1);
  ASSERT_TRUE(outer2);
  ASSERT_TRUE(outer3);
  ASSERT_TRUE(inner1);
  ASSERT_TRUE(inner2);
  ASSERT_TRUE(inner3);

  // Verify that the outer elements belong to the outer focusgroup.
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(outer1, FocusgroupType::kLinear),
      outer);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(outer2, FocusgroupType::kLinear),
      outer);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(outer3, FocusgroupType::kLinear),
      outer);

  // Verify that the inner elements belong to the inner focusgroup, not the
  // outer one.
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(inner1, FocusgroupType::kLinear),
      inner);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(inner2, FocusgroupType::kLinear),
      inner);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(inner3, FocusgroupType::kLinear),
      inner);

  // Verify that NextElement within outer focusgroup skips the inner focusgroup
  // elements.
  EXPECT_EQ(utils::NextElement(outer1, /* skip_subtree */ false), outer2);
  EXPECT_EQ(utils::NextElement(outer2, /* skip_subtree */ false), inner);

  // When we encounter the inner focusgroup container, NextElement should skip
  // its subtree and go to the next element in the outer focusgroup.
  EXPECT_EQ(utils::NextElement(inner, /* skip_subtree */ true), outer3);

  // Verify that within the inner focusgroup, navigation works independently.
  EXPECT_EQ(utils::NextElement(inner1, /* skip_subtree */ false), inner2);
  EXPECT_EQ(utils::NextElement(inner2, /* skip_subtree */ false), inner3);
}

TEST_F(FocusgroupControllerTest, GetFocusgroupOwnerOfItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=fg focusgroup="toolbar">
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
      <span id=non_focusable>Not focusable</span>
      <div id=opted_out focusgroup="none">
        <span id=opted_out_item tabindex=0></span>
      </div>
      <div id=nested_fg focusgroup="toolbar">
        <span id=nested_item tabindex=0></span>
      </div>
    </div>
    <span id=outside_item tabindex=0></span>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* nested_fg = GetElementById("nested_fg");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* non_focusable = GetElementById("non_focusable");
  auto* opted_out_item = GetElementById("opted_out_item");
  auto* nested_item = GetElementById("nested_item");
  auto* outside_item = GetElementById("outside_item");

  // Basic focusgroup items should return their owner.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(item1), fg);
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(item2), fg);
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(item1, fg));
  EXPECT_TRUE(utils::IsFocusgroupItemWithOwner(item2, fg));

  // Non-focusable elements are not considered items, so expect nullptr.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(non_focusable), nullptr);
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(non_focusable, fg));

  // Opted-out item elements are not considered items, so expect nullptr.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(opted_out_item), nullptr);
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(opted_out_item, fg));

  // Nested focusgroup item is part of nested focusgroup.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(nested_item), nested_fg);
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(nested_item, fg));

  // Element outside any focusgroup should have no owner.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(outside_item), nullptr);
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(outside_item, fg));

  // Null element in should return nullptr.
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(nullptr), nullptr);
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(nullptr, fg));
}

TEST_F(FocusgroupControllerTest, SegmentDetectionBasic) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  // All items in a single segment (no boundaries).
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="item1">Item 1</button>
      <button id="item2">Item 2</button>
      <button id="item3">Item 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item1), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item1), item3);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item2), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item2), item3);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item3), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item3), item3);
}

TEST_F(FocusgroupControllerTest, SegmentDetectionWithOptedOutBoundary) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="item1">Item 1</button>
      <button id="item2">Item 2</button>
      <div focusgroup="none">
        <button id="boundary">Boundary</button>
      </div>
      <button id="item3">Item 3</button>
      <button id="item4">Item 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* boundary = GetElementById("boundary");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");

  // Segment 1: [item1, item2].
  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item1), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item1), item2);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item2), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item2), item2);

  // Boundary element is not a focusgroup item (opted out).
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(boundary), nullptr);

  // Segment 2: [item3, item4].
  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item3), item3);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item3), item4);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item4), item3);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item4), item4);
}

TEST_F(FocusgroupControllerTest, SegmentDetectionMultipleBoundaries) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="item1">Item 1</button>
      <div focusgroup="none">
        <button id="boundary1">Boundary 1</button>
      </div>
      <button id="item2">Item 2</button>
      <div focusgroup="none">
        <button id="boundary2">Boundary 2</button>
      </div>
      <button id="item3">Item 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item1), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item1), item1);
  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item2), item2);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item2), item2);
  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item3), item3);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item3), item3);
}

TEST_F(FocusgroupControllerTest, SegmentDetectionOptedOutNotFocusable) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="item1">Item 1</button>
      <button id="item2">Item 2</button>
      <div focusgroup="none">
        <div id="not_boundary">Not a boundary (not focusable)</div>
      </div>
      <button id="item3">Item 3</button>
      <button id="item4">Item 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* item1 = GetElementById("item1");
  auto* item4 = GetElementById("item4");
  auto* not_boundary = GetElementById("not_boundary");

  // The opted-out element is not focusable, so it doesn't create a boundary.
  // All items remain in one segment.
  EXPECT_FALSE(not_boundary->IsFocusable());

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item1), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item1), item4);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*item4), item1);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*item4), item4);
}

TEST_F(FocusgroupControllerTest, SegmentDetectionNonFocusgroupItem) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="item1">Item 1</button>
      <div id="not_item">Not an item (not focusable)</div>
      <button id="item2">Item 2</button>
    </div>
    <button id="outside">Outside focusgroup</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* not_item = GetElementById("not_item");
  auto* outside = GetElementById("outside");

  // Non-focusgroup items should return nullptr.
  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*not_item), nullptr);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*not_item), nullptr);

  EXPECT_EQ(utils::FirstFocusgroupItemInSegment(*outside), nullptr);
  EXPECT_EQ(utils::LastFocusgroupItemInSegment(*outside), nullptr);
}

TEST_F(FocusgroupControllerTest, EntryElementLowestPositiveTabindex) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="5">Button 1</button>
      <button id="btn2" tabindex="2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
      <button id="btn4" tabindex="1">Button 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn4 = GetElementById("btn4");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn4);

  // Should select btn4 (tabindex="1") - lowest positive tabindex.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn4);

  // Reverse for backward direction.
  entry = utils::GetEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(entry, btn1);

  // IsEntryElementForFocusgroupSegment should give the same results.
  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(
      *btn4, *fg, mojom::blink::FocusType::kForward));
  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kBackward));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kForward));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(
      *btn4, *fg, mojom::blink::FocusType::kBackward));
}

TEST_F(FocusgroupControllerTest, EntryElementZeroTabindexForward) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="0">Button 1</button>
      <button id="btn2" tabindex="0">Button 2</button>
      <button id="btn3" tabindex="0">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // Forward direction: should select first element with tabindex=0.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn1);
}

TEST_F(FocusgroupControllerTest, EntryElementZeroTabindexBackward) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="0">Button 1</button>
      <button id="btn2" tabindex="0">Button 2</button>
      <button id="btn3" tabindex="0">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn2 = GetElementById("btn2");
  auto* btn3 = GetElementById("btn3");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(btn3);

  // Backward direction: should select last element with tabindex=0.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(entry, btn3);
}

TEST_F(FocusgroupControllerTest, EntryElementNegativeTabindexForward) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="-1">Button 1</button>
      <button id="btn2" tabindex="-1">Button 2</button>
      <button id="btn3" tabindex="-1">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // Forward direction: should select first element with tabindex=-1.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn1);
}

TEST_F(FocusgroupControllerTest, EntryElementNegativeTabindexBackward) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="-1">Button 1</button>
      <button id="btn2" tabindex="-1">Button 2</button>
      <button id="btn3" tabindex="-1">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn2 = GetElementById("btn2");
  auto* btn3 = GetElementById("btn3");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(btn3);

  // Backward direction: should select last element with tabindex=-1.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(entry, btn3);
}

TEST_F(FocusgroupControllerTest, EntryElementPriorityOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="neg" tabindex="-1">Negative</button>
      <button id="zero" tabindex="0">Zero</button>
      <button id="pos5" tabindex="5">Positive 5</button>
      <button id="pos2" tabindex="2">Positive 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* neg = GetElementById("neg");
  auto* pos2 = GetElementById("pos2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(neg);
  ASSERT_TRUE(pos2);

  // Should select lowest positive tabindex (pos2) over zero and negative.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *neg, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, pos2);
}

TEST_F(FocusgroupControllerTest, EntryElementZeroOverNegative) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="neg1" tabindex="-1">Negative 1</button>
      <button id="zero" tabindex="0">Zero</button>
      <button id="neg2" tabindex="-1">Negative 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* neg1 = GetElementById("neg1");
  auto* zero = GetElementById("zero");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(neg1);
  ASSERT_TRUE(zero);

  // Should select zero tabindex over negative.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *neg1, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, zero);
}

TEST_F(FocusgroupControllerTest, EntryElementWithAlreadyFocused) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="1">Button 1</button>
      <button id="btn2" tabindex="2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  btn2->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Should return nullptr if another item in segment is already focused.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, nullptr);
}

TEST_F(FocusgroupControllerTest, EntryElementMemoryRestoration) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="1">Button 1</button>
      <button id="btn2" tabindex="2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn3 = GetElementById("btn3");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn3);

  fg->SetFocusgroupLastFocused(*btn3);

  // Should restore memory item (btn3) even though btn1 has lower tabindex.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn3);
}

TEST_F(FocusgroupControllerTest, EntryElementSegmentBoundary) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="1">Button 1</button>
      <div focusgroup="none">
        <button id="barrier">Barrier</button>
      </div>
      <button id="btn2" tabindex="2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // btn1 and btn2 are in different segments, so entry element
  // for btn2's segment should be btn2 (lowest positive in that segment).
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn2);

  // Entry element for btn1's segment should be btn1 (only item in segment).
  entry = utils::GetEntryElementForFocusgroupSegment(
      *btn1, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn1);
}

TEST_F(FocusgroupControllerTest, EntryElementMemoryOutsideSegment) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="5">Button 1</button>
      <div focusgroup="none">
        <button id="barrier">Barrier</button>
      </div>
      <button id="btn2" tabindex="2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  fg->SetFocusgroupLastFocused(*btn1);

  // Memory should not be restored since btn1 is in a different segment
  // Should fall back to lowest positive tabindex in btn2's segment (btn2).
  auto* entry = utils::GetEntryElementForFocusgroupSegment(
      *btn2, *fg, mojom::blink::FocusType::kForward);
  EXPECT_EQ(entry, btn2);
}

}  // namespace blink
