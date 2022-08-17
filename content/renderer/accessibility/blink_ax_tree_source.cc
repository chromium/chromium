// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_tree_source.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/ax_serialization_utils.h"
#include "content/public/common/content_features.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_disallow_transition_scope.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

#if DCHECK_IS_ON()
WebAXObject ParentObjectUnignored(WebAXObject child) {
  WebAXObject parent = child.ParentObject();
  while (!parent.IsDetached() && !parent.AccessibilityIsIncludedInTree())
    parent = parent.ParentObject();
  return parent;
}

// Check that |parent| is the first unignored parent of |child|.
void CheckParentUnignoredOf(WebAXObject parent, WebAXObject child) {
  WebAXObject preexisting_parent = ParentObjectUnignored(child);
  DCHECK(preexisting_parent.Equals(parent))
      << "Child thinks it has a different preexisting parent:"
      << "\nChild: " << child.ToString(true).Utf8()
      << "\nPassed-in parent: " << parent.ToString(true).Utf8()
      << "\nPreexisting parent: " << preexisting_parent.ToString(true).Utf8();
}
#endif

}  // namespace

ScopedFreezeBlinkAXTreeSource::ScopedFreezeBlinkAXTreeSource(
    BlinkAXTreeSource* tree_source)
    : tree_source_(tree_source) {
  tree_source_->Freeze();
}

ScopedFreezeBlinkAXTreeSource::~ScopedFreezeBlinkAXTreeSource() {
  tree_source_->Thaw();
}

BlinkAXTreeSource::BlinkAXTreeSource(RenderFrameImpl* render_frame,
                                     ui::AXMode mode)
    : render_frame_(render_frame), accessibility_mode_(mode) {}

BlinkAXTreeSource::~BlinkAXTreeSource() {}

void BlinkAXTreeSource::Freeze() {
  CHECK(!frozen_);
  frozen_ = true;

  if (render_frame_ && render_frame_->GetWebFrame())
    document_ = render_frame_->GetWebFrame()->GetDocument();
  else
    document_ = WebDocument();

  root_ = ComputeRoot();

  if (!document_.IsNull())
    focus_ = WebAXObject::FromWebDocumentFocused(document_);
  else
    focus_ = WebAXObject();

  WebAXObject::Freeze(document_);
}

void BlinkAXTreeSource::Thaw() {
  CHECK(frozen_);
  WebAXObject::Thaw(document_);
  document_ = WebDocument();
  focus_ = WebAXObject();
  root_ = WebAXObject();
  frozen_ = false;
}

void BlinkAXTreeSource::SetRoot(WebAXObject root) {
  CHECK(!frozen_);
  explicit_root_ = root;
}

#if defined(AX_FAIL_FAST_BUILD)
// TODO(accessibility) Remove once it's clear this never triggers.
bool BlinkAXTreeSource::IsInTree(WebAXObject node) const {
  CHECK(frozen_);
  while (IsValid(node)) {
    if (node.Equals(root()))
      return true;
    node = GetParent(node);
  }
  return false;
}
#endif

void BlinkAXTreeSource::SetAccessibilityMode(ui::AXMode new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  accessibility_mode_ = new_mode;
}

bool BlinkAXTreeSource::ShouldLoadInlineTextBoxes(
    const blink::WebAXObject& obj) const {
#if !BUILDFLAG(IS_ANDROID)
  // If inline text boxes are enabled globally, no need to explicitly load them.
  if (accessibility_mode_.has_mode(ui::AXMode::kInlineTextBoxes))
    return false;
#endif

  // On some platforms, like Android, we only load inline text boxes for
  // a subset of nodes:
  //
  // Within the subtree of a focused editable text area.
  // When specifically enabled for a subtree via |load_inline_text_boxes_ids_|.

  int32_t focus_id = focus().AxID();
  WebAXObject ancestor = obj;
  while (!ancestor.IsDetached()) {
    int32_t ancestor_id = ancestor.AxID();
    if (base::Contains(load_inline_text_boxes_ids_, ancestor_id) ||
        (ancestor_id == focus_id && ancestor.IsEditable())) {
      return true;
    }
    ancestor = ancestor.ParentObject();
  }

  return false;
}

void BlinkAXTreeSource::SetLoadInlineTextBoxesForId(int32_t id) {
  // Keeping stale IDs in the set is harmless but we don't want it to keep
  // growing without bound, so clear out any unnecessary IDs whenever this
  // method is called.
  for (auto iter = load_inline_text_boxes_ids_.begin();
       iter != load_inline_text_boxes_ids_.end();) {
    if (GetFromId(*iter).IsDetached())
      iter = load_inline_text_boxes_ids_.erase(iter);
    else
      ++iter;
  }

  load_inline_text_boxes_ids_.insert(id);
}

bool BlinkAXTreeSource::GetTreeData(ui::AXTreeData* tree_data) const {
  CHECK(frozen_);
  tree_data->doctype = "html";
  tree_data->loaded = root().IsLoaded();
  tree_data->loading_progress = root().EstimatedLoadingProgress();
  tree_data->mimetype =
      document().IsXHTMLDocument() ? "text/xhtml" : "text/html";
  tree_data->title = document().Title().Utf8();
  tree_data->url = document().Url().GetString().Utf8();

  if (!focus().IsNull())
    tree_data->focus_id = focus().AxID();

  bool is_selection_backward = false;
  WebAXObject anchor_object, focus_object;
  int anchor_offset, focus_offset;
  ax::mojom::TextAffinity anchor_affinity, focus_affinity;
  root().Selection(is_selection_backward, anchor_object, anchor_offset,
                   anchor_affinity, focus_object, focus_offset, focus_affinity);
  if (!anchor_object.IsNull() && !focus_object.IsNull() && anchor_offset >= 0 &&
      focus_offset >= 0) {
    int32_t anchor_id = anchor_object.AxID();
    int32_t focus_id = focus_object.AxID();
    tree_data->sel_is_backward = is_selection_backward;
    tree_data->sel_anchor_object_id = anchor_id;
    tree_data->sel_anchor_offset = anchor_offset;
    tree_data->sel_focus_object_id = focus_id;
    tree_data->sel_focus_offset = focus_offset;
    tree_data->sel_anchor_affinity = anchor_affinity;
    tree_data->sel_focus_affinity = focus_affinity;
  }

  // Get the tree ID for this frame.
  if (WebLocalFrame* web_frame = document().GetFrame())
    tree_data->tree_id = web_frame->GetAXTreeID();

  tree_data->root_scroller_id = root().RootScroller().AxID();

  if (accessibility_mode_.has_mode(ui::AXMode::kHTMLMetadata)) {
    WebElement head = GetMainDocument().Head();
    for (WebNode child = head.FirstChild(); !child.IsNull();
         child = child.NextSibling()) {
      if (!child.IsElementNode())
        continue;
      WebElement elem = child.To<WebElement>();
      if (elem.HasHTMLTagName("SCRIPT")) {
        if (elem.GetAttribute("type") != "application/ld+json")
          continue;
      } else if (!elem.HasHTMLTagName("LINK") &&
                 !elem.HasHTMLTagName("TITLE") &&
                 !elem.HasHTMLTagName("META")) {
        continue;
      }
      std::string tag = base::ToLowerASCII(elem.TagName().Utf8());
      std::string html = "<" + tag;
      for (unsigned i = 0; i < elem.AttributeCount(); i++) {
        html += " " + elem.AttributeLocalName(i).Utf8() + "=\"" +
                elem.AttributeValue(i).Utf8() + "\"";
      }
      html += ">" + elem.InnerHTML().Utf8() + "</" + tag + ">";
      tree_data->metadata.push_back(html);
    }
  }

  return true;
}

WebAXObject BlinkAXTreeSource::GetRoot() const {
  if (frozen_)
    return root_;
  else
    return ComputeRoot();
}

WebAXObject BlinkAXTreeSource::GetFromId(int32_t id) const {
  return WebAXObject::FromWebDocumentByID(GetMainDocument(), id);
}

int32_t BlinkAXTreeSource::GetId(WebAXObject node) const {
  return node.AxID();
}

void BlinkAXTreeSource::GetChildren(
    WebAXObject parent,
    std::vector<WebAXObject>* out_children) const {
  CHECK(frozen_);

  if (ui::CanHaveInlineTextBoxChildren(parent.Role()) &&
      ShouldLoadInlineTextBoxes(parent)) {
    parent.LoadInlineTextBoxes();
  }

  for (unsigned i = 0; i < parent.ChildCount(); i++) {
    WebAXObject child = parent.ChildAt(i);

    // The child may be invalid due to issues in blink accessibility code.
    if (child.IsDetached()) {
      NOTREACHED() << "Should not try to serialize an invalid child:"
                   << "\nParent: " << parent.ToString(true).Utf8()
                   << "\nChild: " << child.ToString(true).Utf8();
      continue;
    }

    if (!child.AccessibilityIsIncludedInTree()) {
      NOTREACHED() << "Should not receive unincluded child."
                   << "\nChild: " << child.ToString(true).Utf8()
                   << "\nParent: " << parent.ToString(true).Utf8();
      continue;
    }

#if DCHECK_IS_ON()
    CheckParentUnignoredOf(parent, child);
#endif

    // These should not be produced by Blink. They are only needed on Mac and
    // handled in AXTableInfo on the browser side.
    DCHECK_NE(child.Role(), ax::mojom::Role::kColumn);
    DCHECK_NE(child.Role(), ax::mojom::Role::kTableHeaderContainer);

    // If an optional exclude_offscreen flag is set (only intended to be
    // used for a one-time snapshot of the accessibility tree), prune any
    // node that's entirely offscreen from the tree.
    if (exclude_offscreen() && child.IsOffScreen())
      continue;

    out_children->push_back(child);
  }
}

WebAXObject BlinkAXTreeSource::GetParent(WebAXObject node) const {
  CHECK(frozen_);

  // Blink returns ignored objects when walking up the parent chain,
  // we have to skip those here. Also, stop when we get to the root
  // element.
  do {
    if (node.Equals(root()))
      return WebAXObject();
    node = node.ParentObject();
  } while (!node.IsDetached() && !node.AccessibilityIsIncludedInTree());

  return node;
}

bool BlinkAXTreeSource::IsIgnored(WebAXObject node) const {
  return node.AccessibilityIsIgnored();
}

bool BlinkAXTreeSource::IsValid(WebAXObject node) const {
  return !node.IsDetached();  // This also checks if it's null.
}

bool BlinkAXTreeSource::IsEqual(WebAXObject node1, WebAXObject node2) const {
  return node1.Equals(node2);
}

WebAXObject BlinkAXTreeSource::GetNull() const {
  return WebAXObject();
}

std::string BlinkAXTreeSource::GetDebugString(blink::WebAXObject node) const {
  return node.ToString(true).Utf8();
}

void BlinkAXTreeSource::SerializerClearedNode(int32_t node_id) {
  GetRoot().SerializerClearedNode(node_id);
}

void BlinkAXTreeSource::SerializeNode(WebAXObject src,
                                      ui::AXNodeData* dst) const {
#if DCHECK_IS_ON()
  // Never causes a document lifecycle change during serialization,
  // because the assumption is that layout is in a safe, stable state.
  WebDocument document = GetMainDocument();
  blink::WebDisallowTransitionScope disallow(&document);
#endif

  dst->id = src.AxID();
  dst->role = src.Role();

  if (src.IsDetached() || !src.AccessibilityIsIncludedInTree()) {
    dst->AddState(ax::mojom::State::kIgnored);
    NOTREACHED();
    return;
  }

  // TODO(crbug.com/1068668): AX onion soup - finish migrating the rest of
  // this function inside of AXObject::Serialize and removing
  // unneeded WebAXObject interfaces.
  src.Serialize(dst, accessibility_mode_);

  TRACE_EVENT2("accessibility", "BlinkAXTreeSource::SerializeNode", "role",
               ui::ToString(dst->role), "id", dst->id);

  if (accessibility_mode_.has_mode(ui::AXMode::kPDF)) {
    // Return early. None of the following attributes are needed for PDFs.
    return;
  }

  // Return early. The following attributes are unnecessary for ignored nodes.
  // Exception: focusable ignored nodes are fully serialized, so that reasonable
  // verbalizations can be made if they actually receive focus.
  if (src.AccessibilityIsIgnored() &&
      !dst->HasState(ax::mojom::State::kFocusable)) {
    return;
  }

  if (dst->id == image_data_node_id_) {
    // In general, string attributes should be truncated using
    // TruncateAndAddStringAttribute, but ImageDataUrl contains a data url
    // representing an image, so add it directly using AddStringAttribute.
    dst->AddStringAttribute(ax::mojom::StringAttribute::kImageDataUrl,
                            src.ImageDataUrl(max_image_data_size_).Utf8());
  }
}

blink::WebDocument BlinkAXTreeSource::GetMainDocument() const {
  CHECK(frozen_);
  return document_;
}

WebAXObject BlinkAXTreeSource::ComputeRoot() const {
  if (!explicit_root_.IsNull())
    return explicit_root_;

  if (!render_frame_ || !render_frame_->GetWebFrame())
    return WebAXObject();

  WebDocument document = render_frame_->GetWebFrame()->GetDocument();
  if (!document.IsNull())
    return WebAXObject::FromWebDocument(document);

  return WebAXObject();
}

void BlinkAXTreeSource::TruncateAndAddStringAttribute(
    ui::AXNodeData* dst,
    ax::mojom::StringAttribute attribute,
    const std::string& value,
    uint32_t max_len) const {
  if (value.size() > max_len) {
    std::string truncated;
    base::TruncateUTF8ToByteSize(value, max_len, &truncated);
    dst->AddStringAttribute(attribute, truncated);
  } else {
    dst->AddStringAttribute(attribute, value);
  }
}

}  // namespace content
