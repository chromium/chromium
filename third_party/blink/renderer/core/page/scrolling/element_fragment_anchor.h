// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ELEMENT_FRAGMENT_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ELEMENT_FRAGMENT_ANCHOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LocalFrame;
class KURL;

// An element fragment anchor is a FragmentAnchor based on a single element.
// This is the traditional fragment anchor of the web. For example, the fragment
// string will be used to find an element in the page with a matching id and use
// that to scroll into view and focus.
//
// While the page is loading, the fragment anchor tries to repeatedly scroll
// the element into view since it's position may change as a result of layouts.
// TODO(bokan): Maybe we no longer need the repeated scrolling since that
// should be handled by scroll-anchoring?
class CORE_EXPORT ElementFragmentAnchor final : public FragmentAnchor {
 public:
  // Parses the URL fragment and, if possible, creates and returns a fragment
  // based on an Element in the page. Returns nullptr otherwise. Produces side
  // effects related to fragment targeting in the page in either case.
  static ElementFragmentAnchor* TryCreate(const KURL& url,
                                          LocalFrame& frame,
                                          bool should_scroll);

  ElementFragmentAnchor(Node& anchor_node, LocalFrame& frame);
  ~ElementFragmentAnchor() override = default;

  // Will attempt to scroll the anchor into view.
  bool Invoke() override;

  // Will attempt to focus the anchor.
  void Installed() override;

  // Used to let the anchor know the frame's been scrolled and so we should
  // abort keeping the fragment target in view to avoid fighting with user
  // scrolls.
  void DidScroll(ScrollType type) override;

  // Attempts to focus the anchor if we couldn't focus right after install
  // (because rendering was blocked at the time). This can cause script to run
  // so we can't do it in Invoke.
  void PerformPreRafActions() override;

  // We can dispose of the fragment once load has been completed.
  void DidCompleteLoad() override;

  // Does nothing as an element anchor does not have any dismissal work.
  bool Dismiss() override;

  void Trace(blink::Visitor*) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ElementFragmentAnchorTest,
                           AnchorRemovedBeforeBeginFrameCrash);

  void ApplyFocusIfNeeded();

  WeakMember<Node> anchor_node_;
  Member<LocalFrame> frame_;
  bool needs_focus_;

  // While this is true, the fragment is still "active" in the sense that we
  // want the owner to continue calling Invoke(). Once this is false, calling
  // Invoke has no effect and the fragment can be disposed (unless focus is
  // still needed).
  bool needs_invoke_ = false;

  DISALLOW_COPY_AND_ASSIGN(ElementFragmentAnchor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_ELEMENT_FRAGMENT_ANCHOR_H_
