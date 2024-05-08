// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGINATION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGINATION_STATE_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class ComputedStyle;
class Document;
class LayoutBlockFlow;
class LayoutObject;

// Holds structures and state needed during pagination (layout and painting).
//
// Created before layout for pagination, and destroyed when leaving paginated
// layout.
//
// For each page box that is being laid out, layout objects will be created for
// each part of a page box that may need to paint something (page container,
// page border box, and any @page margins).
//
// PaginationState also keeps track of the current page to print.
class PaginationState : public GarbageCollected<PaginationState> {
 public:
  PaginationState();

  void Trace(Visitor*) const;

  // Create an anonymous layout object to measure, lay out and paint some part
  // of a page box. A page box (and its margins) does not exist in the DOM, but
  // is created by layout. Such layout objects will not be attached to the
  // layout object tree. PaginationState "owns" the layout objects returned from
  // this function, so that they are destroyed in a controlled manner (layout
  // objects cannot simply be garbage collected).
  LayoutBlockFlow* CreateAnonymousPageLayoutObject(Document&,
                                                   const ComputedStyle&);

  // Destroy all layout objects created and returned from
  // CreateAnonymousPageLayoutObject().
  void DestroyAnonymousPageLayoutObjects();

  void SetCurrentPageNumber(wtf_size_t n) { current_page_number_ = n; }
  wtf_size_t CurrentPageNumber() const { return current_page_number_; }

 private:
  HeapVector<Member<LayoutObject>> anonymous_page_objects_;
  wtf_size_t current_page_number_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGINATION_STATE_H_
