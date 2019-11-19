// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_tree_source.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_features.h"
#include "content/renderer/accessibility/ax_image_annotator.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebAXObject;
using blink::WebAXObjectAttribute;
using blink::WebAXObjectVectorAttribute;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFloatRect;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

// Images smaller than this number, in CSS pixels, will never get annotated.
// Note that OCR works on pretty small images, so this shouldn't be too large.
const int kMinImageAnnotationWidth = 16;
const int kMinImageAnnotationHeight = 16;

void AddIntListAttributeFromWebObjects(ax::mojom::IntListAttribute attr,
                                       const WebVector<WebAXObject>& objects,
                                       AXContentNodeData* dst) {
  std::vector<int32_t> ids;
  for (size_t i = 0; i < objects.size(); i++)
    ids.push_back(objects[i].AxID());
  if (!ids.empty())
    dst->AddIntListAttribute(attr, ids);
}

class AXContentNodeDataSparseAttributeAdapter
    : public blink::WebAXSparseAttributeClient {
 public:
  AXContentNodeDataSparseAttributeAdapter(AXContentNodeData* dst) : dst_(dst) {
    DCHECK(dst_);
  }
  ~AXContentNodeDataSparseAttributeAdapter() override {}

 private:
  AXContentNodeData* dst_;

  void AddBoolAttribute(blink::WebAXBoolAttribute attribute,
                        bool value) override {
    switch (attribute) {
      case blink::WebAXBoolAttribute::kAriaBusy:
        dst_->AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, value);
        break;
      default:
        NOTREACHED();
    }
  }

  void AddIntAttribute(blink::WebAXIntAttribute attribute,
                       int32_t value) override {
    switch (attribute) {
      case blink::WebAXIntAttribute::kAriaColumnCount:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount, value);
        break;
      case blink::WebAXIntAttribute::kAriaRowCount:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount, value);
        break;
      default:
        NOTREACHED();
    }
  }

  void AddUIntAttribute(blink::WebAXUIntAttribute attribute,
                        uint32_t value) override {
    switch (attribute) {
      case blink::WebAXUIntAttribute::kAriaColumnIndex:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                              value);
        break;
      case blink::WebAXUIntAttribute::kAriaColumnSpan:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnSpan,
                              value);
        break;
      case blink::WebAXUIntAttribute::kAriaRowIndex:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex,
                              value);
        break;
      case blink::WebAXUIntAttribute::kAriaRowSpan:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellRowSpan, value);
        break;
      default:
        NOTREACHED();
    }
  }

  void AddStringAttribute(blink::WebAXStringAttribute attribute,
                          const blink::WebString& value) override {
    switch (attribute) {
      case blink::WebAXStringAttribute::kAriaKeyShortcuts:
        dst_->AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                                 value.Utf8());
        break;
      case blink::WebAXStringAttribute::kAriaRoleDescription:
        dst_->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                 value.Utf8());
        break;
      default:
        NOTREACHED();
    }
  }

  void AddObjectAttribute(WebAXObjectAttribute attribute,
                          const WebAXObject& value) override {
    switch (attribute) {
      case WebAXObjectAttribute::kAriaActiveDescendant:
        // TODO(dmazzoni): WebAXObject::ActiveDescendant currently returns
        // more information than the sparse interface does.
        // ******** Why is this a TODO? ********
        break;
      case WebAXObjectAttribute::kAriaDetails:
        dst_->AddIntAttribute(ax::mojom::IntAttribute::kDetailsId,
                              value.AxID());
        break;
      case WebAXObjectAttribute::kAriaErrorMessage:
        // Use WebAXObject::ErrorMessage(), which provides both ARIA error
        // messages as well as built-in HTML form validation messages.
        break;
      default:
        NOTREACHED();
    }
  }

  void AddObjectVectorAttribute(
      WebAXObjectVectorAttribute attribute,
      const blink::WebVector<WebAXObject>& value) override {
    switch (attribute) {
      case WebAXObjectVectorAttribute::kAriaControls:
        AddIntListAttributeFromWebObjects(
            ax::mojom::IntListAttribute::kControlsIds, value, dst_);
        break;
      case WebAXObjectVectorAttribute::kAriaFlowTo:
        AddIntListAttributeFromWebObjects(
            ax::mojom::IntListAttribute::kFlowtoIds, value, dst_);
        break;
      default:
        NOTREACHED();
    }
  }
};

WebAXObject ParentObjectUnignored(WebAXObject child) {
  WebAXObject parent = child.ParentObject();
  while (!parent.IsDetached() && !parent.AccessibilityIsIncludedInTree())
    parent = parent.ParentObject();
  return parent;
}

// Returns true if |ancestor| is the first unignored parent of |child|,
// which means that when walking up the parent chain from |child|,
// |ancestor| is the *first* ancestor that isn't marked as
// accessibilityIsIgnored().
bool IsParentUnignoredOf(WebAXObject ancestor,
                         WebAXObject child) {
  WebAXObject parent = ParentObjectUnignored(child);
  return parent.Equals(ancestor);
}

// Helper function that searches in the subtree of |obj| to a max
// depth of |max_depth| for an image.
//
// Returns true on success, or false if it finds more than one image,
// or any node with a name, or anything deeper than |max_depth|.
bool SearchForExactlyOneInnerImage(WebAXObject obj,
                                   WebAXObject* inner_image,
                                   int max_depth) {
  DCHECK(inner_image);

  // If it's the first image, set |inner_image|. If we already
  // found an image, fail.
  if (obj.Role() == ax::mojom::Role::kImage) {
    if (!inner_image->IsDetached())
      return false;
    *inner_image = obj;
  }

  // Fail if we recursed to |max_depth| and there's more of a subtree.
  if (max_depth == 0 && obj.ChildCount())
    return false;

  // If we found something else with a name, fail.
  if (obj.Role() != ax::mojom::Role::kRootWebArea) {
    blink::WebString web_name = obj.GetName();
    if (!base::ContainsOnlyChars(web_name.Utf8(), base::kWhitespaceASCII))
      return false;
  }

  // Recurse.
  for (unsigned int i = 0; i < obj.ChildCount(); i++) {
    if (!SearchForExactlyOneInnerImage(obj.ChildAt(i), inner_image,
                                       max_depth - 1))
      return false;
  }

  return !inner_image->IsDetached();
}

// Return true if the subtree of |obj|, to a max depth of 2, contains
// exactly one image. Return that image in |inner_image|.
bool FindExactlyOneInnerImageInMaxDepthTwo(WebAXObject obj,
                                           WebAXObject* inner_image) {
  DCHECK(inner_image);
  return SearchForExactlyOneInnerImage(obj, inner_image, /* max_depth = */ 2);
}

std::string GetEquivalentAriaRoleString(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kArticle:
      return "article";
    case ax::mojom::Role::kBanner:
      return "banner";
    case ax::mojom::Role::kButton:
      return "button";
    case ax::mojom::Role::kComplementary:
      return "complementary";
    case ax::mojom::Role::kFigure:
      return "figure";
    case ax::mojom::Role::kFooter:
      return "contentinfo";
    case ax::mojom::Role::kHeader:
      return "banner";
    case ax::mojom::Role::kHeading:
      return "heading";
    case ax::mojom::Role::kImage:
      return "img";
    case ax::mojom::Role::kMain:
      return "main";
    case ax::mojom::Role::kNavigation:
      return "navigation";
    case ax::mojom::Role::kRadioButton:
      return "radio";
    case ax::mojom::Role::kRegion:
      return "region";
    case ax::mojom::Role::kSection:
      // A <section> element uses the 'region' ARIA role mapping.
      return "region";
    case ax::mojom::Role::kSlider:
      return "slider";
    case ax::mojom::Role::kTime:
      return "time";
    default:
      break;
  }

  return std::string();
}

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
    : render_frame_(render_frame), accessibility_mode_(mode), frozen_(false) {
  image_annotation_debugging_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityLabelsDebugging);
}

BlinkAXTreeSource::~BlinkAXTreeSource() {
}

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
}

void BlinkAXTreeSource::Thaw() {
  CHECK(frozen_);
  frozen_ = false;
}

void BlinkAXTreeSource::SetRoot(WebAXObject root) {
  CHECK(!frozen_);
  explicit_root_ = root;
}

bool BlinkAXTreeSource::IsInTree(WebAXObject node) const {
  CHECK(frozen_);
  while (IsValid(node)) {
    if (node.Equals(root()))
      return true;
    node = GetParent(node);
  }
  return false;
}

void BlinkAXTreeSource::SetAccessibilityMode(ui::AXMode new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  accessibility_mode_ = new_mode;
}

bool BlinkAXTreeSource::ShouldLoadInlineTextBoxes(
    const blink::WebAXObject& obj) const {
#if !defined(OS_ANDROID)
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

bool BlinkAXTreeSource::GetTreeData(AXContentTreeData* tree_data) const {
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
                     anchor_affinity, focus_object, focus_offset,
                     focus_affinity);
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

  // Get the tree ID for this frame and the parent frame.
  WebLocalFrame* web_frame = document().GetFrame();
  if (web_frame) {
    RenderFrame* render_frame = RenderFrame::FromWebFrame(web_frame);
    tree_data->routing_id = render_frame->GetRoutingID();

    // Get the tree ID for the parent frame.
    blink::WebFrame* parent_web_frame = web_frame->Parent();
    if (parent_web_frame) {
      tree_data->parent_routing_id =
          RenderFrame::GetRoutingIdForWebFrame(parent_web_frame);
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

  if ((parent.Role() == ax::mojom::Role::kStaticText ||
       parent.Role() == ax::mojom::Role::kLineBreak) &&
      ShouldLoadInlineTextBoxes(parent)) {
    parent.LoadInlineTextBoxes();
  }

  bool is_iframe = false;
  WebNode node = parent.GetNode();
  if (!node.IsNull() && node.IsElementNode())
    is_iframe = node.To<WebElement>().HasHTMLTagName("iframe");

  for (unsigned i = 0; i < parent.ChildCount(); i++) {
    WebAXObject child = parent.ChildAt(i);

    // The child may be invalid due to issues in blink accessibility code.
    if (child.IsDetached())
      continue;

    // Skip children whose parent isn't |parent|.
    // As an exception, include children of an iframe element.
    if (!is_iframe && !IsParentUnignoredOf(parent, child))
      continue;

    // Skip table headers and columns, they're only needed on Mac
    // and soon we'll get rid of this code entirely.
    if (child.Role() == ax::mojom::Role::kColumn ||
        child.Role() == ax::mojom::Role::kLayoutTableColumn ||
        child.Role() == ax::mojom::Role::kTableHeaderContainer)
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

void BlinkAXTreeSource::SerializeNode(WebAXObject src,
                                      AXContentNodeData* dst) const {
  dst->role = src.Role();
  AXStateFromBlink(src, dst);
  dst->id = src.AxID();

  TRACE_EVENT1("accessibility", "BlinkAXTreeSource::SerializeNode", "role",
               ui::ToString(dst->role));

  WebAXObject offset_container;
  WebFloatRect bounds_in_container;
  SkMatrix44 container_transform;
  bool clips_children = false;
  src.GetRelativeBounds(offset_container, bounds_in_container,
                        container_transform, &clips_children);
  dst->relative_bounds.bounds = bounds_in_container;
#if !defined(OS_ANDROID) && !defined(OS_MACOSX)
  if (src.Equals(root())) {
    WebView* web_view = render_frame_->GetRenderView()->GetWebView();
    std::unique_ptr<gfx::Transform> container_transform_gfx =
        std::make_unique<gfx::Transform>(container_transform);
    container_transform_gfx->Scale(web_view->PageScaleFactor(),
                                   web_view->PageScaleFactor());
    container_transform_gfx->Translate(
        gfx::Vector2dF(-web_view->VisualViewportOffset().x,
                       -web_view->VisualViewportOffset().y));
    if (!container_transform_gfx->IsIdentity())
      dst->relative_bounds.transform = std::move(container_transform_gfx);
  } else if (!container_transform.isIdentity())
    dst->relative_bounds.transform =
        base::WrapUnique(new gfx::Transform(container_transform));
#else
  if (!container_transform.isIdentity())
    dst->relative_bounds.transform =
        base::WrapUnique(new gfx::Transform(container_transform));
#endif  // !defined(OS_ANDROID) && !defined(OS_MACOSX)
  if (!offset_container.IsDetached())
    dst->relative_bounds.offset_container_id = offset_container.AxID();
  if (clips_children)
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);

  if (src.IsLineBreakingObject()) {
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                          true);
  }

  AXContentNodeDataSparseAttributeAdapter sparse_attribute_adapter(dst);
  src.GetSparseAXAttributes(sparse_attribute_adapter);

  WebAXObject chooser_popup = src.ChooserPopup();
  if (!chooser_popup.IsNull()) {
    int32_t chooser_popup_id = chooser_popup.AxID();
    auto controls_ids =
        dst->GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds);
    controls_ids.push_back(chooser_popup_id);
    dst->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                             controls_ids);
  }

  ax::mojom::NameFrom name_from;
  blink::WebVector<WebAXObject> name_objects;
  blink::WebString web_name = src.GetName(name_from, name_objects);
  if ((!web_name.IsEmpty() && !web_name.IsNull()) ||
      name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
    int max_length = dst->role == ax::mojom::Role::kStaticText
                         ? kMaxStaticTextLength
                         : kMaxStringAttributeLength;
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kName,
                                  web_name.Utf8(), max_length);
    dst->SetNameFrom(name_from);
    AddIntListAttributeFromWebObjects(
        ax::mojom::IntListAttribute::kLabelledbyIds, name_objects, dst);
  }

  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<WebAXObject> description_objects;
  blink::WebString web_description =
      src.Description(name_from, description_from, description_objects);
  if (!web_description.IsEmpty()) {
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kDescription,
                                  web_description.Utf8());
    dst->SetDescriptionFrom(description_from);
    AddIntListAttributeFromWebObjects(
        ax::mojom::IntListAttribute::kDescribedbyIds, description_objects, dst);
  }

  blink::WebString web_title = src.Title(name_from);
  if (!web_title.IsEmpty()) {
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kTooltip,
                                  web_title.Utf8());
  }

  if (src.ValueDescription().length()) {
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kValue,
                                  src.ValueDescription().Utf8());
  } else {
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kValue,
                                  src.StringValue().Utf8());
  }

  switch (src.Restriction()) {
    case blink::kWebAXRestrictionReadOnly:
      dst->SetRestriction(ax::mojom::Restriction::kReadOnly);
      break;
    case blink::kWebAXRestrictionDisabled:
      dst->SetRestriction(ax::mojom::Restriction::kDisabled);
      break;
    case blink::kWebAXRestrictionNone:
      if (src.CanSetValueAttribute())
        dst->AddAction(ax::mojom::Action::kSetValue);
      break;
  }

  if (!src.Url().IsEmpty())
    TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kUrl,
                                  src.Url().GetString().Utf8());

  // The following set of attributes are only accessed when the accessibility
  // mode is set to screen reader mode, otherwise only the more basic
  // attributes are populated.
  if (accessibility_mode_.has_mode(ui::AXMode::kScreenReader)) {
    blink::WebString web_placeholder = src.Placeholder(name_from);
    if (!web_placeholder.IsEmpty())
      TruncateAndAddStringAttribute(dst,
                                    ax::mojom::StringAttribute::kPlaceholder,
                                    web_placeholder.Utf8());

    if (dst->role == ax::mojom::Role::kColorWell)
      dst->AddIntAttribute(ax::mojom::IntAttribute::kColorValue,
                           src.ColorValue());

    if (dst->role == ax::mojom::Role::kLink) {
      WebAXObject target = src.InPageLinkTarget();
      if (!target.IsNull()) {
        int32_t target_id = target.AxID();
        dst->AddIntAttribute(ax::mojom::IntAttribute::kInPageLinkTargetId,
                             target_id);
      }
    }

    if (dst->role == ax::mojom::Role::kRadioButton) {
      AddIntListAttributeFromWebObjects(
          ax::mojom::IntListAttribute::kRadioGroupIds,
          src.RadioButtonsInGroup(), dst);
    }

    // Text attributes.
    if (src.BackgroundColor())
      dst->AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                           src.BackgroundColor());

    if (src.GetColor())
      dst->AddIntAttribute(ax::mojom::IntAttribute::kColor, src.GetColor());

    WebAXObject parent = ParentObjectUnignored(src);
    if (src.FontFamily().length()) {
      if (parent.IsNull() || parent.FontFamily() != src.FontFamily())
        TruncateAndAddStringAttribute(dst,
                                      ax::mojom::StringAttribute::kFontFamily,
                                      src.FontFamily().Utf8());
    }

    // Font size is in pixels.
    if (src.FontSize())
      dst->AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                             src.FontSize());

    if (src.FontWeight()) {
      dst->AddFloatAttribute(ax::mojom::FloatAttribute::kFontWeight,
                             src.FontWeight());
    }

    if (src.AriaCurrentState() != ax::mojom::AriaCurrentState::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                           static_cast<int32_t>(src.AriaCurrentState()));
    }

    if (src.InvalidState() != ax::mojom::InvalidState::kNone)
      dst->SetInvalidState(src.InvalidState());
    if (src.InvalidState() == ax::mojom::InvalidState::kOther &&
        src.AriaInvalidValue().length()) {
      TruncateAndAddStringAttribute(
          dst, ax::mojom::StringAttribute::kAriaInvalidValue,
          src.AriaInvalidValue().Utf8());
    }

    if (src.CheckedState() != ax::mojom::CheckedState::kNone) {
      dst->SetCheckedState(src.CheckedState());
    }

    if (dst->role == ax::mojom::Role::kListItem &&
        src.GetListStyle() != ax::mojom::ListStyle::kNone) {
      dst->SetListStyle(src.GetListStyle());
    }

    if (src.GetTextDirection() != ax::mojom::TextDirection::kNone) {
      dst->SetTextDirection(src.GetTextDirection());
    }

    if (src.GetTextPosition() != ax::mojom::TextPosition::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTextPosition,
                           static_cast<int32_t>(src.GetTextPosition()));
    }

    int32_t text_style = 0;
    ax::mojom::TextDecorationStyle text_overline_style;
    ax::mojom::TextDecorationStyle text_strikethrough_style;
    ax::mojom::TextDecorationStyle text_underline_style;
    src.GetTextStyleAndTextDecorationStyle(&text_style, &text_overline_style,
                                           &text_strikethrough_style,
                                           &text_underline_style);
    if (text_style) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTextStyle, text_style);
    }

    if (text_overline_style != ax::mojom::TextDecorationStyle::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTextOverlineStyle,
                           static_cast<int32_t>(text_overline_style));
    }

    if (text_strikethrough_style != ax::mojom::TextDecorationStyle::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTextStrikethroughStyle,
                           static_cast<int32_t>(text_strikethrough_style));
    }

    if (text_underline_style != ax::mojom::TextDecorationStyle::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTextUnderlineStyle,
                           static_cast<int32_t>(text_underline_style));
    }

    if (dst->role == ax::mojom::Role::kInlineTextBox) {
      WebVector<int> src_character_offsets;
      src.CharacterOffsets(src_character_offsets);
      std::vector<int32_t> character_offsets;
      character_offsets.reserve(src_character_offsets.size());
      for (size_t i = 0; i < src_character_offsets.size(); ++i)
        character_offsets.push_back(src_character_offsets[i]);
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets,
                               character_offsets);

      WebVector<int> src_word_starts;
      WebVector<int> src_word_ends;
      src.GetWordBoundaries(src_word_starts, src_word_ends);
      std::vector<int32_t> word_starts;
      std::vector<int32_t> word_ends;
      word_starts.reserve(src_word_starts.size());
      word_ends.reserve(src_word_starts.size());
      for (size_t i = 0; i < src_word_starts.size(); ++i) {
        word_starts.push_back(src_word_starts[i]);
        word_ends.push_back(src_word_ends[i]);
      }
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                               word_starts);
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                               word_ends);
    }

    if (src.AccessKey().length()) {
      TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kAccessKey,
                                    src.AccessKey().Utf8());
    }

    if (src.AutoComplete().length()) {
      TruncateAndAddStringAttribute(dst,
                                    ax::mojom::StringAttribute::kAutoComplete,
                                    src.AutoComplete().Utf8());
    }

    if (src.Action() != ax::mojom::DefaultActionVerb::kNone) {
      dst->SetDefaultActionVerb(src.Action());
    }

    if (src.HasComputedStyle()) {
      TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kDisplay,
                                    src.ComputedStyleDisplay().Utf8());
    }

    if (src.Language().length()) {
      if (parent.IsNull() || parent.Language() != src.Language())
        TruncateAndAddStringAttribute(
            dst, ax::mojom::StringAttribute::kLanguage, src.Language().Utf8());
    }

    if (src.KeyboardShortcut().length() &&
        !dst->HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts)) {
      TruncateAndAddStringAttribute(dst,
                                    ax::mojom::StringAttribute::kKeyShortcuts,
                                    src.KeyboardShortcut().Utf8());
    }

    if (!src.NextOnLine().IsDetached()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                           src.NextOnLine().AxID());
    }

    if (!src.PreviousOnLine().IsDetached()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                           src.PreviousOnLine().AxID());
    }

    if (!src.AriaActiveDescendant().IsDetached()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                           src.AriaActiveDescendant().AxID());
    }

    if (!src.ErrorMessage().IsDetached()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kErrormessageId,
                           src.ErrorMessage().AxID());
    }

    if (ui::IsHeading(dst->role) && src.HeadingLevel()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                           src.HeadingLevel());
    } else if ((dst->role == ax::mojom::Role::kTreeItem ||
                dst->role == ax::mojom::Role::kRow) &&
               src.HierarchicalLevel()) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                           src.HierarchicalLevel());
    }

    if (src.SetSize())
      dst->AddIntAttribute(ax::mojom::IntAttribute::kSetSize, src.SetSize());

    if (src.PosInSet())
      dst->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, src.PosInSet());

    if (src.CanvasHasFallbackContent())
      dst->AddBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback, true);

    // Spelling, grammar and other document markers.
    WebVector<ax::mojom::MarkerType> src_marker_types;
    WebVector<int> src_marker_starts;
    WebVector<int> src_marker_ends;
    src.Markers(src_marker_types, src_marker_starts, src_marker_ends);
    DCHECK_EQ(src_marker_types.size(), src_marker_starts.size());
    DCHECK_EQ(src_marker_starts.size(), src_marker_ends.size());

    if (src_marker_types.size()) {
      std::vector<int32_t> marker_types;
      std::vector<int32_t> marker_starts;
      std::vector<int32_t> marker_ends;
      marker_types.reserve(src_marker_types.size());
      marker_starts.reserve(src_marker_starts.size());
      marker_ends.reserve(src_marker_ends.size());
      for (size_t i = 0; i < src_marker_types.size(); ++i) {
        marker_types.push_back(static_cast<int32_t>(src_marker_types[i]));
        marker_starts.push_back(src_marker_starts[i]);
        marker_ends.push_back(src_marker_ends[i]);
      }
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes,
                               marker_types);
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                               marker_starts);
      dst->AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                               marker_ends);
    }

    if (src.IsInLiveRegion()) {
      dst->AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic,
                            src.LiveRegionAtomic());
      if (!src.LiveRegionStatus().IsEmpty()) {
        TruncateAndAddStringAttribute(dst,
                                      ax::mojom::StringAttribute::kLiveStatus,
                                      src.LiveRegionStatus().Utf8());
      }
      TruncateAndAddStringAttribute(dst,
                                    ax::mojom::StringAttribute::kLiveRelevant,
                                    src.LiveRegionRelevant().Utf8());
      // If we are not at the root of an atomic live region.
      if (src.ContainerLiveRegionAtomic() &&
          !src.LiveRegionRoot().IsDetached() && !src.LiveRegionAtomic()) {
        dst->AddIntAttribute(ax::mojom::IntAttribute::kMemberOfId,
                             src.LiveRegionRoot().AxID());
      }
      dst->AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                            src.ContainerLiveRegionAtomic());
      dst->AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveBusy,
                            src.ContainerLiveRegionBusy());
      TruncateAndAddStringAttribute(
          dst, ax::mojom::StringAttribute::kContainerLiveStatus,
          src.ContainerLiveRegionStatus().Utf8());
      TruncateAndAddStringAttribute(
          dst, ax::mojom::StringAttribute::kContainerLiveRelevant,
          src.ContainerLiveRegionRelevant().Utf8());
    }

    if (dst->role == ax::mojom::Role::kProgressIndicator ||
        dst->role == ax::mojom::Role::kMeter ||
        dst->role == ax::mojom::Role::kScrollBar ||
        dst->role == ax::mojom::Role::kSlider ||
        dst->role == ax::mojom::Role::kSpinButton ||
        (dst->role == ax::mojom::Role::kSplitter &&
         src.CanSetFocusAttribute())) {
      float value;
      if (src.ValueForRange(&value))
        dst->AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                               value);

      float max_value;
      if (src.MaxValueForRange(&max_value)) {
        dst->AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                               max_value);
      }

      float min_value;
      if (src.MinValueForRange(&min_value)) {
        dst->AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                               min_value);
      }

      float step_value;
      if (src.StepValueForRange(&step_value)) {
        dst->AddFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                               step_value);
      }
    }

    if (dst->role == ax::mojom::Role::kDialog ||
        dst->role == ax::mojom::Role::kAlertDialog) {
      dst->AddBoolAttribute(ax::mojom::BoolAttribute::kModal, src.IsModal());
    }

    if (dst->role == ax::mojom::Role::kRootWebArea)
      TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kHtmlTag,
                                    "#document");

    const bool is_table_like_role = ui::IsTableLike(dst->role);
    if (is_table_like_role) {
      int column_count = src.ColumnCount();
      int row_count = src.RowCount();
      if (column_count > 0 && row_count > 0) {
        dst->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                             column_count);
        dst->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount,
                             row_count);
      }

      int aria_colcount = src.AriaColumnCount();
      if (aria_colcount)
        dst->AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                             aria_colcount);

      int aria_rowcount = src.AriaRowCount();
      if (aria_rowcount)
        dst->AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount,
                             aria_rowcount);
    }

    if (ui::IsTableRow(dst->role)) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex,
                           src.RowIndex());
      WebAXObject header = src.RowHeader();
      if (!header.IsDetached())
        dst->AddIntAttribute(ax::mojom::IntAttribute::kTableRowHeaderId,
                             header.AxID());
    }

    if (ui::IsCellOrTableHeader(dst->role)) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                           src.CellColumnIndex());
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                           src.CellColumnSpan());
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                           src.CellRowIndex());
      dst->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan,
                           src.CellRowSpan());
    }

    if (ui::IsCellOrTableHeader(dst->role) || ui::IsTableRow(dst->role)) {
      // aria-rowindex and aria-colindex are supported on cells, headers and
      // rows.
      int aria_rowindex = src.AriaRowIndex();
      if (aria_rowindex)
        dst->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex,
                             aria_rowindex);

      int aria_colindex = src.AriaColumnIndex();
      if (aria_colindex) {
        dst->AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                             aria_colindex);
      }
    }

    if (ui::IsTableHeader(dst->role) &&
        src.SortDirection() != ax::mojom::SortDirection::kNone) {
      dst->AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                           static_cast<int32_t>(src.SortDirection()));
    }

    if (dst->role == ax::mojom::Role::kImage)
      AddImageAnnotations(src, dst);

    // If a link or web area isn't otherwise labeled and contains
    // exactly one image (searching only to a max depth of 2),
    // annotate the link/web area with the image's annotation, too.
    if (dst->role == ax::mojom::Role::kLink ||
        dst->role == ax::mojom::Role::kRootWebArea) {
      WebAXObject inner_image;
      if (FindExactlyOneInnerImageInMaxDepthTwo(src, &inner_image))
        AddImageAnnotations(inner_image, dst);
    }

    WebNode node = src.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      WebElement element = node.To<WebElement>();
      if (element.HasHTMLTagName("input") && element.HasAttribute("type")) {
        TruncateAndAddStringAttribute(dst,
                                      ax::mojom::StringAttribute::kInputType,
                                      element.GetAttribute("type").Utf8());
      }
    }
  }

  // The majority of the rest of this code computes attributes needed for
  // all modes, not just for screen readers.

  WebNode node = src.GetNode();
  bool is_iframe = false;

  if (!node.IsNull() && node.IsElementNode()) {
    WebElement element = node.To<WebElement>();
    is_iframe = element.HasHTMLTagName("iframe");

    if (accessibility_mode_.has_mode(ui::AXMode::kHTML)) {
      // TODO(ctguil): The tagName in WebKit is lower cased but
      // HTMLElement::nodeName calls localNameUpper. Consider adding
      // a WebElement method that returns the original lower cased tagName.
      TruncateAndAddStringAttribute(
          dst, ax::mojom::StringAttribute::kHtmlTag,
          base::ToLowerASCII(element.TagName().Utf8()));
      for (unsigned i = 0; i < element.AttributeCount(); ++i) {
        std::string name =
            base::ToLowerASCII(element.AttributeLocalName(i).Utf8());
        std::string value = element.AttributeValue(i).Utf8();
        dst->html_attributes.push_back(std::make_pair(name, value));
      }

// TODO(nektar): Turn off kHTMLAccessibilityMode for automation and Mac
// and remove ifdef.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
      if (dst->role == ax::mojom::Role::kMath && element.InnerHTML().length()) {
        TruncateAndAddStringAttribute(dst,
                                      ax::mojom::StringAttribute::kInnerHtml,
                                      element.InnerHTML().Utf8());
      }
#endif
    }

    if (src.IsEditable()) {
      if (src.IsEditableRoot())
        dst->AddBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot, true);

      if (src.IsControl() && !src.IsRichlyEditable()) {
        // Only for simple input controls -- rich editable areas use AXTreeData.
          dst->AddIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                               src.SelectionStart());
          dst->AddIntAttribute(ax::mojom::IntAttribute::kTextSelEnd,
                               src.SelectionEnd());
      }
    }

    // ARIA role.
    if (element.HasAttribute("role")) {
      TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kRole,
                                    element.GetAttribute("role").Utf8());
    } else {
      std::string role = GetEquivalentAriaRoleString(dst->role);
      if (!role.empty())
        TruncateAndAddStringAttribute(dst, ax::mojom::StringAttribute::kRole,
                                      role);
    }

    // Browser plugin (used in a <webview>).
    BrowserPlugin* browser_plugin = BrowserPlugin::GetFromNode(element);
    if (browser_plugin) {
      dst->AddContentIntAttribute(
          AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID,
          browser_plugin->browser_plugin_instance_id());
    }

    // Frames and iframes.
    WebFrame* frame = WebFrame::FromFrameOwnerElement(element);
    if (frame) {
      dst->AddContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID,
                                  RenderFrame::GetRoutingIdForWebFrame(frame));
    }
  }

  // Add the ids of *indirect* children - those who are children of this node,
  // but whose parent is *not* this node. One example is a table
  // cell, which is a child of both a row and a column. Because the cell's
  // parent is the row, the row adds it as a child, and the column adds it
  // as an indirect child.
  int child_count = src.ChildCount();
  std::vector<int32_t> indirect_child_ids;
  for (int i = 0; i < child_count; ++i) {
    WebAXObject child = src.ChildAt(i);
    if (!is_iframe && !child.IsDetached() && !IsParentUnignoredOf(src, child))
      indirect_child_ids.push_back(child.AxID());
  }
  if (indirect_child_ids.size() > 0) {
    dst->AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                             indirect_child_ids);
  }

  if (src.IsScrollableContainer()) {
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable, true);
    const gfx::Point& scroll_offset = src.GetScrollOffset();
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollX, scroll_offset.x());
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollY, scroll_offset.y());

    const gfx::Point& min_scroll_offset = src.MinimumScrollOffset();
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin,
                         min_scroll_offset.x());
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin,
                         min_scroll_offset.y());

    const gfx::Point& max_scroll_offset = src.MaximumScrollOffset();
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollXMax,
                         max_scroll_offset.x());
    dst->AddIntAttribute(ax::mojom::IntAttribute::kScrollYMax,
                         max_scroll_offset.y());
  }

  if (dst->id == image_data_node_id_) {
    // In general, string attributes should be truncated using
    // TruncateAndAddStringAttribute, but ImageDataUrl contains a data url
    // representing an image, so add it directly using AddStringAttribute.
    dst->AddStringAttribute(ax::mojom::StringAttribute::kImageDataUrl,
                            src.ImageDataUrl(max_image_data_size_).Utf8());
  }

  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  WebVector<ax::mojom::Dropeffect> src_dropeffects;
  src.Dropeffects(src_dropeffects);
  if (!src_dropeffects.empty()) {
    for (auto&& dropeffect : src_dropeffects) {
      dst->AddDropeffect(dropeffect);
    }
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
    AXContentNodeData* dst,
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

void BlinkAXTreeSource::AddImageAnnotations(blink::WebAXObject& src,
                                            AXContentNodeData* dst) const {
  if (!base::FeatureList::IsEnabled(features::kExperimentalAccessibilityLabels))
    return;

  // Reject ignored objects
  if (src.AccessibilityIsIgnored()) {
    return;
  }

  // Reject images that are explicitly empty, or that have a name already.
  //
  // In the future, we may annotate some images that have a name
  // if we think we can add additional useful information.
  ax::mojom::NameFrom name_from;
  blink::WebVector<WebAXObject> name_objects;
  blink::WebString web_name = src.GetName(name_from, name_objects);

  // Normally we don't assign an annotation to an image if it already
  // has a name. There are a few exceptions where we ignore the name.
  bool treat_name_as_empty = false;

  // When visual debugging is enabled, the "title" attribute is set to a
  // string beginning with a "%". If the name comes from that string we
  // can ignore it, and treat the name as empty.
  if (image_annotation_debugging_ &&
      base::StartsWith(web_name.Utf8(), "%", base::CompareCase::SENSITIVE))
    treat_name_as_empty = true;

  // If the image's name is explicitly empty, or if it has a name (and
  // we're not treating the name as empty), then it's ineligible for
  // an annotation.
  if ((name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty ||
       !web_name.IsEmpty()) &&
      !treat_name_as_empty) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // If the name of a document (root web area) starts with the filename,
  // it probably means the user opened an image in a new tab.
  // If so, we can treat the name as empty and give it an annotation.
  std::string dst_name =
      dst->GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (dst->role == ax::mojom::Role::kRootWebArea) {
    std::string filename = GURL(document().Url()).ExtractFileName();
    if (base::StartsWith(dst_name, filename, base::CompareCase::SENSITIVE))
      treat_name_as_empty = true;
  }

  // |dst| may be a document or link containing an image. Skip annotating
  // it if it already has text other than whitespace.
  if (!base::ContainsOnlyChars(dst_name, base::kWhitespaceASCII) &&
      !treat_name_as_empty) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // Skip images that are too small to label. This also catches
  // unloaded images where the size is unknown.
  WebAXObject offset_container;
  WebFloatRect bounds;
  SkMatrix44 container_transform;
  bool clips_children = false;
  src.GetRelativeBounds(offset_container, bounds, container_transform,
                        &clips_children);
  if (bounds.width < kMinImageAnnotationWidth ||
      bounds.height < kMinImageAnnotationHeight) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // Skip images in documents which are not http, https, file and data schemes.
  GURL gurl = document().Url();
  if (!(gurl.SchemeIsHTTPOrHTTPS() || gurl.SchemeIsFile() ||
        gurl.SchemeIs(url::kDataScheme))) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme);
    return;
  }

  if (!image_annotator_) {
    if (!first_unlabeled_image_id_.has_value() ||
        first_unlabeled_image_id_.value() == src.AxID()) {
      dst->SetImageAnnotationStatus(
          ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
      first_unlabeled_image_id_ = src.AxID();
    } else {
      dst->SetImageAnnotationStatus(
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);
    }
    return;
  }

  if (image_annotator_->HasAnnotationInCache(src)) {
    dst->AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                            image_annotator_->GetImageAnnotation(src));
    dst->SetImageAnnotationStatus(
        image_annotator_->GetImageAnnotationStatus(src));
  } else if (image_annotator_->HasImageInCache(src)) {
    image_annotator_->OnImageUpdated(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  } else if (!image_annotator_->HasImageInCache(src)) {
    image_annotator_->OnImageAdded(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  }
}

}  // namespace content
