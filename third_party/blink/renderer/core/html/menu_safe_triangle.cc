// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/menu_safe_triangle.h"

#include <algorithm>
#include <array>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {
// Amount to inflate each corner of the safe triangle (by pushing it away
// from the triangle's center), in pixels.
constexpr float kTrianglePadding = 10.f;

// How long we allow the mouse pointer to move in the safe triangle without
// crossing into the submenu.
constexpr base::TimeDelta kSafeTriangleDuration = base::Milliseconds(1000);

bool IsInclusiveFlatTreeAncestorOf(Node* possible_ancestor,
                                   Node* possible_descendant) {
  return std::ranges::contains(
      FlatTreeTraversal::InclusiveAncestorsOf(*possible_descendant),
      *possible_ancestor);
}

}  // namespace

/* static */
void MenuSafeTriangle::MaybeCreate(HTMLMenuItemElement* invoker_menu_item,
                                   HTMLMenuListElement* invoked_submenu) {
  // Only do safe triangle behavior for <menuitem>s in a <menulist>, not
  // a <menubar>.
  if (!IsA<HTMLMenuListElement>(invoker_menu_item->OwningMenuElement())) {
    return;
  }

  // Start from the last known mouse position, since we want to use the safe
  // triangle if the mouse position is inside the menuitem even if the user
  // invoked the menuitem through something other than the mouse (e.g., if
  // using a combination of mouse and keyboard).
  Document& document = invoker_menu_item->GetDocument();
  EventHandler& event_handler = document.GetFrame()->GetEventHandler();
  if (event_handler.IsMousePositionUnknown()) {
    return;
  }
  gfx::PointF last_mouse_position =
      event_handler.LastKnownMousePositionInRootFrame();

  // We might need to create the layout object for the submenu.
  invoked_submenu->GetDocument().UpdateStyleAndLayoutTreeForElement(
      invoked_submenu, DocumentUpdateReason::kPopover);

  LayoutObject* item_obj = invoker_menu_item->GetLayoutObject();
  LayoutObject* submenu_obj = invoked_submenu->GetLayoutObject();
  if (!item_obj || !submenu_obj) {
    return;
  }

  // Check whether the last known mouse position is inside the <menuitem>.
  //
  // This is perhaps a little unusual.  Arguably it might be more typical to
  // do this only when we've gotten to this point as a result of a mouse or
  // pointer event (and then use the coordinates from the event).  However,
  // using the last known position in all cases does lead to this feature
  // working in some additional cases when the user uses a mix of input
  // methods (such as a mix of keyboard and mouse input), which is arguably
  // better.
  //
  // Furthermore, passing the coordinates from the events through to this
  // function requires passing those events or coordinates through a rather
  // large number of functions, so this approach is also easier.
  Vector<gfx::QuadF> item_quads;
  item_obj->AbsoluteQuads(item_quads);
  if (std::ranges::none_of(item_quads, [&](const gfx::QuadF& quad) -> bool {
        return quad.Contains(last_mouse_position);
      })) {
    return;
  }

  Vector<gfx::QuadF> submenu_quads;
  submenu_obj->AbsoluteQuads(submenu_quads);
  if (submenu_quads.size() != 1) {
    // If the submenu is fragmented, we don't do a safe triangle.
    return;
  }

  const gfx::QuadF& submenu_quad = submenu_quads[0];

  // Find the edge of submenu_quad that's closest to last_mouse_position.  We
  // can't assume the edges are infinite lines, though, since if we extend the
  // edges infinitely we'll get the wrong answer.
  using EdgeType = std::pair<gfx::PointF, gfx::PointF>;
  const std::array<const EdgeType, 4> edge_options = {
      EdgeType(submenu_quad.p1(), submenu_quad.p2()),
      EdgeType(submenu_quad.p2(), submenu_quad.p3()),
      EdgeType(submenu_quad.p3(), submenu_quad.p4()),
      EdgeType(submenu_quad.p4(), submenu_quad.p1()),
  };
  auto distance_to_edge =
      [&last_mouse_position](const EdgeType& edge) -> float {
    gfx::Vector2dF to_p1 = edge.first - last_mouse_position;
    gfx::Vector2dF to_p2 = edge.second - last_mouse_position;
    gfx::Vector2dF edge_direction = edge.second - edge.first;
    edge_direction.Normalize();
    float dot_p1 = DotProduct(to_p1, edge_direction);
    float dot_p2 = DotProduct(to_p2, edge_direction);
    if (std::signbit(dot_p1) == std::signbit(dot_p2)) {
      // The closest point on the edge is one of the corners.
      //
      // TODO(https://crbug.com/406566432): If this corner is *actually* the
      // closest point (a rare case), do a better job breaking the tie between
      // the edges at that corner.
      return std::min(to_p1.Length(), to_p2.Length());
    } else {
      // Use the formula for closest point on an infinite line, since it's
      // between the corners.
      gfx::Vector2dF parallel_component = ScaleVector2d(edge_direction, dot_p1);
      gfx::Vector2dF perpendicular_component = to_p1 - parallel_component;
      return perpendicular_component.Length();
    }
  };
  std::array<float, 4> distances;
  std::ranges::transform(edge_options, distances.begin(), distance_to_edge);

  auto min_distance = std::ranges::min_element(distances);
  const EdgeType& closest_edge = edge_options[min_distance - distances.begin()];

  // Represent the triangle as a degenerate quad, repeating the
  // click point twice.
  gfx::QuadF triangle(last_mouse_position, last_mouse_position,
                      closest_edge.first, closest_edge.second);

  // Not a normal concept of center given the degenerate quad above, but
  // probably good enough.
  gfx::PointF center = triangle.CenterPoint();
  auto pad_corner = [&center](const gfx::PointF& p) -> gfx::PointF {
    gfx::Vector2dF offset = p - center;
    auto len = offset.Length();
    if (len != 0) {
      offset.Scale(kTrianglePadding / len);
    }
    return p + offset;
  };
  triangle.set_p1(pad_corner(triangle.p1()));
  triangle.set_p2(pad_corner(triangle.p2()));
  triangle.set_p3(pad_corner(triangle.p3()));
  triangle.set_p4(pad_corner(triangle.p4()));

  MenuSafeTriangle* safe_triangle = MakeGarbageCollected<MenuSafeTriangle>(
      invoker_menu_item, invoked_submenu, triangle, submenu_quad);
  document.SetMenuSafeTriangle(safe_triangle);
}

void MenuSafeTriangle::Recheck() {
  Document& document = invoker_menu_item_->GetDocument();
  CHECK_EQ(document.GetMenuSafeTriangle(), this);

  Document::PopoverStack& menu_stack = document.MenuStack();
  wtf_size_t submenu_index = menu_stack.Find(invoked_submenu_);
  if (submenu_index == kNotFound) {
    // Our submenu is no longer open.
    DCHECK(!invoked_submenu_->popoverOpen());
    Finish();
    return;
  }

  if (std::ranges::any_of(
          base::span(menu_stack).subspan(submenu_index + 1),
          [](HTMLElement* e) -> bool { return IsA<HTMLMenuListElement>(e); })) {
    // There is another menu open, on top of our submenu in the popover stack.
    // The innermost submenu is the only one that should have a safe triangle.
    Finish();
    return;
  }

  // TODO(https://crbug.com/406566432): We probably want to recompute both the
  // submenu quad and the triangle on every Recheck() (to account for menus
  // that quickly animate into their final position) rather than caching them
  // only at the start of the safe triangle operation.

  EventHandler& event_handler = document.GetFrame()->GetEventHandler();
  if (!event_handler.IsMousePositionUnknown()) {
    gfx::PointF last_mouse_position =
        event_handler.LastKnownMousePositionInRootFrame();
    if (triangle_.Contains(last_mouse_position)) {
      // The mouse is still in the safe triangle.
      return;
    }

    // We also need to check if the mouse is in the submenu itself, since the
    // submenu contains area outside of the safe triangle, and we need to
    // continue the safe triangle until we gain interest on the submenu or its
    // descendant (which waits for a timer).
    // TODO(https://crbug.com/406566432): We should stop the safe triangle
    // timer from expiring if we've reached the submenu.
    if (submenu_quad_.Contains(last_mouse_position)) {
      // The mouse is in the submenu.
      return;
    }
  }

  // The mouse is no longer in the safe triangle or the submenu.  Remove
  // ourself.
  Finish();
}

MenuSafeTriangle::MenuSafeTriangle(HTMLMenuItemElement* invoker_menu_item,
                                   HTMLMenuListElement* invoked_submenu,
                                   const gfx::QuadF& triangle,
                                   const gfx::QuadF& submenu_quad)
    : invoker_menu_item_(invoker_menu_item),
      invoked_submenu_(invoked_submenu),
      triangle_(triangle),
      submenu_quad_(submenu_quad),
      expire_timer_(invoker_menu_item->GetExecutionContext()->GetTaskRunner(
                        TaskType::kInternalDefault),
                    this,
                    &MenuSafeTriangle::ExpireTimerFired) {
  CHECK(invoker_menu_item_);
  CHECK(invoked_submenu_);
  // TODO(https://crbug.com/406566432): Right now this is just a simple timer.
  // But it's possible that we want a pair of timers:  a longer one created
  // with the safe triangle as this one is, and a shorter one whose timeout
  // gets reset on mouse movement.
  expire_timer_.StartOneShot(kSafeTriangleDuration, FROM_HERE);
}

void MenuSafeTriangle::Trace(Visitor* visitor) const {
  visitor->Trace(invoker_menu_item_);
  visitor->Trace(invoked_submenu_);
  visitor->Trace(deferred_interest_gained_);
  visitor->Trace(deferred_interest_lost_);
  visitor->Trace(expire_timer_);
}

void MenuSafeTriangle::InterestGainedData::Trace(Visitor* visitor) const {
  visitor->Trace(invoker);
  visitor->Trace(target);
}

void MenuSafeTriangle::InterestLostData::Trace(Visitor* visitor) const {
  visitor->Trace(invoker);
  visitor->Trace(target);
}

void MenuSafeTriangle::Finish(bool from_timer) {
  Document& document = invoker_menu_item_->GetDocument();
  CHECK_EQ(document.GetMenuSafeTriangle(), this);
  document.SetMenuSafeTriangle(nullptr);

  for (const InterestLostData* data : deferred_interest_lost_) {
    data->invoker->InterestLost(data->target, data->cancelable, data->behavior);
  }
  for (const InterestGainedData* data : deferred_interest_gained_) {
    data->invoker->InterestGained(data->target, data->state);
  }

  // Clear out our state so that we'll notice if Finish is called twice, but
  // also cancel the timer to ensure (along with the
  // SetMenuSafeTriangle(nullptr) above) that it's not.
  invoker_menu_item_ = nullptr;
  invoked_submenu_ = nullptr;
  deferred_interest_lost_.clear();
  deferred_interest_gained_.clear();
  if (!from_timer) {
    expire_timer_.Stop();
  }
}

void MenuSafeTriangle::ExpireTimerFired(TimerBase* timer) {
  Finish(/*from_timer=*/true);
}

bool MenuSafeTriangle::ShouldDeferInterestGained(Element* invoker,
                                                 Element* target,
                                                 Element::InterestState state) {
  auto removed_losts = std::ranges::remove_if(
      deferred_interest_lost_, [invoker](const InterestLostData* data) -> bool {
        return data->invoker == invoker;
      });
  if (!removed_losts.empty()) {
    deferred_interest_lost_.erase(removed_losts.begin(), removed_losts.end());
    // We removed the deferred lost(s), so say that we're deferring this gain,
    // since the gain and the loss cancel each other out.
    return true;
  }

  // If the gain is on the submenu or its descendant, we've served our purpose
  // since the mouse pointer has now reached the submenu.
  if (IsInclusiveFlatTreeAncestorOf(invoked_submenu_, invoker)) {
    Finish();
    return false;
  }

  // If the gain is on the menuitem or its descendant, don't defer it.
  if (IsInclusiveFlatTreeAncestorOf(invoker_menu_item_, invoker)) {
    return false;
  }

  auto it =
      std::ranges::find_if(deferred_interest_gained_,
                           [invoker](const InterestGainedData* data) -> bool {
                             return data->invoker == invoker;
                           });
  if (it != deferred_interest_gained_.end()) {
    // We have an existing deferred entry.  Mutate it to match this one (does
    // that make sense?) and continue deferring.
    (*it)->state = state;
    return true;
  }

  deferred_interest_gained_.push_back(
      MakeGarbageCollected<InterestGainedData>(invoker, target, state));
  return true;
}

bool MenuSafeTriangle::ShouldDeferInterestLost(
    Element* invoker,
    Element* target,
    Element::InterestLostCancelable cancelable,
    Element::InterestLostPopoverBehavior behavior) {
  auto removed_gains =
      std::ranges::remove_if(deferred_interest_gained_,
                             [invoker](const InterestGainedData* data) -> bool {
                               return data->invoker == invoker;
                             });
  if (!removed_gains.empty()) {
    deferred_interest_gained_.erase(removed_gains.begin(), removed_gains.end());
    // We removed the deferred gain(s), so say that we're deferring this lost,
    // since the gain and the loss cancel each other out.
    return true;
  }

  // Only defer interest lost on the submenu and items higher in the popover
  // stack.
  auto can_defer = [this, target]() -> bool {
    Document::PopoverStack& menu_stack =
        invoked_submenu_->GetDocument().MenuStack();
    wtf_size_t index = menu_stack.Find(invoked_submenu_);
    if (index != kNotFound) {
      do {
        if (menu_stack[index] == target) {
          return true;
        }
      } while (index-- != 0);
    }
    return false;
  };
  if (!can_defer()) {
    return false;
  }

  auto it = std::ranges::find_if(
      deferred_interest_lost_, [invoker](const InterestLostData* data) -> bool {
        return data->invoker == invoker;
      });
  if (it != deferred_interest_lost_.end()) {
    // We have an existing deferred entry.  Mutate it to match this one (does
    // that make sense?) and continue deferring.
    (*it)->cancelable = cancelable;
    (*it)->behavior = behavior;
    return true;
  }

  deferred_interest_lost_.push_back(MakeGarbageCollected<InterestLostData>(
      invoker, target, cancelable, behavior));
  return true;
}

}  // namespace blink
