// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/scroll_into_view_options.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

Node* FindAnchorFromFragment(const String& fragment, Document& doc) {
  Element* anchor_node = doc.FindAnchor(fragment);

  // Implement the rule that "" and "top" both mean top of page as in other
  // browsers.
  if (!anchor_node &&
      (fragment.IsEmpty() || DeprecatedEqualIgnoringCase(fragment, "top")))
    return &doc;

  return anchor_node;
}

}  // namespace

ElementFragmentAnchor* ElementFragmentAnchor::TryCreate(const KURL& url,
                                                        LocalFrame& frame,
                                                        bool should_scroll) {
  DCHECK(frame.GetDocument());
  Document& doc = *frame.GetDocument();

  // If our URL has no ref, then we have no place we need to jump to.
  // OTOH If CSS target was set previously, we want to set it to 0, recalc
  // and possibly paint invalidation because :target pseudo class may have been
  // set (see bug 11321).
  // Similarly for svg, if we had a previous svgView() then we need to reset
  // the initial view if we don't have a fragment.
  if (!url.HasFragmentIdentifier() && !doc.CssTarget() && !doc.IsSVGDocument())
    return nullptr;

  String fragment = url.FragmentIdentifier();

  Node* anchor_node = nullptr;

  // Try the raw fragment for HTML documents, but skip it for `svgView()`:
  if (!doc.IsSVGDocument())
    anchor_node = FindAnchorFromFragment(fragment, doc);

  // https://html.spec.whatwg.org/C/#the-indicated-part-of-the-document
  // 5. Let decodedFragment be the result of running UTF-8 decode without BOM
  // on fragmentBytes.
  if (!anchor_node) {
    fragment = DecodeURLEscapeSequences(fragment, DecodeURLMode::kUTF8);
    anchor_node = FindAnchorFromFragment(fragment, doc);
  }

  // Setting to null will clear the current target.
  auto* target = DynamicTo<Element>(anchor_node);
  doc.SetCSSTarget(target);

  if (doc.IsSVGDocument()) {
    if (SVGSVGElement* svg = ToSVGSVGElementOrNull(doc.documentElement()))
      svg->SetupInitialView(fragment, target);
  }

  if (target) {
    target->ActivateDisplayLockIfNeeded(
        DisplayLockActivationReason::kFragmentNavigation);
    target->DispatchActivateInvisibleEventIfNeeded();
  }

  if (doc.IsSVGDocument() && (!frame.IsMainFrame() || !target))
    return nullptr;

  if (!anchor_node)
    return nullptr;

  // Element fragment anchors only need to be kept alive if they need scrolling.
  if (!should_scroll)
    return nullptr;

  return MakeGarbageCollected<ElementFragmentAnchor>(*anchor_node, frame);
}

ElementFragmentAnchor::ElementFragmentAnchor(Node& anchor_node,
                                             LocalFrame& frame)
    : anchor_node_(&anchor_node),
      frame_(&frame),
      needs_focus_(!anchor_node.IsDocumentNode()) {
  DCHECK(frame_->View());
}

bool ElementFragmentAnchor::Invoke() {
  if (!frame_ || !anchor_node_)
    return false;

  // Don't remove the fragment anchor until focus has been applied.
  if (!needs_invoke_)
    return needs_focus_;

  Document& doc = *frame_->GetDocument();

  if (!doc.HaveRenderBlockingResourcesLoaded() || !frame_->View())
    return true;

  Frame* boundary_frame = frame_->FindUnsafeParentScrollPropagationBoundary();

  // FIXME: Handle RemoteFrames
  auto* boundary_local_frame = DynamicTo<LocalFrame>(boundary_frame);
  if (boundary_local_frame) {
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(false);
  }

  auto* element_to_scroll = DynamicTo<Element>(anchor_node_.Get());
  if (!element_to_scroll)
    element_to_scroll = doc.documentElement();

  if (element_to_scroll) {
    ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
    options->setBlock("start");
    options->setInlinePosition("nearest");
    element_to_scroll->ScrollIntoViewNoVisualUpdate(options);
  }

  if (boundary_local_frame) {
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(true);
  }

  if (AXObjectCache* cache = doc.ExistingAXObjectCache())
    cache->HandleScrolledToAnchor(anchor_node_);

  // Scroll into view above will cause us to clear needs_invoke_ via the
  // DidScroll so recompute it here.
  needs_invoke_ = !doc.IsLoadCompleted() || needs_focus_;

  return needs_invoke_;
}

void ElementFragmentAnchor::Installed() {
  DCHECK(frame_->GetDocument());

  // If rendering isn't ready yet, we'll focus and scroll as part of the
  // document lifecycle.
  if (frame_->GetDocument()->HaveRenderBlockingResourcesLoaded())
    ApplyFocusIfNeeded();

  needs_invoke_ = true;
}

void ElementFragmentAnchor::DidScroll(ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  // If the user/page scrolled, avoid clobbering the scroll offset by removing
  // the anchor on the next invocation. Note: we may get here as a result of
  // calling Invoke() because of the ScrollIntoView but that's ok because
  // needs_invoke_ is recomputed at the end of that method.
  needs_invoke_ = false;
}

void ElementFragmentAnchor::DidCompleteLoad() {
  DCHECK(frame_);
  DCHECK(frame_->View());

  // If there is a pending layout, the fragment anchor will be cleared when it
  // finishes.
  if (!frame_->View()->NeedsLayout())
    needs_invoke_ = false;
}

void ElementFragmentAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(anchor_node_);
  visitor->Trace(frame_);
  FragmentAnchor::Trace(visitor);
}

void ElementFragmentAnchor::PerformPreRafActions() {
  ApplyFocusIfNeeded();
}

void ElementFragmentAnchor::ApplyFocusIfNeeded() {
  // SVG images can load synchronously during style recalc but it's ok to focus
  // since we disallow scripting. For everything else, focus() could run script
  // so make sure we're at a valid point to do so.
  DCHECK(frame_->GetDocument()->IsSVGDocument() ||
         !ScriptForbiddenScope::IsScriptForbidden());

  if (!needs_focus_)
    return;

  if (!frame_->GetDocument()->HaveRenderBlockingResourcesLoaded())
    return;

  if (!anchor_node_)
    return;

  // If the anchor accepts keyboard focus and fragment scrolling is allowed,
  // move focus there to aid users relying on keyboard navigation.
  // If anchorNode is not focusable or fragment scrolling is not allowed,
  // clear focus, which matches the behavior of other browsers.
  frame_->GetDocument()->UpdateStyleAndLayoutTree();
  auto* element = DynamicTo<Element>(anchor_node_.Get());
  if (element && element->IsFocusable()) {
    element->focus();
  } else {
    frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
        anchor_node_);
    frame_->GetDocument()->ClearFocusedElement();
  }
  needs_focus_ = false;
}

bool ElementFragmentAnchor::Dismiss() {
  return false;
}

}  // namespace blink
