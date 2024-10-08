// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"

#include <inttypes.h>

#include <memory>

#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
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
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/dynamic_annotations.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

const unsigned kMaxLayoutRoots = 10;
const unsigned kMaxQuads = 10;

void InspectorParseHtmlBeginData(perfetto::TracedValue context,
                                 Document* document,
                                 int start_line) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("startLine", start_line);
  dict.Add("frame", IdentifiersFactory::FrameId(document->GetFrame()));
  dict.Add("url", document->Url().GetString());
  SetCallStack(document->GetAgent().isolate(), dict);
}

void InspectorParseHtmlEndData(perfetto::TracedValue context, int end_line) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("endLine", end_line);
}

void GetNavigationTracingData(perfetto::TracedValue context,
                              Document* document) {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("navigationId", IdentifiersFactory::LoaderId(document->Loader()));
}

int GetModifierFromEvent(const UIEventWithKeyState& event) {
  int modifier = 0;
  if (event.altKey())

    modifier |= 1;
  if (event.ctrlKey())
    modifier |= 2;
  if (event.metaKey())
    modifier |= 4;
  if (event.shiftKey())
    modifier |= 8;
  return modifier;
}

}  //  namespace

String ToHexString(const void* p) {
  return String::Format("0x%" PRIx64,
                        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)));
}

void SetCallStack(v8::Isolate* isolate, perfetto::TracedDictionary& dict) {
  static const unsigned char* trace_category_enabled = nullptr;
  WTF_ANNOTATE_BENIGN_RACE(&trace_category_enabled, "trace_event category");
  if (!trace_category_enabled) {
    trace_category_enabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"));
  }
  if (!*trace_category_enabled)
    return;
  // The CPU profiler stack trace does not include call site line numbers.
  // So we collect the top frame with  CaptureSourceLocation() to
  // get the binding call site info.
  auto source_location = CaptureSourceLocation();
  if (source_location->HasStackTrace())
    dict.Add("stackTrace", source_location);
  v8::CpuProfiler::CollectSample(isolate);
}

void InspectorTraceEvents::WillSendRequest(
    ExecutionContext* execution_context,
    DocumentLoader* loader,
    const KURL& fetch_context_url,
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const ResourceLoaderOptions& resource_loader_options,
    ResourceType resource_type,
    RenderBlockingBehavior render_blocking_behavior,
    base::TimeTicks timestamp) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "devtools.timeline", "ResourceSendRequest", TRACE_EVENT_SCOPE_THREAD,
      timestamp, "data", [&](perfetto::TracedValue ctx) {
        inspector_send_request_event::Data(
            std::move(ctx), execution_context, loader, request.InspectorId(),
            frame, request, resource_type, render_blocking_behavior,
            resource_loader_options);
      });
}

void InspectorTraceEvents::WillSendNavigationRequest(
    uint64_t identifier,
    DocumentLoader* loader,
    const KURL& url,
    const AtomicString& http_method,
    EncodedFormData*) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ResourceSendRequest", inspector_send_navigation_request_event::Data,
      loader, identifier, frame, url, http_method);
}

void InspectorTraceEvents::DidReceiveResourceResponse(
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceResponse& response,
    const Resource*) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT("ResourceReceiveResponse",
                                        inspector_receive_response_event::Data,
                                        loader, identifier, frame, response);
}

void InspectorTraceEvents::DidReceiveData(
    uint64_t identifier,
    DocumentLoader* loader,
    base::SpanOrSize<const char> encoded_data) {
  LocalFrame* frame = loader ? loader->GetFrame() : nullptr;
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ResourceReceivedData", inspector_receive_data_event::Data, loader,
      identifier, frame, encoded_data.size());
}

void InspectorTraceEvents::DidFinishLoading(uint64_t identifier,
                                            DocumentLoader* loader,
                                            base::TimeTicks finish_time,
                                            int64_t encoded_data_length,
                                            int64_t decoded_body_length) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ResourceFinish", inspector_resource_finish_event::Data, loader,
      identifier, finish_time, false, encoded_data_length, decoded_body_length);
}

void InspectorTraceEvents::DidFailLoading(
    CoreProbeSink* sink,
    uint64_t identifier,
    DocumentLoader* loader,
    const ResourceError&,
    const base::UnguessableToken& devtools_frame_or_worker_token) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ResourceFinish", inspector_resource_finish_event::Data, loader,
      identifier, base::TimeTicks(), true, 0, 0);
}

void InspectorTraceEvents::MarkResourceAsCached(DocumentLoader* loader,
                                                uint64_t identifier) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ResourceMarkAsCached", inspector_mark_resource_cached_event::Data,
      loader, identifier);
}

void InspectorTraceEvents::Will(const probe::ExecuteScript&) {}

void InspectorTraceEvents::Did(const probe::ExecuteScript& probe) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       [&](perfetto::TracedValue context) {
                         inspector_update_counters_event::Data(
                             std::move(context), probe.context->GetIsolate());
                       });
}

void InspectorTraceEvents::Will(const probe::ParseHTML& probe) {
  // FIXME: Pass in current input length.
  TRACE_EVENT_BEGIN1("devtools.timeline", "ParseHTML", "beginData",
                     [&](perfetto::TracedValue context) {
                       InspectorParseHtmlBeginData(
                           std::move(context), probe.parser->GetDocument(),
                           probe.parser->LineNumber().ZeroBasedInt());
                     });
}

void InspectorTraceEvents::Did(const probe::ParseHTML& probe) {
  TRACE_EVENT_END1("devtools.timeline", "ParseHTML", "endData",
                   [&](perfetto::TracedValue context) {
                     InspectorParseHtmlEndData(
                         std::move(context),
                         probe.parser->LineNumber().ZeroBasedInt() - 1);
                   });
  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters",
      TRACE_EVENT_SCOPE_THREAD, "data", [&](perfetto::TracedValue context) {
        inspector_update_counters_event::Data(
            std::move(context), probe.document->GetAgent().isolate());
      });
}

void InspectorTraceEvents::Will(const probe::CallFunction& probe) {}

void InspectorTraceEvents::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       [&](perfetto::TracedValue context) {
                         inspector_update_counters_event::Data(
                             std::move(context), probe.context->GetIsolate());
                       });
}

void InspectorTraceEvents::PaintTiming(Document* document,
                                       const char* name,
                                       double timestamp) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading,rail,devtools.timeline", name,
                                   trace_event::ToTraceTimestamp(timestamp),
                                   "frame",
                                   GetFrameIdForTracing(document->GetFrame()),
                                   "data", [&](perfetto::TracedValue context) {
                                     GetNavigationTracingData(
                                         std::move(context), document);
                                   });
}

void InspectorTraceEvents::FrameStartedLoading(LocalFrame* frame) {
  TRACE_EVENT_INSTANT1("devtools.timeline", "FrameStartedLoading",
                       TRACE_EVENT_SCOPE_THREAD, "frame",
                       GetFrameIdForTracing(frame));
}

namespace {

void SetNodeInfo(perfetto::TracedDictionary& dict,
                 Node* node,
                 perfetto::StaticString id_field_name,
                 perfetto::StaticString name_field_name = nullptr) {
  dict.Add(id_field_name, IdentifiersFactory::IntIdForNode(node));
  if (name_field_name.value)
    dict.Add(name_field_name, node->DebugName());
}

const char* PseudoTypeToString(CSSSelector::PseudoType pseudo_type) {
  switch (pseudo_type) {
#define DEFINE_STRING_MAPPING(pseudoType) \
  case CSSSelector::k##pseudoType:        \
    return #pseudoType;
    DEFINE_STRING_MAPPING(PseudoActiveViewTransition)
    DEFINE_STRING_MAPPING(PseudoActiveViewTransitionType)
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
    DEFINE_STRING_MAPPING(PseudoStateDeprecatedSyntax)
    DEFINE_STRING_MAPPING(PseudoLink)
    DEFINE_STRING_MAPPING(PseudoVisited)
    DEFINE_STRING_MAPPING(PseudoAny)
    DEFINE_STRING_MAPPING(PseudoIs)
    DEFINE_STRING_MAPPING(PseudoWhere)
    DEFINE_STRING_MAPPING(PseudoWebkitAnyLink)
    DEFINE_STRING_MAPPING(PseudoAnyLink)
    DEFINE_STRING_MAPPING(PseudoAutofill)
    DEFINE_STRING_MAPPING(PseudoWebKitAutofill)
    DEFINE_STRING_MAPPING(PseudoAutofillPreviewed)
    DEFINE_STRING_MAPPING(PseudoAutofillSelected)
    DEFINE_STRING_MAPPING(PseudoHasSlotted)
    DEFINE_STRING_MAPPING(PseudoHover)
    DEFINE_STRING_MAPPING(PseudoDrag)
    DEFINE_STRING_MAPPING(PseudoFocus)
    DEFINE_STRING_MAPPING(PseudoFocusVisible)
    DEFINE_STRING_MAPPING(PseudoFocusWithin)
    DEFINE_STRING_MAPPING(PseudoActive)
    DEFINE_STRING_MAPPING(PseudoChecked)
    DEFINE_STRING_MAPPING(PseudoCurrent)
    DEFINE_STRING_MAPPING(PseudoEnabled)
    DEFINE_STRING_MAPPING(PseudoFullPageMedia)
    DEFINE_STRING_MAPPING(PseudoDefault)
    DEFINE_STRING_MAPPING(PseudoDir)
    DEFINE_STRING_MAPPING(PseudoDisabled)
    DEFINE_STRING_MAPPING(PseudoOptional)
    DEFINE_STRING_MAPPING(PseudoPlaceholderShown)
    DEFINE_STRING_MAPPING(PseudoRequired)
    DEFINE_STRING_MAPPING(PseudoReadOnly)
    DEFINE_STRING_MAPPING(PseudoReadWrite)
    DEFINE_STRING_MAPPING(PseudoUserInvalid)
    DEFINE_STRING_MAPPING(PseudoUserValid)
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
    DEFINE_STRING_MAPPING(PseudoFileSelectorButton)
    DEFINE_STRING_MAPPING(PseudoResizer)
    DEFINE_STRING_MAPPING(PseudoRoot)
    DEFINE_STRING_MAPPING(PseudoScope)
    DEFINE_STRING_MAPPING(PseudoScrollbar)
    DEFINE_STRING_MAPPING(PseudoScrollbarButton)
    DEFINE_STRING_MAPPING(PseudoScrollbarCorner)
    DEFINE_STRING_MAPPING(PseudoScrollbarThumb)
    DEFINE_STRING_MAPPING(PseudoScrollbarTrack)
    DEFINE_STRING_MAPPING(PseudoScrollbarTrackPiece)
    DEFINE_STRING_MAPPING(PseudoScrollMarker)
    DEFINE_STRING_MAPPING(PseudoScrollMarkerGroup)
    DEFINE_STRING_MAPPING(PseudoScrollNextButton)
    DEFINE_STRING_MAPPING(PseudoScrollPrevButton)
    DEFINE_STRING_MAPPING(PseudoColumn)
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
    DEFINE_STRING_MAPPING(PseudoPaused)
    DEFINE_STRING_MAPPING(PseudoPermissionElementInvalidStyle)
    DEFINE_STRING_MAPPING(PseudoPermissionElementOccluded)
    DEFINE_STRING_MAPPING(PseudoPermissionGranted)
    DEFINE_STRING_MAPPING(PseudoPictureInPicture)
    DEFINE_STRING_MAPPING(PseudoPlaying)
    DEFINE_STRING_MAPPING(PseudoInRange)
    DEFINE_STRING_MAPPING(PseudoOutOfRange)
    DEFINE_STRING_MAPPING(PseudoTrue)
    DEFINE_STRING_MAPPING(PseudoWebKitCustomElement)
    DEFINE_STRING_MAPPING(PseudoBlinkInternalElement)
    DEFINE_STRING_MAPPING(PseudoCue)
    DEFINE_STRING_MAPPING(PseudoFutureCue)
    DEFINE_STRING_MAPPING(PseudoPastCue)
    DEFINE_STRING_MAPPING(PseudoDefined)
    DEFINE_STRING_MAPPING(PseudoHost)
    DEFINE_STRING_MAPPING(PseudoHostContext)
    DEFINE_STRING_MAPPING(PseudoSlotted)
    DEFINE_STRING_MAPPING(PseudoSpatialNavigationFocus)
    DEFINE_STRING_MAPPING(PseudoHasDatalist)
    DEFINE_STRING_MAPPING(PseudoIsHtml)
    DEFINE_STRING_MAPPING(PseudoListBox)
    DEFINE_STRING_MAPPING(PseudoMultiSelectFocus)
    DEFINE_STRING_MAPPING(PseudoOpen)
    DEFINE_STRING_MAPPING(PseudoClosed)
    DEFINE_STRING_MAPPING(PseudoSelectHasChildButton)
    DEFINE_STRING_MAPPING(PseudoPicker)
    DEFINE_STRING_MAPPING(PseudoDialogInTopLayer)
    DEFINE_STRING_MAPPING(PseudoPopoverInTopLayer)
    DEFINE_STRING_MAPPING(PseudoPopoverOpen)
    DEFINE_STRING_MAPPING(PseudoHostHasNonAutoAppearance)
    DEFINE_STRING_MAPPING(PseudoVideoPersistent)
    DEFINE_STRING_MAPPING(PseudoVideoPersistentAncestor)
    DEFINE_STRING_MAPPING(PseudoXrOverlay)
    DEFINE_STRING_MAPPING(PseudoSearchText)
    DEFINE_STRING_MAPPING(PseudoTargetText)
    DEFINE_STRING_MAPPING(PseudoSelectorFragmentAnchor)
    DEFINE_STRING_MAPPING(PseudoModal)
    DEFINE_STRING_MAPPING(PseudoHighlight)
    DEFINE_STRING_MAPPING(PseudoSpellingError)
    DEFINE_STRING_MAPPING(PseudoGrammarError)
    DEFINE_STRING_MAPPING(PseudoHas)
    DEFINE_STRING_MAPPING(PseudoRelativeAnchor)
    DEFINE_STRING_MAPPING(PseudoViewTransition)
    DEFINE_STRING_MAPPING(PseudoViewTransitionGroup);
    DEFINE_STRING_MAPPING(PseudoViewTransitionImagePair);
    DEFINE_STRING_MAPPING(PseudoViewTransitionNew);
    DEFINE_STRING_MAPPING(PseudoViewTransitionOld);
    DEFINE_STRING_MAPPING(PseudoDetailsContent)
    DEFINE_STRING_MAPPING(PseudoParent);
    DEFINE_STRING_MAPPING(PseudoUnparsed)
#undef DEFINE_STRING_MAPPING
  }
}

String UrlForFrame(LocalFrame* frame) {
  KURL url = frame->GetDocument()->Url();
  url.RemoveFragmentIdentifier();
  return url.GetString();
}

const char* NotStreamedReasonString(ScriptStreamer::NotStreamingReason reason) {
  switch (reason) {
    case ScriptStreamer::NotStreamingReason::kNotHTTP:
      return "not http/https protocol";
    case ScriptStreamer::NotStreamingReason::kRevalidate:
      return "revalidation event";
    case ScriptStreamer::NotStreamingReason::kContextNotValid:
      return "script context not valid";
    case ScriptStreamer::NotStreamingReason::kEncodingNotSupported:
      return "encoding not supported";
    case ScriptStreamer::NotStreamingReason::kThreadBusy:
      return "script streamer thread busy";
    case ScriptStreamer::NotStreamingReason::kV8CannotStream:
      return "V8 cannot stream script";
    case ScriptStreamer::NotStreamingReason::kScriptTooSmall:
      return "script too small";
    case ScriptStreamer::NotStreamingReason::kNoResourceBuffer:
      return "resource no longer alive";
    case ScriptStreamer::NotStreamingReason::kHasCodeCache:
      return "script has code-cache available";
    case ScriptStreamer::NotStreamingReason::kStreamerNotReadyOnGetSource:
      return "streamer not ready";
    case ScriptStreamer::NotStreamingReason::kInlineScript:
      return "inline script";
    case ScriptStreamer::NotStreamingReason::kErrorOccurred:
      return "an error occurred";
    case ScriptStreamer::NotStreamingReason::kStreamingDisabled:
      return "already disabled streaming";
    case ScriptStreamer::NotStreamingReason::kSecondScriptResourceUse:
      return "already used streamed data";
    case ScriptStreamer::NotStreamingReason::kWorkerTopLevelScript:
      return "worker top-level scripts are not streamable";
    case ScriptStreamer::NotStreamingReason::kModuleScript:
      return "module script";
    case ScriptStreamer::NotStreamingReason::kNoDataPipe:
      return "no data pipe received";
    case ScriptStreamer::NotStreamingReason::kDisabledByFeatureList:
      return "streaming disabled from the feature list";
    case ScriptStreamer::NotStreamingReason::kErrorScriptTypeMismatch:
      return "wrong script type";
    case ScriptStreamer::NotStreamingReason::kNonJavascriptModule:
      return "not a javascript module";
    case ScriptStreamer::NotStreamingReason::kLoadingCancelled:
      return "loading was cancelled";
    case ScriptStreamer::NotStreamingReason::
        kBackgroundResponseProcessorWillBeUsed:
      return "Backgound streaming will be used";
    case ScriptStreamer::NotStreamingReason::kNonJavascriptModuleBackground:
      return "not a javascript module (background)";
    case ScriptStreamer::NotStreamingReason::kHasCodeCacheBackground:
      return "script has code-cache available (background)";
    case ScriptStreamer::NotStreamingReason::kScriptTooSmallBackground:
      return "script too small (background)";
    case ScriptStreamer::NotStreamingReason::kErrorOccurredBackground:
      return "an error occurred (background)";
    case ScriptStreamer::NotStreamingReason::kEncodingNotSupportedBackground:
      return "encoding not supported (background)";
    case ScriptStreamer::NotStreamingReason::kDidntTryToStartStreaming:
    case ScriptStreamer::NotStreamingReason::kAlreadyLoaded:
    case ScriptStreamer::NotStreamingReason::kInvalid:
      NOTREACHED();
  }
}

}  // namespace

namespace inspector_schedule_style_invalidation_tracking_event {
void FillCommonPart(perfetto::TracedDictionary& dict,
                    ContainerNode& node,
                    const InvalidationSet& invalidation_set,
                    const char* invalidated_selector) {
  dict.Add("frame", IdentifiersFactory::FrameId(node.GetDocument().GetFrame()));
  SetNodeInfo(dict, &node, "nodeId", "nodeName");
  dict.Add("invalidationSet",
           DescendantInvalidationSetToIdString(invalidation_set));
  dict.Add("invalidatedSelectorId", invalidated_selector);
  auto source_location = CaptureSourceLocation();
  if (source_location->HasStackTrace())
    dict.Add("stackTrace", source_location);
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
  switch (priority) {
    case ResourceLoadPriority::kVeryLow:
      return "VeryLow";
    case ResourceLoadPriority::kLow:
      return "Low";
    case ResourceLoadPriority::kMedium:
      return "Medium";
    case ResourceLoadPriority::kHigh:
      return "High";
    case ResourceLoadPriority::kVeryHigh:
      return "VeryHigh";
    case ResourceLoadPriority::kUnresolved:
      return nullptr;
  }
}

const char* FetchPriorityString(
    mojom::blink::FetchPriorityHint fetch_priority) {
  switch (fetch_priority) {
    case mojom::blink::FetchPriorityHint::kAuto:
      return "auto";
    case mojom::blink::FetchPriorityHint::kLow:
      return "low";
    case mojom::blink::FetchPriorityHint::kHigh:
      return "high";
  }
}

void inspector_schedule_style_invalidation_tracking_event::IdChange(
    perfetto::TracedValue context,
    Element& element,
    const InvalidationSet& invalidation_set,
    const AtomicString& id) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, invalidation_set, kId);
  dict.Add("changedId", id);
}

void inspector_schedule_style_invalidation_tracking_event::ClassChange(
    perfetto::TracedValue context,
    Element& element,
    const InvalidationSet& invalidation_set,
    const AtomicString& class_name) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, invalidation_set, kClass);
  dict.Add("changedClass", class_name);
}

void inspector_schedule_style_invalidation_tracking_event::AttributeChange(
    perfetto::TracedValue context,
    Element& element,
    const InvalidationSet& invalidation_set,
    const QualifiedName& attribute_name) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, invalidation_set, kAttribute);
  dict.Add("changedAttribute", attribute_name.ToString());
}

void inspector_schedule_style_invalidation_tracking_event::PseudoChange(
    perfetto::TracedValue context,
    Element& element,
    const InvalidationSet& invalidation_set,
    CSSSelector::PseudoType pseudo_type) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, invalidation_set, kAttribute);
  dict.Add("changedPseudo", PseudoTypeToString(pseudo_type));
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
    kInvalidationSetInvalidatesSelf[] = "Invalidation set invalidates self";
const char inspector_style_invalidator_invalidate_event::
    kInvalidationSetInvalidatesSubtree[] =
        "Invalidation set invalidates subtree";
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
void FillCommonPart(perfetto::TracedDictionary& dict,
                    ContainerNode& node,
                    const char* reason) {
  dict.Add("frame", IdentifiersFactory::FrameId(node.GetDocument().GetFrame()));
  SetNodeInfo(dict, &node, "nodeId", "nodeName");
  dict.Add("reason", reason);
}
void FillSelectors(
    perfetto::TracedDictionary& dict,
    const InvalidationSet& invalidation_set,
    InvalidationSetToSelectorMap::SelectorFeatureType feature_type,
    const AtomicString& feature_value) {
  const InvalidationSetToSelectorMap::IndexedSelectorList* selectors =
      InvalidationSetToSelectorMap::Lookup(&invalidation_set, feature_type,
                                           feature_value);
  if (selectors != nullptr && selectors->size() > 0) {
    dict.Add("selectorCount", selectors->size());
    auto array = dict.AddArray("selectors");
    for (auto selector : *selectors) {
      array.Append(selector->GetSelectorText());
    }
  }
}
}  // namespace inspector_style_invalidator_invalidate_event

void inspector_style_invalidator_invalidate_event::Data(
    perfetto::TracedValue context,
    Element& element,
    const char* reason) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, reason);
}

void inspector_style_invalidator_invalidate_event::SelectorPart(
    perfetto::TracedValue context,
    Element& element,
    const char* reason,
    const InvalidationSet& invalidation_set,
    const AtomicString& selector_part) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, element, reason);
  InvalidationSetToSelectorMap::SelectorFeatureType feature_type =
      InvalidationSetToSelectorMap::SelectorFeatureType::kUnknown;
  if (reason == kInvalidationSetMatchedClass) {
    feature_type = InvalidationSetToSelectorMap::SelectorFeatureType::kClass;
  } else if (reason == kInvalidationSetMatchedId) {
    feature_type = InvalidationSetToSelectorMap::SelectorFeatureType::kId;
  } else if (reason == kInvalidationSetMatchedTagName) {
    feature_type = InvalidationSetToSelectorMap::SelectorFeatureType::kTagName;
  } else if (reason == kInvalidationSetMatchedAttribute) {
    feature_type =
        InvalidationSetToSelectorMap::SelectorFeatureType::kAttribute;
  } else if (reason == kInvalidationSetInvalidatesSubtree) {
    feature_type =
        InvalidationSetToSelectorMap::SelectorFeatureType::kWholeSubtree;
  }
  if (feature_type !=
      InvalidationSetToSelectorMap::SelectorFeatureType::kUnknown) {
    FillSelectors(dict, invalidation_set, feature_type, selector_part);
  }

  {
    auto array = dict.AddArray("invalidationList");
    array.Append(invalidation_set);
  }
  dict.Add("selectorPart", selector_part);
}

void inspector_style_invalidator_invalidate_event::InvalidationList(
    perfetto::TracedValue context,
    ContainerNode& node,
    const Vector<scoped_refptr<InvalidationSet>>& invalidation_list) {
  auto dict = std::move(context).WriteDictionary();
  FillCommonPart(dict, node, kElementHasPendingInvalidationList);
  dict.Add("invalidationList", invalidation_list);
}

void inspector_style_recalc_invalidation_tracking_event::Data(
    perfetto::TracedValue context,
    Node* node,
    StyleChangeType change_type,
    const StyleChangeReasonForTracing& reason) {
  DCHECK(node);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame",
           IdentifiersFactory::FrameId(node->GetDocument().GetFrame()));
  SetNodeInfo(dict, node, "nodeId", "nodeName");
  dict.Add("subtree", change_type == kSubtreeStyleChange);
  dict.Add("reason", reason.ReasonString());
  dict.Add("extraData", reason.GetExtraData());
  auto source_location = CaptureSourceLocation();
  if (source_location->HasStackTrace())
    dict.Add("stackTrace", source_location);
}

void inspector_layout_event::BeginData(perfetto::TracedValue context,
                                       LocalFrameView* frame_view) {
  bool is_partial;
  unsigned needs_layout_objects;
  unsigned total_objects;
  LocalFrame& frame = frame_view->GetFrame();
  frame.View()->CountObjectsNeedingLayout(needs_layout_objects, total_objects,
                                          is_partial);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("dirtyObjects", needs_layout_objects);
  dict.Add("totalObjects", total_objects);
  dict.Add("partialLayout", is_partial);
  dict.Add("frame", IdentifiersFactory::FrameId(&frame));
  SetCallStack(frame.DomWindow()->GetIsolate(), dict);
}

static void CreateQuad(perfetto::TracedValue context, const gfx::QuadF& quad) {
  auto array = std::move(context).WriteArray();
  array.Append(quad.p1().x());
  array.Append(quad.p1().y());
  array.Append(quad.p2().x());
  array.Append(quad.p2().y());
  array.Append(quad.p3().x());
  array.Append(quad.p3().y());
  array.Append(quad.p4().x());
  array.Append(quad.p4().y());
}

static void SetGeneratingNodeInfo(
    perfetto::TracedDictionary& dict,
    const LayoutObject* layout_object,
    perfetto::StaticString id_field_name,
    perfetto::StaticString name_field_name = nullptr) {
  Node* node = nullptr;
  for (; layout_object && !node; layout_object = layout_object->Parent())
    node = layout_object->GeneratingNode();
  if (!node)
    return;

  SetNodeInfo(dict, node, id_field_name, name_field_name);
}

static void CreateLayoutRoot(perfetto::TracedValue context,
                             const LayoutObjectWithDepth& layout_root) {
  auto dict = std::move(context).WriteDictionary();
  SetGeneratingNodeInfo(dict, layout_root.object, "nodeId");
  dict.Add("depth", static_cast<int>(layout_root.depth));
  Vector<gfx::QuadF> quads;
  layout_root.object->AbsoluteQuads(quads);
  if (quads.size() > kMaxQuads)
    quads.Shrink(kMaxQuads);
  {
    auto array = dict.AddArray("quads");
    for (auto& quad : quads)
      CreateQuad(array.AppendItem(), quad);
  }
}

static void SetHeaders(perfetto::TracedValue context,
                       const HTTPHeaderMap& headers) {
  auto array = std::move(context).WriteArray();
  for (auto& header : headers) {
    auto item_dict = array.AppendDictionary();
    item_dict.Add("name", header.key);
    item_dict.Add("value", header.value);
  }
}

void inspector_layout_event::EndData(
    perfetto::TracedValue context,
    const HeapVector<LayoutObjectWithDepth>& layout_roots) {
  auto dict = std::move(context).WriteDictionary();
  {
    auto array = dict.AddArray("layoutRoots");
    unsigned numRoots = 0u;
    for (auto& layout_root : layout_roots) {
      if (++numRoots > kMaxLayoutRoots)
        break;
      CreateLayoutRoot(array.AppendItem(), layout_root);
    }
  }
}

namespace layout_invalidation_reason {
const char kUnknown[] = "Unknown";
const char kSizeChanged[] = "Size changed";
const char kAncestorMoved[] = "Ancestor moved";
const char kStyleChange[] = "Style changed";
const char kDomChanged[] = "DOM changed";
const char kTextChanged[] = "Text changed";
const char kPrintingChanged[] = "Printing changed";
const char kPaintPreview[] = "Enter/exit paint preview";
const char kAttributeChanged[] = "Attribute changed";
const char kColumnsChanged[] = "Attribute changed";
const char kChildAnonymousBlockChanged[] = "Child anonymous block changed";
const char kAnonymousBlockChange[] = "Anonymous block change";
const char kFontsChanged[] = "Fonts changed";
const char kFullscreen[] = "Fullscreen change";
const char kChildChanged[] = "Child changed";
const char kListValueChange[] = "List value change";
const char kListStyleTypeChange[] = "List style type change";
const char kCounterStyleChange[] = "Counter style change";
const char kImageChanged[] = "Image changed";
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
const char kDevtools[] = "Inspected by devtools";
const char kAnchorPositioning[] = "Anchor positioning";
}  // namespace layout_invalidation_reason

void inspector_layout_invalidation_tracking_event::Data(
    perfetto::TracedValue context,
    const LayoutObject* layout_object,
    LayoutInvalidationReasonForTracing reason) {
  DCHECK(layout_object);
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(layout_object->GetFrame()));
  SetGeneratingNodeInfo(dict, layout_object, "nodeId", "nodeName");
  dict.Add("reason", reason);
  auto source_location = CaptureSourceLocation();
  if (source_location->HasStackTrace())
    dict.Add("stackTrace", source_location);
}

void inspector_change_resource_priority_event::Data(
    perfetto::TracedValue context,
    DocumentLoader* loader,
    uint64_t identifier,
    const ResourceLoadPriority& load_priority) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("priority", ResourcePriorityString(load_priority));
}

namespace {
String GetRenderBlockingStringFromBehavior(
    RenderBlockingBehavior render_blocking_behavior) {
  switch (render_blocking_behavior) {
    case RenderBlockingBehavior::kUnset:
      return String();
    case RenderBlockingBehavior::kBlocking:
      return "blocking";
    case RenderBlockingBehavior::kNonBlocking:
      return "non_blocking";
    case RenderBlockingBehavior::kNonBlockingDynamic:
      return "dynamically_injected_non_blocking";
    case RenderBlockingBehavior::kInBodyParserBlocking:
      return "in_body_parser_blocking";
    case RenderBlockingBehavior::kPotentiallyBlocking:
      return "potentially_blocking";
  }
}

}  // namespace

void SetInitiator(Document* document,
                  FetchInitiatorInfo initiator_info,
                  perfetto::TracedDictionary& dict) {
  auto initiator =
      InspectorNetworkAgent::BuildInitiatorObject(document, initiator_info, 0);
  auto initiatorDict = dict.AddDictionary("initiator");

  initiatorDict.Add("fetchType", initiator_info.name);
  initiatorDict.Add("type", initiator->getType());
  if (initiator->hasColumnNumber()) {
    initiatorDict.Add("columnNumber", initiator->getColumnNumber(-1));
  }
  if (initiator->hasLineNumber()) {
    initiatorDict.Add("lineNumber", initiator->getLineNumber(-1));
  }
  if (initiator->hasUrl()) {
    initiatorDict.Add("url", initiator->getUrl(""));
  }
}

void inspector_send_request_event::Data(
    perfetto::TracedValue context,
    ExecutionContext* execution_context,
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    const ResourceRequest& request,
    ResourceType resource_type,
    RenderBlockingBehavior render_blocking_behavior,
    const ResourceLoaderOptions& resource_loader_options) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", IdentifiersFactory::RequestId(loader, identifier));
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("url", request.Url().GetString());
  dict.Add("requestMethod", request.HttpMethod());
  dict.Add("isLinkPreload",
           resource_loader_options.initiator_info.is_link_preload);
  String resource_type_string = InspectorPageAgent::ResourceTypeJson(
      InspectorPageAgent::ToResourceType(resource_type));
  dict.Add("resourceType", resource_type_string);
  String render_blocking_string =
      GetRenderBlockingStringFromBehavior(render_blocking_behavior);
  if (!render_blocking_string.IsNull()) {
    dict.Add("renderBlocking", render_blocking_string);
  }
  const char* priority = ResourcePriorityString(request.Priority());
  if (priority)
    dict.Add("priority", priority);
  dict.Add("fetchPriorityHint",
           FetchPriorityString(request.GetFetchPriorityHint()));
  SetCallStack(execution_context->GetIsolate(), dict);
  SetInitiator(frame ? frame->GetDocument() : nullptr,
               resource_loader_options.initiator_info, dict);
}

void inspector_change_render_blocking_behavior_event::Data(
    perfetto::TracedValue context,
    DocumentLoader* loader,
    uint64_t identifier,
    const ResourceRequestHead& request,
    RenderBlockingBehavior render_blocking_behavior) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("url", request.Url().GetString());
  String render_blocking_string =
      GetRenderBlockingStringFromBehavior(render_blocking_behavior);
  if (!render_blocking_string.IsNull()) {
    dict.Add("renderBlocking", render_blocking_string);
  }
}

void inspector_send_navigation_request_event::Data(
    perfetto::TracedValue context,
    DocumentLoader* loader,
    uint64_t identifier,
    LocalFrame* frame,
    const KURL& url,
    const AtomicString& http_method) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", IdentifiersFactory::LoaderId(loader));
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("url", url.GetString());
  dict.Add("requestMethod", http_method);
  dict.Add("resourceType", protocol::Network::ResourceTypeEnum::Document);
  const char* priority =
      ResourcePriorityString(ResourceLoadPriority::kVeryHigh);
  if (priority)
    dict.Add("priority", priority);
  dict.Add("fetchPriorityHint",
           FetchPriorityString(mojom::blink::FetchPriorityHint::kAuto));
  SetCallStack(frame->DomWindow()->GetIsolate(), dict);
}

namespace {
void RecordTiming(perfetto::TracedValue context,
                  const ResourceLoadTiming& timing) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestTime", timing.RequestTime().since_origin().InSecondsF());
  dict.Add("proxyStart", timing.CalculateMillisecondDelta(timing.ProxyStart()));
  dict.Add("proxyEnd", timing.CalculateMillisecondDelta(timing.ProxyEnd()));
  dict.Add("dnsStart",
           timing.CalculateMillisecondDelta(timing.DomainLookupStart()));
  dict.Add("dnsEnd",
           timing.CalculateMillisecondDelta(timing.DomainLookupEnd()));
  dict.Add("connectStart",
           timing.CalculateMillisecondDelta(timing.ConnectStart()));
  dict.Add("connectEnd", timing.CalculateMillisecondDelta(timing.ConnectEnd()));
  dict.Add("sslStart", timing.CalculateMillisecondDelta(timing.SslStart()));
  dict.Add("sslEnd", timing.CalculateMillisecondDelta(timing.SslEnd()));
  dict.Add("workerStart",
           timing.CalculateMillisecondDelta(timing.WorkerStart()));
  dict.Add("workerReady",
           timing.CalculateMillisecondDelta(timing.WorkerReady()));
  dict.Add("sendStart", timing.CalculateMillisecondDelta(timing.SendStart()));
  dict.Add("sendEnd", timing.CalculateMillisecondDelta(timing.SendEnd()));
  dict.Add("receiveHeadersStart",
           timing.CalculateMillisecondDelta(timing.ReceiveHeadersStart()));
  dict.Add("receiveHeadersEnd",
           timing.CalculateMillisecondDelta(timing.ReceiveHeadersEnd()));
  dict.Add("pushStart", timing.PushStart().since_origin().InSecondsF());
  dict.Add("pushEnd", timing.PushEnd().since_origin().InSecondsF());
}
}  // namespace

void inspector_receive_response_event::Data(perfetto::TracedValue context,
                                            DocumentLoader* loader,
                                            uint64_t identifier,
                                            LocalFrame* frame,
                                            const ResourceResponse& response) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("connectionId", response.ConnectionID());
  dict.Add("connectionReused", response.ConnectionReused());
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("statusCode", response.HttpStatusCode());
  dict.Add("mimeType", response.MimeType().GetString());
  dict.Add("encodedDataLength", response.EncodedDataLength());
  dict.Add("fromCache", response.WasCached());
  dict.Add("fromServiceWorker", response.WasFetchedViaServiceWorker());

  if (response.WasFetchedViaServiceWorker()) {
    switch (response.GetServiceWorkerResponseSource()) {
      case network::mojom::FetchResponseSource::kCacheStorage:
        dict.Add("serviceWorkerResponseSource", "cacheStorage");
        break;
      case network::mojom::FetchResponseSource::kHttpCache:
        dict.Add("serviceWorkerResponseSource", "httpCache");
        break;
      case network::mojom::FetchResponseSource::kNetwork:
        dict.Add("serviceWorkerResponseSource", "network");
        break;
      case network::mojom::FetchResponseSource::kUnspecified:
        dict.Add("serviceWorkerResponseSource", "fallbackCode");
    }
  }
  if (!response.ResponseTime().is_null()) {
    dict.Add("responseTime",
             response.ResponseTime().InMillisecondsFSinceUnixEpoch());
  }
  if (!response.CacheStorageCacheName().empty()) {
    dict.Add("cacheStorageCacheName", response.CacheStorageCacheName());
  }
  if (response.GetResourceLoadTiming()) {
    RecordTiming(dict.AddItem("timing"), *response.GetResourceLoadTiming());
  }
  if (response.WasFetchedViaServiceWorker()) {
    dict.Add("fromServiceWorker", true);
  }
  if (response.GetServiceWorkerRouterInfo()) {
    auto info = dict.AddDictionary("staticRoutingInfo");
    info.Add("ruleIdMatched",
             response.GetServiceWorkerRouterInfo()->RuleIdMatched());
    info.Add("matchedSourceType",
             response.GetServiceWorkerRouterInfo()->MatchedSourceType());
  }

  SetHeaders(dict.AddItem("headers"), response.HttpHeaderFields());
  dict.Add("protocol", InspectorNetworkAgent::GetProtocolAsString(response));
}

void inspector_receive_data_event::Data(perfetto::TracedValue context,
                                        DocumentLoader* loader,
                                        uint64_t identifier,
                                        LocalFrame* frame,
                                        uint64_t encoded_data_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("encodedDataLength", encoded_data_length);
}

void inspector_resource_finish_event::Data(perfetto::TracedValue context,
                                           DocumentLoader* loader,
                                           uint64_t identifier,
                                           base::TimeTicks finish_time,
                                           bool did_fail,
                                           int64_t encoded_data_length,
                                           int64_t decoded_body_length) {
  String request_id = IdentifiersFactory::RequestId(loader, identifier);

  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("didFail", did_fail);
  dict.Add("encodedDataLength", encoded_data_length);
  dict.Add("decodedBodyLength", decoded_body_length);
  if (!finish_time.is_null())
    dict.Add("finishTime", finish_time.since_origin().InSecondsF());
}

void inspector_mark_resource_cached_event::Data(perfetto::TracedValue context,
                                                DocumentLoader* loader,
                                                uint64_t identifier) {
  auto dict = std::move(context).WriteDictionary();
  String request_id = IdentifiersFactory::RequestId(loader, identifier);
  dict.Add("requestId", request_id);
}

static LocalFrame* FrameForExecutionContext(ExecutionContext* context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(context))
    return window->GetFrame();
  return nullptr;
}

static void GenericTimerData(perfetto::TracedDictionary& dict,
                             ExecutionContext* context,
                             int timer_id) {
  dict.Add("timerId", timer_id);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));
}

void inspector_timer_install_event::Data(perfetto::TracedValue trace_context,
                                         ExecutionContext* context,
                                         int timer_id,
                                         base::TimeDelta timeout,
                                         bool single_shot) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTimerData(dict, context, timer_id);
  dict.Add("timeout", timeout.InMillisecondsF());
  dict.Add("singleShot", single_shot);
  SetCallStack(context->GetIsolate(), dict);
}

void inspector_timer_remove_event::Data(perfetto::TracedValue trace_context,
                                        ExecutionContext* context,
                                        int timer_id) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTimerData(dict, context, timer_id);
  SetCallStack(context->GetIsolate(), dict);
}

void inspector_timer_fire_event::Data(perfetto::TracedValue trace_context,
                                      ExecutionContext* context,
                                      int timer_id) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTimerData(dict, context, timer_id);
}

void inspector_animation_frame_event::Data(perfetto::TracedValue trace_context,
                                           ExecutionContext* context,
                                           int callback_id) {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("id", callback_id);
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    dict.Add("frame", IdentifiersFactory::FrameId(window->GetFrame()));
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
    dict.Add("worker", ToHexString(scope));
  }
  SetCallStack(context->GetIsolate(), dict);
}

void GenericIdleCallbackEvent(perfetto::TracedDictionary& dict,
                              ExecutionContext* context,
                              int id) {
  dict.Add("id", id);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(context->GetIsolate(), dict);
}

void inspector_idle_callback_request_event::Data(
    perfetto::TracedValue trace_context,
    ExecutionContext* context,
    int id,
    double timeout) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericIdleCallbackEvent(dict, context, id);
  dict.Add("timeout", timeout);
}

void inspector_idle_callback_cancel_event::Data(
    perfetto::TracedValue trace_context,
    ExecutionContext* context,
    int id) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericIdleCallbackEvent(dict, context, id);
}

void inspector_idle_callback_fire_event::Data(
    perfetto::TracedValue trace_context,
    ExecutionContext* context,
    int id,
    double allotted_milliseconds,
    bool timed_out) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericIdleCallbackEvent(dict, context, id);
  dict.Add("allottedMilliseconds", allotted_milliseconds);
  dict.Add("timedOut", timed_out);
}

void inspector_parse_author_style_sheet_event::Data(
    perfetto::TracedValue context,
    const CSSStyleSheetResource* cached_style_sheet) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("styleSheetUrl", cached_style_sheet->Url().GetString());
}

void inspector_xhr_ready_state_change_event::Data(
    perfetto::TracedValue trace_context,
    ExecutionContext* context,
    XMLHttpRequest* request) {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("url", request->Url().GetString());
  dict.Add("readyState", request->readyState());
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(context->GetIsolate(), dict);
}

void inspector_xhr_load_event::Data(perfetto::TracedValue trace_context,
                                    ExecutionContext* context,
                                    XMLHttpRequest* request) {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("url", request->Url().GetString());
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(context->GetIsolate(), dict);
}

void inspector_paint_event::Data(perfetto::TracedValue context,
                                 LocalFrame* frame,
                                 const LayoutObject* layout_object,
                                 const gfx::QuadF& quad,
                                 int layer_id) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  CreateQuad(dict.AddItem("clip"), quad);
  SetGeneratingNodeInfo(dict, layout_object, "nodeId");
  dict.Add("layerId", layer_id);
  SetCallStack(frame->DomWindow()->GetIsolate(), dict);
}

void FrameEventData(perfetto::TracedDictionary& dict, LocalFrame* frame) {
  DCHECK(frame);
  dict.Add("isMainFrame", frame->IsMainFrame());
  dict.Add("isOutermostMainFrame", frame->IsOutermostMainFrame());
  // TODO(dgozman): this does not work with OOPIF, so everyone who
  // uses it should migrate to frame instead.
  dict.Add("page", IdentifiersFactory::FrameId(&frame->LocalFrameRoot()));
}

void FillCommonFrameData(perfetto::TracedDictionary& dict, LocalFrame* frame) {
  DCHECK(frame);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("url", UrlForFrame(frame));
  dict.Add("name", frame->Tree().GetName());

  FrameOwner* owner = frame->Owner();
  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(owner)) {
    dict.Add("nodeId", IdentifiersFactory::IntIdForNode(frame_owner_element));
  }
  Frame* parent = frame->Tree().Parent();
  if (IsA<LocalFrame>(parent))
    dict.Add("parent", IdentifiersFactory::FrameId(parent));
}

void inspector_commit_load_event::Data(perfetto::TracedValue context,
                                       LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  FrameEventData(dict, frame);
  FillCommonFrameData(dict, frame);
}

void inspector_layerize_event::Data(perfetto::TracedValue context,
                                    LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  FrameEventData(dict, frame);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
}

void inspector_mark_load_event::Data(perfetto::TracedValue context,
                                     LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  FrameEventData(dict, frame);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
}

void inspector_pre_paint_event::Data(perfetto::TracedValue context,
                                     LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  FrameEventData(dict, frame);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
}

void inspector_scroll_layer_event::Data(perfetto::TracedValue context,
                                        LayoutObject* layout_object) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(layout_object->GetFrame()));
  SetGeneratingNodeInfo(dict, layout_object, "nodeId");
}

namespace {
void FillLocation(perfetto::TracedDictionary& dict,
                  const String& url,
                  const TextPosition& text_position) {
  dict.Add("url", url);
  dict.Add("lineNumber", text_position.line_.OneBasedInt());
  dict.Add("columnNumber", text_position.column_.OneBasedInt());
}
}  // namespace

void inspector_evaluate_script_event::Data(perfetto::TracedValue context,
                                           v8::Isolate* isolate,
                                           LocalFrame* frame,
                                           const String& url,
                                           const TextPosition& text_position) {
  auto dict = std::move(context).WriteDictionary();
  FillLocation(dict, url, text_position);
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(isolate, dict);
}

void inspector_target_rundown_event::Data(perfetto::TracedValue context,
                                          ExecutionContext* execution_context,
                                          v8::Isolate* isolate,
                                          ScriptState* scriptState,
                                          int scriptId) {
  // Target related info
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;
  if (!frame) {
    return;
  }
  auto dict = std::move(context).WriteDictionary();
  String frameType = "page";
  if (frame->Parent() || frame->IsFencedFrameRoot()) {
    frameType = "iframe";
  }
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("frameType", frameType);
  dict.Add("url", window->Url().GetString());
  dict.Add("isolate", base::NumberToString(reinterpret_cast<size_t>(isolate)));

  // ExecutionContext related info
  DOMWrapperWorld& world = scriptState->World();
  String executionContextType = "default";
  const SecurityOrigin* origin = frame->DomWindow()->GetSecurityOrigin();
  if (world.IsIsolatedWorld()) {
    executionContextType = "isolated";
  } else if (world.IsWorkerOrWorkletWorld()) {
    executionContextType = "worker";
  }
  dict.Add("v8context", scriptState->GetToken().ToString());
  dict.Add("isDefault", world.IsMainWorld());
  dict.Add("contextType", executionContextType);
  dict.Add("origin", origin ? origin->ToRawString() : String());
  dict.Add("scriptId", scriptId);
}

void inspector_parse_script_event::Data(perfetto::TracedValue context,
                                        uint64_t identifier,
                                        const String& url) {
  String request_id = IdentifiersFactory::RequestId(
      static_cast<ExecutionContext*>(nullptr), identifier);
  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("url", url);
}

void inspector_deserialize_script_event::Data(perfetto::TracedValue context,
                                              uint64_t identifier,
                                              const String& url) {
  String request_id = IdentifiersFactory::RequestId(
      static_cast<ExecutionContext*>(nullptr), identifier);
  auto dict = std::move(context).WriteDictionary();
  dict.Add("requestId", request_id);
  dict.Add("url", url);
}

inspector_compile_script_event::V8ConsumeCacheResult::V8ConsumeCacheResult(
    int cache_size,
    bool rejected,
    bool full)
    : cache_size(cache_size), rejected(rejected), full(full) {}

void inspector_compile_script_event::Data(
    perfetto::TracedValue context,
    const String& url,
    const TextPosition& text_position,
    std::optional<V8ConsumeCacheResult> consume_cache_result,
    bool eager,
    bool streamed,
    ScriptStreamer::NotStreamingReason not_streaming_reason) {
  auto dict = std::move(context).WriteDictionary();
  FillLocation(dict, url, text_position);

  if (consume_cache_result) {
    dict.Add("consumedCacheSize", consume_cache_result->cache_size);
    dict.Add("cacheRejected", consume_cache_result->rejected);
    dict.Add("cacheKind", consume_cache_result->full ? "full" : "normal");
  }
  if (eager) {
    // Eager compilation is rare so only add this key when it's set.
    dict.Add("eager", true);
  }
  dict.Add("streamed", streamed);
  if (!streamed) {
    dict.Add("notStreamedReason",
             NotStreamedReasonString(not_streaming_reason));
  }
}

void inspector_produce_script_cache_event::Data(
    perfetto::TracedValue context,
    const String& url,
    const TextPosition& text_position,
    int cache_size) {
  auto dict = std::move(context).WriteDictionary();
  FillLocation(dict, url, text_position);
  dict.Add("producedCacheSize", cache_size);
}

void inspector_handle_post_message_event::Data(
    perfetto::TracedValue context,
    ExecutionContext* execution_context,
    const MessageEvent& message_event) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("traceId", base::NumberToString(message_event.GetTraceId()));
}

void inspector_schedule_post_message_event::Data(
    perfetto::TracedValue context,
    ExecutionContext* execution_context,
    uint64_t trace_id) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("traceId", base::NumberToString(trace_id));
  SetCallStack(execution_context->GetIsolate(), dict);
}

void inspector_function_call_event::Data(
    perfetto::TracedValue trace_context,
    ExecutionContext* context,
    const v8::Local<v8::Function>& function) {
  auto dict = std::move(trace_context).WriteDictionary();
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));

  if (function.IsEmpty())
    return;

  v8::Local<v8::Function> original_function = GetBoundFunction(function);
  v8::Local<v8::Value> function_name = original_function->GetDebugName();
  if (!function_name.IsEmpty() && function_name->IsString()) {
    dict.Add("functionName", ToCoreString(context->GetIsolate(),
                                          function_name.As<v8::String>()));
  }
  std::unique_ptr<SourceLocation> location =
      CaptureSourceLocation(context->GetIsolate(), original_function);
  dict.Add("scriptId", String::Number(location->ScriptId()));
  dict.Add("url", location->Url());
  dict.Add("lineNumber", location->LineNumber());
  dict.Add("columnNumber", location->ColumnNumber());
}

void inspector_paint_image_event::Data(perfetto::TracedValue context,
                                       const LayoutImage& layout_image,
                                       const gfx::RectF& src_rect,
                                       const gfx::RectF& dest_rect) {
  auto dict = std::move(context).WriteDictionary();
  SetGeneratingNodeInfo(dict, &layout_image, "nodeId");
  if (const ImageResourceContent* content = layout_image.CachedImage())
    dict.Add("url", content->Url().ElidedString());

  dict.Add("x", dest_rect.x());
  dict.Add("y", dest_rect.y());
  dict.Add("width", dest_rect.width());
  dict.Add("height", dest_rect.height());
  dict.Add("srcWidth", src_rect.width());
  dict.Add("srcHeight", src_rect.height());
}

void inspector_paint_image_event::Data(perfetto::TracedValue context,
                                       const LayoutObject& owning_layout_object,
                                       const StyleImage& style_image) {
  auto dict = std::move(context).WriteDictionary();
  SetGeneratingNodeInfo(dict, &owning_layout_object, "nodeId");
  if (const ImageResourceContent* content = style_image.CachedImage())
    dict.Add("url", content->Url().ElidedString());
}

void inspector_paint_image_event::Data(perfetto::TracedValue context,
                                       Node* node,
                                       const StyleImage& style_image,
                                       const gfx::RectF& src_rect,
                                       const gfx::RectF& dest_rect) {
  auto dict = std::move(context).WriteDictionary();
  if (node)
    SetNodeInfo(dict, node, "nodeId", nullptr);
  if (const ImageResourceContent* content = style_image.CachedImage())
    dict.Add("url", content->Url().ElidedString());

  dict.Add("x", dest_rect.x());
  dict.Add("y", dest_rect.y());
  dict.Add("width", dest_rect.width());
  dict.Add("height", dest_rect.height());
  dict.Add("srcWidth", src_rect.width());
  dict.Add("srcHeight", src_rect.height());
}

void inspector_paint_image_event::Data(
    perfetto::TracedValue context,
    const LayoutObject* owning_layout_object,
    const ImageResourceContent& image_content) {
  auto dict = std::move(context).WriteDictionary();
  SetGeneratingNodeInfo(dict, owning_layout_object, "nodeId");
  dict.Add("url", image_content.Url().ElidedString());
}

static size_t UsedHeapSize(v8::Isolate* isolate) {
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

void inspector_update_counters_event::Data(perfetto::TracedValue context,
                                           v8::Isolate* isolate) {
  auto dict = std::move(context).WriteDictionary();
  if (IsMainThread()) {
    dict.Add("documents", InstanceCounters::CounterValue(
                              InstanceCounters::kDocumentCounter));
    dict.Add("nodes",
             InstanceCounters::CounterValue(InstanceCounters::kNodeCounter));
    dict.Add("jsEventListeners",
             InstanceCounters::CounterValue(
                 InstanceCounters::kJSEventListenerCounter));
  }
  dict.Add("jsHeapSizeUsed", static_cast<double>(UsedHeapSize(isolate)));
}

void inspector_invalidate_layout_event::Data(perfetto::TracedValue context,
                                             LocalFrame* frame,
                                             DOMNodeId nodeId) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("nodeId", nodeId);
  SetCallStack(frame->DomWindow()->GetIsolate(), dict);
}

void inspector_recalculate_styles_event::Data(perfetto::TracedValue context,
                                              LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  SetCallStack(frame->DomWindow()->GetIsolate(), dict);
}

void inspector_event_dispatch_event::Data(perfetto::TracedValue context,
                                          const Event& event,
                                          v8::Isolate* isolate) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("type", event.type());
  bool record_input_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.inputs"),
      &record_input_enabled);
  if (record_input_enabled) {
    const auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
    if (keyboard_event) {
      dict.Add("modifier", GetModifierFromEvent(*keyboard_event));
      dict.Add(
          "timestamp",
          keyboard_event->PlatformTimeStamp().since_origin().InMicroseconds());
      dict.Add("code", keyboard_event->code());
      dict.Add("key", keyboard_event->key());
    }

    const auto* mouse_event = DynamicTo<MouseEvent>(event);
    const auto* wheel_event = DynamicTo<WheelEvent>(event);
    if (mouse_event || wheel_event) {
      dict.Add("x", mouse_event->x());
      dict.Add("y", mouse_event->y());
      dict.Add("modifier", GetModifierFromEvent(*mouse_event));
      dict.Add(
          "timestamp",
          mouse_event->PlatformTimeStamp().since_origin().InMicroseconds());
      dict.Add("button", mouse_event->button());
      dict.Add("buttons", mouse_event->buttons());
      dict.Add("clickCount", mouse_event->detail());
      if (wheel_event) {
        dict.Add("deltaX", wheel_event->deltaX());
        dict.Add("deltaY", wheel_event->deltaY());
      }
    }
  }
  SetCallStack(isolate, dict);
}

void inspector_time_stamp_event::Data(perfetto::TracedValue trace_context,
                                      ExecutionContext* context,
                                      const String& message) {
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("message", message);
  if (LocalFrame* frame = FrameForExecutionContext(context))
    dict.Add("frame", IdentifiersFactory::FrameId(frame));
}

void inspector_tracing_session_id_for_worker_event::Data(
    perfetto::TracedValue context,
    const base::UnguessableToken& worker_devtools_token,
    const base::UnguessableToken& parent_devtools_token,
    const KURL& url,
    PlatformThreadId worker_thread_id) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::IdFromToken(parent_devtools_token));
  dict.Add("url", url.GetString());
  dict.Add("workerId", IdentifiersFactory::IdFromToken(worker_devtools_token));
  dict.Add("workerThreadId", worker_thread_id);
}

void inspector_tracing_started_in_frame::Data(perfetto::TracedValue context,
                                              const String& session_id,
                                              LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("sessionId", session_id);
  dict.Add("page", IdentifiersFactory::FrameId(&frame->LocalFrameRoot()));
  dict.Add("persistentIds", true);
  {
    auto frames_array = dict.AddArray("frames");
    for (Frame* f = frame; f; f = f->Tree().TraverseNext(frame)) {
      auto* local_frame = DynamicTo<LocalFrame>(f);
      if (!local_frame)
        continue;
      auto frame_dict = frames_array.AppendDictionary();
      FillCommonFrameData(frame_dict, local_frame);
    }
  }
}

void inspector_set_layer_tree_id::Data(perfetto::TracedValue context,
                                       LocalFrame* frame) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("frame", IdentifiersFactory::FrameId(frame));
  dict.Add("layerTreeId",
           frame->GetPage()->GetChromeClient().GetLayerTreeId(*frame));
}

void inspector_animation_event::Data(perfetto::TracedValue context,
                                     const Animation& animation) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("id", String::Number(animation.SequenceNumber()));
  dict.Add("state", animation.PlayStateString());
  if (const AnimationEffect* effect = animation.effect()) {
    dict.Add("displayName",
             InspectorAnimationAgent::AnimationDisplayName(animation));
    dict.Add("name", animation.id());
    if (auto* frame_effect = DynamicTo<KeyframeEffect>(effect)) {
      if (Element* target = frame_effect->EffectTarget())
        SetNodeInfo(dict, target, "nodeId", "nodeName");
    }
  }
}

void inspector_animation_state_event::Data(perfetto::TracedValue context,
                                           const Animation& animation) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("state", animation.PlayStateString());
}

void inspector_animation_compositor_event::Data(
    perfetto::TracedValue context,
    CompositorAnimations::FailureReasons failure_reasons,
    const PropertyHandleSet& unsupported_properties) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("compositeFailed", failure_reasons);
  {
    auto unsupported_properties_array = dict.AddArray("unsupportedProperties");
    for (const PropertyHandle& p : unsupported_properties) {
      unsupported_properties_array.Append(
          p.GetCSSPropertyName().ToAtomicString());
    }
  }
}

void inspector_hit_test_event::EndData(perfetto::TracedValue context,
                                       const HitTestRequest& request,
                                       const HitTestLocation& location,
                                       const HitTestResult& result) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("x", location.RoundedPoint().x());
  dict.Add("y", location.RoundedPoint().y());
  if (location.IsRectBasedTest())
    dict.Add("rect", true);
  if (location.IsRectilinear())
    dict.Add("rectilinear", true);
  if (request.TouchEvent())
    dict.Add("touch", true);
  if (request.Move())
    dict.Add("move", true);
  if (request.ListBased())
    dict.Add("listBased", true);
  else if (Node* node = result.InnerNode())
    SetNodeInfo(dict, node, "nodeId", "nodeName");
}

void inspector_async_task::Data(perfetto::TracedValue context,
                                const StringView& name) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("name", name.ToString());
}

}  // namespace blink
