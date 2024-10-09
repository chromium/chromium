// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/blink_ax_tree_source.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"

namespace blink {

BlinkAXTreeSource::BlinkAXTreeSource(AXObjectCacheImpl& ax_object_cache,
                                     bool is_snapshot)
    : ax_object_cache_(ax_object_cache), is_snapshot_(is_snapshot) {}

BlinkAXTreeSource::~BlinkAXTreeSource() = default;

static ax::mojom::blink::TextAffinity ToAXAffinity(TextAffinity affinity) {
  switch (affinity) {
    case TextAffinity::kUpstream:
      return ax::mojom::blink::TextAffinity::kUpstream;
    case TextAffinity::kDownstream:
      return ax::mojom::blink::TextAffinity::kDownstream;
    default:
      NOTREACHED_IN_MIGRATION();
      return ax::mojom::blink::TextAffinity::kDownstream;
  }
}

void BlinkAXTreeSource::Selection(
    const AXObject* obj,
    bool& is_selection_backward,
    const AXObject** anchor_object,
    int& anchor_offset,
    ax::mojom::blink::TextAffinity& anchor_affinity,
    const AXObject** focus_object,
    int& focus_offset,
    ax::mojom::blink::TextAffinity& focus_affinity) const {
  is_selection_backward = false;
  *anchor_object = nullptr;
  anchor_offset = -1;
  anchor_affinity = ax::mojom::blink::TextAffinity::kDownstream;
  *focus_object = nullptr;
  focus_offset = -1;
  focus_affinity = ax::mojom::blink::TextAffinity::kDownstream;

  if (!obj || obj->IsDetached())
    return;

  const AXObject* focus = GetFocusedObject();
  if (!focus || focus->IsDetached())
    return;

  const auto ax_selection =
      focus->IsAtomicTextField()
          ? AXSelection::FromCurrentSelection(ToTextControl(*focus->GetNode()))
          : AXSelection::FromCurrentSelection(*focus->GetDocument());
  if (!ax_selection)
    return;

  const AXPosition base = ax_selection.Anchor();
  *anchor_object = base.ContainerObject();
  const AXPosition extent = ax_selection.Focus();
  *focus_object = extent.ContainerObject();

  is_selection_backward = base > extent;
  if (base.IsTextPosition()) {
    anchor_offset = base.TextOffset();
    anchor_affinity = ToAXAffinity(base.Affinity());
  } else {
    anchor_offset = base.ChildIndex();
  }

  if (extent.IsTextPosition()) {
    focus_offset = extent.TextOffset();
    focus_affinity = ToAXAffinity(extent.Affinity());
  } else {
    focus_offset = extent.ChildIndex();
  }
}

static ui::AXTreeID GetAXTreeID(LocalFrame* local_frame) {
  const std::optional<base::UnguessableToken>& embedding_token =
      local_frame->GetEmbeddingToken();
  if (embedding_token && !embedding_token->is_empty())
    return ui::AXTreeID::FromToken(embedding_token.value());
  return ui::AXTreeIDUnknown();
}

bool BlinkAXTreeSource::GetTreeData(ui::AXTreeData* tree_data) const {
  CHECK(frozen_);
  const AXObject* root = GetRoot();
  tree_data->doctype = "html";
  tree_data->loaded = root->IsLoaded();
  tree_data->loading_progress = root->EstimatedLoadingProgress();
  const Document& document = ax_object_cache_->GetDocument();
  tree_data->mimetype = document.IsXHTMLDocument() ? "text/xhtml" : "text/html";
  tree_data->title = document.title().Utf8();
  tree_data->url = document.Url().GetString().Utf8();

  if (const AXObject* focus = GetFocusedObject())
    tree_data->focus_id = focus->AXObjectID();

  bool is_selection_backward = false;
  const AXObject *anchor_object, *focus_object;
  int anchor_offset, focus_offset;
  ax::mojom::blink::TextAffinity anchor_affinity, focus_affinity;
  Selection(root, is_selection_backward, &anchor_object, anchor_offset,
            anchor_affinity, &focus_object, focus_offset, focus_affinity);
  if (anchor_object && focus_object && anchor_offset >= 0 &&
      focus_offset >= 0) {
    int32_t anchor_id = anchor_object->AXObjectID();
    int32_t focus_id = focus_object->AXObjectID();
    tree_data->sel_is_backward = is_selection_backward;
    tree_data->sel_anchor_object_id = anchor_id;
    tree_data->sel_anchor_offset = anchor_offset;
    tree_data->sel_focus_object_id = focus_id;
    tree_data->sel_focus_offset = focus_offset;
    tree_data->sel_anchor_affinity = anchor_affinity;
    tree_data->sel_focus_affinity = focus_affinity;
  }

  // Get the tree ID for this frame.
  if (LocalFrame* local_frame = document.GetFrame())
    tree_data->tree_id = GetAXTreeID(local_frame);

  if (auto* root_scroller = root->RootScroller())
    tree_data->root_scroller_id = root_scroller->AXObjectID();
  else
    tree_data->root_scroller_id = 0;

  if (ax_object_cache_->GetAXMode().has_mode(ui::AXMode::kHTMLMetadata)) {
    if (HTMLHeadElement* head = ax_object_cache_->GetDocument().head()) {
      for (Node* child = head->firstChild(); child;
           child = child->nextSibling()) {
        const Element* elem = DynamicTo<Element>(*child);
        if (!elem) {
          continue;
        }
        if (IsA<HTMLScriptElement>(*elem)) {
          if (elem->getAttribute(html_names::kTypeAttr) !=
              "application/ld+json") {
            continue;
          }
        } else if (!IsA<HTMLLinkElement>(*elem) &&
                   !IsA<HTMLTitleElement>(*elem) &&
                   !IsA<HTMLMetaElement>(*elem)) {
          continue;
        }
        // TODO(chrishtr): replace the below with elem->outerHTML().
        String tag = elem->tagName().LowerASCII();
        String html = "<" + tag;
        for (unsigned i = 0; i < elem->Attributes().size(); i++) {
          html = html + String(" ") + elem->Attributes().at(i).LocalName() +
                 String("=\"") + elem->Attributes().at(i).Value() + "\"";
        }
        html = html + String(">") + elem->innerHTML() + String("</") + tag +
               String(">");
        tree_data->metadata.push_back(html.Utf8());
      }
    }
  }

  return true;
}

void BlinkAXTreeSource::Freeze() {
  CHECK(!frozen_);
  frozen_ = true;

  // The root cannot be null.
  root_ = ax_object_cache_->Root();
  CHECK(root_);
  focus_ = ax_object_cache_->FocusedObject();
  CHECK(focus_);
}

void BlinkAXTreeSource::Thaw() {
  CHECK(frozen_);
  frozen_ = false;
  root_ = nullptr;
  focus_ = nullptr;
}

const AXObject* BlinkAXTreeSource::GetRoot() const {
  CHECK(frozen_);
  CHECK(root_);
  return root_;
}

const AXObject* BlinkAXTreeSource::GetFocusedObject() const {
  CHECK(frozen_);
  CHECK(focus_);
  return focus_;
}

const AXObject* BlinkAXTreeSource::GetFromId(int32_t id) const {
  const AXObject* result = ax_object_cache_->ObjectFromAXID(id);
  if (result && !result->IsIncludedInTree()) {
    DCHECK(false) << "Should not serialize an unincluded object:" << "\nChild: "
                  << result->ToString().Utf8();
    return nullptr;
  }
  return result;
}

int32_t BlinkAXTreeSource::GetId(const AXObject* node) const {
  return node->AXObjectID();
}

size_t BlinkAXTreeSource::GetChildCount(const AXObject* node) const {
  if (ShouldTruncateInlineTextBoxes() &&
      ui::CanHaveInlineTextBoxChildren(node->RoleValue())) {
    return 0;
  }
  return node->ChildCountIncludingIgnored();
}

AXObject* BlinkAXTreeSource::ChildAt(const AXObject* node, size_t index) const {
  if (ShouldTruncateInlineTextBoxes()) {
    CHECK(!ui::CanHaveInlineTextBoxChildren(node->RoleValue()));
  }
  auto* child = node->ChildAtIncludingIgnored(static_cast<int>(index));

  // The child may be invalid due to issues in blink accessibility code.
  CHECK(child);
  if (child->IsDetached()) {
    NOTREACHED(base::NotFatalUntil::M127)
        << "Should not try to serialize an invalid child:" << "\nParent: "
        << node->ToString().Utf8() << "\nChild: " << child->ToString().Utf8();
    return nullptr;
  }

  if (!child->IsIncludedInTree()) {
    NOTREACHED(base::NotFatalUntil::M127)
        << "Should not receive unincluded child."
        << "\nChild: " << child->ToString().Utf8()
        << "\nParent: " << node->ToString().Utf8();
    return nullptr;
  }

  // These should not be produced by Blink. They are only needed on Mac and
  // handled in AXTableInfo on the browser side.
  DCHECK_NE(child->RoleValue(), ax::mojom::blink::Role::kColumn);
  DCHECK_NE(child->RoleValue(), ax::mojom::blink::Role::kTableHeaderContainer);
  DCHECK(child->ParentObjectIncludedInTree() == node)
      << "Child thinks it has a different preexisting parent:"
      << "\nChild: " << child << "\nPassed-in parent: " << node
      << "\nPreexisting parent: " << child->ParentObjectIncludedInTree();

  return child;
}

AXObject* BlinkAXTreeSource::GetParent(const AXObject* node) const {
  return node->ParentObjectIncludedInTree();
}

bool BlinkAXTreeSource::IsIgnored(const AXObject* node) const {
  if (!node || node->IsDetached())
    return false;
  return node->IsIgnored();
}

bool BlinkAXTreeSource::IsEqual(const AXObject* node1, const AXObject* node2) const {
  return node1 == node2;
}

AXObject* BlinkAXTreeSource::GetNull() const {
  return nullptr;
}

std::string BlinkAXTreeSource::GetDebugString(const AXObject* node) const {
  if (!node || node->IsDetached())
    return "";
  return node->ToString().Utf8();
}

void BlinkAXTreeSource::SerializeNode(const AXObject* src,
                                      ui::AXNodeData* dst) const {
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  DocumentLifecycle::DisallowTransitionScope disallow(
      ax_object_cache_->GetDocument().Lifecycle());
#endif

  if (!src || src->IsDetached() || !src->IsIncludedInTree()) {
    dst->AddState(ax::mojom::blink::State::kIgnored);
    dst->id = -1;
    dst->role = ax::mojom::blink::Role::kUnknown;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  src->Serialize(dst, ax_object_cache_->GetAXMode(), is_snapshot_);
}

void BlinkAXTreeSource::Trace(Visitor* visitor) const {
  visitor->Trace(ax_object_cache_);
  visitor->Trace(root_);
  visitor->Trace(focus_);
}

}  // namespace blink
