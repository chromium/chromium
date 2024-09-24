// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {
// TODO(bokan): Move this into FragmentDirective after
// https://crrev.com/c/3216206 lands.
String RemoveFragmentDirectives(const String& url_fragment) {
  wtf_size_t directive_delimiter_ix = url_fragment.Find(":~:");
  if (directive_delimiter_ix == kNotFound)
    return url_fragment;

  return url_fragment.Substring(0, directive_delimiter_ix);
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

  String fragment =
      RemoveFragmentDirectives(url.FragmentIdentifier().ToString());
  Node* anchor_node = doc.FindAnchor(fragment);

  // Setting to null will clear the current target.
  auto* target = DynamicTo<Element>(anchor_node);
  doc.SetCSSTarget(target);

  if (doc.IsSVGDocument()) {
    if (auto* svg = DynamicTo<SVGSVGElement>(doc.documentElement())) {
      String decoded = DecodeURLEscapeSequences(fragment, DecodeURLMode::kUTF8);
      svg->SetViewSpec(svg->ParseViewSpec(decoded, target));
    }
  }

  if (target) {
    target->ActivateDisplayLockIfNeeded(
        DisplayLockActivationReason::kFragmentNavigation);
  }

  if (doc.IsSVGDocument() && (!frame.IsMainFrame() || !target))
    return nullptr;

  if (!anchor_node)
    return nullptr;

  // Element fragment anchors only need to be kept alive if they need scrolling.
  if (!should_scroll)
    return nullptr;

  HTMLDetailsElement::ExpandDetailsAncestors(*anchor_node);
  DisplayLockUtilities::RevealHiddenUntilFoundAncestors(*anchor_node);

  return MakeGarbageCollected<ElementFragmentAnchor>(*anchor_node, frame);
}

ElementFragmentAnchor::ElementFragmentAnchor(Node& anchor_node,
                                             LocalFrame& frame)
    : FragmentAnchor(frame),
      anchor_node_(&anchor_node),
      needs_focus_(!anchor_node.IsDocumentNode()) {
  DCHECK(frame_->View());
}

bool ElementFragmentAnchor::Invoke() {
  TRACE_EVENT("blink", "ElementFragmentAnchor::Invoke");
  if (!frame_ || !anchor_node_)
    return false;

  // Don't remove the fragment anchor until focus has been applied.
  if (!needs_invoke_)
    return needs_focus_;

  Document& doc = *frame_->GetDocument();

  if (!doc.HaveRenderBlockingResourcesLoaded() || !frame_->View())
    return true;

  Member<Element> element_to_scroll = DynamicTo<Element>(anchor_node_.Get());
  if (!element_to_scroll)
    element_to_scroll = doc.documentElement();

  if (element_to_scroll) {
    ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
    options->setBlock("start");
    options->setInlinePosition("nearest");
    ScrollElementIntoViewWithOptions(element_to_scroll, options);
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

  if (needs_focus_) {
    // Attempts to focus the anchor if we couldn't focus above. This can cause
    // script to run so we can't do it from Invoke.
    frame_->GetDocument()->EnqueueAnimationFrameTask(WTF::BindOnce(
        &ElementFragmentAnchor::ApplyFocusIfNeeded, WrapPersistent(this)));
  }

  needs_invoke_ = true;
}

void ElementFragmentAnchor::DidScroll(mojom::blink::ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  // If the user/page scrolled, avoid clobbering the scroll offset by removing
  // the anchor on the next invocation. Note: we may get here as a result of
  // calling Invoke() because of the ScrollIntoView but that's ok because
  // needs_invoke_ is recomputed at the end of that method.
  needs_invoke_ = false;
}

void ElementFragmentAnchor::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_node_);
  visitor->Trace(frame_);
  FragmentAnchor::Trace(visitor);
}

void ElementFragmentAnchor::ApplyFocusIfNeeded() {
  // SVG images can load synchronously during style recalc but it's ok to focus
  // since we disallow scripting. For everything else, focus() could run script
  // so make sure we're at a valid point to do so.
  DCHECK(frame_->GetDocument()->IsSVGDocument() ||
         !ScriptForbiddenScope::IsScriptForbidden());

  if (!needs_focus_)
    return;

  if (!anchor_node_) {
    needs_focus_ = false;
    return;
  }

  if (!frame_->GetDocument()->HaveRenderBlockingResourcesLoaded()) {
    return;
  }

  frame_->GetDocument()->UpdateStyleAndLayoutTree();

  // If caret browsing is enabled, move the caret to the beginning of the
  // fragment, or to the first non-inert position after it.
  if (frame_->IsCaretBrowsingEnabled()) {
    const Position& pos = Position::FirstPositionInOrBeforeNode(*anchor_node_);
    if (pos.IsConnected()) {
      frame_->Selection().SetSelection(
          SelectionInDOMTree::Builder().Collapse(pos).Build(),
          SetSelectionOptions::Builder()
              .SetShouldCloseTyping(true)
              .SetShouldClearTypingStyle(true)
              .SetDoNotSetFocus(true)
              .Build());
    }
  }

  // If the anchor accepts keyboard focus and fragment scrolling is allowed,
  // move focus there to aid users relying on keyboard navigation.
  // If anchorNode is not focusable or fragment scrolling is not allowed,
  // clear focus, which matches the behavior of other browsers.
  auto* element = DynamicTo<Element>(anchor_node_.Get());
  if (element && element->IsFocusable()) {
    element->Focus();
  } else {
    frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
        anchor_node_);
    frame_->GetDocument()->ClearFocusedElement();
  }
  needs_focus_ = false;
}

}  // namespace blink
