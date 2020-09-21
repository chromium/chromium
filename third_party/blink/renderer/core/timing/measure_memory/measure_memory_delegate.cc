// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/measure_memory/measure_memory_delegate.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_measure_memory_breakdown.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

MeasureMemoryDelegate::MeasureMemoryDelegate(v8::Isolate* isolate,
                                             v8::Local<v8::Context> context,
                                             ResultCallback callback)
    : isolate_(isolate),
      context_(isolate, context),
      callback_(std::move(callback)) {
  context_.SetPhantom();
}

// Returns true if the given context should be included in the current memory
// measurement. Currently it is very conservative and allows only the same
// origin contexts that belong to the same JavaScript origin.
// With COOP/COEP we will be able to relax this restriction for the contexts
// that opt-in into memory measurement.
bool MeasureMemoryDelegate::ShouldMeasure(v8::Local<v8::Context> context) {
  if (context_.IsEmpty()) {
    // The original context was garbage collected in the meantime.
    return false;
  }
  v8::Local<v8::Context> original_context = context_.NewLocal(isolate_);
  ExecutionContext* original_execution_context =
      ExecutionContext::From(original_context);
  ExecutionContext* execution_context = ExecutionContext::From(context);
  if (!original_execution_context || !execution_context) {
    // One of the contexts is detached or is created by DevTools.
    return false;
  }
  if (original_execution_context->GetAgent() != execution_context->GetAgent()) {
    // Context do not belong to the same JavaScript agent.
    return false;
  }
  if (ScriptState::From(context)->World().IsIsolatedWorld()) {
    // Context belongs to an extension. Skip it.
    return false;
  }
  const SecurityOrigin* original_security_origin =
      original_execution_context->GetSecurityContext().GetSecurityOrigin();
  const SecurityOrigin* security_origin =
      execution_context->GetSecurityContext().GetSecurityOrigin();
  if (!original_security_origin->IsSameOriginWith(security_origin)) {
    // TODO(ulan): Check for COOP/COEP and allow cross-origin contexts that
    // opted in for memory measurement.
    // Until then we allow cross-origin measurement only for site-isolated
    // web pages.
    return Platform::Current()->IsLockedToSite();
  }
  return true;
}

namespace {
// Helper functions for constructing a memory measurement result.

LocalFrame* GetLocalFrame(v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (!window) {
    // The context was detached. Ignore it.
    return nullptr;
  }
  return window->GetFrame();
}

// Returns true if all frames on the path from the main frame to
// the given frame (excluding the given frame) have the same origin.
bool AllAncestorsAndOpenersAreSameOrigin(const WebFrame* main_frame,
                                         const WebFrame* frame) {
  while (frame != main_frame) {
    frame = frame->Parent() ? frame->Parent() : frame->Opener();
    if (!main_frame->GetSecurityOrigin().CanAccess(frame->GetSecurityOrigin()))
      return false;
  }
  return true;
}

// Returns the URL corresponding to the given frame. It is:
// - document's URL if the frame is a same-origin top frame.
// - the src attribute of the owner iframe element if the frame is
//   an iframe.
// - nullopt, otherwise.
// Preconditions:
// - If the frame is cross-origin, then all its ancestors/openers
//   must be of the same origin as the main frame.
// - The frame must be attached to the DOM tree and the main frame
//   must be reachable via child => parent and openee => opener edges.
String GetUrl(const WebFrame* main_frame, const WebFrame* frame) {
  DCHECK(AllAncestorsAndOpenersAreSameOrigin(main_frame, frame));
  if (!frame->Parent()) {
    // TODO(ulan): Turn this conditional into a DCHECK once the API
    // is gated behind COOP+COEP. Only same-origin frames can appear here.
    if (!main_frame->GetSecurityOrigin().CanAccess(frame->GetSecurityOrigin()))
      return {};
    // The frame must be local because it is in the same browsing context
    // group as the main frame and has the same origin.
    // Check to avoid memory corruption in the case if our invariant is off.
    CHECK(IsA<LocalFrame>(WebFrame::ToCoreFrame(*frame)));
    LocalFrame* local = To<LocalFrame>(WebFrame::ToCoreFrame(*frame));
    return local->GetDocument()->Url().GetString();
  }
  FrameOwner* frame_owner = WebFrame::ToCoreFrame(*frame)->Owner();
  // The frame owner must be local because the parent of the frame has
  // the same origin as the main frame. Also the frame cannot be provisional
  // here because it is attached and has a document.
  // Check to avoid of memory corruption in the case if our invariant is off.
  CHECK(IsA<HTMLFrameOwnerElement>(frame_owner));
  HTMLFrameOwnerElement* owner_element = To<HTMLFrameOwnerElement>(frame_owner);
  switch (owner_element->OwnerType()) {
    case mojom::blink::FrameOwnerElementType::kIframe:
      return owner_element->getAttribute(html_names::kSrcAttr);
    case mojom::blink::FrameOwnerElementType::kObject:
    case mojom::blink::FrameOwnerElementType::kEmbed:
    case mojom::blink::FrameOwnerElementType::kFrame:
    case mojom::blink::FrameOwnerElementType::kPortal:
      // TODO(ulan): return the data/src attribute after adding tests.
      return {};
    case mojom::blink::FrameOwnerElementType::kNone:
      // The main frame was handled as a local frame above.
      NOTREACHED();
      return {};
  }
}

// To avoid information leaks cross-origin iframes are considered opaque for
// the purposes of attribution. This means the memory of all iframes nested
// in a cross-origin iframe is attributed to the cross-origin iframe.
// See https://github.com/WICG/performance-measure-memory for more details.
//
// Given the main frame and a frame, this function find the first cross-origin
// frame in the path from the main frame to the given frame. Edges in the path
// are parent/child and opener/openee edges.
// If the path doesn't exist then it returns nullptr.
// If there are no cross-origin frames, then it returns the given frame.
//
// Precondition: the frame must be attached to the DOM tree.
const WebFrame* GetAttributionFrame(const WebFrame* main_frame,
                                    const WebFrame* frame) {
  WebSecurityOrigin main_security_origin = main_frame->GetSecurityOrigin();
  // Walk up the tree and the openers to find the first cross-origin frame
  // on the path from the main frame to the given frame.
  const WebFrame* result = frame;
  while (frame != main_frame) {
    if (frame->Parent()) {
      frame = frame->Parent();
    } else if (frame->Opener()) {
      frame = frame->Opener();
    } else {
      // The opener was reset. We cannot get the attribution.
      return nullptr;
    }
    if (!main_security_origin.CanAccess(frame->GetSecurityOrigin()))
      result = frame;
  }
  // The result frame must be attached because we started from an attached
  // frame (precondition) and followed the parent and opener references until
  // the main frame, which is also attached.
  DCHECK(WebFrame::ToCoreFrame(*result)->IsAttached());
  return result;
}

// Return per-frame sizes based on the given per-context size.
// TODO(ulan): Revisit this after Origin Trial and see if the results
// are precise enough or if we need to additionally group by JS agent.
HashMap<const WebFrame*, size_t> GroupByFrame(
    const WebFrame* main_frame,
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t& detached_size,
    size_t& unknown_frame_size) {
  detached_size = 0;
  unknown_frame_size = 0;
  HashMap<const WebFrame*, size_t> per_frame;
  for (const auto& context_size : context_sizes) {
    const WebFrame* frame =
        WebFrame::FromFrame(GetLocalFrame(context_size.first));
    if (!frame) {
      detached_size += context_size.second;
      continue;
    }
    frame = GetAttributionFrame(main_frame, frame);
    if (!frame) {
      unknown_frame_size += context_size.second;
      continue;
    }
    auto it = per_frame.find(frame);
    if (it == per_frame.end()) {
      per_frame.insert(frame, context_size.second);
    } else {
      it->value += context_size.second;
    }
  }
  return per_frame;
}

MeasureMemoryBreakdown* CreateMeasureMemoryBreakdown(
    size_t bytes,
    const Vector<String>& types,
    const String& url) {
  MeasureMemoryBreakdown* result = MeasureMemoryBreakdown::Create();
  result->setBytes(bytes);
  result->setUserAgentSpecificTypes(types);
  result->setAttribution(url.length() ? Vector<String>{url} : Vector<String>());
  return result;
}

}  // anonymous namespace

// Constructs a memory measurement result based on the given list of (context,
// size) pairs and resolves the promise.
void MeasureMemoryDelegate::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>& context_sizes,
    size_t unattributed_size) {
  if (context_.IsEmpty()) {
    // The context was garbage collected in the meantime.
    return;
  }
  v8::Local<v8::Context> context = context_.NewLocal(isolate_);
  const WebFrame* main_frame = WebFrame::FromFrame(GetLocalFrame(context));
  if (!main_frame) {
    // The context was detached in the meantime.
    return;
  }
  DCHECK(!main_frame->Parent());
  v8::Context::Scope context_scope(context);
  size_t total_size = 0;
  for (const auto& context_size : context_sizes) {
    total_size += context_size.second;
  }
  HeapVector<Member<MeasureMemoryBreakdown>> breakdown;
  size_t detached_size;
  size_t unknown_frame_size;
  HashMap<const WebFrame*, size_t> per_frame(GroupByFrame(
      main_frame, context_sizes, detached_size, unknown_frame_size));
  size_t attributed_size = 0;
  const String kWindow("Window");
  const String kJS("JS");
  const Vector<String> js_window_types = {kWindow, kJS};
  for (const auto& it : per_frame) {
    String url = GetUrl(main_frame, it.key);
    if (url.IsNull()) {
      unknown_frame_size += it.value;
      continue;
    }
    attributed_size += it.value;
    breakdown.push_back(
        CreateMeasureMemoryBreakdown(it.value, js_window_types, url));
  }
  if (detached_size) {
    const String kDetached("Detached");
    breakdown.push_back(CreateMeasureMemoryBreakdown(
        detached_size, Vector<String>{kWindow, kJS, kDetached}, ""));
  }
  if (unattributed_size) {
    const String kShared("Shared");
    breakdown.push_back(CreateMeasureMemoryBreakdown(
        unattributed_size, Vector<String>{kWindow, kJS, kShared}, ""));
  }
  if (unknown_frame_size) {
    breakdown.push_back(CreateMeasureMemoryBreakdown(
        unknown_frame_size, Vector<String>{kWindow, kJS}, ""));
  }
  std::move(callback_).Run(breakdown);
}

}  // namespace blink
