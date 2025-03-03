// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_media.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

constexpr MapCoordinatesFlags kMapCoordinatesFlags =
    kTraverseDocumentBoundaries | kApplyRemoteViewportTransform;
constexpr VisualRectFlags kVisualRectFlags = static_cast<VisualRectFlags>(
    kUseGeometryMapper | kVisualRectApplyRemoteViewportTransform);

constexpr float kHeading1FontSizeMultiplier = 2;
constexpr float kHeading3FontSizeMultiplier = 1.17;
constexpr float kHeading5FontSizeMultiplier = 0.83;
constexpr float kHeading6FontSizeMultiplier = 0.67;

// TODO(crbug.com/383128653): This is duplicating logic from
// UnsupportedTagTypeValueForNode, consider reusing it.
bool IsHeadingTag(const HTMLElement& element) {
  return element.HasTagName(html_names::kH1Tag) ||
         element.HasTagName(html_names::kH2Tag) ||
         element.HasTagName(html_names::kH3Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH5Tag) ||
         element.HasTagName(html_names::kH6Tag);
}

mojom::blink::AIPageContentAnchorRel GetAnchorRel(const AtomicString& rel) {
  if (rel == "noopener") {
    return mojom::blink::AIPageContentAnchorRel::kRelationNoOpener;
  } else if (rel == "noreferrer") {
    return mojom::blink::AIPageContentAnchorRel::kRelationNoReferrer;
  } else if (rel == "opener") {
    return mojom::blink::AIPageContentAnchorRel::kRelationOpener;
  } else if (rel == "privacy-policy") {
    return mojom::blink::AIPageContentAnchorRel::kRelationPrivacyPolicy;
  } else if (rel == "terms-of-service") {
    return mojom::blink::AIPageContentAnchorRel::kRelationTermsOfService;
  }
  return mojom::blink::AIPageContentAnchorRel::kRelationUnknown;
}

// Returns the relative text size of the object compared to the document
// default. Ratios are based on browser defaults for headings, which are as
// follows:
//
// Heading 1: 2em
// Heading 2: 1.5em
// Heading 3: 1.17em
// Heading 4: 1em
// Heading 5: 0.83em
// Heading 6: 0.67em
mojom::blink::AIPageContentTextSize GetTextSize(
    const ComputedStyle& style,
    const ComputedStyle& document_style) {
  float font_size_multiplier =
      style.ComputedFontSize() / document_style.ComputedFontSize();
  if (font_size_multiplier >= kHeading1FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kXL;
  } else if (font_size_multiplier >= kHeading3FontSizeMultiplier &&
             font_size_multiplier < kHeading1FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kL;
  } else if (font_size_multiplier >= kHeading5FontSizeMultiplier &&
             font_size_multiplier < kHeading3FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kM;
  } else if (font_size_multiplier >= kHeading6FontSizeMultiplier &&
             font_size_multiplier < kHeading5FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kS;
  } else {  // font_size_multiplier < kHeading6FontSizeMultiplier
    return mojom::blink::AIPageContentTextSize::kXS;
  }
}

// If the style has a non-normal font weight, has applied text decorations, or
// is a super/subscript, then the text is considered to have emphasis.
bool HasEmphasis(const ComputedStyle& style) {
  return style.GetFontWeight() != kNormalWeightValue ||
         style.GetFontStyle() != kNormalSlopeValue ||
         style.HasAppliedTextDecorations() ||
         style.VerticalAlign() == EVerticalAlign::kSub ||
         style.VerticalAlign() == EVerticalAlign::kSuper;
}

RGBA32 GetColor(const ComputedStyle& style) {
  return style.VisitedDependentColor(GetCSSPropertyColor()).Rgb();
}

const LayoutIFrame* GetIFrame(const LayoutObject& object) {
  return DynamicTo<LayoutIFrame>(object);
}

bool IsGenericContainer(
    const LayoutObject& object,
    const Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) {
  if (object.Style()->GetPosition() == EPosition::kFixed) {
    return true;
  }

  if (object.Style()->GetPosition() == EPosition::kSticky) {
    return true;
  }

  if (object.Style()->ScrollsOverflow()) {
    return true;
  }

  if (object.IsInTopOrViewTransitionLayer()) {
    return true;
  }

  if (const auto* element = DynamicTo<HTMLElement>(object.GetNode())) {
    if (element->HasTagName(html_names::kFigureTag)) {
      return true;
    }

    if (element->IsFocused()) {
      return true;
    }
  }

  if (AXObjectCache* ax_object_cache =
          object.GetDocument().ExistingAXObjectCache()) {
    if (Node* ax_focused_node = ax_object_cache->GetAccessibilityFocus()) {
      if (object.GetNode() == ax_focused_node) {
        return true;
      }
    }
  }

  if (!annotated_roles.empty()) {
    return true;
  }

  return false;
}

void AddAnnotatedRoles(
    const LayoutObject& object,
    Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) {
  const auto& style = object.StyleRef();
  if (style.ContentVisibility() == EContentVisibility::kHidden) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kContentHidden);
  }

  // Element specific roles below.
  const auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (!element) {
    return;
  }
  if (element->HasTagName(html_names::kHeaderTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "banner") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kHeader);
  }
  if (element->HasTagName(html_names::kNavTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "navigation") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kNav);
  }
  if (element->HasTagName(html_names::kSearchTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "search") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSearch);
  }
  if (element->HasTagName(html_names::kMainTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "main") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kMain);
  }
  if (element->HasTagName(html_names::kArticleTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "article") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kArticle);
  }
  if (element->HasTagName(html_names::kSectionTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "region") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSection);
  }
  if (element->HasTagName(html_names::kAsideTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "complementary") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kAside);
  }
  if (element->HasTagName(html_names::kFooterTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "contentinfo") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kFooter);
  }
}

std::optional<DOMNodeId> GetDomNodeId(const LayoutObject& object) {
  auto* node = object.GetNode();
  if (object.IsLayoutView()) {
    node = &object.GetDocument();
  }

  if (!node) {
    return std::nullopt;
  }
  return DOMNodeIds::IdForNode(node);
}

bool IsVisible(const LayoutObject& object) {
  // Don't add content when node is invisible.
  return object.Style()->Visibility() == EVisibility::kVisible;
}

bool ShouldSkipSubtree(const LayoutObject& object) {
  auto* layout_embedded_content = DynamicTo<LayoutEmbeddedContent>(object);
  if (layout_embedded_content) {
    auto* layout_iframe = GetIFrame(object);

    // Skip embedded content that is not an iframe.
    // TODO(crbug.com/381273397): Add content for embed and object.
    if (!layout_iframe) {
      return true;
    }

    // Skip iframe nodes which don't have a Document.
    if (!layout_iframe->ChildFrameView()) {
      return true;
    }
  }

  // List markers are communicated by the kOrderedList and kUnorderedList
  // annotated roles.
  if (object.IsListMarker()) {
    return true;
  }

  // Table caption is communicated by the table name.
  if (object.IsTableCaption()) {
    return true;
  }

  // Skip empty text.
  auto* layout_text = DynamicTo<LayoutText>(object);
  if (layout_text && layout_text->IsAllCollapsibleWhitespace()) {
    return true;
  }

  if (DynamicTo<LayoutHTMLCanvas>(object)) {
    return true;
  }

  if (DynamicTo<LayoutSVGRoot>(object)) {
    return true;
  }

  return false;
}

void ProcessTextNode(const LayoutText& layout_text,
                     mojom::blink::AIPageContentAttributes& attributes,
                     const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kText;
  CHECK(IsVisible(layout_text));

  auto text_style = mojom::blink::AIPageContentTextStyle::New();
  text_style->text_size = GetTextSize(*layout_text.Style(), document_style);
  text_style->has_emphasis = HasEmphasis(*layout_text.Style());
  text_style->color = GetColor(*layout_text.Style());

  auto text_info = mojom::blink::AIPageContentTextInfo::New();
  text_info->text_content = layout_text.TransformedText();
  text_info->text_style = std::move(text_style);
  attributes.text_info = std::move(text_info);
}

void ProcessImageNode(const LayoutImage& layout_image,
                      mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kImage;
  CHECK(IsVisible(layout_image));

  if (DynamicTo<LayoutMedia>(layout_image)) {
    return;
  }

  auto image_info = mojom::blink::AIPageContentImageInfo::New();

  if (auto* image_element =
          DynamicTo<HTMLImageElement>(layout_image.GetNode())) {
    // TODO(crbug.com/383127202): A11y stack generates alt text using image
    // data which could be reused for this.
    image_info->image_caption = image_element->AltText();
  }

  // TODO(crbug.com/382558422): Include image source origin.
  attributes.image_info = std::move(image_info);
}

void ProcessAnchorNode(const HTMLAnchorElement& anchor_element,
                       mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kAnchor;
  if (!IsVisible(*anchor_element.GetLayoutObject())) {
    return;
  }

  auto anchor_data = mojom::blink::AIPageContentAnchorData::New();
  anchor_data->url = anchor_element.Url();
  for (unsigned i = 0; i < anchor_element.relList().length(); ++i) {
    anchor_data->rel.push_back(GetAnchorRel(anchor_element.relList().item(i)));
  }
  attributes.anchor_data = std::move(anchor_data);
}

void ProcessTableNode(const LayoutTable& layout_table,
                      mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kTable;
  if (!IsVisible(layout_table)) {
    return;
  }

  auto table_data = mojom::blink::AIPageContentTableData::New();
  for (auto* section = layout_table.FirstChild(); section;
       section = section->NextSibling()) {
    if (section->IsTableCaption()) {
      StringBuilder table_name;
      auto* caption = To<LayoutTableCaption>(section);
      for (auto* child = caption->FirstChild(); child;
           child = child->NextSibling()) {
        if (const auto* layout_text = DynamicTo<LayoutText>(*child)) {
          table_name.Append(layout_text->TransformedText());
        }
      }
      table_data->table_name = table_name.ToString();
    }
  }
  attributes.table_data = std::move(table_data);
}

void ProcessFormNode(const HTMLFormElement& form_element,
                     mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kForm;
  if (!IsVisible(*form_element.GetLayoutObject())) {
    return;
  }
  auto form_data = mojom::blink::AIPageContentFormData::New();
  if (const auto& name = form_element.GetName()) {
    form_data->form_name = name;
  }
  attributes.form_data = std::move(form_data);
}

void ProcessFormControlNode(const HTMLFormControlElement& form_control_element,
                            mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kFormControl;
  if (!IsVisible(*form_control_element.GetLayoutObject())) {
    return;
  }
  auto form_control_data = mojom::blink::AIPageContentFormControlData::New();
  form_control_data->form_control_type = form_control_element.FormControlType();
  form_control_data->field_name = form_control_element.GetName();
  form_control_data->is_required = form_control_element.IsRequired();
  if (const auto* text_control_element =
          DynamicTo<TextControlElement>(form_control_element)) {
    form_control_data->field_value = text_control_element->Value();
    form_control_data->placeholder =
        text_control_element->GetPlaceholderValue();
  }
  if (const auto* html_input_element =
          DynamicTo<HTMLInputElement>(form_control_element)) {
    form_control_data->is_checked = html_input_element->Checked();
  }
  if (const auto* select_element =
          DynamicTo<HTMLSelectElement>(form_control_element)) {
    for (auto& option_element : select_element->GetOptionList()) {
      auto select_option = mojom::blink::AIPageContentSelectOption::New();
      select_option->value = option_element.value();
      select_option->text = option_element.text();
      select_option->is_selected = option_element.Selected();
      form_control_data->select_options.push_back(std::move(select_option));
    }
  }
  attributes.form_control_data = std::move(form_control_data);
}

mojom::blink::AIPageContentTableRowType GetTableRowType(
    const LayoutTableRow& layout_table_row) {
  if (auto* section = layout_table_row.Section()) {
    if (auto* table_section_element =
            DynamicTo<HTMLElement>(section->GetNode())) {
      if (table_section_element->HasTagName(html_names::kTheadTag)) {
        return mojom::blink::AIPageContentTableRowType::kHeader;
      } else if (table_section_element->HasTagName(html_names::kTfootTag)) {
        return mojom::blink::AIPageContentTableRowType::kFooter;
      }
    }
  }
  return mojom::blink::AIPageContentTableRowType::kBody;
}

void ProcessTableRowNode(const LayoutTableRow& layout_table_row,
                         mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kTableRow;
  if (!IsVisible(layout_table_row)) {
    return;
  }

  auto table_row_data = mojom::blink::AIPageContentTableRowData::New();
  table_row_data->row_type = GetTableRowType(layout_table_row);
  attributes.table_row_data = std::move(table_row_data);
}

// Records latency metrics for the given latency and total latency.
void RecordLatencyMetrics(base::TimeDelta latency,
                          base::TimeDelta latency_with_scheduling_delay,
                          bool is_main_frame,
                          const mojom::blink::AIPageContentOptions& options) {
  if (is_main_frame) {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.MainFrame", latency);
  } else {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame",
        latency);
  }

  if (options.on_critical_path) {
    if (is_main_frame) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "Critical."
          "MainFrame",
          latency_with_scheduling_delay);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "Critical."
          "RemoteSubFrame",
          latency_with_scheduling_delay);
    }
  } else {
    if (is_main_frame) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "NonCritical."
          "MainFrame",
          latency_with_scheduling_delay);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "NonCritical."
          "RemoteSubFrame",
          latency_with_scheduling_delay);
    }
  }
}

// Runs the given tasks.
void RunTasks(WTF::Vector<base::OnceClosure> tasks) {
  for (auto& task : tasks) {
    std::move(task).Run();
  }
}

bool ShouldRunLifecycleForSyncExtraction(
    const mojom::blink::AIPageContentOptions& options) {
  // Including hidden searchable content requires layout for nodes which are
  // skipped during rendering. So we need a special lifecycle for them and can't
  // use the computed state from the regular lifecycle update.
  return options.on_critical_path || options.include_hidden_searchable_content;
}

}  // namespace

// static
const char AIPageContentAgent::kSupplementName[] = "AIPageContentAgent";

// static
AIPageContentAgent* AIPageContentAgent::From(Document& document) {
  return Supplement<Document>::From<AIPageContentAgent>(document);
}

// static
void AIPageContentAgent::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver) {
  CHECK(frame && frame->GetDocument());
  CHECK(frame->IsLocalRoot());

  auto& document = *frame->GetDocument();
  auto* agent = AIPageContentAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<AIPageContentAgent>(
        base::PassKey<AIPageContentAgent>(), *frame);
    Supplement<Document>::ProvideTo(document, agent);
  }
  agent->Bind(std::move(receiver));
}

AIPageContentAgent* AIPageContentAgent::GetOrCreateForTesting(
    Document& document) {
  auto* agent = AIPageContentAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<AIPageContentAgent>(
        base::PassKey<AIPageContentAgent>(), *document.GetFrame());
    Supplement<Document>::ProvideTo(document, agent);
  }
  return agent;
}

AIPageContentAgent::AIPageContentAgent(base::PassKey<AIPageContentAgent>,
                                       LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()) {}

AIPageContentAgent::~AIPageContentAgent() = default;

void AIPageContentAgent::Bind(
    mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver) {
  receiver_set_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
}

void AIPageContentAgent::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_set_);
  Supplement<Document>::Trace(visitor);
}

void AIPageContentAgent::DidFinishPostLifecycleSteps(const LocalFrameView&) {
  RunTasksIfReady();
}

void AIPageContentAgent::GetAIPageContent(
    mojom::blink::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (ShouldRunLifecycleForSyncExtraction(*options)) {
    GetAIPageContentSync(std::move(options), std::move(callback), start_time);
    return;
  }

  if (!is_registered_) {
    is_registered_ = true;
    if (LocalFrameView* view = GetSupplementable()->View()) {
      view->RegisterForLifecycleNotifications(this);
    }
  }

  // Running lifecycle beyond layout is expensive and the information is only
  // needed to compute geometry. Limit the update to layout if we don't need the
  // geometry.
  // We don't expect many overlapping calls to this service as the browser will
  // only issue one request at a time.
  if (options->include_geometry) {
    geometry_tasks_.push_back(WTF::BindOnce(
        &AIPageContentAgent::GetAIPageContentSync, WrapWeakPersistent(this),
        std::move(options), std::move(callback), start_time));
  } else {
    layout_clean_tasks_.push_back(WTF::BindOnce(
        &AIPageContentAgent::GetAIPageContentSync, WrapWeakPersistent(this),
        std::move(options), std::move(callback), start_time));
  }

  // Run tasks if the document lifecycle is at least as advanced.
  RunTasksIfReady();
}

void AIPageContentAgent::RunTasksIfReady() {
  if (GetSupplementable()->Lifecycle().GetState() >=
      DocumentLifecycle::kPrePaintClean) {
    RunTasks(std::move(geometry_tasks_));
  }
  if (GetSupplementable()->Lifecycle().GetState() >=
      DocumentLifecycle::kLayoutClean) {
    RunTasks(std::move(layout_clean_tasks_));
  }
}

void AIPageContentAgent::GetAIPageContentSync(
    mojom::blink::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback,
    base::TimeTicks start_time) const {
  const auto sync_start_time = base::TimeTicks::Now();

  auto content = GetAIPageContentInternal(*options);
  if (!content) {
    std::move(callback).Run(nullptr);
    return;
  }

  const auto end_time = base::TimeTicks::Now();
  RecordLatencyMetrics(end_time - sync_start_time, end_time - start_time,
                       GetSupplementable()->GetFrame()->IsOutermostMainFrame(),
                       *options);
  std::move(callback).Run(std::move(content));
}

mojom::blink::AIPageContentPtr AIPageContentAgent::GetAIPageContentInternal(
    const mojom::blink::AIPageContentOptions& options) const {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->View()) {
    return nullptr;
  }

  auto* builder = MakeGarbageCollected<ContentBuilder>(options);
  return builder->Build(*frame);
}

AIPageContentAgent::ContentBuilder::ContentBuilder(
    const mojom::blink::AIPageContentOptions& options)
    : options_(options),
      content_node_id_map_(MakeGarbageCollected<ContentNodeIdMap>()) {}

AIPageContentAgent::ContentBuilder::~ContentBuilder() = default;

mojom::blink::AIPageContentPtr AIPageContentAgent::ContentBuilder::Build(
    LocalFrame& frame) {
  auto& document = *frame.GetDocument();

  mojom::blink::AIPageContentPtr page_content =
      mojom::blink::AIPageContent::New();

  // Force activatable locks so content which is accessible via find-in-page is
  // styled/laid out and included when walking the tree below.
  //
  // TODO(crbug.com/387355768): Consider limiting the lock to nodes with
  // activation reason of FindInPage.
  std::vector<DisplayLockDocumentState::ScopedForceActivatableDisplayLocks>
      forced_activatable_locks;
  if (ShouldRunLifecycleForSyncExtraction(*options_)) {
    if (options_->include_hidden_searchable_content) {
      forced_activatable_locks.emplace_back(
          document.GetDisplayLockDocumentState()
              .GetScopedForceActivatableLocks());
      document.View()->ForAllChildLocalFrameViews(
          [&](LocalFrameView& frame_view) {
            if (!frame_view.GetFrame().GetDocument()) {
              return;
            }

            forced_activatable_locks.emplace_back(
                frame_view.GetFrame()
                    .GetDocument()
                    ->GetDisplayLockDocumentState()
                    .GetScopedForceActivatableLocks());
          });
    }

    // Running lifecycle beyond layout is expensive and the information is only
    // needed to compute geometry. Limit the update to layout if we don't need
    // the geometry.
    if (options_->include_geometry) {
      document.View()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kUnknown);
    } else {
      document.View()->UpdateLifecycleToLayoutClean(
          DocumentUpdateReason::kUnknown);
    }
  }

  auto* layout_view = document.GetLayoutView();
  auto* document_style = layout_view->Style();
  auto root_node = MaybeGenerateContentNode(*layout_view, *document_style);
  CHECK(root_node);

  WalkChildren(*layout_view, *root_node, *document_style);
  page_content->root_node = std::move(root_node);

  // Must add page and frame interaction info after the entire tree is built.
  AddPageInteractionInfo(document, *page_content);
  AddFrameInteractionInfo(frame, *page_content);

  return page_content;
}

bool AIPageContentAgent::ContentBuilder::WalkChildren(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const ComputedStyle& document_style) const {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return false;
  }

  bool has_visible_content = false;
  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (ShouldSkipSubtree(*child)) {
      continue;
    }

    has_visible_content |= IsVisible(*child);

    bool child_has_visible_content = false;
    auto child_content_node = MaybeGenerateContentNode(*child, document_style);
    if (child_content_node &&
        child_content_node->content_attributes->attribute_type ==
            mojom::blink::AIPageContentAttributeType::kIframe) {
      // If the child is an iframe, it does its own tree walk.
    } else {
      auto& node_for_child =
          child_content_node ? *child_content_node : content_node;
      child_has_visible_content =
          WalkChildren(*child, node_for_child, document_style);
      has_visible_content |= child_has_visible_content;
    }

    const bool should_add_node_for_child =
        IsVisible(*child) || child_has_visible_content;
    if (should_add_node_for_child && child_content_node) {
      content_node.children_nodes.emplace_back(std::move(child_content_node));
    }
  }

  return has_visible_content;
}

void AIPageContentAgent::ContentBuilder::ProcessIframe(
    const LayoutIFrame& object,
    mojom::blink::AIPageContentNode& content_node) const {
  CHECK(IsVisible(object));

  content_node.content_attributes->attribute_type =
      mojom::blink::AIPageContentAttributeType::kIframe;

  auto& frame = object.ChildFrameView()->GetFrame();

  auto iframe_data = mojom::blink::AIPageContentIframeData::New();
  iframe_data->frame_token = frame.GetFrameToken();
  iframe_data->likely_ad_frame = frame.IsAdFrame();
  content_node.content_attributes->iframe_data = std::move(iframe_data);

  auto* local_frame = DynamicTo<LocalFrame>(frame);
  auto* child_layout_view =
      local_frame ? local_frame->ContentLayoutObject() : nullptr;
  if (child_layout_view) {
    // Add a node for the iframe's LayoutView for consistency with remote
    // frames.
    auto child_content_node = MaybeGenerateContentNode(
        *child_layout_view, *child_layout_view->Style());
    CHECK(child_content_node);

    // We could consider removing an iframe with no visible content. But this is
    // likely not common and should be done in the browser so it's consistently
    // done for local and remote frames.
    WalkChildren(*child_layout_view, *child_content_node,
                 *child_layout_view->Style());
    content_node.children_nodes.emplace_back(std::move(child_content_node));
  }

  if (local_frame) {
    // Must add frame interaction info after the entire frame subtree is built.
    AddFrameInteractionInfo(*local_frame,
                            *content_node.content_attributes->iframe_data);
  }
}

mojom::blink::AIPageContentNodePtr
AIPageContentAgent::ContentBuilder::MaybeGenerateContentNode(
    const LayoutObject& object,
    const ComputedStyle& document_style) const {
  auto content_node = mojom::blink::AIPageContentNode::New();
  content_node->content_attributes =
      mojom::blink::AIPageContentAttributes::New();
  mojom::blink::AIPageContentAttributes& attributes =
      *content_node->content_attributes;
  AddAnnotatedRoles(object, attributes.annotated_roles);

  // Set the attribute type and add any special attributes if the attribute type
  // requires it.
  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (const auto* iframe = GetIFrame(object)) {
    // If the `iframe` is invisible, it's Document can't override this and must
    // also be invisible.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessIframe(*iframe, *content_node);
  } else if (object.IsLayoutView()) {
    attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kRoot;
  } else if (object.IsText()) {
    // Since text is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessTextNode(To<LayoutText>(object), attributes, document_style);
  } else if (object.IsLayoutImage()) {
    // Since image is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessImageNode(To<LayoutImage>(object), attributes);
  } else if (const auto* anchor_element =
                 DynamicTo<HTMLAnchorElement>(object.GetNode())) {
    ProcessAnchorNode(*anchor_element, attributes);
  } else if (object.IsTable()) {
    ProcessTableNode(To<LayoutTable>(object), attributes);
  } else if (object.IsTableRow()) {
    ProcessTableRowNode(To<LayoutTableRow>(object), attributes);
  } else if (object.IsTableCell()) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kTableCell;
  } else if (const auto* form_element =
                 DynamicTo<HTMLFormElement>(object.GetNode())) {
    ProcessFormNode(*form_element, attributes);
  } else if (const auto* form_control =
                 DynamicTo<HTMLFormControlElement>(object.GetNode())) {
    ProcessFormControlNode(*form_control, attributes);
  } else if (element && IsHeadingTag(*element)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kHeading;
  } else if (element && element->HasTagName(html_names::kPTag)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kParagraph;
  } else if (element && element->HasTagName(html_names::kOlTag)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kOrderedList;
  } else if (element && (element->HasTagName(html_names::kUlTag) ||
                         element->HasTagName(html_names::kDlTag))) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kUnorderedList;
  } else if (element && (element->HasTagName(html_names::kLiTag) ||
                         element->HasTagName(html_names::kDtTag) ||
                         element->HasTagName(html_names::kDdTag))) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kListItem;
  } else if (IsGenericContainer(object, attributes.annotated_roles)) {
    // Be sure to set annotated roles before calling IsGenericContainer, as
    // IsGenericContainer will check for annotated roles.
    // Keep container at the bottom of the list as it is the least specific.
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kContainer;
  } else {
    // If no attribute type was set, do not generate a content node.
    return nullptr;
  }

  // Set the content node id once it is clear that the node will be generated.
  attributes.content_node_id = content_node_id_counter_;
  content_node_id_counter_++;
  if (Node* node = object.GetNode()) {
    content_node_id_map_->insert(node, attributes.content_node_id);
  }

  if (auto dom_node_id = AddDomNodeId(object, attributes)) {
    attributes.common_ancestor_dom_node_id = *dom_node_id;
  }

  AddNodeGeometry(object, attributes);

  AddNodeInteractionInfo(object, attributes);

  return content_node;
}

std::optional<DOMNodeId> AIPageContentAgent::ContentBuilder::AddDomNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (auto node_id = GetDomNodeId(object)) {
    attributes.dom_node_ids.push_back(*node_id);
    return node_id;
  }

  return std::nullopt;
}

void AIPageContentAgent::ContentBuilder::AddNodeGeometry(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (!options_->include_geometry) {
    return;
  }

  attributes.geometry = mojom::blink::AIPageContentGeometry::New();
  mojom::blink::AIPageContentGeometry& geometry = *attributes.geometry;

  geometry.outer_bounding_box =
      object.AbsoluteBoundingBoxRect(kMapCoordinatesFlags);

  gfx::RectF visible_bounding_box =
      object.LocalBoundingBoxRectForAccessibility();
  object.MapToVisualRectInAncestorSpace(nullptr, visible_bounding_box,
                                        kVisualRectFlags);
  geometry.visible_bounding_box = ToEnclosingRect(visible_bounding_box);

  geometry.is_fixed_or_sticky_position =
      object.Style()->GetPosition() == EPosition::kFixed ||
      object.Style()->GetPosition() == EPosition::kSticky;
}

void AIPageContentAgent::ContentBuilder::AddPageInteractionInfo(
    const Document& document,
    mojom::blink::AIPageContent& page_content) const {
  page_content.page_interaction_info =
      mojom::blink::AIPageContentPageInteractionInfo::New();
  mojom::blink::AIPageContentPageInteractionInfo& page_interaction_info =
      *page_content.page_interaction_info;

  // Focused element
  if (Element* element = document.FocusedElement()) {
    auto focused_node_id = content_node_id_map_->find(element);
    if (focused_node_id != content_node_id_map_->end()) {
      page_interaction_info.focused_node_id = focused_node_id->value;
    }
  }

  // Accessibility focus
  if (AXObjectCache* ax_object_cache = document.ExistingAXObjectCache()) {
    if (Node* ax_focused_node = ax_object_cache->GetAccessibilityFocus()) {
      auto accessibility_focused_node_id =
          content_node_id_map_->find(ax_focused_node);
      if (accessibility_focused_node_id != content_node_id_map_->end()) {
        page_interaction_info.accessibility_focused_node_id =
            accessibility_focused_node_id->value;
      }
    }
  }

  // Mouse location
  LocalFrame* frame = document.GetFrame();
  CHECK(frame);
  EventHandler& event_handler = frame->GetEventHandler();
  page_interaction_info.mouse_position =
      gfx::ToRoundedPoint(event_handler.LastKnownMousePositionInRootFrame());
}

void AIPageContentAgent::ContentBuilder::AddFrameInteractionInfo(
    const LocalFrame& frame,
    mojom::blink::AIPageContent& page_content) const {
  page_content.main_frame_interaction_info =
      mojom::blink::AIPageContentFrameInteractionInfo::New();
  mojom::blink::AIPageContentFrameInteractionInfo& frame_interaction_info =
      *page_content.main_frame_interaction_info;
  AddFrameInteractionInfo(frame, frame_interaction_info);
}

void AIPageContentAgent::ContentBuilder::AddFrameInteractionInfo(
    const LocalFrame& frame,
    mojom::blink::AIPageContentIframeData& iframe_data) const {
  iframe_data.frame_interaction_info =
      mojom::blink::AIPageContentFrameInteractionInfo::New();
  mojom::blink::AIPageContentFrameInteractionInfo& frame_interaction_info =
      *iframe_data.frame_interaction_info;
  AddFrameInteractionInfo(frame, frame_interaction_info);
}

void AIPageContentAgent::ContentBuilder::AddFrameInteractionInfo(
    const LocalFrame& frame,
    mojom::blink::AIPageContentFrameInteractionInfo& frame_interaction_info)
    const {
  // Selection
  if (!frame.SelectedText().empty()) {
    frame_interaction_info.selection =
        mojom::blink::AIPageContentSelection::New();
    mojom::blink::AIPageContentSelection& selection =
        *frame_interaction_info.selection;
    selection.selected_text = frame.SelectedText();

    const SelectionInDOMTree& frame_selection =
        frame.Selection().GetSelectionInDOMTree();
    const Position& start_position = frame_selection.ComputeStartPosition();
    const Position& end_position = frame_selection.ComputeEndPosition();
    Node* start_node = start_position.ComputeContainerNode();
    Node* end_node = end_position.ComputeContainerNode();
    auto start_node_id = content_node_id_map_->find(start_node);
    auto end_node_id = content_node_id_map_->find(end_node);
    if (start_node_id != content_node_id_map_->end()) {
      selection.start_node_id = start_node_id->value;
      selection.start_offset = start_position.ComputeOffsetInContainerNode();
    }
    if (end_node_id != content_node_id_map_->end()) {
      selection.end_node_id = end_node_id->value;
      selection.end_offset = end_position.ComputeOffsetInContainerNode();
    }
  }
}

void AIPageContentAgent::ContentBuilder::AddNodeInteractionInfo(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  attributes.node_interaction_info =
      mojom::blink::AIPageContentNodeInteractionInfo::New();
  mojom::blink::AIPageContentNodeInteractionInfo& node_interaction_info =
      *attributes.node_interaction_info;
  const ComputedStyle& style = *object.Style();
  node_interaction_info.scrolls_overflow_x = style.ScrollsOverflowX();
  node_interaction_info.scrolls_overflow_y = style.ScrollsOverflowY();
  bool is_selectable = object.IsSelectable();
  node_interaction_info.is_selectable = is_selectable;

  if (auto* node = object.GetNode()) {
    node_interaction_info.is_editable = IsEditable(*node);
  }

  if (auto* box = DynamicTo<LayoutBox>(object)) {
    if (box->CanResize()) {
      EResize resize = style.UsedResize();
      node_interaction_info.can_resize_vertical =
          resize == EResize::kVertical || resize == EResize::kBoth;
      node_interaction_info.can_resize_horizontal =
          resize == EResize::kHorizontal || resize == EResize::kBoth;
    }
  }

  if (auto* element = DynamicTo<HTMLElement>(object.GetNode())) {
    node_interaction_info.is_focusable = element->IsFocusable();
    node_interaction_info.is_draggable = element->draggable();
    node_interaction_info.is_clickable = element->IsMaybeClickable();
  }
}

void AIPageContentAgent::ContentBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(content_node_id_map_);
}

}  // namespace blink
