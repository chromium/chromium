/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_REQUEST_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class LayoutObject;

class HitTestRequest {
  DISALLOW_NEW();

 public:
  enum RequestType {
    kReadOnly = 1 << 1,
    kActive = 1 << 2,
    kMove = 1 << 3,
    kRelease = 1 << 4,
    kIgnoreClipping = 1 << 5,
    kSVGClipContent = 1 << 6,
    kTouchEvent = 1 << 7,
    kAllowChildFrameContent = 1 << 8,
    kChildFrameHitTest = 1 << 9,
    kIgnorePointerEventsNone = 1 << 10,
    kRetargetForInert = 1 << 11,
    // Collect a list of nodes instead of just one.
    // (This is for elementsFromPoint and rect-based tests).
    kListBased = 1 << 12,
    // When using list-based testing, this flag causes us to continue hit
    // testing after a hit has been found.
    kPenetratingList = 1 << 13,
    kAvoidCache = 1 << 14,
    kIgnoreZeroOpacityObjects = 1 << 15,
    kHitTestVisualOverflow = 1 << 16,
  };

  typedef unsigned HitTestRequestType;

  HitTestRequest(HitTestRequestType request_type,
                 const LayoutObject* stop_node = nullptr)
      : request_type_(request_type), stop_node_(stop_node) {
    // Penetrating lists should also be list-based.
    DCHECK(!(request_type & kPenetratingList) || (request_type & kListBased));
  }

  bool ReadOnly() const { return request_type_ & kReadOnly; }
  bool Active() const { return request_type_ & kActive; }
  bool Move() const { return request_type_ & kMove; }
  bool Release() const { return request_type_ & kRelease; }
  bool IgnoreClipping() const { return request_type_ & kIgnoreClipping; }
  bool SvgClipContent() const { return request_type_ & kSVGClipContent; }
  bool TouchEvent() const { return request_type_ & kTouchEvent; }
  bool AllowsChildFrameContent() const {
    return request_type_ & kAllowChildFrameContent;
  }
  bool IsChildFrameHitTest() const {
    return request_type_ & kChildFrameHitTest;
  }
  bool IgnorePointerEventsNone() const {
    return request_type_ & kIgnorePointerEventsNone;
  }
  bool RetargetForInert() const { return request_type_ & kRetargetForInert; }
  bool ListBased() const { return request_type_ & kListBased; }
  bool PenetratingList() const { return request_type_ & kPenetratingList; }
  bool AvoidCache() const { return request_type_ & kAvoidCache; }

  // Convenience functions
  bool TouchMove() const { return Move() && TouchEvent(); }

  HitTestRequestType GetType() const { return request_type_; }
  const LayoutObject* GetStopNode() const { return stop_node_; }

  // The Cacheability bits don't affect hit testing computation.
  // TODO(dtapuska): These bits really shouldn't be fields on the HitTestRequest
  // as they don't influence the result; but rather are hints on the output as
  // to what to do. Perhaps move these fields to another enum?
  static const HitTestRequestType kCacheabilityBits =
      kReadOnly | kActive | kMove | kRelease | kTouchEvent;
  bool EqualForCacheability(const HitTestRequest& value) const {
    return (request_type_ | kCacheabilityBits) ==
               (value.request_type_ | kCacheabilityBits) &&
           stop_node_ == value.stop_node_;
  }

 private:
  HitTestRequestType request_type_;
  // If non-null, do not hit test the children of this object.
  const LayoutObject* stop_node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TEST_REQUEST_H_
