// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_TRACE_EVENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_TRACE_EVENTS_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace base {
class UnguessableToken;
}

namespace v8 {
class Function;
template <typename T>
class Local;
}  // namespace v8

namespace WTF {
class TextPosition;
}

namespace blink {
class Animation;
class CSSStyleSheetResource;
class ContainerNode;
class Document;
class DocumentLoader;
class Element;
class EncodedFormData;
class Event;
class ExecutionContext;
struct FetchInitiatorInfo;
class FloatRect;
class GraphicsLayer;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class ImageResourceContent;
class InvalidationSet;
class KURL;
class LayoutImage;
class LayoutObject;
class LocalFrame;
class LocalFrameView;
class Node;
struct PhysicalRect;
class QualifiedName;
class Resource;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class StyleChangeReasonForTracing;
class StyleImage;
class XMLHttpRequest;
enum class ResourceType : uint8_t;

namespace probe {
class CallFunction;
class ExecuteScript;
class ParseHTML;
}  // namespace probe

class CORE_EXPORT InspectorTraceEvents
    : public GarbageCollected<InspectorTraceEvents> {
 public:
  InspectorTraceEvents() = default;

  void WillSendRequest(uint64_t identifier,
                       DocumentLoader*,
                       const KURL& fetch_context_url,
                       const ResourceRequest&,
                       const ResourceResponse& redirect_response,
                       const FetchInitiatorInfo&,
                       ResourceType);
  void WillSendNavigationRequest(uint64_t identifier,
                                 DocumentLoader*,
                                 const KURL&,
                                 const AtomicString& http_method,
                                 EncodedFormData*);
  void DidReceiveResourceResponse(uint64_t identifier,
                                  DocumentLoader*,
                                  const ResourceResponse&,
                                  const Resource*);
  void DidReceiveData(uint64_t identifier,
                      DocumentLoader*,
                      const char* data,
                      uint64_t data_length);
  void DidFinishLoading(uint64_t identifier,
                        DocumentLoader*,
                        base::TimeTicks monotonic_finish_time,
                        int64_t encoded_data_length,
                        int64_t decoded_body_length,
                        bool should_report_corb_blocking);
  void DidFailLoading(uint64_t identifier,
                      DocumentLoader*,
                      const ResourceError&);
  void MarkResourceAsCached(DocumentLoader* loader, uint64_t identifier);

  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  void Will(const probe::ParseHTML&);
  void Did(const probe::ParseHTML&);

  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  void PaintTiming(Document*, const char* name, double timestamp);

  void FrameStartedLoading(LocalFrame*);

  void Trace(blink::Visitor*) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectorTraceEvents);
};

namespace inspector_layout_event {
std::unique_ptr<TracedValue> BeginData(LocalFrameView*);
std::unique_ptr<TracedValue> EndData(LayoutObject* root_for_this_layout);
}  // namespace inspector_layout_event

namespace inspector_schedule_style_invalidation_tracking_event {
extern const char kAttribute[];
extern const char kClass[];
extern const char kId[];
extern const char kPseudo[];
extern const char kRuleSet[];

std::unique_ptr<TracedValue> AttributeChange(Element&,
                                             const InvalidationSet&,
                                             const QualifiedName&);
std::unique_ptr<TracedValue> ClassChange(Element&,
                                         const InvalidationSet&,
                                         const AtomicString&);
std::unique_ptr<TracedValue> IdChange(Element&,
                                      const InvalidationSet&,
                                      const AtomicString&);
std::unique_ptr<TracedValue> PseudoChange(Element&,
                                          const InvalidationSet&,
                                          CSSSelector::PseudoType);
std::unique_ptr<TracedValue> RuleSetInvalidation(ContainerNode&,
                                                 const InvalidationSet&);
}  // namespace inspector_schedule_style_invalidation_tracking_event

#define TRACE_SCHEDULE_STYLE_INVALIDATION(element, invalidationSet,          \
                                          changeType, ...)                   \
  TRACE_EVENT_INSTANT1(                                                      \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),   \
      "ScheduleStyleInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data", \
      inspector_schedule_style_invalidation_tracking_event::changeType(      \
          (element), (invalidationSet), ##__VA_ARGS__));

namespace inspector_style_recalc_invalidation_tracking_event {
std::unique_ptr<TracedValue> Data(Node*, const StyleChangeReasonForTracing&);
}

String DescendantInvalidationSetToIdString(const InvalidationSet&);

namespace inspector_style_invalidator_invalidate_event {
extern const char kElementHasPendingInvalidationList[];
extern const char kInvalidateCustomPseudo[];
extern const char kInvalidationSetMatchedAttribute[];
extern const char kInvalidationSetMatchedClass[];
extern const char kInvalidationSetMatchedId[];
extern const char kInvalidationSetMatchedTagName[];
extern const char kInvalidationSetMatchedPart[];

std::unique_ptr<TracedValue> Data(Element&, const char* reason);
std::unique_ptr<TracedValue> SelectorPart(Element&,
                                          const char* reason,
                                          const InvalidationSet&,
                                          const String&);
std::unique_ptr<TracedValue> InvalidationList(
    ContainerNode&,
    const Vector<scoped_refptr<InvalidationSet>>&);
}  // namespace inspector_style_invalidator_invalidate_event

#define TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason)              \
  TRACE_EVENT_INSTANT1(                                                    \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"), \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,    \
      "data",                                                              \
      inspector_style_invalidator_invalidate_event::Data(                  \
          (element), (inspector_style_invalidator_invalidate_event::reason)))

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_SELECTORPART(                   \
    element, reason, invalidationSet, singleSelectorPart)                    \
  TRACE_EVENT_INSTANT1(                                                      \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),   \
      "StyleInvalidatorInvalidationTracking", TRACE_EVENT_SCOPE_THREAD,      \
      "data",                                                                \
      inspector_style_invalidator_invalidate_event::SelectorPart(            \
          (element), (inspector_style_invalidator_invalidate_event::reason), \
          (invalidationSet), (singleSelectorPart)))

// From a web developer's perspective: what caused this layout? This is strictly
// for tracing. Blink logic must not depend on these.
namespace layout_invalidation_reason {
extern const char kUnknown[];
extern const char kSizeChanged[];
extern const char kAncestorMoved[];
extern const char kStyleChange[];
extern const char kDomChanged[];
extern const char kTextChanged[];
extern const char kPrintingChanged[];
extern const char kAttributeChanged[];
extern const char kColumnsChanged[];
extern const char kChildAnonymousBlockChanged[];
extern const char kAnonymousBlockChange[];
extern const char kFullscreen[];
extern const char kChildChanged[];
extern const char kListValueChange[];
extern const char kListStyleTypeChange[];
extern const char kImageChanged[];
extern const char kLineBoxesChanged[];
extern const char kSliderValueChanged[];
extern const char kAncestorMarginCollapsing[];
extern const char kFieldsetChanged[];
extern const char kTextAutosizing[];
extern const char kSvgResourceInvalidated[];
extern const char kFloatDescendantChanged[];
extern const char kCountersChanged[];
extern const char kGridChanged[];
extern const char kMenuOptionsChanged[];
extern const char kRemovedFromLayout[];
extern const char kAddedToLayout[];
extern const char kTableChanged[];
extern const char kPaddingChanged[];
extern const char kTextControlChanged[];
// FIXME: This is too generic, we should be able to split out transform and
// size related invalidations.
extern const char kSvgChanged[];
extern const char kScrollbarChanged[];
extern const char kDisplayLock[];
}  // namespace layout_invalidation_reason

// LayoutInvalidationReasonForTracing is strictly for tracing. Blink logic must
// not depend on this value.
typedef const char LayoutInvalidationReasonForTracing[];

namespace inspector_layout_invalidation_tracking_event {
std::unique_ptr<TracedValue> CORE_EXPORT
Data(const LayoutObject*, LayoutInvalidationReasonForTracing);
}

namespace inspector_change_resource_priority_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  const ResourceLoadPriority&);
}

namespace inspector_send_request_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  LocalFrame*,
                                  const ResourceRequest&);
}

namespace inspector_send_navigation_request_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  LocalFrame*,
                                  const KURL&,
                                  const AtomicString& http_method);
}

namespace inspector_receive_response_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  LocalFrame*,
                                  const ResourceResponse&);
}

namespace inspector_receive_data_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  LocalFrame*,
                                  uint64_t encoded_data_length);
}

namespace inspector_resource_finish_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*,
                                  uint64_t identifier,
                                  base::TimeTicks finish_time,
                                  bool did_fail,
                                  int64_t encoded_data_length,
                                  int64_t decoded_body_length);
}

namespace inspector_mark_resource_cached_event {
std::unique_ptr<TracedValue> Data(DocumentLoader*, uint64_t identifier);
}

namespace inspector_timer_install_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  int timer_id,
                                  base::TimeDelta timeout,
                                  bool single_shot);
}

namespace inspector_timer_remove_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int timer_id);
}

namespace inspector_timer_fire_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int timer_id);
}

namespace inspector_idle_callback_request_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int id, double timeout);
}

namespace inspector_idle_callback_cancel_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int id);
}

namespace inspector_idle_callback_fire_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  int id,
                                  double allotted_milliseconds,
                                  bool timed_out);
}

namespace inspector_animation_frame_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, int callback_id);
}

namespace inspector_parse_author_style_sheet_event {
std::unique_ptr<TracedValue> Data(const CSSStyleSheetResource*);
}

namespace inspector_xhr_ready_state_change_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, XMLHttpRequest*);
}

namespace inspector_xhr_load_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, XMLHttpRequest*);
}

namespace inspector_paint_event {
std::unique_ptr<TracedValue> Data(LayoutObject*,
                                  const PhysicalRect& clip_rect,
                                  const GraphicsLayer*);
}

namespace inspector_paint_image_event {
std::unique_ptr<TracedValue> Data(const LayoutImage&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);
std::unique_ptr<TracedValue> Data(const LayoutObject&, const StyleImage&);
std::unique_ptr<TracedValue> Data(Node*,
                                  const StyleImage&,
                                  const FloatRect& src_rect,
                                  const FloatRect& dest_rect);
std::unique_ptr<TracedValue> Data(const LayoutObject*,
                                  const ImageResourceContent&);
}  // namespace inspector_paint_image_event

namespace inspector_commit_load_event {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace inspector_mark_load_event {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace inspector_scroll_layer_event {
std::unique_ptr<TracedValue> Data(LayoutObject*);
}

namespace inspector_update_layer_tree_event {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace inspector_evaluate_script_event {
std::unique_ptr<TracedValue> Data(LocalFrame*,
                                  const String& url,
                                  const WTF::TextPosition&);
}

namespace inspector_parse_script_event {
std::unique_ptr<TracedValue> Data(uint64_t identifier, const String& url);
}

namespace inspector_compile_script_event {

struct V8CacheResult {
  struct ProduceResult {
    explicit ProduceResult(int cache_size);
    int cache_size;
  };
  struct ConsumeResult {
    ConsumeResult(v8::ScriptCompiler::CompileOptions consume_options,
                  int cache_size,
                  bool rejected);
    v8::ScriptCompiler::CompileOptions consume_options;
    int cache_size;
    bool rejected;
  };
  V8CacheResult() = default;
  V8CacheResult(base::Optional<ProduceResult>, base::Optional<ConsumeResult>);

  base::Optional<ProduceResult> produce_result;
  base::Optional<ConsumeResult> consume_result;
};

std::unique_ptr<TracedValue> Data(const String& url,
                                  const WTF::TextPosition&,
                                  const V8CacheResult&,
                                  bool streamed,
                                  ScriptStreamer::NotStreamingReason);
}  // namespace inspector_compile_script_event

namespace inspector_function_call_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*,
                                  const v8::Local<v8::Function>&);
}

namespace inspector_update_counters_event {
std::unique_ptr<TracedValue> Data();
}

namespace inspector_invalidate_layout_event {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace inspector_recalculate_styles_event {
std::unique_ptr<TracedValue> Data(LocalFrame*);
}

namespace inspector_event_dispatch_event {
std::unique_ptr<TracedValue> Data(const Event&);
}

namespace inspector_time_stamp_event {
std::unique_ptr<TracedValue> Data(ExecutionContext*, const String& message);
}

namespace inspector_tracing_session_id_for_worker_event {
std::unique_ptr<TracedValue> Data(
    const base::UnguessableToken& worker_devtools_token,
    const base::UnguessableToken& parent_devtools_token,
    const KURL& url,
    PlatformThreadId worker_thread_id);
}

namespace inspector_tracing_started_in_frame {
std::unique_ptr<TracedValue> Data(const String& session_id, LocalFrame*);
}

namespace inspector_set_layer_tree_id {
std::unique_ptr<TracedValue> Data(LocalFrame* local_root);
}

namespace inspector_animation_event {
std::unique_ptr<TracedValue> Data(const Animation&);
}

namespace inspector_animation_state_event {
std::unique_ptr<TracedValue> Data(const Animation&);
}

namespace inspector_hit_test_event {
std::unique_ptr<TracedValue> EndData(const HitTestRequest&,
                                     const HitTestLocation&,
                                     const HitTestResult&);
}

namespace inspector_async_task {
std::unique_ptr<TracedValue> Data(const StringView&);
}

CORE_EXPORT String ToHexString(const void* p);
CORE_EXPORT void SetCallStack(TracedValue*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_TRACE_EVENTS_H_
