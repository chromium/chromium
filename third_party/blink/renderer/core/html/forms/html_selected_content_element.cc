// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_selected_content_element.h"

#include "base/functional/bind.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/shadow_including_tree_order_traversal.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool ShouldOmitFromSelectedContent(const Element& element) {
  return element.HasTagName(html_names::kScriptTag) ||
         IsA<SVGScriptElement>(element) ||
         element.HasTagName(html_names::kFrameTag) ||
         element.HasTagName(html_names::kIFrameTag) ||
         element.HasTagName(html_names::kEmbedTag) ||
         element.HasTagName(html_names::kObjectTag);
}

// Clones the children of `option` and appends them to `nodes`, omitting
// script, SVG script, frame, iframe, embed, and object elements, including
// those in shadow trees.
// https://html.spec.whatwg.org/#clone-an-option-into-a-selectedcontent
void CloneOptionChildrenForSelectedContent(HTMLOptionElement& option,
                                           VectorOf<Node>& nodes) {
  HeapVector<Member<Element>> omitted_elements;
  for (Node& child : NodeTraversal::ChildrenOf(option)) {
    Node* clone = child.cloneNode(/*deep=*/true);
    if (RuntimeEnabledFeatures::SelectedcontentOmitScriptsAndFramesEnabled()) {
      for (Node& node :
           ShadowIncludingTreeOrderTraversal::InclusiveDescendantsOf(*clone)) {
        if (auto* element = DynamicTo<Element>(node);
            element && ShouldOmitFromSelectedContent(*element)) {
          omitted_elements.push_back(element);
        }
      }
      if (auto* element = DynamicTo<Element>(clone);
          element && ShouldOmitFromSelectedContent(*element)) {
        // This element would be removed from the document fragment, so just
        // don't include it.
        continue;
      }
    }
    nodes.push_back(clone);
  }
  // The clones aren't connected to a document yet, so removing elements from
  // them can't run script. None of these elements should be in user-agent
  // shadow trees, which shouldn't be modified. Omitted clones of the option's
  // children don't have a parent, so remove() does nothing for them.
  for (Element* element : omitted_elements) {
    CHECK(!element->IsInUserAgentShadowRoot());
    option.AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A <" + element->localName() +
            "> element in an <option> was not cloned into <selectedcontent>. "
            "script, frame, iframe, embed, and object elements are not cloned "
            "into <selectedcontent>.");
    element->remove();
  }
}

}  // namespace

HTMLSelectedContentElement::HTMLSelectedContentElement(Document& document)
    : HTMLElement(html_names::kSelectedcontentTag, document) {
  UseCounter::Count(document, WebFeature::kHTMLSelectedContentElement);
}

void HTMLSelectedContentElement::CloneContentsFromOptionElement(
    HTMLOptionElement* option) {
  if (disabled_) {
    return;
  }

  CHECK(!GetDocument().StatePreservingAtomicMoveInProgress());
  DCHECK(!ScriptForbiddenScope::IsScriptForbidden());
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  VectorOf<Node> nodes;
  if (option) {
    // The owner select may be null if the option was removed from its select
    // after the caller resolved it.
    HTMLSelectElement* owner_select = option->OwnerSelectElement();
    CHECK(!owner_select || !owner_select->IsMultiple());
    CloneOptionChildrenForSelectedContent(*option, nodes);
  }

  // `ASSERT_NO_EXCEPTION` is safe here because `ReplaceChildren()` only
  // throws exceptions when encountering DOM hierarchy errors, which
  // shouldn't happen here.
  ReplaceChildren(nodes, ASSERT_NO_EXCEPTION);
}

void HTMLSelectedContentElement::CloneMultipleOptionsFromSelectElement(
    HTMLSelectElement& select) {
  CHECK(RuntimeEnabledFeatures::SelectedcontentMultipleEnabled());
  CHECK(select.IsMultiple());
  // TODO(crbug.com/458113204): This disabled check does not exist in the spec.
  // It should be added to the spec or removed.
  if (disabled_) {
    return;
  }

  VectorOf<Node> nodes;

  for (Element* option : *select.selectedOptions()) {
    HTMLDivElement* container =
        MakeGarbageCollected<HTMLDivElement>(GetDocument());
    VectorOf<Node> option_nodes;
    CloneOptionChildrenForSelectedContent(To<HTMLOptionElement>(*option),
                                          option_nodes);
    for (Node* option_node : option_nodes) {
      container->appendChild(option_node);
    }
    nodes.push_back(container);
  }

  // `ASSERT_NO_EXCEPTION` is safe here because `ReplaceChildren()` only
  // throws exceptions when encountering DOM hierarchy errors, which
  // shouldn't happen here.
  ReplaceChildren(nodes, ASSERT_NO_EXCEPTION);
}

Node::InsertionNotificationRequest HTMLSelectedContentElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  // We need to call SelectedContentElementInserted in InsertedInto instead of
  // DidNotifySubtreeInsertionsToDocument because
  // DidNotifySubtreeInsertionsToDocument calls other methods which iterate
  // through all descendant selectedcontent elements within the nearest
  // ancestor select element, which we optimize with a TreeOrderedList which
  // gets updated in SelectedContentElementInserted.
  //
  // The disabled state is also calculated here instead of the post-connection
  // steps to avoid having time in between insertion and post-insertion where
  // the element isn't considered disabled yet.

  disabled_ = false;
  HTMLSelectElement* first_ancestor_select = nullptr;
  for (auto* ancestor = parentNode(); ancestor;
       ancestor = ancestor->parentNode()) {
    if (IsA<HTMLOptionElement>(ancestor) ||
        IsA<HTMLSelectedContentElement>(ancestor)) {
      // Putting a <selectedcontent> inside an <option> or another
      // <selectedcontent> can lead to infinite loops.
      disabled_ = true;
    }
    if (auto* select = DynamicTo<HTMLSelectElement>(ancestor)) {
      if (first_ancestor_select) {
        // If there are multiple ancestor selects, then cloning can lead to
        // infinite loops, so disable this element.
        disabled_ = true;
        break;
      }
      first_ancestor_select = select;
    }
  }

  if (nearest_ancestor_select_ != first_ancestor_select) {
    CHECK(!nearest_ancestor_select_);
    nearest_ancestor_select_ = first_ancestor_select;
    nearest_ancestor_select_->SelectedContentElementInserted(this);
  }

  return Node::InsertionNotificationRequest::
      kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLSelectedContentElement::DidNotifySubtreeInsertionsToDocument() {
  // Clone from the nearest ancestor select element if this element isn't
  // disabled.
  // TODO(crbug.com/40236878): Use a flat tree traversal here.
  DCHECK_EQ(nearest_ancestor_select_,
            Traversal<HTMLSelectElement>::FirstAncestor(*this));
  UpdateFromAncestorSelect();
}

void HTMLSelectedContentElement::RemovedFrom(ContainerNode& removed_from) {
  HTMLElement::RemovedFrom(removed_from);
  auto* new_nearest_ancestor_select =
      Traversal<HTMLSelectElement>::FirstAncestor(*this);
  if (new_nearest_ancestor_select == nearest_ancestor_select_) {
    return;
  }
  CHECK(nearest_ancestor_select_);
  CHECK(!new_nearest_ancestor_select);
  nearest_ancestor_select_->SelectedContentElementRemoved(this);
  nearest_ancestor_select_ = new_nearest_ancestor_select;
  disabled_ = false;
}

void HTMLSelectedContentElement::MovedFrom(ContainerNode& old_parent) {
  GetDocument().GetAgent().event_loop()->EnqueueMicrotask(
      BindOnce(&HTMLSelectedContentElement::UpdateFromAncestorSelect,
               WrapWeakPersistent(this)));
}

void HTMLSelectedContentElement::UpdateFromAncestorSelect() {
  if (!disabled_ && nearest_ancestor_select_) {
    nearest_ancestor_select_->UpdateIndividualSelectedcontent(*this);
  }
}

void HTMLSelectedContentElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
  visitor->Trace(nearest_ancestor_select_);
}

}  // namespace blink
