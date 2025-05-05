// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
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
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
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
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

constexpr MapCoordinatesFlags kMapCoordinatesFlags =
    kTraverseDocumentBoundaries | kApplyRemoteViewportTransform;
constexpr VisualRectFlags kVisualRectFlags = static_cast<VisualRectFlags>(
    kUseGeometryMapper | kVisualRectApplyRemoteViewportTransform |
    kIgnoreFilters);

constexpr float kHeading1FontSizeMultiplier = 2;
constexpr float kHeading3FontSizeMultiplier = 1.17;
constexpr float kHeading5FontSizeMultiplier = 0.83;
constexpr float kHeading6FontSizeMultiplier = 0.67;

ListBasedHitTestBehavior CollectHitTestNodes(std::vector<DOMNodeId>& hit_nodes,
                                             const Node& node,
                                             DOMNodeId dom_node_id) {
  if (node.GetLayoutObject()) {
    hit_nodes.push_back(dom_node_id);
  }
  return kContinueHitTesting;
}

gfx::Rect ComputeVisibleBoundingBox(const LayoutObject& object) {
  gfx::RectF visible_bounding_box =
      object.LocalBoundingBoxRectForAccessibility();

  // TODO(khushalsagar): It might be more optimal to derive this from output of
  // paint.
  object.MapToVisualRectInAncestorSpace(nullptr, visible_bounding_box,
                                        kVisualRectFlags);
  return ToEnclosingRect(visible_bounding_box);
}

void ComputeScrollerInfo(
    const LayoutObject& object,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) {
  if (!object.IsBoxModelObject()) {
    return;
  }

  auto* scrollable_area = To<LayoutBoxModelObject>(object).GetScrollableArea();
  if (!scrollable_area) {
    return;
  }

  const auto scrolling_bounds = scrollable_area->ContentsSize();
  const auto visible_area = scrollable_area->VisibleContentRect();

  // If the visible area covers the scrollable area, scrolling this node will be
  // a no-op.
  if (scrolling_bounds == visible_area.size()) {
    DCHECK_EQ(visible_area.x(), 0);
    DCHECK_EQ(visible_area.y(), 0);

    return;
  }

  auto scroller_info = mojom::blink::AIPageContentScrollerInfo::New();
  scroller_info->scrolling_bounds = scrolling_bounds;
  scroller_info->visible_area = visible_area;
  scroller_info->user_scrollable_horizontal =
      scrollable_area->UserInputScrollable(kHorizontalScrollbar);
  scroller_info->user_scrollable_vertical =
      scrollable_area->UserInputScrollable(kVerticalScrollbar);
  interaction_info.scroller_info = std::move(scroller_info);
}

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

void ProcessSVGNode(const LayoutSVGRoot& layout_svg,
                    mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kSVG;
  CHECK(IsVisible(layout_svg));

  auto* element = DynamicTo<Element>(layout_svg.GetNode());
  if (!element) {
    return;
  }

  auto svg_data = mojom::blink::AIPageContentSVGData::New();
  svg_data->inner_text = element->innerText();
  attributes.svg_data = std::move(svg_data);
}

void ProcessCanvasNode(const LayoutHTMLCanvas& layout_canvas,
                       mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kCanvas;
  CHECK(IsVisible(layout_canvas));

  auto canvas_data = mojom::blink::AIPageContentCanvasData::New();
  canvas_data->layout_size = ToRoundedSize(layout_canvas.Size());
  attributes.canvas_data = std::move(canvas_data);
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
    // Don't include password values as they are sensitive.
    if (form_control_data->form_control_type !=
        mojom::blink::FormControlType::kInputPassword) {
      form_control_data->field_value = text_control_element->Value();
    }
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
      select_option->disabled = option_element.IsDisabledFormControl();
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
void RecordLatencyMetrics(base::TimeTicks start_time,
                          base::TimeTicks synchronous_execution_start_time,
                          base::TimeTicks end_time,
                          bool is_main_frame,
                          const mojom::blink::AIPageContentOptions& options) {
  const base::TimeDelta latency = end_time - synchronous_execution_start_time;
  const base::TimeDelta latency_with_scheduling_delay = end_time - start_time;

  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());

  if (is_main_frame) {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.MainFrame", latency);
    TRACE_EVENT_BEGIN("loading", "AIPageContentGenerationMainFrame",
                      trace_track, synchronous_execution_start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame",
        latency);
    TRACE_EVENT_BEGIN("loading", "AIPageContentGenerationRemoteSubFrame",
                      trace_track, synchronous_execution_start_time);
  }
  TRACE_EVENT_END("loading", trace_track, end_time);

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

// Returns true if extracting the content can't be deferred until the next
// frame.
bool NeedsSyncExtraction(const mojom::blink::AIPageContentOptions& options) {
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
  for (auto& task : std::move(async_extraction_tasks_)) {
    std::move(task).Run();
  }
  async_extraction_tasks_.clear();
}

void AIPageContentAgent::GetAIPageContent(
    mojom::blink::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  LocalFrameView* view = GetSupplementable()->View();

  // If there's no lifecycle pending, we can't rely on post lifecycle
  // notifications and the layout is likely clean.
  const bool can_do_sync_extraction = !view || !view->LifecycleUpdatePending();

  if (can_do_sync_extraction || NeedsSyncExtraction(*options)) {
    GetAIPageContentSync(std::move(options), std::move(callback), start_time);
    return;
  }

  if (!is_registered_) {
    is_registered_ = true;
    view->RegisterForLifecycleNotifications(this);
  }

  // We don't expect many overlapping calls to this service as the browser will
  // only issue one request at a time.
  async_extraction_tasks_.push_back(WTF::BindOnce(
      &AIPageContentAgent::GetAIPageContentSync, WrapWeakPersistent(this),
      std::move(options), std::move(callback), start_time));
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
  RecordLatencyMetrics(start_time, sync_start_time, end_time,
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

  ContentBuilder builder(options);
  return builder.Build(*frame);
}

AIPageContentAgent::ContentBuilder::ContentBuilder(
    const mojom::blink::AIPageContentOptions& options)
    : options_(options) {}

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

  auto* layout_view = document.GetLayoutView();
  auto* document_style = layout_view->Style();

  // Add nodes which have a currently active user interaction (selection, focus
  // etc) before walking the tree to ensure we promote interactive DOM nodes to
  // ContentNodes.
  //
  // Note: This is different from `NodeInteractionInfo` which tracks whether a
  // node supports any interaction.
  AddPageInteractionInfo(document, *page_content);
  auto frame_data = mojom::blink::AIPageContentFrameData::New();
  AddFrameData(frame, *frame_data);
  page_content->frame_data = std::move(frame_data);

  auto root_node = MaybeGenerateContentNode(*layout_view, *document_style);
  CHECK(root_node);
  WalkChildren(*layout_view, *root_node, *document_style);
  page_content->root_node = std::move(root_node);

  if (stack_depth_exceeded_) {
    ukm::builders::OptimizationGuide_AIPageContentAgent(document.UkmSourceID())
        .SetNodeDepthLimitExceeded(true)
        .Record(document.UkmRecorder());
  }

  return page_content;
}

void AIPageContentAgent::ContentBuilder::AddMetaData(
    const LocalFrame& frame,
    WTF::Vector<mojom::blink::AIPageContentMetaPtr>& meta_data) const {
  int max = options_->max_meta_elements;
  if (max == 0) {
    return;
  }

  int count = 0;
  const HTMLHeadElement* head = frame.GetDocument()->head();
  if (!head) {
    return;
  }
  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::ChildrenOf(*head)) {
    auto name = meta_element.GetName();
    if (name.empty()) {
      continue;
    }
    auto meta = mojom::blink::AIPageContentMeta::New();
    meta->name = name;
    auto content = meta_element.Content();
    if (content.empty()) {
      meta->content = "";
    } else {
      meta->content = content;
    }
    meta_data.push_back(std::move(meta));
    count++;
    if (count >= max) {
      break;
    }
  }
}

bool AIPageContentAgent::ContentBuilder::IsGenericContainer(
    const LayoutObject& object,
    const mojom::blink::AIPageContentAttributes& attributes) const {
  if (object.Style()->GetPosition() == EPosition::kFixed) {
    return true;
  }

  if (object.Style()->GetPosition() == EPosition::kSticky) {
    return true;
  }

  // This has some duplication with the scrollability in InteractionInfo but is
  // still required for 2 reasons:
  // 1. The interaction info is only computed when actionable elements are
  //    requested.
  // 2. The interaction info is meant to capture the current state (is the
  //    element scrollable given the current content). This is a heuristic to
  //    decide whether a node is likely to be a "container" based on the author
  //    making it scrollable.
  // TODO(khushalsagar): Consider removing this, no consumer relies on this
  // behaviour.
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
  }

  if (!attributes.annotated_roles.empty()) {
    return true;
  }

  if (attributes.node_interaction_info) {
    return true;
  }

  if (attributes.label_for_dom_node_id) {
    return true;
  }

  // Use `ExistingIdForNode` since an Id should have already been generated if
  // this node is interactive.
  if (interactive_dom_node_ids_.contains(
          DOMNodeIds::ExistingIdForNode(object.GetNode()))) {
    return true;
  }

  return false;
}

void AIPageContentAgent::ContentBuilder::AddInteractiveNode(
    DOMNodeId dom_node_id) {
  CHECK_NE(dom_node_id, kInvalidDOMNodeId);
  interactive_dom_node_ids_.insert(dom_node_id);
}

bool AIPageContentAgent::ContentBuilder::WalkChildren(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const RecursionData& recursion_data) {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return false;
  }

  // The max tree depth is the mojo kMaxRecursionDepth minus a buffer to leave
  // room for the root node, attributes of the final node, and mojo wrappers
  // used in message creation.
  static const int kMaxTreeDepth = kMaxRecursionDepth - 8;
  if (recursion_data.stack_depth > kMaxTreeDepth) {
    stack_depth_exceeded_ = true;
    return false;
  }

  bool has_visible_content = false;
  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (ShouldSkipSubtree(*child)) {
      continue;
    }

    RecursionData child_recursion_data(recursion_data);
    auto* child_element = DynamicTo<Element>(child->GetNode());
    if (!child_recursion_data.is_aria_disabled && child_element &&
        AXObject::IsAriaAttributeTrue(*child_element,
                                      html_names::kAriaDisabledAttr)) {
      child_recursion_data.is_aria_disabled = true;
    }

    has_visible_content |= IsVisible(*child);

    bool child_has_visible_content = false;
    auto child_content_node =
        MaybeGenerateContentNode(*child, child_recursion_data);
    if (child_content_node &&
        // If the child is an iframe, it does its own tree walk.
        // TODO(crbug.com/405173553): Moving ProcessIframe here might simplify
        // tree construction and keep stack depth counting in one place.
        (child_content_node->content_attributes->attribute_type ==
             mojom::blink::AIPageContentAttributeType::kIframe ||
         // We don't capture the SVG layout internally so there's no need to
         // walk their tree.
         child_content_node->content_attributes->attribute_type ==
             mojom::blink::AIPageContentAttributeType::kSVG ||
         // There's no layout nodes under a canvas, the content is just the
         // canvas buffer.
         child_content_node->content_attributes->attribute_type ==
             mojom::blink::AIPageContentAttributeType::kCanvas)) {
    } else {
      if (child_content_node) {
        child_recursion_data.stack_depth++;
      }

      auto& node_for_child =
          child_content_node ? *child_content_node : content_node;
      child_has_visible_content =
          WalkChildren(*child, node_for_child, child_recursion_data);
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
    mojom::blink::AIPageContentNode& content_node,
    const RecursionData& recursion_data) {
  CHECK(IsVisible(object));

  content_node.content_attributes->attribute_type =
      mojom::blink::AIPageContentAttributeType::kIframe;

  auto& frame = object.ChildFrameView()->GetFrame();

  auto iframe_data = mojom::blink::AIPageContentIframeData::New();
  iframe_data->frame_token = frame.GetFrameToken();
  iframe_data->likely_ad_frame = frame.IsAdFrame();

  content_node.content_attributes->iframe_data = std::move(iframe_data);

  auto* local_frame = DynamicTo<LocalFrame>(frame);

  // Add interaction metadata before walking the tree to ensure we promote
  // interactive DOM nodes to ContentNodes.
  if (local_frame && local_frame->GetDocument()) {
    auto frame_data = mojom::blink::AIPageContentFrameData::New();
    AddFrameData(*local_frame, *frame_data);
    content_node.content_attributes->iframe_data->local_frame_data =
        std::move(frame_data);
  }

  auto* child_layout_view =
      local_frame ? local_frame->ContentLayoutObject() : nullptr;
  if (child_layout_view) {
    RecursionData child_recursion_data(*child_layout_view->Style());
    // The aria attribute values don't pierce frame boundaries.
    child_recursion_data.is_aria_disabled = false;
    child_recursion_data.stack_depth = recursion_data.stack_depth + 1;

    // Add a node for the iframe's LayoutView for consistency with remote
    // frames.
    auto child_content_node =
        MaybeGenerateContentNode(*child_layout_view, child_recursion_data);
    CHECK(child_content_node);

    // We could consider removing an iframe with no visible content. But this is
    // likely not common and should be done in the browser so it's consistently
    // done for local and remote frames.
    WalkChildren(*child_layout_view, *child_content_node, child_recursion_data);
    content_node.children_nodes.emplace_back(std::move(child_content_node));
  }
}

mojom::blink::AIPageContentNodePtr
AIPageContentAgent::ContentBuilder::MaybeGenerateContentNode(
    const LayoutObject& object,
    const RecursionData& recursion_data) {
  auto content_node = mojom::blink::AIPageContentNode::New();
  content_node->content_attributes =
      mojom::blink::AIPageContentAttributes::New();
  mojom::blink::AIPageContentAttributes& attributes =
      *content_node->content_attributes;

  // Compute state that is used to decide whether this node generates a
  // ContentNode before making the decision below.
  AddAnnotatedRoles(object, attributes.annotated_roles);
  AddForDomNodeId(object, attributes);
  // Interaction info depends on aria role.
  AddAriaRole(object, attributes);
  AddNodeInteractionInfo(object, attributes, recursion_data.is_aria_disabled);

  // Set the attribute type and add any special attributes if the attribute type
  // requires it.
  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (const auto* iframe = GetIFrame(object)) {
    // If the `iframe` is invisible, it's Document can't override this and must
    // also be invisible.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessIframe(*iframe, *content_node, recursion_data);
  } else if (object.IsLayoutView()) {
    attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kRoot;
  } else if (object.IsText()) {
    // Since text is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessTextNode(To<LayoutText>(object), attributes,
                    recursion_data.document_style);
  } else if (object.IsLayoutImage()) {
    // Since image is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessImageNode(To<LayoutImage>(object), attributes);
  } else if (object.IsSVGRoot()) {
    // Since we add the full text under SVG directly, don't add anything if the
    // SVG is hidden.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessSVGNode(To<LayoutSVGRoot>(object), attributes);
  } else if (object.IsCanvas()) {
    // No content will be rendered if the canvas is hidden.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessCanvasNode(To<LayoutHTMLCanvas>(object), attributes);
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
  } else if (IsGenericContainer(object, attributes)) {
    // Be sure to set annotated roles before calling IsGenericContainer, as
    // IsGenericContainer will check for annotated roles.
    // Keep container at the bottom of the list as it is the least specific.
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kContainer;
  } else {
    // If no attribute type was set, do not generate a content node.
    return nullptr;
  }

  if (auto dom_node_id = GetDomNodeId(object)) {
    attributes.dom_node_id = *dom_node_id;
  }

  AddNodeGeometry(object, attributes);
  AddLabel(object, attributes);

  return content_node;
}

void AIPageContentAgent::ContentBuilder::AddLabel(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (!options_->enable_experimental_actionable_data) {
    return;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    return;
  }

  // TODO(khushalsagar): Look at `AXNodeObject::TextAlternative` which has other
  // sources for this.
  StringBuilder accumulated_text;
  const auto& aria_label =
      element->FastGetAttribute(html_names::kAriaLabelAttr);
  if (!aria_label.GetString().ContainsOnlyWhitespaceOrEmpty()) {
    accumulated_text.Append(aria_label);
  }

  const GCedHeapVector<Member<Element>>* aria_labelledby_elements =
      element->ElementsFromAttributeOrInternals(
          html_names::kAriaLabelledbyAttr);
  if (!aria_labelledby_elements) {
    attributes.label = accumulated_text.ToString();
    return;
  }

  for (const auto& label_element : *aria_labelledby_elements) {
    // We need to use textContent instead of innerText since aria labelled by
    // nodes don't need to be in the layout.
    auto text_content = label_element->textContent(true);
    if (text_content.ContainsOnlyWhitespaceOrEmpty()) {
      continue;
    }

    if (!accumulated_text.empty()) {
      accumulated_text.Append(" ");
    }

    accumulated_text.Append(text_content);
  }

  attributes.label = accumulated_text.ToString();
}

void AIPageContentAgent::ContentBuilder::AddForDomNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (!options_->enable_experimental_actionable_data) {
    return;
  }

  auto* label = DynamicTo<HTMLLabelElement>(object.GetNode());
  if (!label) {
    return;
  }

  auto* control = label->Control();
  if (!control) {
    return;
  }

  attributes.label_for_dom_node_id = DOMNodeIds::IdForNode(control);
}

void AIPageContentAgent::ContentBuilder::AddAnnotatedRoles(
    const LayoutObject& object,
    Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) const {
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
  if (paid_content_.IsPaidElement(element)) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kPaidContent);
  }
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
  geometry.visible_bounding_box = ComputeVisibleBoundingBox(object);

  geometry.is_fixed_or_sticky_position =
      object.Style()->GetPosition() == EPosition::kFixed ||
      object.Style()->GetPosition() == EPosition::kSticky;
}

void AIPageContentAgent::ContentBuilder::ComputeHitTestableNodesInViewport(
    const LocalFrame& frame,
    mojom::blink::AIPageContentFrameData& frame_data) {
  if (!options_->enable_experimental_actionable_data) {
    return;
  }

  const Document& document = *frame.GetDocument();
  if (!document.GetLayoutView()) {
    return;
  }

  const auto viewport_rect =
      ComputeVisibleBoundingBox(*document.GetLayoutView());
  if (viewport_rect.IsEmpty()) {
    return;
  }

  const auto local_visible_viewport_rect =
      document.GetLayoutView()->AbsoluteToLocalRect(PhysicalRect(viewport_rect),
                                                    kMapCoordinatesFlags);
  HitTestLocation location(local_visible_viewport_rect);

  std::vector<DOMNodeId> hit_nodes;
  HitTestRequest::HitNodeCb hit_node_cb =
      WTF::BindRepeating(&CollectHitTestNodes, std::ref(hit_nodes));
  HitTestRequest request(
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
          HitTestRequest::kListBased | HitTestRequest::kPenetratingList |
          HitTestRequest::kAvoidCache | HitTestRequest::kHitNodeCbWithId,
      nullptr, std::move(hit_node_cb));
  HitTestResult result(request, location);
  document.GetLayoutView()->HitTest(location, result);

  int32_t next_z_order = 1;
  std::for_each(hit_nodes.rbegin(), hit_nodes.rend(), [&](auto node_id) {
    if (dom_node_to_z_order_.contains(node_id)) {
      return;
    }

    auto* node = DOMNodeIds::NodeForId(node_id);
    CHECK(node);

    if (!node->IsDocumentNode() &&
        !document.ElementForHitTest(node,
                                    TreeScope::HitTestPointType::kInternal)) {
      return;
    }
    dom_node_to_z_order_[node_id] = next_z_order++;
  });
}

void AIPageContentAgent::ContentBuilder::AddPageInteractionInfo(
    const Document& document,
    mojom::blink::AIPageContent& page_content) {
  page_content.page_interaction_info =
      mojom::blink::AIPageContentPageInteractionInfo::New();
  mojom::blink::AIPageContentPageInteractionInfo& page_interaction_info =
      *page_content.page_interaction_info;

  // Focused element
  if (Element* element = document.FocusedElement()) {
    page_interaction_info.focused_dom_node_id = DOMNodeIds::IdForNode(element);
    AddInteractiveNode(*page_interaction_info.focused_dom_node_id);
  }

  // Accessibility focus
  if (AXObjectCache* ax_object_cache = document.ExistingAXObjectCache()) {
    if (Node* ax_focused_node = ax_object_cache->GetAccessibilityFocus()) {
      page_interaction_info.accessibility_focused_dom_node_id =
          DOMNodeIds::IdForNode(ax_focused_node);
      AddInteractiveNode(
          *page_interaction_info.accessibility_focused_dom_node_id);
    }
  }

  // Mouse location
  LocalFrame* frame = document.GetFrame();
  CHECK(frame);
  EventHandler& event_handler = frame->GetEventHandler();
  page_interaction_info.mouse_position =
      gfx::ToRoundedPoint(event_handler.LastKnownMousePositionInRootFrame());
}

void AIPageContentAgent::ContentBuilder::AddFrameData(
    const LocalFrame& frame,
    mojom::blink::AIPageContentFrameData& frame_data) {
  frame_data.frame_interaction_info =
      mojom::blink::AIPageContentFrameInteractionInfo::New();
  frame_data.title = frame.GetDocument()->title();
  AddFrameInteractionInfo(frame, *frame_data.frame_interaction_info);
  AddMetaData(frame, frame_data.meta_data);

  if (RuntimeEnabledFeatures::AIPageContentPaidContentAnnotationEnabled()) {
    if (paid_content_.QueryPaidElements(*frame.GetDocument())) {
      frame_data.contains_paid_content = true;
    }
  }

  ComputeHitTestableNodesInViewport(frame, frame_data);
}

void AIPageContentAgent::ContentBuilder::AddFrameInteractionInfo(
    const LocalFrame& frame,
    mojom::blink::AIPageContentFrameInteractionInfo& frame_interaction_info) {
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

    if (start_node) {
      selection.start_dom_node_id = DOMNodeIds::IdForNode(start_node);
      AddInteractiveNode(selection.start_dom_node_id);

      selection.start_offset = start_position.ComputeOffsetInContainerNode();
    }

    if (end_node) {
      selection.end_dom_node_id = DOMNodeIds::IdForNode(end_node);
      AddInteractiveNode(selection.end_dom_node_id);

      selection.end_offset = end_position.ComputeOffsetInContainerNode();
    }
  }
}

void AIPageContentAgent::ContentBuilder::AddInteractionInfoForHitTesting(
    const Node* node,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) const {
  if (!options_->enable_experimental_actionable_data) {
    return;
  }

  auto it = dom_node_to_z_order_.find(DOMNodeIds::ExistingIdForNode(node));
  if (it != dom_node_to_z_order_.end()) {
    interaction_info.document_scoped_z_order = it->second;
  }
}

void AIPageContentAgent::ContentBuilder::AddAriaRole(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) {
  if (!options_->enable_experimental_actionable_data) {
    return;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    attributes.aria_role = ax::mojom::blink::Role::kUnknown;
    return;
  }

  auto aria_role = AXObject::AriaAttribute(*element, html_names::kRoleAttr);
  if (aria_role.empty()) {
    attributes.aria_role = ax::mojom::blink::Role::kUnknown;
    return;
  }

  attributes.aria_role = AXObject::FirstValidRoleInRoleString(aria_role);
}

void AIPageContentAgent::ContentBuilder::AddNodeInteractionInfo(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes,
    bool is_aria_disabled) const {
  // The node is not hit-testable which also means no interaction is supported.
  const ComputedStyle& style = *object.Style();
  if (style.UsedPointerEvents() == EPointerEvents::kNone) {
    return;
  }

  const auto* node = object.GetNode();
  if (!node) {
    return;
  }

  // Nodes which are not interactive can still consume events if they are
  // hit-testable.
  auto node_interaction_info =
      mojom::blink::AIPageContentNodeInteractionInfo::New();
  AddInteractionInfoForHitTesting(node, *node_interaction_info);

  auto* form_control_element = DynamicTo<HTMLFormControlElement>(node);
  const bool disabled =
      (form_control_element && form_control_element->IsActuallyDisabled()) ||
      is_aria_disabled;
  if (disabled) {
    if (node_interaction_info->document_scoped_z_order) {
      attributes.node_interaction_info = std::move(node_interaction_info);
    }

    return;
  }

  ComputeScrollerInfo(object, *node_interaction_info);

  // If experimental data is disabled, only scrollable nodes are included.
  if (!options_->enable_experimental_actionable_data) {
    if (node_interaction_info->scroller_info) {
      attributes.node_interaction_info = std::move(node_interaction_info);
    }

    return;
  }

  node_interaction_info->is_selectable =
      style.UsedUserSelect() != EUserSelect::kNone;

  node_interaction_info->is_editable = IsEditable(*node);

  if (auto* box = DynamicTo<LayoutBox>(object)) {
    if (box->CanResize()) {
      EResize resize = style.UsedResize();
      node_interaction_info->can_resize_vertical =
          resize == EResize::kVertical || resize == EResize::kBoth;
      node_interaction_info->can_resize_horizontal =
          resize == EResize::kHorizontal || resize == EResize::kBoth;
    }
  }

  if (auto* element = DynamicTo<Element>(object.GetNode())) {
    node_interaction_info->is_focusable = element->IsFocusable();
    node_interaction_info->is_clickable =
        element->IsMaybeClickable() || ui::IsClickable(*attributes.aria_role);

    if (auto* html_element = DynamicTo<HTMLElement>(element)) {
      node_interaction_info->is_draggable = html_element->draggable();
    }
  }

  const bool needs_interaction_info =
      node_interaction_info->scroller_info ||
      // The common case is for the content to be selectable. So assume that's
      // the default and only force a ContentNode if we need to indicate some
      // content is not selectable.
      !node_interaction_info->is_selectable ||
      node_interaction_info->is_editable ||
      node_interaction_info->can_resize_horizontal ||
      node_interaction_info->can_resize_vertical ||
      node_interaction_info->is_focusable ||
      node_interaction_info->is_draggable ||
      node_interaction_info->is_clickable ||
      node_interaction_info->document_scoped_z_order;

  if (!needs_interaction_info) {
    return;
  }

  attributes.node_interaction_info = std::move(node_interaction_info);
}

AIPageContentAgent::ContentBuilder::RecursionData::RecursionData(
    const ComputedStyle& document_style)
    : document_style(document_style) {}

}  // namespace blink
