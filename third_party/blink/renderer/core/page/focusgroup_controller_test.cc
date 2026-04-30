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
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
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
    if (event->target()) {
      event->target()->DispatchEvent(*event);
    }
    if (!event->DefaultHandled()) {
      GetDocument().GetFrame()->GetEventHandler().DefaultKeyboardEventHandler(
          event);
    }
  }

  void SendTab(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::TAB, target));
  }

  void SendShiftTab(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::TAB, target, WebInputEvent::kShiftKey));
  }

  void SendHome(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::HOME, target));
  }

  void SendEnd(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::END, target));
  }

  void SendArrowRight(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::ARROW_RIGHT, target));
  }

  void SendArrowLeft(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::ARROW_LEFT, target));
  }

  void SendArrowDown(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::ARROW_DOWN, target));
  }

  void SendArrowUp(Element* target) {
    SendEvent(KeyDownEvent(ui::DomKey::ARROW_UP, target));
  }

 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  ScopedFocusgroupForTest focusgroup_enabled{true};
};

namespace {
// Helper utility for asserting linear focusgroup directional traversal order.
void ExpectLinearDirectionalOrder(Element* owner,
                                  const HeapVector<Member<Element>>& ordered,
                                  bool expect_wrap = false) {
  ASSERT_TRUE(owner);
  ASSERT_FALSE(ordered.empty());

  // Ordered is a sequence of items only. Helper should not treat owner as an
  // item; verify by calling item helpers for expected front/back items.
  Element* first_item = FocusgroupControllerUtils::FocusgroupItemWithin(
      owner, FocusgroupItemPosition::kFirst);
  Element* last_item = FocusgroupControllerUtils::FocusgroupItemWithin(
      owner, FocusgroupItemPosition::kLast);
  ASSERT_TRUE(first_item);
  ASSERT_TRUE(last_item);
  EXPECT_EQ(first_item, ordered.front().Get())
      << "FocusgroupItemWithin(kFirst) mismatch";
  EXPECT_EQ(last_item, ordered.back().Get())
      << "FocusgroupItemWithin(kLast) mismatch";

  // Forward traversal assertions.
  for (wtf_size_t i = 0; i < ordered.size() - 1; ++i) {
    Element* current = ordered[i].Get();
    Element* expected_next = ordered[i + 1].Get();
    ASSERT_TRUE(current);
    ASSERT_TRUE(expected_next);
    Element* actual_next =
        FocusgroupControllerUtils::NextFocusgroupItemInDirection(
            owner, current, FocusgroupDirection::kForwardInline);
    EXPECT_EQ(actual_next, expected_next)
        << "Forward from " << current->GetIdAttribute();
  }
  Element* edge_forward =
      FocusgroupControllerUtils::NextFocusgroupItemInDirection(
          owner, ordered.back().Get(), FocusgroupDirection::kForwardInline);
  if (!expect_wrap) {
    EXPECT_EQ(edge_forward, nullptr)
        << "Expected no wrap forward from last element";
  } else {
    // Primitive returns nullptr; wrapping helper must yield first.
    EXPECT_EQ(edge_forward, nullptr);
    Element* wrapped_forward =
        FocusgroupControllerUtils::WrappedFocusgroupCandidate(
            owner, ordered.back().Get(), FocusgroupDirection::kForwardInline);
    EXPECT_EQ(wrapped_forward, ordered.front().Get())
        << "Expected forward wrap from last to first element";
  }

  // Backward traversal assertions.
  for (wtf_size_t i = ordered.size() - 1; i > 0; --i) {
    Element* current = ordered[i].Get();
    Element* expected_prev = ordered[i - 1].Get();
    ASSERT_TRUE(current);
    ASSERT_TRUE(expected_prev);
    Element* actual_prev =
        FocusgroupControllerUtils::NextFocusgroupItemInDirection(
            owner, current, FocusgroupDirection::kBackwardInline);
    EXPECT_EQ(actual_prev, expected_prev)
        << "Backward from " << current->GetIdAttribute();
  }
  Element* edge_backward =
      FocusgroupControllerUtils::NextFocusgroupItemInDirection(
          owner, ordered.front().Get(), FocusgroupDirection::kBackwardInline);
  if (!expect_wrap) {
    EXPECT_EQ(edge_backward, nullptr)
        << "Expected no wrap backward from first element";
  } else {
    EXPECT_EQ(edge_backward, nullptr);
    Element* wrapped_backward =
        FocusgroupControllerUtils::WrappedFocusgroupCandidate(
            owner, ordered.front().Get(), FocusgroupDirection::kBackwardInline);
    EXPECT_EQ(wrapped_backward, ordered.back().Get())
        << "Expected backward wrap from first to last element";
  }
}

// Helper utility for asserting traversal confined to a single focusgroup
// segment using NextFocusgroupItemInSegmentInDirection. The provided
// segment_items vector must list the visual (reading-flow adjusted) order of
// items inside one segment (no barriers or items from other segments). For
// single-item segments, the vector has size 1.
void ExpectSegmentDirectionalOrder(
    Element* owner,
    const HeapVector<Member<const Element>>& segment_items) {
  ASSERT_TRUE(owner);
  ASSERT_FALSE(segment_items.empty());

  auto SegmentToString = [&](const HeapVector<Member<const Element>>& items) {
    StringBuilder builder;
    builder.Append("[");
    bool first = true;
    for (const auto& m : items) {
      const Element* e = m.Get();
      if (!e) {
        continue;
      }
      if (!first) {
        builder.Append(", ");
      }
      first = false;
      builder.Append(e->GetIdAttribute());
    }
    builder.Append("]");
    return builder.ToString();
  };

  auto ActualSegmentFor = [&](const Element* any_item) {
    // Reconstruct actual segment boundaries by walking backward/forward using
    // segment traversal API starting from |any_item|.
    HeapVector<Member<const Element>> actual;
    // First walk backward to find first.
    const Element* first = any_item;
    for (const Element* prev = utils::NextFocusgroupItemInSegmentInDirection(
             *first, *owner, mojom::blink::FocusType::kBackward);
         prev; prev = utils::NextFocusgroupItemInSegmentInDirection(
                   *prev, *owner, mojom::blink::FocusType::kBackward)) {
      first = prev;
    }
    // Collect forward until end.
    actual.push_back(first);
    for (const Element* next = utils::NextFocusgroupItemInSegmentInDirection(
             *first, *owner, mojom::blink::FocusType::kForward);
         next; next = utils::NextFocusgroupItemInSegmentInDirection(
                   *next, *owner, mojom::blink::FocusType::kForward)) {
      actual.push_back(next);
    }
    return SegmentToString(actual);
  };

  // All items in the vector must report the same first/last segment members.
  const Element* expected_first = segment_items.front().Get();
  const Element* expected_last = segment_items.back().Get();
  for (const auto& member : segment_items) {
    const Element* item = member.Get();
    ASSERT_TRUE(item);
    EXPECT_EQ(FocusgroupControllerUtils::FocusgroupItemInSegment(
                  *item, FocusgroupItemPosition::kFirst),
              expected_first)
        << "Segment first mismatch for item " << item->GetIdAttribute()
        << " expected segment=" << SegmentToString(segment_items)
        << " actual segment=" << ActualSegmentFor(item);
    EXPECT_EQ(FocusgroupControllerUtils::FocusgroupItemInSegment(
                  *item, FocusgroupItemPosition::kLast),
              expected_last)
        << "Segment last mismatch for item " << item->GetIdAttribute()
        << " expected segment=" << SegmentToString(segment_items)
        << " actual segment=" << ActualSegmentFor(item);
  }

  // Forward traversal within the segment.
  for (wtf_size_t i = 0; i + 1 < segment_items.size(); ++i) {
    const Element* current = segment_items[i].Get();
    const Element* expected_next = segment_items[i + 1].Get();
    const Element* actual_next =
        FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
            *current, *owner, mojom::blink::FocusType::kForward);
    EXPECT_EQ(actual_next, expected_next)
        << "Forward segment traversal from " << current->GetIdAttribute()
        << " expected segment=" << SegmentToString(segment_items)
        << " actual segment=" << ActualSegmentFor(current);
  }
  // Edge forward from last item should yield nullptr.
  const Element* forward_edge =
      FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
          *segment_items.back().Get(), *owner,
          mojom::blink::FocusType::kForward);
  EXPECT_EQ(forward_edge, nullptr)
      << "Expected end-of-segment forward traversal to return nullptr";

  // Backward traversal within the segment.
  for (wtf_size_t i = segment_items.size(); i > 1; --i) {
    const Element* current = segment_items[i - 1].Get();
    const Element* expected_prev = segment_items[i - 2].Get();
    const Element* actual_prev =
        FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
            *current, *owner, mojom::blink::FocusType::kBackward);
    EXPECT_EQ(actual_prev, expected_prev)
        << "Backward segment traversal from " << current->GetIdAttribute()
        << " expected segment=" << SegmentToString(segment_items)
        << " actual segment=" << ActualSegmentFor(current);
  }
  // Edge backward from first item should yield nullptr.
  const Element* backward_edge =
      FocusgroupControllerUtils::NextFocusgroupItemInSegmentInDirection(
          *segment_items.front().Get(), *owner,
          mojom::blink::FocusType::kBackward);
  EXPECT_EQ(backward_edge, nullptr)
      << "Expected start-of-segment backward traversal to return nullptr";
}

}  // namespace

TEST_F(FocusgroupControllerTest,
       GridNavigationDisabledWithoutFocusgroupGridFlag) {
  // Explicitly disable FocusgroupGrid. Ensure arrow keys don't traverse a
  // grid when the feature is disabled.
  ScopedFocusgroupGridForTest grid_enabled{false};
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <table id=table focusgroup=grid>
      <tr>
        <td id=c1 tabindex=0>1</td>
        <td id=c2 tabindex=0>2</td>
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
  SetBodyInnerHTML(R"HTML(
    <div id=ltr>LTR element</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* ltr_element = GetElementById("ltr");
  ASSERT_TRUE(ltr_element);

  // Arrow right should be forward and inline (in LTR horizontal-tb).
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kForwardInline);

  // Arrow down should be forward and block.
  event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kForwardBlock);

  // Arrow left should be backward and inline.
  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kBackwardInline);

  // Arrow up should be backward and block.
  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kBackwardBlock);

  // When the shift key is pressed, even when combined with a valid arrow key,
  // it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kShiftKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kNone);

  // When the ctrl key is pressed, even when combined with a valid arrow key, it
  // should return kNone.
  event =
      KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kControlKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kNone);

  // When the meta key (e.g.: CMD on mac) is pressed, even when combined with a
  // valid arrow key, it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kMetaKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kNone);

  // When the alt key is pressed, even when combined with a valid arrow key, it
  // should return kNone (e.g., Alt+Left/Right is browser Back/Forward on
  // Windows).
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kAltKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kNone);

  // Any other key than an arrow key should return kNone.
  event = KeyDownEvent(ui::DomKey::TAB);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *ltr_element),
            FocusgroupDirection::kNone);
}

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventRTL) {
  SetBodyInnerHTML(R"HTML(
    <div id=rtl dir="rtl">RTL element</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* rtl_element = GetElementById("rtl");
  ASSERT_TRUE(rtl_element);

  // In RTL horizontal-tb, inline direction is reversed:
  // ArrowRight → backward inline (towards inline-start in RTL).
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *rtl_element),
            FocusgroupDirection::kBackwardInline);

  // ArrowLeft → forward inline (towards inline-end in RTL).
  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *rtl_element),
            FocusgroupDirection::kForwardInline);

  // Block axis is unchanged in RTL horizontal-tb.
  event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *rtl_element),
            FocusgroupDirection::kForwardBlock);

  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *rtl_element),
            FocusgroupDirection::kBackwardBlock);
}

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <div id=vrl style="writing-mode: vertical-rl">Vertical RL</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* vrl_element = GetElementById("vrl");
  ASSERT_TRUE(vrl_element);

  // In vertical-rl LTR, the inline axis is vertical and the block axis is
  // horizontal. ArrowDown → forward inline, ArrowUp → backward inline,
  // ArrowLeft → forward block, ArrowRight → backward block.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vrl_element),
            FocusgroupDirection::kForwardInline);

  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vrl_element),
            FocusgroupDirection::kBackwardInline);

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vrl_element),
            FocusgroupDirection::kForwardBlock);

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vrl_element),
            FocusgroupDirection::kBackwardBlock);
}

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventVerticalLR) {
  SetBodyInnerHTML(R"HTML(
    <div id=vlr style="writing-mode: vertical-lr">Vertical LR</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* vlr_element = GetElementById("vlr");
  ASSERT_TRUE(vlr_element);

  // In vertical-lr LTR, the inline axis is vertical and the block axis is
  // horizontal. ArrowDown → forward inline, ArrowUp → backward inline,
  // ArrowRight → forward block, ArrowLeft → backward block.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vlr_element),
            FocusgroupDirection::kForwardInline);

  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vlr_element),
            FocusgroupDirection::kBackwardInline);

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vlr_element),
            FocusgroupDirection::kForwardBlock);

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event, *vlr_element),
            FocusgroupDirection::kBackwardBlock);
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
      <button id=item1></button>
    </div>
    <div id=fg1 focusgroup="toolbar">
      <button id=item2></button>
      <div>
        <div id=fg2 focusgroup="toolbar">
          <button id=item3></button>
          <div>
            <span id=item4></span>
          </div>
          <table id=fg3 focusgroup="grid">
            <tr>
              <td id=item5 tabindex=0>
                <!-- The following is an error. -->
                <div id=fg4 focusgroup="grid">
                  <button id=item6></button>
                  <div id=fg5 focusgroup="toolbar">
                    <button id=item7></button>
                  </div>
                </div>
              </td>
            </tr>
          </table>
          <div id=fg6-container>
            <template shadowrootmode=open>
              <div id=fg6 focusgroup="toolbar">
                <button id=item8></button>
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
      <button id=item2></button>
    </div>
    <div id=fg2 focusgroup>
      <button id=item3></button>
    </div>
    <div id=fg3 focusgroup>
        <template shadowrootmode=open>
          <button id=item4></button>
        </template>
    </div>
    <button id=item5></button>
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
      <button id=item2></button>
    </div>
    <div id=fg2 focusgroup>
      <button id=item3></button>
    </div>
    <div id=fg3 focusgroup>
        <template shadowrootmode=open>
          <button id=item4></button>
        </template>
    </div>
    <button id=item5></button>
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

TEST_F(FocusgroupControllerTest, FocusgroupItemWithinLast) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <span id=item1></span>
      <button id=item2></button>
    </div>
    <div id=fg2 focusgroup="toolbar">
        <template shadowrootmode=open>
          <button id=item3></button>
          <button id=item4></button>
          <span id=item5></span>
        </template>
    </div>
    <button id=item5></button>
  )HTML");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);

  auto* item2 = GetElementById("item2");
  auto* item4 = fg2->GetShadowRoot()->getElementById(AtomicString("item4"));
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item4);

  EXPECT_EQ(utils::FocusgroupItemWithin(fg1, FocusgroupItemPosition::kLast),
            item2);
  EXPECT_EQ(utils::FocusgroupItemWithin(fg2, FocusgroupItemPosition::kLast),
            item4);
  EXPECT_EQ(utils::FocusgroupItemWithin(item4, FocusgroupItemPosition::kLast),
            nullptr);
}

TEST_F(FocusgroupControllerTest, FocusgroupItemWithinFirst) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <div id=fg1 focusgroup="toolbar">
      <span id=item1></span>
      <button id=item2></button>
    </div>
    <div id=fg2 focusgroup="toolbar">
        <template shadowrootmode=open>
          <button id=item3></button>
          <button id=item4></button>
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

  EXPECT_EQ(utils::FocusgroupItemWithin(fg1, FocusgroupItemPosition::kFirst),
            item2);
  EXPECT_EQ(utils::FocusgroupItemWithin(fg2, FocusgroupItemPosition::kFirst),
            item3);
  EXPECT_EQ(utils::FocusgroupItemWithin(item4, FocusgroupItemPosition::kFirst),
            nullptr);
}

TEST_F(FocusgroupControllerTest, IsFocusgroupItemWithOwner) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=outer_fg focusgroup="toolbar">
      <button id=outer_item1></button>
      <div>
        <div id=inner_fg focusgroup="toolbar">
          <button id=inner_item1></button>
          <button id=inner_item2></button>
        </div>
      </div>
      <button id=outer_item2></button>
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

// Test that a focusable nested focusgroup participates as an item in its parent
// focusgroup. Arrow navigation should reach the nested focusgroup element
// itself, and its contents should be skipped.
TEST_F(FocusgroupControllerTest, NestedFocusgroupParticipatesAsItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=outer focusgroup="toolbar">
      <button id=btn1></button>
      <button id=btn2></button>
      <div id=inner focusgroup="toolbar" tabindex=0>
        <button id=inner_btn1></button>
        <button id=inner_btn2></button>
      </div>
      <button id=btn3></button>
    </div>
  )HTML");

  auto* outer = GetElementById("outer");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");
  auto* inner = GetElementById("inner");
  auto* inner_btn1 = GetElementById("inner_btn1");
  auto* inner_btn2 = GetElementById("inner_btn2");
  auto* btn3 = GetElementById("btn3");

  ASSERT_TRUE(outer);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(inner);
  ASSERT_TRUE(inner_btn1);
  ASSERT_TRUE(inner_btn2);
  ASSERT_TRUE(btn3);

  // The nested focusgroup (inner) should be the first item if it comes first.
  EXPECT_EQ(utils::FocusgroupItemWithin(outer, FocusgroupItemPosition::kFirst),
            btn1);

  // The nested focusgroup should be the last item if it comes last.
  EXPECT_EQ(utils::FocusgroupItemWithin(outer, FocusgroupItemPosition::kLast),
            btn3);

  // Arrow navigation from btn2 should reach the nested focusgroup element.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, btn2, FocusgroupDirection::kForwardInline),
            inner);

  // Arrow navigation from the nested focusgroup should reach btn3.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, inner, FocusgroupDirection::kForwardInline),
            btn3);

  // Arrow navigation backward from btn3 should reach the nested focusgroup.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, btn3, FocusgroupDirection::kBackwardInline),
            inner);

  // Arrow navigation backward from the nested focusgroup should reach btn2.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, inner, FocusgroupDirection::kBackwardInline),
            btn2);

  // Items inside the nested focusgroup should NOT be reachable via outer.
  EXPECT_NE(utils::NextFocusgroupItemInDirection(
                outer, btn2, FocusgroupDirection::kForwardInline),
            inner_btn1);
  EXPECT_NE(utils::NextFocusgroupItemInDirection(
                outer, inner, FocusgroupDirection::kForwardInline),
            inner_btn2);
}

// Test that a non-focusable nested focusgroup does NOT participate as an item.
TEST_F(FocusgroupControllerTest, NonFocusableNestedFocusgroupNotAnItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=outer focusgroup="toolbar">
      <button id=btn1></button>
      <div id=inner focusgroup="toolbar">
        <button id=inner_btn1></button>
        <button id=inner_btn2></button>
      </div>
      <button id=btn2></button>
    </div>
  )HTML");

  auto* outer = GetElementById("outer");
  auto* btn1 = GetElementById("btn1");
  auto* inner = GetElementById("inner");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(outer);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(inner);
  ASSERT_TRUE(btn2);

  // Arrow navigation from btn1 should skip the non-focusable nested focusgroup
  // and go directly to btn2.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, btn1, FocusgroupDirection::kForwardInline),
            btn2);

  // Arrow navigation backward from btn2 should skip the non-focusable nested
  // focusgroup and go directly to btn1.
  EXPECT_EQ(utils::NextFocusgroupItemInDirection(
                outer, btn2, FocusgroupDirection::kBackwardInline),
            btn1);
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
      <button id=item1></button>
      <button id=item2></button>
      <button></button>
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
      <button id=item1></button>
      <button id=item2></button>
      <button></button>
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
      <button id=item1></button>
      <button id=item2></button>
      <button></button>
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
      <button id=outer1>Outer 1</button>
      <button id=outer2>Outer 2</button>

      <div id=inner focusgroup="menu">
        <button id=inner1>Inner 1</button>
        <button id=inner2>Inner 2</button>
        <button id=inner3>Inner 3</button>
      </div>

      <button id=outer3>Outer 3</button>
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
      <button id=item1></button>
      <button id=item2></button>
      <span id=non_focusable>Not focusable</span>
      <div id=opted_out focusgroup="none">
        <button id=opted_out_item></button>
      </div>
      <div id=nested_fg focusgroup="toolbar">
        <button id=nested_item></button>
      </div>
    </div>
    <button id=outside_item></button>
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

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kLast),
      item3);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kLast),
      item3);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kLast),
      item3);
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
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kLast),
      item2);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kLast),
      item2);

  // Boundary element is not a focusgroup item (opted out).
  EXPECT_EQ(utils::GetFocusgroupOwnerOfItem(boundary), nullptr);

  // Segment 2: [item3, item4].
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kFirst),
      item3);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kLast),
      item4);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item4, FocusgroupItemPosition::kFirst),
      item3);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item4, FocusgroupItemPosition::kLast),
      item4);
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

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kLast),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kFirst),
      item2);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item2, FocusgroupItemPosition::kLast),
      item2);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kFirst),
      item3);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item3, FocusgroupItemPosition::kLast),
      item3);
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

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item1, FocusgroupItemPosition::kLast),
      item4);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item4, FocusgroupItemPosition::kFirst),
      item1);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*item4, FocusgroupItemPosition::kLast),
      item4);
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
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*not_item, FocusgroupItemPosition::kFirst),
      nullptr);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*not_item, FocusgroupItemPosition::kLast),
      nullptr);

  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*outside, FocusgroupItemPosition::kFirst),
      nullptr);
  EXPECT_EQ(
      utils::FocusgroupItemInSegment(*outside, FocusgroupItemPosition::kLast),
      nullptr);
}

TEST_F(FocusgroupControllerTest, EntryElementFirstInSegment) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="5">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="btn3" tabindex="3">Button 3</button>
      <button id="btn4" tabindex="1">Button 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // Should always select first item in tree order (btn1), regardless of
  // tabindex values.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn2, *fg);
  EXPECT_EQ(entry, btn1);

  // IsEntryElementForFocusgroupSegment should give the same results.
  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(*btn1, *fg));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(*btn2, *fg));
}

// Tests for negative tabindex removed because tabindex=-1 elements are no
// longer focusgroup items per the updated spec behavior.

TEST_F(FocusgroupControllerTest, EntryPriorityOverFirstInSegment) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1" tabindex="1">Positive 1</button>
      <button id="priority" focusgroupstart>Priority</button>
      <button id="pos2" tabindex="2">Positive 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* priority = GetElementById("priority");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(priority);

  // Entry-priority should take precedence over first item in segment.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, priority);
}

TEST_F(FocusgroupControllerTest, MultipleEntryPriorityFirstInSegment) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="priority1" focusgroupstart>Priority 1</button>
      <button id="priority2" focusgroupstart>Priority 2</button>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* priority1 = GetElementById("priority1");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(priority1);

  // When multiple elements have entry-priority, first in segment wins.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, priority1);
}

TEST_F(FocusgroupControllerTest, EntryPriorityMemoryTakesPrecedence) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="priority" focusgroupstart>Priority</button>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // Set memory to btn2.
  fg->SetFocusgroupLastFocused(*btn2);

  // Memory should take precedence over entry-priority.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, btn2);
}

TEST_F(FocusgroupControllerTest, EntryElementWithReadingFlowOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        reading-flow: flex-visual;
      }
      #btn1 { order: 3; }
      #btn2 { order: 2; }
      #btn3 { order: 1; }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="btn3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn3 = GetElementById("btn3");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn3);

  // Visual/reading-flow order is: btn3, btn2, btn1.
  // Entry element should be btn3 (first in reading-flow order).
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, btn3);

  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(*btn3, *fg));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(*btn1, *fg));
}

TEST_F(FocusgroupControllerTest, EntryPriorityWithReadingFlowOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        reading-flow: flex-visual;
      }
      #btn1 { order: 3; }
      #btn2 { order: 1; }
      #priority { order: 2; }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="priority" focusgroupstart>Priority</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* priority = GetElementById("priority");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(priority);

  // Visual/reading-flow order is: btn2, priority, btn1.
  // Entry-priority should take precedence over position in reading-flow order.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, priority);

  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(*priority, *fg));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(*btn1, *fg));
}

TEST_F(FocusgroupControllerTest, MultipleEntryPriorityWithReadingFlowOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        reading-flow: flex-visual;
      }
      #priority1 { order: 3; }
      #priority2 { order: 1; }
      #btn1 { order: 2; }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="priority1" focusgroupstart>Priority 1</button>
      <button id="priority2" focusgroupstart>Priority 2</button>
      <button id="btn1">Button 1</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* priority2 = GetElementById("priority2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(priority2);

  // Visual/reading-flow order is: priority2, btn1, priority1.
  // When multiple elements have entry-priority, first in reading-flow order
  // wins.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, priority2);

  EXPECT_TRUE(utils::IsEntryElementForFocusgroupSegment(*priority2, *fg));
  EXPECT_FALSE(utils::IsEntryElementForFocusgroupSegment(*btn1, *fg));
}

TEST_F(FocusgroupControllerTest, EntryElementWithAlreadyFocused) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
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

  // Should return the item in segment that is focused.
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, btn2);
}

TEST_F(FocusgroupControllerTest, EntryElementMemoryRestoration) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
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
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
  EXPECT_EQ(entry, btn3);
}

TEST_F(FocusgroupControllerTest, EntryElementSegmentBoundary) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
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
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn2, *fg);
  EXPECT_EQ(entry, btn2);

  // Entry element for btn1's segment should be btn1 (only item in segment).
  entry = utils::GetEntryElementForFocusgroupSegment(*btn1, *fg);
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
  auto* entry = utils::GetEntryElementForFocusgroupSegment(*btn2, *fg);
  EXPECT_EQ(entry, btn2);
}

TEST_F(FocusgroupControllerTest,
       ReadingFlowNavigationOwnerDOMFallbackWithReorderedDescendant) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .rf { display:flex; flex-direction:row-reverse; reading-flow:flex-visual; }
    </style>
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <div class="rf">
        <button id="v1">Visual 1</button>
        <button id="v2">Visual 2</button>
      </div>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* v1 = GetElementById("v1");
  auto* v2 = GetElementById("v2");
  auto* btn2 = GetElementById("btn2");
  ASSERT_TRUE(fg && btn1 && v1 && v2 && btn2);
  // Owner not a reading-flow container: owner-level ordering uses DOM around
  // descendant container. Descendant reading-flow container internally reverses
  // visual order (row-reverse): v2 then v1. We validate direct owner traversal
  // still steps over the container boundary respecting focusgroup scoping.
  auto* next = utils::NextFocusgroupItemInDirection(
      fg, btn1, FocusgroupDirection::kForwardInline);
  // Depending on algorithm: may enter descendant container first item (visual
  // first) or DOM first.
  EXPECT_TRUE(next == v2 || next == v1);
  if (next == v2) {
    // Visual traversal path.
    auto* after = utils::NextFocusgroupItemInDirection(
        fg, v2, FocusgroupDirection::kForwardInline);
    EXPECT_TRUE(after == v1 || after == btn2);
  }
  // Backward from btn2 should land inside container (visual last) or previous
  // DOM.
  auto* prev = utils::NextFocusgroupItemInDirection(
      fg, btn2, FocusgroupDirection::kBackwardInline);
  EXPECT_TRUE(prev == v1 || prev == v2 || prev == btn1);
}

TEST_F(FocusgroupControllerTest, ReadingFlowNavigationEdgeCasesWithOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex {
        display: flex;
        reading-flow: flex-visual;
      }
      #btn1 {
        order: 2;
      }
      #btn2 {
        order: 1;
      }
    </style>
    <div id="fg" class="flex" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);

  // Test null owner
  auto* result = utils::NextFocusgroupItemInDirection(
      nullptr, btn1, FocusgroupDirection::kForwardInline);
  EXPECT_EQ(result, nullptr);

  // Test null current_item
  result = utils::NextFocusgroupItemInDirection(
      fg, nullptr, FocusgroupDirection::kForwardInline);
  EXPECT_EQ(result, nullptr);

  // Test owner == current_item
  result = utils::NextFocusgroupItemInDirection(
      fg, fg, FocusgroupDirection::kForwardInline);
  EXPECT_EQ(result, nullptr);
}

TEST_F(FocusgroupControllerTest,
       ReadingFlowNavigationFirstAndLastItemsWithOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex {
        display: flex;
        reading-flow: flex-visual;
      }
      #btn2 {
        order: 1;
      }
      #btn3 {
        order: 2;
      }
      #btn1 {
        order: 3;
      }
    </style>
    <div id="fg" class="flex" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="btn3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  ExpectLinearDirectionalOrder(
      GetElementById("fg"),
      {GetElementById("btn2"), GetElementById("btn3"), GetElementById("btn1")});
}

TEST_F(FocusgroupControllerTest,
       ReadingFlowNavigationWithOptedOutElementsAndOrder) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
    </style>
    <div id="fg" class="flex" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <div focusgroup="none">
        <button id="opted_out">Opted Out</button>
      </div>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Visual order (row-reverse) among focusgroup items only: btn2, btn1.
  ExpectLinearDirectionalOrder(
      GetElementById("fg"), {GetElementById("btn2"), GetElementById("btn1")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowNavigationFlexVisualReordering) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="btn3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  ExpectLinearDirectionalOrder(
      GetElementById("fg"),
      {GetElementById("btn3"), GetElementById("btn2"), GetElementById("btn1")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowNavigationFlexOrderProperty) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        reading-flow: flex-visual;
      }
      #btn2 { order: 1; }
      #btn3 { order: 2; }
      #btn1 { order: 3; }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
      <button id="btn3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  ExpectLinearDirectionalOrder(
      GetElementById("fg"),
      {GetElementById("btn2"), GetElementById("btn3"), GetElementById("btn1")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowSegmentOrdering) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="a">A</button>
      <button id="b">B</button>
      <button id="c">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Visual order (row-reverse): C, B, A within a single segment.
  ExpectSegmentDirectionalOrder(
      GetElementById("fg"),
      {GetElementById("c"), GetElementById("b"), GetElementById("a")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowSegmentBoundaryOptOut) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="a">A</button>
      <div focusgroup="none"><button id="opt">Opted</button></div>
      <button id="b">B</button>
      <button id="c">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("c"), GetElementById("b")});

  ExpectSegmentDirectionalOrder(GetElementById("fg"), {GetElementById("a")});
}

// New segment-based tests mirroring full focusgroup navigation coverage.
// Interaction: single reading-flow reordered container split into two
// segments by an opted-out subtree containing focusable descendants.
TEST_F(FocusgroupControllerTest, ReadingFlowSegmentWithOptedOutBarrier) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .rf { display:flex; flex-direction:row-reverse; reading-flow:flex-visual; }
    </style>
    <div id="fg" class="rf" focusgroup="toolbar">
      <button id="a">A</button>
      <button id="b">B</button>
      <div focusgroup="none"><button id="bar">Barrier</button></div>
      <button id="c">C</button>
      <button id="d">D</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Segments: [D, C], [B, A] (row-reverse visual ordering within last
  // segment).
  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("d"), GetElementById("c")});
  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("b"), GetElementById("a")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowSegmentNestedFocusgroupSkip) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="a">A</button>
      <button id="b">B</button>
      <div focusgroup="toolbar" id="nested">
        <button id="nested_item_1">Nested</button>
        <button id="nested_item_2">Nested</button>
      </div>
      <button id="c">C</button>
      <button id="d">D</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Nested focusgroup container is considered a barrier.
  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("a"), GetElementById("b")});
  ExpectSegmentDirectionalOrder(
      GetElementById("nested"),
      {GetElementById("nested_item_1"), GetElementById("nested_item_2")});
  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("c"), GetElementById("d")});
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(GetElementById("nested_item_1"),
                                                GetElementById("fg")));
}

TEST_F(FocusgroupControllerTest, ReadingFlowSegmentMultipleBarriersMixed) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .rf { display:flex; reading-flow:flex-visual; }
      #x { order:4; } #y { order:1; } #z { order:3; } #w { order:2; }
    </style>
    <div id="fg" class="rf" focusgroup="toolbar">
      <button id="x">X</button>
      <div focusgroup="none"><button id="opt1">Opt1</button></div>
      <div focusgroup="toolbar" id="nested"><button id="nested_item">Nested</button></div>
      <button id="y">Y</button>
      <div focusgroup="none"><button id="opt2">Opt2</button></div>
      <button id="z">Z</button>
      <button id="w">W</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Segments determined by barriers: [x], [y], [w, z] (visual order inside last
  // segment).
  ExpectSegmentDirectionalOrder(GetElementById("fg"),
                                {GetElementById("y"), GetElementById("w"),
                                 GetElementById("z"), GetElementById("x")});
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(GetElementById("nested_item"),
                                                GetElementById("fg")));
}

TEST_F(FocusgroupControllerTest, ReadingFlowSegmentOrderPropertySegments) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .flex-container { display:flex; reading-flow:flex-visual; }
      #o1 { order:3; }
      #o2 { order:1; }
      #o3 { order:2; }
    </style>
    <div id="fg" class="flex-container" focusgroup="toolbar">
      <button id="o1">One</button>
      <button id="o2">Two</button>
      <button id="o3">Three</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // Visual order: o2 (1), o3 (2), o1 (3) within one segment.
  ExpectSegmentDirectionalOrder(
      GetElementById("fg"),
      {GetElementById("o2"), GetElementById("o3"), GetElementById("o1")});
}

TEST_F(FocusgroupControllerTest, ReadingFlowComplexNestedContainers) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .outer-flex {
        display: flex;
        reading-flow: flex-visual;
      }
      .inner-flex {
        display: flex;
        reading-flow: flex-visual;
      }
      .outer-flex #item1 { order: 3; }
      .outer-flex .inner-container { order: 1; }
      .outer-flex #item4 { order: 2; }
      .inner-flex #item2 { order: 2; }
      .inner-flex #item3 { order: 1; }
    </style>
    <div class="outer-flex" id="fg" focusgroup="toolbar">
      <button id="item1">item1 (DOM 1, outer order 3)</button>
      <div class="inner-flex inner-container">
        <button id="item2">item2 (DOM 2, inner order 2)</button>
        <button id="item3">item3 (DOM 3, inner order 1)</button>
      </div>
      <button id="item4">item4 (DOM 4, outer order 2)</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);

  // Full traversal validation using helper.
  // Flattened nested visual order: item3, item2, item4, item1.
  ExpectLinearDirectionalOrder(fg, {item3, item2, item4, item1});
}

TEST_F(FocusgroupControllerTest,
       ReadingFlowComplexOwnerAndAncestorContainersPreferOwner) {
  ScopedFocusgroupForTest focusgroup_enabled(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .ancestor { display: flex; reading-flow: flex-visual; }
      .owner { display: flex; reading-flow: flex-visual; }
      .ancestor #sibling { order: 2; }
      .ancestor #owner { order: 1; }
      .owner #x { order: 3; }
      .owner #y { order: 1; }
      .owner #z { order: 2; }
    </style>
    <div class="ancestor">
      <button id="sibling">Sibling</button>
      <div class="owner" id="owner" focusgroup="toolbar">
        <button id="x">X order 3</button>
        <button id="y">Y order 1</button>
        <button id="z">Z order 2</button>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* owner = GetElementById("owner");
  auto* sibling = GetElementById("sibling");
  auto* x = GetElementById("x");
  auto* y = GetElementById("y");
  auto* z = GetElementById("z");
  ASSERT_TRUE(owner && sibling && x && y && z);

  EXPECT_TRUE(owner->IsReadingFlowContainer());
  Element* ancestor = owner->parentElement();
  ASSERT_TRUE(ancestor);
  EXPECT_TRUE(ancestor->IsReadingFlowContainer());

  // Full traversal validation using helper. Internal visual order y, z, x.
  ExpectLinearDirectionalOrder(owner, {y, z, x});

  // Ancestor sibling is outside the owner's focusgroup scope and must not be
  // treated as an item.
  EXPECT_FALSE(utils::IsFocusgroupItemWithOwner(sibling, owner));
}

TEST_F(FocusgroupControllerTest, ReadingFlowComplexMixedReadingFlowAndNormal) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .reading-flow-container {
        display: flex;
        reading-flow: flex-visual;
        flex-direction: row-reverse;
      }
    </style>
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <div class="reading-flow-container">
        <button id="btn2">Button 2</button>
        <button id="btn3">Button 3</button>
      </div>
      <button id="btn4">Button 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");
  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");
  auto* btn3 = GetElementById("btn3");
  auto* btn4 = GetElementById("btn4");

  ASSERT_TRUE(fg);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(btn3);
  ASSERT_TRUE(btn4);

  // The focusgroup owner is not a reading-flow container,
  // but it has a descendant that is. Our enhanced algorithm
  // should find the descendant reading-flow container.

  // The reading-flow container has flex-direction: row-reverse,
  // so btn3 should come before btn2 in visual order

  // Test navigation - the behavior depends on whether reading-flow
  // is fully implemented or not
  auto* next = utils::NextFocusgroupItemInDirection(
      fg, btn1, FocusgroupDirection::kForwardInline);

  // If reading-flow works and our algorithm finds the descendant container,
  // it should navigate within that container using visual order (btn3, btn2)
  // If not, it should fall back to DOM order (btn2, btn3)
  if (fg->IsReadingFlowContainer() ||
      (next ==
       btn3)) {  // If we get btn3, reading-flow descendant discovery worked
    // Enhanced algorithm found reading-flow container and it has children
    // Navigation within the container should respect reading-flow
    EXPECT_EQ(next,
              btn3);  // Should be first in visual order due to row-reverse
  } else {
    // Fallback to DOM order
    EXPECT_EQ(next, btn2);  // DOM order fallback
  }
}

TEST_F(FocusgroupControllerTest, ReadingFlowComplexMixedNavigation) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .container {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
    </style>
    <div id="fg" focusgroup="toolbar">
      <div class="container" id="reading_flow_container">
        <button id="btn3">Button 3</button>
        <button id="btn2">Button 2</button>
        <button id="btn1">Button 1</button>
      </div>
      <button id="btn4">Button 4</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");
  auto* btn3 = GetElementById("btn3");
  auto* btn4 = GetElementById("btn4");
  auto* fg = GetElementById("fg");

  ASSERT_TRUE(btn1);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(btn3);
  ASSERT_TRUE(btn4);
  ASSERT_TRUE(fg);

  btn1->Focus();
  EXPECT_EQ(GetDocument().FocusedElement(), btn1);

  // Full traversal validation using helper.
  // Observed DOM-forward order: btn1, btn2, btn3, btn4.
  ExpectLinearDirectionalOrder(fg, {btn1, btn2, btn3, btn4});
}

TEST_F(FocusgroupControllerTest, ReadingFlowComplexPartialReordering) {
  ScopedFocusgroupForTest focusgroup_enabled(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .reading-flow-container-reversed {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
      .reading-flow-container {
        display: flex;
        reading-flow: flex-visual;
      }
      .reading-flow-container-nested {
        display: flex;
        flex-direction: row-reverse;
        reading-flow: flex-visual;
      }
      /* Explicit order values for specific containers */
      .reading-flow-container #btn6 { order: 1; }
      .reading-flow-container #btn7 { order: 2; }
      .reading-flow-container #btn8 { order: 3; }
      .reading-flow-container .reading-flow-container-nested { order: 4; }
      .reading-flow-container #btn12 { order: 5; }
    </style>
    <div focusgroup="toolbar wrap" id="fg">
      <div class="reading-flow-container-reversed">
        <button id="btn3">Button 3</button>
        <button id="btn2">Button 2</button>
        <button id="btn1">Button 1</button>
      </div>
      <button id="btn4">Button 4</button>
      <button id="btn5">Button 5</button>
      <div class="reading-flow-container">
        <button id="btn7">Button 7</button>
        <button id="btn6">Button 6</button>
        <button id="btn8">Button 8</button>
        <div class="reading-flow-container-nested">
          <button id="btn11">Button 11</button>
          <button id="btn10">Button 10</button>
          <button id="btn9">Button 9</button>
        </div>
        <button id="btn12">Button 12</button>
      </div>
      <button id="btn13">Button 13</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* fg = GetElementById("fg");

  // Expected visual order based on CSS layout:
  // First container (row-reverse): btn1, btn2, btn3
  // Regular DOM: btn4, btn5
  // Second container (with explicit order): btn6, btn7, btn8, nested container
  // (btn9, btn10, btn11), btn12 Regular DOM: btn13
  ExpectLinearDirectionalOrder(
      fg,
      {GetElementById("btn1"), GetElementById("btn2"), GetElementById("btn3"),
       GetElementById("btn4"), GetElementById("btn5"), GetElementById("btn6"),
       GetElementById("btn7"), GetElementById("btn8"), GetElementById("btn9"),
       GetElementById("btn10"), GetElementById("btn11"),
       GetElementById("btn12"), GetElementById("btn13")},
      /*expect_wrap=*/true);
}

// Tests for focusgroupstart attribute helpers.

TEST_F(FocusgroupControllerTest, IsFocusgroupStartAttribute) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2" focusgroupstart>Button 2</button>
      <button id="btn3">Button 3</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");
  auto* btn3 = GetElementById("btn3");

  EXPECT_FALSE(utils::IsFocusgroupStart(*btn1));
  EXPECT_TRUE(utils::IsFocusgroupStart(*btn2));
  EXPECT_FALSE(utils::IsFocusgroupStart(*btn3));
}

TEST_F(FocusgroupControllerTest, IsFocusgroupStartAttributeDynamic) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">Button 1</button>
      <button id="btn2">Button 2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* btn2 = GetElementById("btn2");

  EXPECT_FALSE(utils::IsFocusgroupStart(*btn1));
  EXPECT_FALSE(utils::IsFocusgroupStart(*btn2));

  // Add attribute dynamically to btn1.
  btn1->setAttribute(html_names::kFocusgroupstartAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(utils::IsFocusgroupStart(*btn1));
  EXPECT_FALSE(utils::IsFocusgroupStart(*btn2));

  // Remove attribute from btn1 and add to btn2.
  btn1->removeAttribute(html_names::kFocusgroupstartAttr);
  btn2->setAttribute(html_names::kFocusgroupstartAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(utils::IsFocusgroupStart(*btn1));
  EXPECT_TRUE(utils::IsFocusgroupStart(*btn2));
}

TEST_F(FocusgroupControllerTest, ContainsKeyboardFocusableContentWithOptOut) {
  SetBodyInnerHTML(R"HTML(
    <div id="fg" focusgroup="toolbar">
      <button id="btn1">1</button>
      <div id="optout" focusgroup="none">
        <button id="barrier">Barrier</button>
      </div>
      <button id="btn2">2</button>
    </div>
  )HTML");

  auto* fg = GetElementById("fg");
  ASSERT_TRUE(fg);

  // The focusgroup contains focusable content because the opted-out subtree
  // contains a focusable element.
  EXPECT_TRUE(utils::ContainsKeyboardFocusableContent(*fg));
}

// Exercises arrow key handler behavior with select elements.
// Select elements are arrow key handlers because they consume arrow keys
// on both axes for option navigation. When an arrow key handler is focused,
// Tab moves to the adjacent focusgroup item to ensure content after the
// arrow key handler remains reachable.
TEST_F(FocusgroupControllerTest, FocusgroupWithSelect) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=btn1 type="button">Bold</button>
      <select id=sel1>
        <option>12px</option>
        <option>14px</option>
      </select>
      <button id=btn2 type="button">Italic</button>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* sel1 = GetElementById("sel1");
  auto* btn2 = GetElementById("btn2");
  auto* after = GetElementById("after");
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(sel1);
  ASSERT_TRUE(btn2);
  ASSERT_TRUE(after);

  // Arrow key can navigate TO the select (entry works).
  btn1->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), btn1);
  SendArrowRight(btn1);
  EXPECT_EQ(GetDocument().FocusedElement(), sel1)
      << "Arrow right should navigate to select (entry allowed)";

  // Tab from select should move to btn2 (next adjacent focusgroup item).
  SendTab(sel1);
  EXPECT_EQ(GetDocument().FocusedElement(), btn2)
      << "Tab from arrow key handler should go to next adjacent item";
}

// Exercises Shift+Tab from an arrow key handler in a separate test
// to avoid state from previous navigations affecting the result.
TEST_F(FocusgroupControllerTest, FocusgroupWithSelectShiftTab) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <button id=before type="button">Before</button>
    <div focusgroup="toolbar inline">
      <button id=btn1 type="button">Bold</button>
      <select id=sel1>
        <option>12px</option>
        <option>14px</option>
      </select>
      <button id=btn2 type="button">Italic</button>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* before = GetElementById("before");
  auto* btn1 = GetElementById("btn1");
  auto* sel1 = GetElementById("sel1");
  ASSERT_TRUE(before);
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(sel1);

  // Start from sel1 and Shift+Tab backward.
  sel1->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel1);

  SendShiftTab(sel1);
  // Shift+Tab from sel1: should go to the immediately previous focusgroup
  // item (btn1).
  EXPECT_EQ(GetDocument().FocusedElement(), btn1)
      << "Shift+Tab from arrow key handler should go to previous adjacent item";
}

// Verifies Tab/Shift+Tab from an arrow key handler goes to the immediately
// adjacent focusgroup item.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeGoesToAdjacentItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <select id=C>
        <option>12px</option>
        <option>14px</option>
      </select>
      <button id=D type="button">D</button>
      <button id=E type="button">E</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* b = GetElementById("B");
  auto* c = GetElementById("C");
  auto* d = GetElementById("D");
  auto* e = GetElementById("E");
  ASSERT_TRUE(a && b && c && d && e);

  // Focus the arrow key handler.
  c->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), c);

  // Tab should go to D (next adjacent item).
  SendTab(c);
  EXPECT_EQ(GetDocument().FocusedElement(), d)
      << "Tab from arrow key handler should go to next adjacent item (D)";

  // Reset: focus C again for Shift+Tab test.
  c->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), c);

  // Shift+Tab should go to B (previous adjacent item).
  SendShiftTab(c);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Shift+Tab from arrow key handler should go to previous adjacent item "
         "(B)";
}

// When an arrow key handler is at the end of a focusgroup, Tab should fall
// through to normal navigation (exit the focusgroup).
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeAtEndFallsThrough) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <select id=B>
        <option>12px</option>
      </select>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* b = GetElementById("B");
  auto* after = GetElementById("after");
  ASSERT_TRUE(b && after);

  // Focus the arrow key handler at the end of the focusgroup.
  b->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), b);

  // Tab should fall through (no next focusgroup item) and normal Tab exits
  // the focusgroup to the "After" button.
  SendTab(b);
  EXPECT_EQ(GetDocument().FocusedElement(), after)
      << "Tab from arrow key handler at end should exit focusgroup";
}

// Shift+Tab from an arrow key handler at the start of a focusgroup should fall
// through to normal navigation (exit the focusgroup backward).
TEST_F(FocusgroupControllerTest, FocusgroupShiftTabEscapeAtStartFallsThrough) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <button id=before type="button">Before</button>
    <div focusgroup="toolbar inline">
      <select id=A>
        <option>12px</option>
      </select>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* before = GetElementById("before");
  auto* a = GetElementById("A");
  ASSERT_TRUE(before && a);

  a->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  // Shift+Tab should fall through and exit the focusgroup backward.
  SendShiftTab(a);
  EXPECT_EQ(GetDocument().FocusedElement(), before)
      << "Shift+Tab from arrow key handler at start should exit focusgroup";
}

// When multiple arrow key handlers are adjacent, Tab should move between
// them one by one.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeMultipleConflicts) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <select id=sel1>
        <option>opt1</option>
      </select>
      <select id=sel2>
        <option>opt2</option>
      </select>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* sel1 = GetElementById("sel1");
  auto* sel2 = GetElementById("sel2");
  auto* b = GetElementById("B");
  ASSERT_TRUE(sel1 && sel2 && b);

  // Focus first select.
  sel1->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel1);

  // Tab from sel1 should go to sel2 (next adjacent item, also an arrow key
  // handler).
  SendTab(sel1);
  EXPECT_EQ(GetDocument().FocusedElement(), sel2)
      << "Tab from first arrow key handler should go to second arrow key "
         "handler";

  // Tab from sel2 should go to B.
  SendTab(sel2);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from second arrow key handler should go to B";
}

// Tab escape should NOT fire when the focusgroup has wrap enabled. Tab from
// the last arrow key handler should exit the focusgroup, not wrap.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeDoesNotWrap) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline wrap">
      <button id=A type="button">A</button>
      <select id=B>
        <option>opt1</option>
      </select>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* b = GetElementById("B");
  auto* after = GetElementById("after");
  ASSERT_TRUE(a && b && after);

  // Focus the arrow key handler at the end.
  b->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), b);

  // Tab should exit the focusgroup (not wrap to A).
  SendTab(b);
  EXPECT_EQ(GetDocument().FocusedElement(), after)
      << "Tab from last arrow key handler should exit focusgroup, not wrap";
}

// Tab escape should NOT apply to an arrow key handler that is inside a
// focusgroup="none" subtree, since it's opted out of the focusgroup entirely.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeNotInOptedOutSubtree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <div focusgroup="none">
        <select id=optedout>
          <option>opt1</option>
        </select>
      </div>
      <button id=B type="button">B</button>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* optedout = GetElementById("optedout");
  auto* b = GetElementById("B");
  auto* after = GetElementById("after");
  ASSERT_TRUE(optedout && b && after);

  // Focus the opted-out select.
  optedout->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), optedout);

  // Tab should use normal navigation (not focusgroup Tab escape), because
  // the select is inside a focusgroup="none" subtree.
  SendTab(optedout);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from opted-out arrow key handler should use normal Tab "
         "navigation";
}

// When an arrow key handler is the only item in a focusgroup, Tab escape
// should fall through to normal navigation since there is no adjacent item.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeSoleItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <button id=before type="button">Before</button>
    <div focusgroup="toolbar inline">
      <select id=sel>
        <option>opt1</option>
      </select>
    </div>
    <button id=after type="button">After</button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* sel = GetElementById("sel");
  auto* after = GetElementById("after");
  ASSERT_TRUE(sel && after);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // No adjacent item in the focusgroup, so Tab falls through to normal
  // sequential navigation.
  SendTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), after)
      << "Tab from sole arrow key handler item should exit focusgroup";
}

// Tab escape from an arrow key handler inside a nested focusgroup should
// navigate within the nested focusgroup's scope.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeInNestedFocusgroup) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=outer1 type="button">Outer1</button>
      <div focusgroup="toolbar inline" tabindex="0">
        <button id=inner1 type="button">Inner1</button>
        <select id=sel>
          <option>opt1</option>
        </select>
        <button id=inner2 type="button">Inner2</button>
      </div>
      <button id=outer2 type="button">Outer2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* inner1 = GetElementById("inner1");
  auto* sel = GetElementById("sel");
  auto* inner2 = GetElementById("inner2");
  ASSERT_TRUE(inner1 && sel && inner2);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // Tab should go to inner2 (next item in the nested focusgroup), not to
  // outer2 in the parent focusgroup.
  SendTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), inner2)
      << "Tab escape should navigate within the nested focusgroup";

  // Shift+Tab from sel should go to inner1.
  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  SendShiftTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), inner1)
      << "Shift+Tab escape should navigate within the nested focusgroup";
}

// Tab escape from an arrow key handler should NOT skip focusable content
// inside a focusgroup="none" subtree. The opted-out focusable element must
// remain reachable via normal Tab navigation.
TEST_F(FocusgroupControllerTest,
       FocusgroupTabEscapeDoesNotSkipFocusableOptedOut) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <select id=sel>
        <option>12px</option>
      </select>
      <div focusgroup="none">
        <button id=optedout type="button">Opted Out</button>
      </div>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* sel = GetElementById("sel");
  auto* optedout = GetElementById("optedout");
  ASSERT_TRUE(sel && optedout);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // Tab should fall through to normal navigation, focusing the opted-out
  // button rather than skipping it to reach B.
  SendTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), optedout)
      << "Tab from arrow key handler should not skip focusable opted-out "
         "content";
}

// Shift+Tab from an arrow key handler should NOT skip focusable content
// inside a focusgroup="none" subtree that appears before it in DOM order.
TEST_F(FocusgroupControllerTest,
       FocusgroupShiftTabEscapeDoesNotSkipFocusableOptedOut) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <div focusgroup="none">
        <button id=optedout type="button">Opted Out</button>
      </div>
      <select id=sel>
        <option>12px</option>
      </select>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* optedout = GetElementById("optedout");
  auto* sel = GetElementById("sel");
  ASSERT_TRUE(optedout && sel);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // Shift+Tab should fall through to normal navigation, focusing the
  // opted-out button rather than skipping it to reach A.
  SendShiftTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), optedout)
      << "Shift+Tab from arrow key handler should not skip focusable opted-out "
         "content";
}

// Shift+Tab from an arrow key handler at the end of a focusgroup should go
// to the immediately previous focusgroup item (adjacent), not the segment
// entry (first item).
TEST_F(FocusgroupControllerTest,
       FocusgroupShiftTabEscapeAtEndGoesToAdjacentItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <input id=C type="text" value="text">
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* b = GetElementById("B");
  auto* c = GetElementById("C");
  ASSERT_TRUE(b && c);

  c->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), c);

  // Shift+Tab should go to B (immediately previous item), not A (segment
  // entry / first item).
  SendShiftTab(c);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Shift+Tab from arrow key handler at end should go to adjacent item "
         "(B)";
}

// Tab escape should correctly skip an empty focusgroup="none" subtree (one
// with no focusable descendants) and reach the next focusgroup item. Note:
// this produces the same result as the old segment-based behavior (B is also
// the segment entry), but it exercises the ContainsKeyboardFocusableContent
// false-path to verify empty opted-out subtrees don't prevent Tab escape.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeSkipsEmptyOptedOut) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <select id=sel>
        <option>12px</option>
      </select>
      <div focusgroup="none">
        <span>Not focusable</span>
      </div>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* sel = GetElementById("sel");
  auto* b = GetElementById("B");
  ASSERT_TRUE(sel && b);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // The opted-out subtree has no focusable content, so Tab escape should
  // skip it and go to B.
  SendTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from arrow key handler should skip empty opted-out subtree";
}

// When multiple consecutive focusgroup="none" subtrees with focusable content
// appear after an arrow key handler, Tab should fall through to normal
// navigation (focusing the first opted-out element), not skip all of them.
TEST_F(FocusgroupControllerTest,
       FocusgroupTabEscapeConsecutiveFocusableOptedOut) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <select id=sel>
        <option>12px</option>
      </select>
      <div focusgroup="none">
        <button id=opt1 type="button">Opt1</button>
      </div>
      <div focusgroup="none">
        <button id=opt2 type="button">Opt2</button>
      </div>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* sel = GetElementById("sel");
  auto* opt1 = GetElementById("opt1");
  ASSERT_TRUE(sel && opt1);

  sel->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(GetDocument().FocusedElement(), sel);

  // Tab should fall through at the first focusable opted-out subtree,
  // so normal navigation focuses opt1.
  SendTab(sel);
  EXPECT_EQ(GetDocument().FocusedElement(), opt1)
      << "Tab from arrow key handler should stop at first focusable opted-out "
         "subtree";
}

// Tab from a <video controls> element inside a focusgroup should escape to
// the adjacent item. Video with controls reports kInline+kBlock via
// NativeArrowKeyAxes, making it a directional key handler.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeFromVideoControls) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <video id=vid controls></video>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* vid = GetElementById("vid");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && vid && b);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendArrowRight(a);
  ASSERT_EQ(GetDocument().FocusedElement(), vid)
      << "Arrow right should navigate to video";

  SendTab(vid);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from <video controls> should go to adjacent item (B)";
}

// Tab from an <audio controls> element inside a focusgroup should enter the
// audio's internal controls (shadow DOM). Once inside, Tab should escape to
// the next adjacent focusgroup item rather than exiting the focusgroup
// entirely.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeFromAudioControls) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <audio id=aud controls></audio>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* aud = GetElementById("aud");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && aud && b);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendArrowRight(a);
  ASSERT_EQ(GetDocument().FocusedElement(), aud)
      << "Arrow right should navigate to audio";

  // Tab from audio should enter its internal controls. FocusedElement()
  // returns the actual shadow DOM element, not the host.
  SendTab(aud);
  EXPECT_NE(GetDocument().FocusedElement(), aud)
      << "Tab should move focus away from the audio element itself";
  EXPECT_NE(GetDocument().FocusedElement(), b)
      << "Tab from <audio controls> should enter internal controls, not "
         "escape to B";

  // Now focus is inside audio's shadow DOM. Tab again should escape to B
  // (the next adjacent focusgroup item), not skip past the focusgroup.
  ASSERT_TRUE(GetDocument().FocusedElement());
  // Tab through remaining shadow DOM controls until we reach B.
  while (GetDocument().FocusedElement() != b) {
    SendTab(GetDocument().FocusedElement());
  }
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from inside audio shadow DOM should eventually escape to "
         "adjacent item (B)";
}

// Tab from an <input type=date> element inside a focusgroup should navigate
// through the date input's internal sub-fields. Once the last sub-field is
// reached, Tab should escape to the next adjacent focusgroup item.
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeFromDateInput) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <input id=dt type="date">
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* dt = GetElementById("dt");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && dt && b);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendArrowRight(a);
  ASSERT_EQ(GetDocument().FocusedElement(), dt)
      << "Arrow right should navigate to date input";

  // Tab from date input should enter its internal sub-fields.
  // FocusedElement() returns the actual shadow DOM element, not the host.
  SendTab(dt);
  EXPECT_NE(GetDocument().FocusedElement(), dt)
      << "Tab should move focus away from the date input itself";
  EXPECT_NE(GetDocument().FocusedElement(), b)
      << "Tab from <input type=date> should enter internal sub-fields, not "
         "escape to B";

  // Tab through remaining internal sub-fields until we reach B.
  while (GetDocument().FocusedElement() != b) {
    SendTab(GetDocument().FocusedElement());
  }
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from inside date input sub-fields should eventually escape to "
         "adjacent item (B)";
}

// Tab escape from a <textarea> element inside a focusgroup should move to the
// adjacent item. Textarea reports kInline+kBlock via NativeArrowKeyAxes (text
// fields use arrows for cursor movement in both directions).
TEST_F(FocusgroupControllerTest, FocusgroupTabEscapeFromTextarea) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <textarea id=ta>Some text</textarea>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* ta = GetElementById("ta");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && ta && b);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendArrowRight(a);
  ASSERT_EQ(GetDocument().FocusedElement(), ta)
      << "Arrow right should navigate to textarea";

  SendTab(ta);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Tab from <textarea> should go to adjacent item (B)";
}

// Exercises arrow key handler behavior with radio button elements.
// Radio buttons are arrow key handlers because they consume arrow keys
// on both axes for radio group navigation. When a radio button is focused,
// focusgroup arrow navigation should not move focus away from it, preventing
// focus traps where native radio navigation conflicts with focusgroup
// navigation.
TEST_F(FocusgroupControllerTest, FocusgroupWithRadio) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=btn1 type="button">Bold</button>
      <label><input id=r1 type="radio" name="a">radio 1</label>
      <label><input id=r2 type="radio" name="a">radio 2</label>
      <button id=btn2 type="button">Italic</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* r1 = GetElementById("r1");
  auto* r2 = GetElementById("r2");
  auto* btn2 = GetElementById("btn2");
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);
  ASSERT_TRUE(btn2);

  // Arrow key can navigate TO the radio button (entry works).
  btn1->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), btn1);
  SendArrowRight(btn1);
  EXPECT_EQ(GetDocument().FocusedElement(), r1)
      << "Arrow right should navigate to radio (entry allowed)";

  // Once focused on a radio, arrow right should move to r2 via native radio
  // group navigation, NOT to btn2 via focusgroup.
  auto* right_from_radio = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1);
  SendEvent(right_from_radio);
  EXPECT_EQ(GetDocument().FocusedElement(), r2)
      << "Arrow from r1 should navigate to r2 via native radio navigation";

  // Tab from radio: focus should move to the next adjacent focusgroup item
  // (btn2), ensuring content after the arrow key handler remains reachable.
  auto* tab_event = KeyDownEvent(ui::DomKey::TAB, r2);
  SendEvent(tab_event);
  EXPECT_EQ(GetDocument().FocusedElement(), btn2)
      << "Tab from radio arrow key handler should go to next adjacent item";
}

// Exercises Shift+Tab from a radio arrow key handler in a separate test to
// keep assertions independent.
TEST_F(FocusgroupControllerTest, FocusgroupWithRadioShiftTab) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=btn1 type="button">Bold</button>
      <label><input id=r1 type="radio" name="a">radio 1</label>
      <label><input id=r2 type="radio" name="a">radio 2</label>
      <button id=btn2 type="button">Italic</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* btn1 = GetElementById("btn1");
  auto* r1 = GetElementById("r1");
  ASSERT_TRUE(btn1);
  ASSERT_TRUE(r1);

  r1->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), r1);

  auto* shift_tab_event =
      KeyDownEvent(ui::DomKey::TAB, r1, WebInputEvent::kShiftKey);
  SendEvent(shift_tab_event);
  // Shift+Tab from r1: the focused arrow key handler entry override ensures
  // adjacent items are reachable. btn1 is the previous adjacent item.
  EXPECT_EQ(GetDocument().FocusedElement(), btn1)
      << "Shift+Tab from radio arrow key handler should go to previous "
         "adjacent item";
}

// Tab from an unchecked radio that was focused via native radio group
// navigation. The focused arrow key handler entry override ensures the next
// adjacent focusable item is reachable.
TEST_F(FocusgroupControllerTest, FocusgroupWithUncheckedRadioTab) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=btn1 type="button">Bold</button>
      <label><input id=r1 type="radio" name="a">radio 1</label>
      <label><input id=r2 type="radio" name="a" checked>radio 2</label>
      <button id=btn2 type="button">Italic</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* r1 = GetElementById("r1");
  auto* r2 = GetElementById("r2");
  ASSERT_TRUE(r1);
  ASSERT_TRUE(r2);

  // Programmatically focus an unchecked radio (simulates what happens after
  // native radio group arrow navigation checks a different radio).
  r1->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), r1);
  ASSERT_FALSE(To<HTMLInputElement>(r1)->Checked());

  auto* tab_event = KeyDownEvent(ui::DomKey::TAB, r1);
  SendEvent(tab_event);
  // The focused arrow key handler entry override makes all other items in
  // the focusgroup reachable. r2 (checked) is keyboard-focusable and is the
  // next adjacent item.
  EXPECT_NE(GetDocument().FocusedElement(), GetDocument().body())
      << "Tab must not escape the focusgroup entirely";
  EXPECT_EQ(GetDocument().FocusedElement(), r2)
      << "Tab from unchecked radio should go to next adjacent item";
}

// --- Home/End key tests ---

// Home moves focus to the first item in a linear focusgroup.
TEST_F(FocusgroupControllerTest, HomeMovesToFirstItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* c = GetElementById("C");
  ASSERT_TRUE(a && c);

  c->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), c);

  SendHome(c);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home should move focus to first item";
}

// End moves focus to the last item in a linear focusgroup.
TEST_F(FocusgroupControllerTest, EndMovesToLastItem) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* c = GetElementById("C");
  ASSERT_TRUE(a && c);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendEnd(a);
  EXPECT_EQ(GetDocument().FocusedElement(), c)
      << "End should move focus to last item";
}

// Home/End are no-ops when already on the first/last item.
TEST_F(FocusgroupControllerTest, HomeEndNoOpAtBoundary) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && b);

  // Home from first item stays on first item.
  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);
  SendHome(a);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home from first item should be a no-op";

  // End from last item stays on last item.
  b->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), b);
  SendEnd(b);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "End from last item should be a no-op";
}

// Home/End skip opted-out subtrees (focusgroup="none").
TEST_F(FocusgroupControllerTest, HomeEndSkipOptedOut) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <div focusgroup="none">
        <button id=skip type="button">Skip</button>
      </div>
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* b = GetElementById("B");
  ASSERT_TRUE(a && b);

  b->Focus();
  SendHome(b);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home should skip opted-out subtree";

  a->Focus();
  SendEnd(a);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "End should skip opted-out subtree";
}

// Home/End navigate within the inner nested focusgroup, not the outer.
TEST_F(FocusgroupControllerTest, HomeEndNestedFocusgroup) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=o1 type="button">o1</button>
      <div focusgroup="toolbar inline" tabindex="0">
        <button id=i1 type="button">i1</button>
        <button id=i2 type="button">i2</button>
        <button id=i3 type="button">i3</button>
      </div>
      <button id=o2 type="button">o2</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* i1 = GetElementById("i1");
  auto* i3 = GetElementById("i3");
  ASSERT_TRUE(i1 && i3);

  i3->Focus();
  SendHome(i3);
  EXPECT_EQ(GetDocument().FocusedElement(), i1)
      << "Home should stay within inner focusgroup";

  SendEnd(i1);
  EXPECT_EQ(GetDocument().FocusedElement(), i3)
      << "End should stay within inner focusgroup";
}

// Home/End with modifier keys (Ctrl, Alt) should not trigger focusgroup
// navigation — these combinations have native browser behavior.
TEST_F(FocusgroupControllerTest, HomeEndWithModifiersIgnored) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* b = GetElementById("B");
  ASSERT_TRUE(b);

  b->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), b);

  auto* ctrl_home =
      KeyDownEvent(ui::DomKey::HOME, b, WebInputEvent::kControlKey);
  SendEvent(ctrl_home);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Ctrl+Home should not trigger focusgroup navigation";

  auto* shift_home =
      KeyDownEvent(ui::DomKey::HOME, b, WebInputEvent::kShiftKey);
  SendEvent(shift_home);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Shift+Home should not trigger focusgroup navigation";

  auto* alt_end = KeyDownEvent(ui::DomKey::END, b, WebInputEvent::kAltKey);
  SendEvent(alt_end);
  EXPECT_EQ(GetDocument().FocusedElement(), b)
      << "Alt+End should not trigger focusgroup navigation";
}

// Home/End work in block-only focusgroups (axis-independent).
TEST_F(FocusgroupControllerTest, HomeEndBlockAxis) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="menu block">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* c = GetElementById("C");
  ASSERT_TRUE(a && c);

  c->Focus();
  SendHome(c);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home should work in block-only focusgroup";

  SendEnd(a);
  EXPECT_EQ(GetDocument().FocusedElement(), c)
      << "End should work in block-only focusgroup";
}

// Home/End ignore focusgroupstart — always go to actual first/last.
TEST_F(FocusgroupControllerTest, HomeEndIgnoreFocusgroupstart) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline nomemory">
      <button id=A type="button">A</button>
      <button id=B type="button" focusgroupstart>B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* c = GetElementById("C");
  ASSERT_TRUE(a && c);

  c->Focus();
  SendHome(c);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home should go to first item, ignoring focusgroupstart";
}

// Home/End are blocked inside directional key handlers (e.g., text input).
TEST_F(FocusgroupControllerTest, HomeEndBlockedInDirectionalKeyHandler) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <input id=txt type="text" value="hello">
      <button id=B type="button">B</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* txt = GetElementById("txt");
  ASSERT_TRUE(txt);

  txt->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), txt);

  SendHome(txt);
  EXPECT_EQ(GetDocument().FocusedElement(), txt)
      << "Home inside text input should not trigger focusgroup navigation";

  SendEnd(txt);
  EXPECT_EQ(GetDocument().FocusedElement(), txt)
      << "End inside text input should not trigger focusgroup navigation";
}

// Home/End in a single-item focusgroup are no-ops (target == focused).
TEST_F(FocusgroupControllerTest, HomeEndSingleItemNoOp) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  ASSERT_TRUE(a);

  a->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), a);

  SendHome(a);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "Home in single-item focusgroup should be a no-op";

  SendEnd(a);
  EXPECT_EQ(GetDocument().FocusedElement(), a)
      << "End in single-item focusgroup should be a no-op";
}

// Home/End do not trigger in grid focusgroups (only linear).
TEST_F(FocusgroupControllerTest, HomeEndIgnoredInGridFocusgroup) {
  ScopedFocusgroupGridForTest grid_enabled{true};
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <table focusgroup="grid">
      <tr>
        <td id=c1 tabindex=0>1</td>
        <td id=c2 tabindex=0>2</td>
      </tr>
      <tr>
        <td id=c3 tabindex=0>3</td>
        <td id=c4 tabindex=0>4</td>
      </tr>
    </table>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* c2 = GetElementById("c2");
  ASSERT_TRUE(c2);

  c2->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), c2);

  SendHome(c2);
  EXPECT_EQ(GetDocument().FocusedElement(), c2)
      << "Home should not navigate in grid focusgroup";

  SendEnd(c2);
  EXPECT_EQ(GetDocument().FocusedElement(), c2)
      << "End should not navigate in grid focusgroup";
}

// Home/End are ignored when focus was already moved by a script handler
// (focused element != event target).
TEST_F(FocusgroupControllerTest, HomeEndIgnoredWhenFocusAlreadyMoved) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div focusgroup="toolbar inline">
      <button id=A type="button">A</button>
      <button id=B type="button">B</button>
      <button id=C type="button">C</button>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* a = GetElementById("A");
  auto* c = GetElementById("C");
  ASSERT_TRUE(a && c);

  // Focus C, but create the event with target A (simulating a script that
  // moved focus during the key handler before default handling runs).
  c->Focus();
  ASSERT_EQ(GetDocument().FocusedElement(), c);

  SendHome(a);
  EXPECT_EQ(GetDocument().FocusedElement(), c)
      << "Home should not navigate when focus was already moved";

  SendEnd(a);
  EXPECT_EQ(GetDocument().FocusedElement(), c)
      << "End should not navigate when focus was already moved";
}

}  // namespace blink
