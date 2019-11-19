// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"

#include <inttypes.h>

#include <memory>

#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

std::unique_ptr<TracedValue> InspectorParseHtmlBeginData(Document* document,
                                                         unsigned start_line) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("startLine", start_line);
  value->SetString("frame", IdentifiersFactory::FrameId(document->GetFrame()));
  value->SetString("url", document->Url().GetString());
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> InspectorParseHtmlEndData(unsigned end_line) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("endLine", end_line);
  return value;
}

std::unique_ptr<TracedValue> GetNavigationTracingData(Document* document) {
  auto data = std::make_unique<TracedValue>();

  data->SetString("navigationId",
                  IdentifiersFactory::LoaderId(document->Loader()));
  return data;
}
}  //  namespace

String ToHexString(const void* p) {
  return String::Format("0x%" PRIx64,
                        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)));
}

void SetCallStack(TracedValue* value) {
  static const unsigned char* trace_category_enabled = nullptr;
  WTF_ANNOTATE_BENIGN_RACE(&trace_category_enabled, "trace_event category");
  if (!trace_category_enabled)
    trace_category_enabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"));
  if (!*trace_category_enabled)
    return;
  // The CPU profiler stack trace does not include call site line numbers.
  // So we collect the top frame with SourceLocation::capture() to get the
  // binding call site info.
  SourceLocation::Capture()->ToTracedValue(value, "stackTrace");
  v8::CpuProfiler::CollectSample(v8::Isolate::GetCurrent());
}

void InspectorTraceEvents::WillSendRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FetchInitiatorInfo&,
    ResourceType) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD,
      "data",
      inspector_send_request_event::Data(loader, identifier, frame, request));
}

void InspectorTraceEvents::WillSendNavigationRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& url,
    const AtomicString& http_method,
    EncodedFormData*) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceSendRequest",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_send_navigation_request_event::Data(
                           loader, identifier, frame, url, http_method));
}

void InspectorTraceEvents::DidReceiveResourceResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    const Resource*) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceiveResponse",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_receive_response_event::Data(
                           loader, identifier, frame, response));
}

void InspectorTraceEvents::DidReceiveData(uint64_t identifier,
                                          DocumentLoader* loader,
                                          const char* data,
                                          uint64_t encoded_data_length) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceReceivedData",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_receive_data_event::Data(
                           loader, identifier, frame, encoded_data_length));
}

void InspectorTraceEvents::DidFinishLoading(uint64_t identifier,
                                            DocumentLoader* loader,
                                            base::TimeTicks finish_time,
                                            int64_t encoded_data_length,
                                            int64_t decoded_body_length,
                                            bool should_report_corb_blocking) {
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
      inspector_resource_finish_event::Data(loader, identifier, finish_time,
                                            false, encoded_data_length,
                                            decoded_body_length));
}

void InspectorTraceEvents::DidFailLoading(uint64_t identifier,
                                          DocumentLoader* loader,
                                          const ResourceError&) {
  TRACE_EVENT_INSTANT1("devtools.timeline", "ResourceFinish",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_resource_finish_event::Data(
                           loader, identifier, base::TimeTicks(), true, 0, 0));
}

void InspectorTraceEvents::MarkResourceAsCached(DocumentLoader* loader,
                                                uint64_t identifier) {
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "ResourceMarkAsCached", TRACE_EVENT_SCOPE_THREAD,
      "data", inspector_mark_resource_cached_event::Data(loader, identifier));
}

void InspectorTraceEvents::Will(const probe::ExecuteScript&) {}

void InspectorTraceEvents::Did(const probe::ExecuteScript&) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_update_counters_event::Data());
}

void InspectorTraceEvents::Will(const probe::ParseHTML& probe) {
  // FIXME: Pass in current input length.
  TRACE_EVENT_BEGIN1(
      "devtools.timeline", "ParseHTML", "beginData",
      InspectorParseHtmlBeginData(probe.parser->GetDocument(),
                                  probe.parser->LineNumber().ZeroBasedInt()));
}

void InspectorTraceEvents::Did(const probe::ParseHTML& probe) {
  TRACE_EVENT_END1(
      "devtools.timeline", "ParseHTML", "endData",
      InspectorParseHtmlEndData(probe.parser->LineNumber().ZeroBasedInt() - 1));
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_update_counters_event::Data());
}

void InspectorTraceEvents::Will(const probe::CallFunction& probe) {
}

void InspectorTraceEvents::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_update_counters_event::Data());
}

void InspectorTraceEvents::PaintTiming(Document* document,
                                       const char* name,
                                       double timestamp) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading,rail,devtools.timeline", name,
                                   trace_event::ToTraceTimestamp(timestamp),
                                   "frame", ToTraceValue(document->GetFrame()),
                                   "data", GetNavigationTracingData(document));
}

void InspectorTraceEvents::FrameStartedLoading(LocalFrame* frame) {
  TRACE_EVENT_INSTANT1("devtools.timeline", "FrameStartedLoading",
                       TRACE_EVENT_SCOPE_THREAD, "frame", ToTraceValue(frame));
}

namespace {

void SetNodeInfo(TracedValue* value,
                 Node* node,
                 const char* id_field_name,
                 const char* name_field_name = nullptr) {
  value->SetIntegerWithCopiedName(id_field_name,
                                  IdentifiersFactory::IntIdForNode(node));
  if (name_field_name)
    value->SetStringWithCopiedName(name_field_name, node->DebugName());
}

const char* PseudoTypeToString(CSSSelector::PseudoType pseudo_type) {
  switch (pseudo_type) {
#define DEFINE_STRING_MAPPING(pseudoType) \
  case CSSSelector::k##pseudoType:        \
    return #pseudoType;
    DEFINE_STRING_MAPPING(PseudoUnknown)
    DEFINE_STRING_MAPPING(PseudoEmpty)
    DEFINE_STRING_MAPPING(PseudoFirstChild)
    DEFINE_STRING_MAPPING(PseudoFirstOfType)
    DEFINE_STRING_MAPPING(PseudoLastChild)
    DEFINE_STRING_MAPPING(PseudoLastOfType)
    DEFINE_STRING_MAPPING(PseudoOnlyChild)
    DEFINE_STRING_MAPPING(PseudoOnlyOfType)
    DEFINE_STRING_MAPPING(PseudoFirstLine)
    DEFINE_STRING_MAPPING(PseudoFirstLetter)
    DEFINE_STRING_MAPPING(PseudoNthChild)
    DEFINE_STRING_MAPPING(PseudoNthOfType)
    DEFINE_STRING_MAPPING(PseudoNthLastChild)
    DEFINE_STRING_MAPPING(PseudoNthLastOfType)
    DEFINE_STRING_MAPPING(PseudoPart)
    DEFINE_STRING_MAPPING(PseudoState)
    DEFINE_STRING_MAPPING(PseudoLink)
    DEFINE_STRING_MAPPING(PseudoVisited)
    DEFINE_STRING_MAPPING(PseudoAny)
    DEFINE_STRING_MAPPING(PseudoIs)
    DEFINE_STRING_MAPPING(PseudoWhere)
    DEFINE_STRING_MAPPING(PseudoWebkitAnyLink)
    DEFINE_STRING_MAPPING(PseudoAnyLink)
    DEFINE_STRING_MAPPING(PseudoAutofill)
    DEFINE_STRING_MAPPING(PseudoAutofillPreviewed)
    DEFINE_STRING_MAPPING(PseudoAutofillSelected)
    DEFINE_STRING_MAPPING(PseudoHover)
    DEFINE_STRING_MAPPING(PseudoDrag)
    DEFINE_STRING_MAPPING(PseudoFocus)
    DEFINE_STRING_MAPPING(PseudoFocusVisible)
    DEFINE_STRING_MAPPING(PseudoFocusWithin)
    DEFINE_STRING_MAPPING(PseudoActive)
    DEFINE_STRING_MAPPING(PseudoChecked)
    DEFINE_STRING_MAPPING(PseudoEnabled)
    DEFINE_STRING_MAPPING(PseudoFullPageMedia)
    DEFINE_STRING_MAPPING(PseudoDefault)
    DEFINE_STRING_MAPPING(PseudoDisabled)
    DEFINE_STRING_MAPPING(PseudoOptional)
    DEFINE_STRING_MAPPING(PseudoPlaceholderShown)
    DEFINE_STRING_MAPPING(PseudoRequired)
    DEFINE_STRING_MAPPING(PseudoReadOnly)
    DEFINE_STRING_MAPPING(PseudoReadWrite)
    DEFINE_STRING_MAPPING(PseudoValid)
    DEFINE_STRING_MAPPING(PseudoInvalid)
    DEFINE_STRING_MAPPING(PseudoIndeterminate)
    DEFINE_STRING_MAPPING(PseudoTarget)
    DEFINE_STRING_MAPPING(PseudoBefore)
    DEFINE_STRING_MAPPING(PseudoAfter)
    DEFINE_STRING_MAPPING(PseudoMarker)
    DEFINE_STRING_MAPPING(PseudoBackdrop)
    DEFINE_STRING_MAPPING(PseudoLang)
    DEFINE_STRING_MAPPING(PseudoNot)
    DEFINE_STRING_MAPPING(PseudoPlaceholder)
    DEFINE_STRING_MAPPING(PseudoResizer)
    DEFINE_STRING_MAPPING(PseudoRoot)
    DEFINE_STRING_MAPPING(PseudoScope)
    DEFINE_STRING_MAPPING(PseudoScrollbar)
    DEFINE_STRING_MAPPING(PseudoScrollbarButton)
    DEFINE_STRING_MAPPING(PseudoScrollbarCorner)
    DEFINE_STRING_MAPPING(PseudoScrollbarThumb)
    DEFINE_STRING_MAPPING(PseudoScrollbarTrack)
    DEFINE_STRING_MAPPING(PseudoScrollbarTrackPiece)
    DEFINE_STRING_MAPPING(PseudoWindowInactive)
    DEFINE_STRING_MAPPING(PseudoCornerPresent)
    DEFINE_STRING_MAPPING(PseudoDecrement)
    DEFINE_STRING_MAPPING(PseudoIncrement)
    DEFINE_STRING_MAPPING(PseudoHorizontal)
    DEFINE_STRING_MAPPING(PseudoVertical)
    DEFINE_STRING_MAPPING(PseudoStart)
    DEFINE_STRING_MAPPING(PseudoEnd)
    DEFINE_STRING_MAPPING(PseudoDoubleButton)
    DEFINE_STRING_MAPPING(PseudoSingleButton)
    DEFINE_STRING_MAPPING(PseudoNoButton)
    DEFINE_STRING_MAPPING(PseudoSelection)
    DEFINE_STRING_MAPPING(PseudoLeftPage)
    DEFINE_STRING_MAPPING(PseudoRightPage)
    DEFINE_STRING_MAPPING(PseudoFirstPage)
    DEFINE_STRING_MAPPING(PseudoFullScreen)
    DEFINE_STRING_MAPPING(PseudoFullScreenAncestor)
    DEFINE_STRING_MAPPING(PseudoFullscreen)
    DEFINE_STRING_MAPPING(PseudoPictureInPicture)
    DEFINE_STRING_MAPPING(PseudoInRange)
    DEFINE_STRING_MAPPING(PseudoOutOfRange)
    DEFINE_STRING_MAPPING(PseudoWebKitCustomElement)
    DEFINE_STRING_MAPPING(PseudoBlinkInternalElement)
    DEFINE_STRING_MAPPING(PseudoCue)
    DEFINE_STRING_MAPPING(PseudoFutureCue)
    DEFINE_STRING_MAPPING(PseudoPastCue)
    DEFINE_STRING_MAPPING(PseudoUnresolved)
    DEFINE_STRING_MAPPING(PseudoDefined)
    DEFINE_STRING_MAPPING(PseudoContent)
    DEFINE_STRING_MAPPING(PseudoHost)
    DEFINE_STRING_MAPPING(PseudoHostContext)
    DEFINE_STRING_MAPPING(PseudoShadow)
    DEFINE_STRING_MAPPING(PseudoSlotted)
    DEFINE_STRING_MAPPING(PseudoSpatialNavigationFocus)
    DEFINE_STRING_MAPPING(PseudoSpatialNavigationInterest)
    DEFINE_STRING_MAPPING(PseudoIsHtml)
    DEFINE_STRING_MAPPING(PseudoListBox)
    DEFINE_STRING_MAPPING(PseudoMultiSelectFocus)
    DEFINE_STRING_MAPPING(PseudoHostHasAppearance)
    DEFINE_STRING_MAPPING(PseudoVideoPersistent)
    DEFINE_STRING_MAPPING(PseudoVideoPersistentAncestor)
    DEFINE_STRING_MAPPING(PseudoXrImmersiveDomOverlay)
#undef DEFINE_STRING_MAPPING
  }

  NOTREACHED();
  return "";
}

String UrlForFrame(LocalFrame* frame) {
  KURL url = frame->GetDocument()->Url();
  url.RemoveFragmentIdentifier();
  return url.GetString();
}

const char* CompileOptionsString(v8::ScriptCompiler::CompileOptions options) {
  switch (options) {
    case v8::ScriptCompiler::kNoCompileOptions:
      return "code";
    case v8::ScriptCompiler::kConsumeCodeCache:
      return "code";
    case v8::ScriptCompiler::kEagerCompile:
      return "full code";
  }
  NOTREACHED();
  return "";
}

const char* NotStreamedReasonString(ScriptStreamer::NotStreamingReason reason) {
  switch (reason) {
    case ScriptStreamer::kNotHTTP:
      return "not http/https protocol";
    case ScriptStreamer::kRevalidate:
      return "revalidation event";
    case ScriptStreamer::kContextNotValid:
      return "script context not valid";
    case ScriptStreamer::kEncodingNotSupported:
      return "encoding not supported";
    case ScriptStreamer::kThreadBusy:
      return "script streamer thread busy";
    case ScriptStreamer::kV8CannotStream:
      return "V8 cannot stream script";
    case ScriptStreamer::kScriptTooSmall:
      return "script too small";
    case ScriptStreamer::kNoResourceBuffer:
      return "resource no longer alive";
    case ScriptStreamer::kHasCodeCache:
      return "script has code-cache available";
    case ScriptStreamer::kStreamerNotReadyOnGetSource:
      return "streamer not ready";
    case ScriptStreamer::kInlineScript:
      return "inline script";
    case ScriptStreamer::kDidntTryToStartStreaming:
      return "start streaming not called";
    case ScriptStreamer::kErrorOccurred:
      return "an error occurred";
    case ScriptStreamer::kStreamingDisabled:
      return "already disabled streaming";
    case ScriptStreamer::kSecondScriptResourceUse:
      return "already used streamed data";
    case ScriptStreamer::kWorkerTopLevelScript:
      return "worker top-level scripts are not streamable";
    case ScriptStreamer::kModuleScript:
      return "module script";
    case ScriptStreamer::kAlreadyLoaded:
    case ScriptStreamer::kCount:
    case ScriptStreamer::kInvalid:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

}  // namespace

namespace inspector_schedule_style_invalidation_tracking_event {
std::unique_ptr<TracedValue> FillCommonPart(
    ContainerNode& node,
    const InvalidationSet& invalidation_set,
    const char* invalidated_selector) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(node.GetDocument().GetFrame()));
  SetNodeInfo(value.get(), &node, "nodeId", "nodeName");
  value->SetString("invalidationSet",
                   DescendantInvalidationSetToIdString(invalidation_set));
  value->SetString("invalidatedSelectorId", invalidated_selector);
  SourceLocation::Capture()->ToTracedValue(value.get(), "stackTrace");
  return value;
}
}  // namespace inspector_schedule_style_invalidation_tracking_event

const char inspector_schedule_style_invalidation_tracking_event::kAttribute[] =
    "attribute";
const char inspector_schedule_style_invalidation_tracking_event::kClass[] =
    "class";
const char inspector_schedule_style_invalidation_tracking_event::kId[] = "id";
const char inspector_schedule_style_invalidation_tracking_event::kPseudo[] =
    "pseudo";
const char inspector_schedule_style_invalidation_tracking_event::kRuleSet[] =
    "ruleset";

const char* ResourcePriorityString(ResourceLoadPriority priority) {
  const char* priority_string = nullptr;
  switch (priority) {
    case ResourceLoadPriority::kVeryLow:
      priority_string = "VeryLow";
      break;
    case ResourceLoadPriority::kLow:
      priority_string = "Low";
      break;
    case ResourceLoadPriority::kMedium:
      priority_string = "Medium";
      break;
    case ResourceLoadPriority::kHigh:
      priority_string = "High";
      break;
    case ResourceLoadPriority::kVeryHigh:
      priority_string = "VeryHigh";
      break;
    case ResourceLoadPriority::kUnresolved:
      break;
  }
  return priority_string;
}

std::unique_ptr<TracedValue>
inspector_schedule_style_invalidation_tracking_event::IdChange(
    Element& element,
    const InvalidationSet& invalidation_set,
    const AtomicString& id) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(element, invalidation_set, kId);
  value->SetString("changedId", id);
  return value;
}

std::unique_ptr<TracedValue>
inspector_schedule_style_invalidation_tracking_event::ClassChange(
    Element& element,
    const InvalidationSet& invalidation_set,
    const AtomicString& class_name) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(element, invalidation_set, kClass);
  value->SetString("changedClass", class_name);
  return value;
}

std::unique_ptr<TracedValue>
inspector_schedule_style_invalidation_tracking_event::AttributeChange(
    Element& element,
    const InvalidationSet& invalidation_set,
    const QualifiedName& attribute_name) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(element, invalidation_set, kAttribute);
  value->SetString("changedAttribute", attribute_name.ToString());
  return value;
}

std::unique_ptr<TracedValue>
inspector_schedule_style_invalidation_tracking_event::PseudoChange(
    Element& element,
    const InvalidationSet& invalidation_set,
    CSSSelector::PseudoType pseudo_type) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(element, invalidation_set, kAttribute);
  value->SetString("changedPseudo", PseudoTypeToString(pseudo_type));
  return value;
}

std::unique_ptr<TracedValue>
inspector_schedule_style_invalidation_tracking_event::RuleSetInvalidation(
    ContainerNode& root_node,
    const InvalidationSet& invalidation_set) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(root_node, invalidation_set, kRuleSet);
  return value;
}

String DescendantInvalidationSetToIdString(const InvalidationSet& set) {
  return ToHexString(&set);
}

const char inspector_style_invalidator_invalidate_event::
    kElementHasPendingInvalidationList[] =
        "Element has pending invalidation list";
const char
    inspector_style_invalidator_invalidate_event::kInvalidateCustomPseudo[] =
        "Invalidate custom pseudo element";
const char inspector_style_invalidator_invalidate_event::
    kInvalidationSetMatchedAttribute[] = "Invalidation set matched attribute";
const char inspector_style_invalidator_invalidate_event::
    kInvalidationSetMatchedClass[] = "Invalidation set matched class";
const char
    inspector_style_invalidator_invalidate_event::kInvalidationSetMatchedId[] =
        "Invalidation set matched id";
const char inspector_style_invalidator_invalidate_event::
    kInvalidationSetMatchedTagName[] = "Invalidation set matched tagName";
const char inspector_style_invalidator_invalidate_event::
    kInvalidationSetMatchedPart[] = "Invalidation set matched part";

namespace inspector_style_invalidator_invalidate_event {
std::unique_ptr<TracedValue> FillCommonPart(ContainerNode& node,
                                            const char* reason) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(node.GetDocument().GetFrame()));
  SetNodeInfo(value.get(), &node, "nodeId", "nodeName");
  value->SetString("reason", reason);
  return value;
}
}  // namespace inspector_style_invalidator_invalidate_event

std::unique_ptr<TracedValue> inspector_style_invalidator_invalidate_event::Data(
    Element& element,
    const char* reason) {
  return FillCommonPart(element, reason);
}

std::unique_ptr<TracedValue>
inspector_style_invalidator_invalidate_event::SelectorPart(
    Element& element,
    const char* reason,
    const InvalidationSet& invalidation_set,
    const String& selector_part) {
  std::unique_ptr<TracedValue> value = FillCommonPart(element, reason);
  value->BeginArray("invalidationList");
  invalidation_set.ToTracedValue(value.get());
  value->EndArray();
  value->SetString("selectorPart", selector_part);
  return value;
}

std::unique_ptr<TracedValue>
inspector_style_invalidator_invalidate_event::InvalidationList(
    ContainerNode& node,
    const Vector<scoped_refptr<InvalidationSet>>& invalidation_list) {
  std::unique_ptr<TracedValue> value =
      FillCommonPart(node, kElementHasPendingInvalidationList);
  value->BeginArray("invalidationList");
  for (const auto& invalidation_set : invalidation_list)
    invalidation_set->ToTracedValue(value.get());
  value->EndArray();
  return value;
}

std::unique_ptr<TracedValue>
inspector_style_recalc_invalidation_tracking_event::Data(
    Node* node,
    const StyleChangeReasonForTracing& reason) {
  DCHECK(node);

  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(node->GetDocument().GetFrame()));
  SetNodeInfo(value.get(), node, "nodeId", "nodeName");
  value->SetString("reason", reason.ReasonString());
  value->SetString("extraData", reason.GetExtraData());
  SourceLocation::Capture()->ToTracedValue(value.get(), "stackTrace");
  return value;
}

std::unique_ptr<TracedValue> inspector_layout_event::BeginData(
    LocalFrameView* frame_view) {
  bool is_partial;
  unsigned needs_layout_objects;
  unsigned total_objects;
  LocalFrame& frame = frame_view->GetFrame();
  frame.View()->CountObjectsNeedingLayout(needs_layout_objects, total_objects,
                                          is_partial);

  auto value = std::make_unique<TracedValue>();
  value->SetInteger("dirtyObjects", needs_layout_objects);
  value->SetInteger("totalObjects", total_objects);
  value->SetBoolean("partialLayout", is_partial);
  value->SetString("frame", IdentifiersFactory::FrameId(&frame));
  SetCallStack(value.get());
  return value;
}

static void CreateQuad(TracedValue* value,
                       const char* name,
                       const FloatQuad& quad) {
  value->BeginArray(name);
  value->PushDouble(quad.P1().X());
  value->PushDouble(quad.P1().Y());
  value->PushDouble(quad.P2().X());
  value->PushDouble(quad.P2().Y());
  value->PushDouble(quad.P3().X());
  value->PushDouble(quad.P3().Y());
  value->PushDouble(quad.P4().X());
  value->PushDouble(quad.P4().Y());
  value->EndArray();
}

static void SetGeneratingNodeInfo(TracedValue* value,
                                  const LayoutObject* layout_object,
                                  const char* id_field_name,
                                  const char* name_field_name = nullptr) {
  Node* node = nullptr;
  for (; layout_object && !node; layout_object = layout_object->Parent())
    node = layout_object->GeneratingNode();
  if (!node)
    return;

  SetNodeInfo(value, node, id_field_name, name_field_name);
}

std::unique_ptr<TracedValue> inspector_layout_event::EndData(
    LayoutObject* root_for_this_layout) {
  Vector<FloatQuad> quads;
  root_for_this_layout->AbsoluteQuads(quads);

  auto value = std::make_unique<TracedValue>();
  if (quads.size() >= 1) {
    CreateQuad(value.get(), "root", quads[0]);
    SetGeneratingNodeInfo(value.get(), root_for_this_layout, "rootNode");
  } else {
    NOTREACHED();
  }
  return value;
}

namespace layout_invalidation_reason {
const char kUnknown[] = "Unknown";
const char kSizeChanged[] = "Size changed";
const char kAncestorMoved[] = "Ancestor moved";
const char kStyleChange[] = "Style changed";
const char kDomChanged[] = "DOM changed";
const char kTextChanged[] = "Text changed";
const char kPrintingChanged[] = "Printing changed";
const char kAttributeChanged[] = "Attribute changed";
const char kColumnsChanged[] = "Attribute changed";
const char kChildAnonymousBlockChanged[] = "Child anonymous block changed";
const char kAnonymousBlockChange[] = "Anonymous block change";
const char kFullscreen[] = "Fullscreen change";
const char kChildChanged[] = "Child changed";
const char kListValueChange[] = "List value change";
const char kListStyleTypeChange[] = "List style type change";
const char kImageChanged[] = "Image changed";
const char kLineBoxesChanged[] = "Line boxes changed";
const char kSliderValueChanged[] = "Slider value changed";
const char kAncestorMarginCollapsing[] = "Ancestor margin collapsing";
const char kFieldsetChanged[] = "Fieldset changed";
const char kTextAutosizing[] = "Text autosizing (font boosting)";
const char kSvgResourceInvalidated[] = "SVG resource invalidated";
const char kFloatDescendantChanged[] = "Floating descendant changed";
const char kCountersChanged[] = "Counters changed";
const char kGridChanged[] = "Grid changed";
const char kMenuOptionsChanged[] = "Menu options changed";
const char kRemovedFromLayout[] = "Removed from layout";
const char kAddedToLayout[] = "Added to layout";
const char kTableChanged[] = "Table changed";
const char kPaddingChanged[] = "Padding changed";
const char kTextControlChanged[] = "Text control changed";
const char kSvgChanged[] = "SVG changed";
const char kScrollbarChanged[] = "Scrollbar changed";
const char kDisplayLock[] = "Display lock";
}  // namespace layout_invalidation_reason

std::unique_ptr<TracedValue> inspector_layout_invalidation_tracking_event::Data(
    const LayoutObject* layout_object,
    LayoutInvalidationReasonForTracing reason) {
  DCHECK(layout_object);
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(layout_object->GetFrame()));
  SetGeneratingNodeInfo(value.get(), layout_object, "nodeId", "nodeName");
  value->SetString("reason", reason);
  SourceLocation::Capture()->ToTracedValue(value.get(), "stackTrace");
  return value;
}

std::unique_ptr<TracedValue> inspector_change_resource_priority_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    const ResourceLoadPriority& load_priority) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", request_id);
  value->SetString("priority", ResourcePriorityString(load_priority));
  return value;
}

std::unique_ptr<TracedValue> inspector_send_request_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    const ResourceRequest& request) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId",
                   IdentifiersFactory::RequestId(loader, identifier));
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  value->SetString("url", request.Url().GetString());
  value->SetString("requestMethod", request.HttpMethod());
  const char* priority = ResourcePriorityString(request.Priority());
  if (priority)
    value->SetString("priority", priority);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_send_navigation_request_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    const KURL& url,
    const AtomicString& http_method) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", IdentifiersFactory::LoaderId(loader));
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  value->SetString("url", url.GetString());
  value->SetString("requestMethod", http_method);
  const char* priority =
      ResourcePriorityString(ResourceLoadPriority::kVeryHigh);
  if (priority)
    value->SetString("priority", priority);
  SetCallStack(value.get());
  return value;
}

namespace {
void RecordTiming(const ResourceLoadTiming& timing, TracedValue* value) {
  value->SetDouble("requestTime",
                   timing.RequestTime().since_origin().InSecondsF());
  value->SetDouble("proxyStart",
                   timing.CalculateMillisecondDelta(timing.ProxyStart()));
  value->SetDouble("proxyEnd",
                   timing.CalculateMillisecondDelta(timing.ProxyEnd()));
  value->SetDouble("dnsStart",
                   timing.CalculateMillisecondDelta(timing.DnsStart()));
  value->SetDouble("dnsEnd", timing.CalculateMillisecondDelta(timing.DnsEnd()));
  value->SetDouble("connectStart",
                   timing.CalculateMillisecondDelta(timing.ConnectStart()));
  value->SetDouble("connectEnd",
                   timing.CalculateMillisecondDelta(timing.ConnectEnd()));
  value->SetDouble("sslStart",
                   timing.CalculateMillisecondDelta(timing.SslStart()));
  value->SetDouble("sslEnd", timing.CalculateMillisecondDelta(timing.SslEnd()));
  value->SetDouble("workerStart",
                   timing.CalculateMillisecondDelta(timing.WorkerStart()));
  value->SetDouble("workerReady",
                   timing.CalculateMillisecondDelta(timing.WorkerReady()));
  value->SetDouble("sendStart",
                   timing.CalculateMillisecondDelta(timing.SendStart()));
  value->SetDouble("sendEnd",
                   timing.CalculateMillisecondDelta(timing.SendEnd()));
  value->SetDouble("receiveHeadersEnd", timing.CalculateMillisecondDelta(
                                            timing.ReceiveHeadersEnd()));
  value->SetDouble("pushStart", timing.PushStart().since_origin().InSecondsF());
  value->SetDouble("pushEnd", timing.PushEnd().since_origin().InSecondsF());
}
}  // namespace

std::unique_ptr<TracedValue> inspector_receive_response_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    const ResourceResponse& response) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", request_id);
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  value->SetInteger("statusCode", response.HttpStatusCode());
  value->SetString("mimeType", response.MimeType().GetString().IsolatedCopy());
  value->SetDouble("encodedDataLength", response.EncodedDataLength());
  value->SetBoolean("fromCache", response.WasCached());
  value->SetBoolean("fromServiceWorker", response.WasFetchedViaServiceWorker());
  if (response.GetResourceLoadTiming()) {
    value->BeginDictionary("timing");
    RecordTiming(*response.GetResourceLoadTiming(), value.get());
    value->EndDictionary();
  }
  if (response.WasFetchedViaServiceWorker())
    value->SetBoolean("fromServiceWorker", true);
  return value;
}

std::unique_ptr<TracedValue> inspector_receive_data_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    uint64_t encoded_data_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", request_id);
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  value->SetDouble("encodedDataLength", encoded_data_length);
  return value;
}

std::unique_ptr<TracedValue> inspector_resource_finish_event::Data(
    DocumentLoader* loader,
    uint64_t identifier,
    base::TimeTicks finish_time,
    bool did_fail,
    int64_t encoded_data_length,
    int64_t decoded_body_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", request_id);
  value->SetBoolean("didFail", did_fail);
  value->SetDouble("encodedDataLength", encoded_data_length);
  value->SetDouble("decodedBodyLength", decoded_body_length);
  if (!finish_time.is_null())
    value->SetDouble("finishTime", finish_time.since_origin().InSecondsF());
  return value;
}

std::unique_ptr<TracedValue> inspector_mark_resource_cached_event::Data(
    DocumentLoader* loader,
    uint64_t identifier) {
  auto value = std::make_unique<TracedValue>();
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  value->SetString("requestId", request_id);
  return value;
}

static LocalFrame* FrameForExecutionContext(ExecutionContext* context) {
  if (auto* document = DynamicTo<Document>(context))
    return document->GetFrame();
  return nullptr;
}

static std::unique_ptr<TracedValue> GenericTimerData(ExecutionContext* context,
                                                     int timer_id) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("timerId", timer_id);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));
  return value;
}

std::unique_ptr<TracedValue> inspector_timer_install_event::Data(
    ExecutionContext* context,
    int timer_id,
    base::TimeDelta timeout,
    bool single_shot) {
  std::unique_ptr<TracedValue> value = GenericTimerData(context, timer_id);
  value->SetDouble("timeout", timeout.InMillisecondsF());
  value->SetBoolean("singleShot", single_shot);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_timer_remove_event::Data(
    ExecutionContext* context,
    int timer_id) {
  std::unique_ptr<TracedValue> value = GenericTimerData(context, timer_id);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_timer_fire_event::Data(
    ExecutionContext* context,
    int timer_id) {
  return GenericTimerData(context, timer_id);
}

std::unique_ptr<TracedValue> inspector_animation_frame_event::Data(
    ExecutionContext* context,
    int callback_id) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("id", callback_id);
  if (auto* document = DynamicTo<Document>(context)) {
    value->SetString("frame",
                     IdentifiersFactory::FrameId(document->GetFrame()));
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
    value->SetString("worker", ToHexString(scope));
  }
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> GenericIdleCallbackEvent(ExecutionContext* context,
                                                      int id) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("id", id);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_idle_callback_request_event::Data(
    ExecutionContext* context,
    int id,
    double timeout) {
  std::unique_ptr<TracedValue> value = GenericIdleCallbackEvent(context, id);
  value->SetInteger("timeout", timeout);
  return value;
}

std::unique_ptr<TracedValue> inspector_idle_callback_cancel_event::Data(
    ExecutionContext* context,
    int id) {
  return GenericIdleCallbackEvent(context, id);
}

std::unique_ptr<TracedValue> inspector_idle_callback_fire_event::Data(
    ExecutionContext* context,
    int id,
    double allotted_milliseconds,
    bool timed_out) {
  std::unique_ptr<TracedValue> value = GenericIdleCallbackEvent(context, id);
  value->SetDouble("allottedMilliseconds", allotted_milliseconds);
  value->SetBoolean("timedOut", timed_out);
  return value;
}

std::unique_ptr<TracedValue> inspector_parse_author_style_sheet_event::Data(
    const CSSStyleSheetResource* cached_style_sheet) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("styleSheetUrl", cached_style_sheet->Url().GetString());
  return value;
}

std::unique_ptr<TracedValue> inspector_xhr_ready_state_change_event::Data(
    ExecutionContext* context,
    XMLHttpRequest* request) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", request->Url().GetString());
  value->SetInteger("readyState", request->readyState());
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_xhr_load_event::Data(
    ExecutionContext* context,
    XMLHttpRequest* request) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", request->Url().GetString());
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

static FloatPoint LocalCoordToFloatPoint(LocalFrameView* view,
                                         const FloatPoint& local) {
  return FloatPoint(view->ConvertToRootFrame(RoundedIntPoint(local)));
}

static void LocalToPageQuad(const LayoutObject& layout_object,
                            const PhysicalRect& rect,
                            FloatQuad* quad) {
  LocalFrame* frame = layout_object.GetFrame();
  LocalFrameView* view = frame->View();
  FloatQuad absolute = layout_object.LocalRectToAbsoluteQuad(rect);
  quad->SetP1(LocalCoordToFloatPoint(view, absolute.P1()));
  quad->SetP2(LocalCoordToFloatPoint(view, absolute.P2()));
  quad->SetP3(LocalCoordToFloatPoint(view, absolute.P3()));
  quad->SetP4(LocalCoordToFloatPoint(view, absolute.P4()));
}

std::unique_ptr<TracedValue> inspector_paint_event::Data(
    LayoutObject* layout_object,
    const PhysicalRect& clip_rect,
    const GraphicsLayer* graphics_layer) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(layout_object->GetFrame()));
  FloatQuad quad;
  LocalToPageQuad(*layout_object, clip_rect, &quad);
  CreateQuad(value.get(), "clip", quad);
  SetGeneratingNodeInfo(value.get(), layout_object, "nodeId");
  int graphics_layer_id = graphics_layer ? graphics_layer->CcLayer()->id() : 0;
  value->SetInteger("layerId", graphics_layer_id);
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> FrameEventData(LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  bool is_main_frame = frame && frame->IsMainFrame();
  value->SetBoolean("isMainFrame", is_main_frame);
  // TODO(dgozman): this does not work with OOPIF, so everyone who
  // uses it should migrate to frame instead.
  value->SetString("page",
                   IdentifiersFactory::FrameId(&frame->LocalFrameRoot()));
  return value;
}

void FillCommonFrameData(TracedValue* frame_data, LocalFrame* frame) {
  frame_data->SetString("frame", IdentifiersFactory::FrameId(frame));
  frame_data->SetString("url", UrlForFrame(frame));
  frame_data->SetString("name", frame->Tree().GetName());

  FrameOwner* owner = frame->Owner();
  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
    frame_data->SetInteger(
        "nodeId", IdentifiersFactory::IntIdForNode(frame_owner_element));
  }
  Frame* parent = frame->Tree().Parent();
  if (IsA<LocalFrame>(parent))
    frame_data->SetString("parent", IdentifiersFactory::FrameId(parent));
}

std::unique_ptr<TracedValue> inspector_commit_load_event::Data(
    LocalFrame* frame) {
  std::unique_ptr<TracedValue> frame_data = FrameEventData(frame);
  FillCommonFrameData(frame_data.get(), frame);
  return frame_data;
}

std::unique_ptr<TracedValue> inspector_mark_load_event::Data(
    LocalFrame* frame) {
  std::unique_ptr<TracedValue> frame_data = FrameEventData(frame);
  frame_data->SetString("frame", IdentifiersFactory::FrameId(frame));
  return frame_data;
}

std::unique_ptr<TracedValue> inspector_scroll_layer_event::Data(
    LayoutObject* layout_object) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::FrameId(layout_object->GetFrame()));
  SetGeneratingNodeInfo(value.get(), layout_object, "nodeId");
  return value;
}

std::unique_ptr<TracedValue> inspector_update_layer_tree_event::Data(
    LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  return value;
}

namespace {
std::unique_ptr<TracedValue> FillLocation(const String& url,
                                          const TextPosition& text_position) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", url);
  value->SetInteger("lineNumber", text_position.line_.OneBasedInt());
  value->SetInteger("columnNumber", text_position.column_.OneBasedInt());
  return value;
}
}  // namespace

std::unique_ptr<TracedValue> inspector_evaluate_script_event::Data(
    LocalFrame* frame,
    const String& url,
    const TextPosition& text_position) {
  std::unique_ptr<TracedValue> value = FillLocation(url, text_position);
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_parse_script_event::Data(
    uint64_t identifier,
    const String& url) {
  String request_id = IdentifiersFactory::RequestId(nullptr, identifier);
  auto value = std::make_unique<TracedValue>();
  value->SetString("requestId", request_id);
  value->SetString("url", url);
  return value;
}

inspector_compile_script_event::V8CacheResult::ProduceResult::ProduceResult(
    int cache_size)
    : cache_size(cache_size) {}

inspector_compile_script_event::V8CacheResult::ConsumeResult::ConsumeResult(
    v8::ScriptCompiler::CompileOptions consume_options,
    int cache_size,
    bool rejected)
    : consume_options(consume_options),
      cache_size(cache_size),
      rejected(rejected) {
  DCHECK_EQ(consume_options, v8::ScriptCompiler::kConsumeCodeCache);
}

inspector_compile_script_event::V8CacheResult::V8CacheResult(
    base::Optional<ProduceResult> produce_result,
    base::Optional<ConsumeResult> consume_result)
    : produce_result(std::move(produce_result)),
      consume_result(std::move(consume_result)) {}

std::unique_ptr<TracedValue> inspector_compile_script_event::Data(
    const String& url,
    const TextPosition& text_position,
    const V8CacheResult& cache_result,
    bool streamed,
    ScriptStreamer::NotStreamingReason not_streaming_reason) {
  std::unique_ptr<TracedValue> value = FillLocation(url, text_position);

  if (cache_result.produce_result) {
    value->SetInteger("producedCacheSize",
                      cache_result.produce_result->cache_size);
  }

  if (cache_result.consume_result) {
    value->SetString(
        "cacheConsumeOptions",
        CompileOptionsString(cache_result.consume_result->consume_options));
    value->SetInteger("consumedCacheSize",
                      cache_result.consume_result->cache_size);
    value->SetBoolean("cacheRejected", cache_result.consume_result->rejected);
  }
  value->SetBoolean("streamed", streamed);
  if (!streamed) {
    value->SetString("notStreamedReason",
                     NotStreamedReasonString(not_streaming_reason));
  }
  return value;
}

std::unique_ptr<TracedValue> inspector_function_call_event::Data(
    ExecutionContext* context,
    const v8::Local<v8::Function>& function) {
  auto value = std::make_unique<TracedValue>();
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));

  if (function.IsEmpty())
    return value;

  v8::Local<v8::Function> original_function = GetBoundFunction(function);
  v8::Local<v8::Value> function_name = original_function->GetDebugName();
  if (!function_name.IsEmpty() && function_name->IsString()) {
    value->SetString("functionName",
                     ToCoreString(function_name.As<v8::String>()));
  }
  std::unique_ptr<SourceLocation> location =
      SourceLocation::FromFunction(original_function);
  value->SetString("scriptId", String::Number(location->ScriptId()));
  value->SetString("url", location->Url());
  value->SetInteger("lineNumber", location->LineNumber());
  value->SetInteger("columnNumber", location->ColumnNumber());
  return value;
}

std::unique_ptr<TracedValue> inspector_paint_image_event::Data(
    const LayoutImage& layout_image,
    const FloatRect& src_rect,
    const FloatRect& dest_rect) {
  auto value = std::make_unique<TracedValue>();
  SetGeneratingNodeInfo(value.get(), &layout_image, "nodeId");
  if (const ImageResourceContent* resource = layout_image.CachedImage())
    value->SetString("url", resource->Url().GetString());

  value->SetInteger("x", dest_rect.X());
  value->SetInteger("y", dest_rect.Y());
  value->SetInteger("width", dest_rect.Width());
  value->SetInteger("height", dest_rect.Height());
  value->SetInteger("srcWidth", src_rect.Width());
  value->SetInteger("srcHeight", src_rect.Height());

  return value;
}

std::unique_ptr<TracedValue> inspector_paint_image_event::Data(
    const LayoutObject& owning_layout_object,
    const StyleImage& style_image) {
  auto value = std::make_unique<TracedValue>();
  SetGeneratingNodeInfo(value.get(), &owning_layout_object, "nodeId");
  if (const ImageResourceContent* resource = style_image.CachedImage())
    value->SetString("url", resource->Url().GetString());
  return value;
}

std::unique_ptr<TracedValue> inspector_paint_image_event::Data(
    Node* node,
    const StyleImage& style_image,
    const FloatRect& src_rect,
    const FloatRect& dest_rect) {
  auto value = std::make_unique<TracedValue>();
  if (node)
    SetNodeInfo(value.get(), node, "nodeId", nullptr);
  if (const ImageResourceContent* resource = style_image.CachedImage())
    value->SetString("url", resource->Url().GetString());

  value->SetInteger("x", dest_rect.X());
  value->SetInteger("y", dest_rect.Y());
  value->SetInteger("width", dest_rect.Width());
  value->SetInteger("height", dest_rect.Height());
  value->SetInteger("srcWidth", src_rect.Width());
  value->SetInteger("srcHeight", src_rect.Height());

  return value;
}

std::unique_ptr<TracedValue> inspector_paint_image_event::Data(
    const LayoutObject* owning_layout_object,
    const ImageResourceContent& image_resource) {
  auto value = std::make_unique<TracedValue>();
  SetGeneratingNodeInfo(value.get(), owning_layout_object, "nodeId");
  value->SetString("url", image_resource.Url().GetString());
  return value;
}

static size_t UsedHeapSize() {
  v8::HeapStatistics heap_statistics;
  v8::Isolate::GetCurrent()->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

std::unique_ptr<TracedValue> inspector_update_counters_event::Data() {
  auto value = std::make_unique<TracedValue>();
  if (IsMainThread()) {
    value->SetInteger("documents", InstanceCounters::CounterValue(
                                       InstanceCounters::kDocumentCounter));
    value->SetInteger("nodes", InstanceCounters::CounterValue(
                                   InstanceCounters::kNodeCounter));
    value->SetInteger("jsEventListeners",
                      InstanceCounters::CounterValue(
                          InstanceCounters::kJSEventListenerCounter));
  }
  value->SetDouble("jsHeapSizeUsed", static_cast<double>(UsedHeapSize()));
  return value;
}

std::unique_ptr<TracedValue> inspector_invalidate_layout_event::Data(
    LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_recalculate_styles_event::Data(
    LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_event_dispatch_event::Data(
    const Event& event) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", event.type());
  SetCallStack(value.get());
  return value;
}

std::unique_ptr<TracedValue> inspector_time_stamp_event::Data(
    ExecutionContext* context,
    const String& message) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("message", message);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    value->SetString("frame", IdentifiersFactory::FrameId(frame));
  return value;
}

std::unique_ptr<TracedValue>
inspector_tracing_session_id_for_worker_event::Data(
    const base::UnguessableToken& worker_devtools_token,
    const base::UnguessableToken& parent_devtools_token,
    const KURL& url,
    PlatformThreadId worker_thread_id) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame",
                   IdentifiersFactory::IdFromToken(parent_devtools_token));
  value->SetString("url", url.GetString());
  value->SetString("workerId",
                   IdentifiersFactory::IdFromToken(worker_devtools_token));
  value->SetDouble("workerThreadId", worker_thread_id);
  return value;
}

std::unique_ptr<TracedValue> inspector_tracing_started_in_frame::Data(
    const String& session_id,
    LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("sessionId", session_id);
  value->SetString("page",
                   IdentifiersFactory::FrameId(&frame->LocalFrameRoot()));
  value->SetBoolean("persistentIds", true);
  value->BeginArray("frames");
  for (Frame* f = frame; f; f = f->Tree().TraverseNext(frame)) {
    auto* local_frame = DynamicTo<LocalFrame>(f);
    if (!local_frame)
      continue;
    value->BeginDictionary();
    FillCommonFrameData(value.get(), local_frame);
    value->EndDictionary();
  }
  value->EndArray();
  return value;
}

std::unique_ptr<TracedValue> inspector_set_layer_tree_id::Data(
    LocalFrame* frame) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("frame", IdentifiersFactory::FrameId(frame));
  value->SetInteger("layerTreeId",
                    frame->GetPage()->GetChromeClient().GetLayerTreeId(*frame));
  return value;
}

std::unique_ptr<TracedValue> inspector_animation_event::Data(
    const Animation& animation) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("id", String::Number(animation.SequenceNumber()));
  value->SetString("state", animation.playState());
  if (const AnimationEffect* effect = animation.effect()) {
    value->SetString("name", animation.id());
    if (effect->IsKeyframeEffect()) {
      if (Element* target = ToKeyframeEffect(effect)->target())
        SetNodeInfo(value.get(), target, "nodeId", "nodeName");
    }
  }
  return value;
}

std::unique_ptr<TracedValue> inspector_animation_state_event::Data(
    const Animation& animation) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("state", animation.playState());
  return value;
}

std::unique_ptr<TracedValue> inspector_hit_test_event::EndData(
    const HitTestRequest& request,
    const HitTestLocation& location,
    const HitTestResult& result) {
  auto value(std::make_unique<TracedValue>());
  value->SetInteger("x", location.RoundedPoint().X());
  value->SetInteger("y", location.RoundedPoint().Y());
  if (location.IsRectBasedTest())
    value->SetBoolean("rect", true);
  if (location.IsRectilinear())
    value->SetBoolean("rectilinear", true);
  if (request.TouchEvent())
    value->SetBoolean("touch", true);
  if (request.Move())
    value->SetBoolean("move", true);
  if (request.ListBased())
    value->SetBoolean("listBased", true);
  else if (Node* node = result.InnerNode())
    SetNodeInfo(value.get(), node, "nodeId", "nodeName");
  return value;
}

std::unique_ptr<TracedValue> inspector_async_task::Data(
    const StringView& name) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("name", name.ToString());
  return value;
}

}  // namespace blink
