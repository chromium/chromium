/*
    Copyright (C) 2007 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"

#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsPointerEventsHitRules {
  unsigned bitfields;
};

ASSERT_SIZE(PointerEventsHitRules, SameSizeAsPointerEventsHitRules);

PointerEventsHitRules::PointerEventsHitRules(EHitTesting hit_testing,
                                             const HitTestRequest& request,
                                             EPointerEvents pointer_events)
    : require_visible(false),
      require_fill(false),
      require_stroke(false),
      can_hit_stroke(false),
      can_hit_fill(false),
      can_hit_bounding_box(false) {
  if (request.SvgClipContent())
    pointer_events = EPointerEvents::kFill;

  if (hit_testing == kSvgGeometryHitTesting) {
    switch (pointer_events) {
      case EPointerEvents::kBoundingBox:
        can_hit_bounding_box = true;
        break;
      case EPointerEvents::kVisiblepainted:
      case EPointerEvents::kAuto:  // "auto" is like "visiblePainted" when in
                                   // SVG content
        require_fill = true;
        require_stroke = true;
        [[fallthrough]];
      case EPointerEvents::kVisible:
        require_visible = true;
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kVisiblefill:
        require_visible = true;
        can_hit_fill = true;
        break;
      case EPointerEvents::kVisiblestroke:
        require_visible = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kPainted:
        require_fill = true;
        require_stroke = true;
        [[fallthrough]];
      case EPointerEvents::kAll:
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kFill:
        can_hit_fill = true;
        break;
      case EPointerEvents::kStroke:
        can_hit_stroke = true;
        break;
      case EPointerEvents::kNone:
        // nothing to do here, defaults are all false.
        break;
    }
  } else {
    switch (pointer_events) {
      case EPointerEvents::kBoundingBox:
        can_hit_bounding_box = true;
        break;
      case EPointerEvents::kVisiblepainted:
      case EPointerEvents::kAuto:  // "auto" is like "visiblePainted" when in
                                   // SVG content
        require_visible = true;
        require_fill = true;
        require_stroke = true;
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kVisiblefill:
      case EPointerEvents::kVisiblestroke:
      case EPointerEvents::kVisible:
        require_visible = true;
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kPainted:
        require_fill = true;
        require_stroke = true;
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kFill:
      case EPointerEvents::kStroke:
      case EPointerEvents::kAll:
        can_hit_fill = true;
        can_hit_stroke = true;
        break;
      case EPointerEvents::kNone:
        // nothing to do here, defaults are all false.
        break;
    }
  }
}

}  // namespace blink
