// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ROOT_SCROLLER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ROOT_SCROLLER_UTIL_H_

namespace blink {

class LayoutBox;
class Node;
class PaintLayer;

namespace root_scroller_util {

// Returns the PaintLayer that'll be used as the root scrolling layer. For the
// <html> element and document Node, this returns the LayoutView's PaintLayer
// rather than <html>'s since scrolling is handled by LayoutView.
PaintLayer* PaintLayerForRootScroller(const Node*);

bool IsGlobal(const LayoutBox&);
bool IsGlobal(const PaintLayer&);
bool IsGlobal(const Node*);

}  // namespace root_scroller_util

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ROOT_SCROLLER_UTIL_H_
