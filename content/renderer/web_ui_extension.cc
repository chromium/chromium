// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/web_ui_histograms_extension.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/presentation_feedback.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"

namespace content {

namespace {

// Resolves `global_resolver` with the presentation timestamp in `details`, or
// null if the frame was not presented. Runs on the main thread, from the
// compositor's swap-promise callback, possibly long after the request.
void ResolvePresentationTime(v8::Isolate* isolate,
                             v8::Global<v8::Context> global_context,
                             v8::Global<v8::Promise::Resolver> global_resolver,
                             const viz::FrameTimingDetails& details) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = global_context.Get(isolate);

  // The frame may have navigated or been detached while the request was
  // pending. Nothing can observe the promise any more, so drop it rather than
  // running script in a detached context.
  if (!blink::WebLocalFrame::FrameForContext(context)) {
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks_scope(
      context, v8::MicrotasksScope::kDoNotRunMicrotasks);

  // When the frame is never swapped (e.g. the commit turned out to have no
  // visual update), ReportTimeSwapPromise does not deliver a failed
  // PresentationFeedback: it synthesizes `details` with only the timestamp
  // set to the failure time and the flags left at zero. Frames that reached
  // viz always carry `received_compositor_frame_timestamp`, so its absence is
  // what identifies these synthesized failures.
  const gfx::PresentationFeedback& feedback = details.presentation_feedback;
  const bool presented = !feedback.failed() && !feedback.timestamp.is_null() &&
                         !details.received_compositor_frame_timestamp.is_null();
  v8::Local<v8::Value> result =
      presented ? gin::ConvertToV8(isolate, feedback.timestamp)
                : v8::Local<v8::Value>(v8::Null(isolate));
  std::ignore = global_resolver.Get(isolate)->Resolve(context, result);
}

}  // namespace

// static
void WebUIExtension::CreateIfNeeded(RenderFrame* render_frame) {
  if (!Get(render_frame)) {
    new WebUIExtension(render_frame);
  }
}

WebUIExtension::WebUIExtension(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<WebUIExtension>(render_frame) {}

WebUIExtension::~WebUIExtension() = default;

void WebUIExtension::BindWebUI(
    mojo::PendingAssociatedReceiver<mojom::WebUI> receiver,
    mojo::PendingAssociatedRemote<mojom::WebUIHost> remote) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr;
  // Binding to `kInternalNavigationAssociated` aligns the `mojom::WebUI`
  // receiver with `FrameBindingsControl` and navigation commit tasks, ensuring
  // properties set via `RenderFrameHost::SetWebUIProperty()` populate
  // `variable_map_` before document parsing and JS custom element constructors
  // evaluate. The default `nullptr` task runner uses
  // `kMainThreadTaskQueueDefault`, causing Mojo task-hopping delays and
  // startup timing races.
  //
  // `BindWebUI()` executes before navigation commit when frame URL state is
  // unknown. Because `SetProperty()` is a passive map insertion without JS or
  // DOM side effects, using this task runner for all WebUIs is safe and
  // eliminates timing races for property consumers without affecting
  // non-consumers.
  if (base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    task_runner = render_frame()->GetTaskRunner(
        blink::TaskType::kInternalNavigationAssociated);
  }
  receiver_.Bind(std::move(receiver), std::move(task_runner));
  remote_.Bind(std::move(remote));
}

void WebUIExtension::DidClearWindowObject() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);

  BindingsPolicySet bindings = render_frame()->GetEnabledBindings();
  if (bindings.Has(BindingsPolicyValue::kWebUi)) {
    InstallDefaultWebUIExtension(isolate, context, chrome);
  }

  if (bindings.Has(BindingsPolicyValue::kWebUiHistograms)) {
    InstallWebUIHistogramsExtension(isolate, context, chrome);
  }
}

void WebUIExtension::WillReleaseScriptContext(v8::Local<v8::Context> context,
                                              int32_t world_id) {
  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }
}

void WebUIExtension::OnDestruct() {
  delete this;
}

void WebUIExtension::SetProperty(const std::string& name,
                                 const std::string& value) {
  variable_map_[name] = value;
}

// Exposes the following methods:
//  - chrome.send: Used to send messages to the browser. Requires the message
//      name as the first argument and can have an optional second argument that
//      should be an array.
//  - chrome.getVariableValue: Returns value for the input variable name if such
//      a value was set by the browser. Else will return an empty string.
//  - chrome.timeTicks.nowInMicroseconds: Returns base::TimeTicks::Now() in
//      microseconds. Used for performance measuring.
//  - chrome.requestPresentationTime: Returns a Promise that resolves with the
//      base::TimeTicks (in microseconds, same clock as
//      chrome.timeTicks.nowInMicroseconds) at which the next frame produced by
//      this widget was presented to the user, or null if no such frame will be
//      presented. Call it from a regular task, after mutating the DOM and in
//      the same task, to learn when that mutation reached the screen. It does
//      not itself schedule a frame. Resolves null when the page is hidden
//      (document.visibilityState !== 'visible'), when no frame is pending (the
//      call came before the mutation, in a later task, or the mutation had no
//      visual effect), or when the compositor reports the frame as not
//      presented. Calling it from a visibilitychange handler as the page
//      becomes visible works, and reports the first frame of the show if the
//      browser has already requested one. Not for use inside
//      requestAnimationFrame or ResizeObserver callbacks: a frame update is
//      already in progress there, so no frame reads as pending.
//
//      For measurement only. Do not gate UI or other functionality on the
//      promise settling: it resolves only after the compositor's presentation
//      bookkeeping, which can lag the frame by one or more animation frames,
//      and in rare situations (e.g. a GPU process restart) it may stay pending
//      for an extended time.
void WebUIExtension::InstallDefaultWebUIExtension(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> chrome) {
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "send"),
            gin::CreateFunctionTemplate(
                isolate, base::BindRepeating(&WebUIExtension::Send,
                                             weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  // Use a trampoline so calls after context release or frame destruction still
  // return an empty string (matching
  // tools/typescript/definitions/chrome_send.d.ts) rather than undefined.
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "getVariableValue"),
            gin::CreateFunctionTemplate(
                isolate, base::BindRepeating(
                             [](base::WeakPtr<WebUIExtension> self,
                                const std::string& name) {
                               return self ? self->GetVariableValue(name)
                                           : std::string();
                             },
                             weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "requestPresentationTime"),
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(&WebUIExtension::RequestPresentationTime,
                                    weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();

  v8::Local<v8::Object> time_ticks =
      GetOrCreateObject(isolate, context, chrome, "timeTicks");
  time_ticks
      ->Set(context, gin::StringToSymbol(isolate, "nowInMicroseconds"),
            gin::CreateFunctionTemplate<&base::TimeTicks::Now>(isolate)
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

void WebUIExtension::Send(gin::Arguments* args) {
  std::string message;
  if (!args->GetNext(&message)) {
    args->ThrowError();
    return;
  }

  // If they've provided an optional message parameter, convert that into a
  // Value to send to the browser process.
  base::ListValue content;
  if (!args->PeekNext().IsEmpty() && !args->PeekNext()->IsUndefined()) {
    v8::Local<v8::Object> obj;
    if (!args->GetNext(&obj) || !obj->IsArray()) {
      args->ThrowError();
      return;
    }

    base::WeakPtr<WebUIExtension> weak_this = weak_ptr_factory_.GetWeakPtr();
    std::unique_ptr<base::Value> value =
        V8ValueConverter::Create()->FromV8Value(
            obj, args->isolate()->GetCurrentContext());
    // The conversion of `obj` can run arbitrary script (getters, toJSON), which
    // may detach the frame or navigate it. Drop the message in that case rather
    // than touching a destroyed WebUIExtension or sending from a stale context.
    if (!weak_this) {
      return;
    }
    DCHECK(value->is_list());
    content = std::move(*value).TakeList();
  }

  if (!remote_.is_bound()) {
    return;
  }
  remote_->Send(message, std::move(content));
}

std::string WebUIExtension::GetVariableValue(const std::string& name) const {
  auto it = variable_map_.find(name);
  if (it == variable_map_.end()) {
    return std::string();
  }
  return it->second;
}

void WebUIExtension::RequestPresentationTime(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) {
    return;
  }

  // NotifyPresentationTime() does not schedule a frame, so only queue the
  // request if one is already on the way. Otherwise resolve null now:
  //  - A hidden page produces no frames. The swap promise would stay queued
  //    until the page is next shown and then report that unrelated frame.
  //    Page visibility (what document.visibilityState reports) is checked
  //    rather than widget visibility: the browser shows the Page before the
  //    RenderWidget (see WebContentsImpl::UpdateVisibilityAndNotifyPageAndView)
  //    and hides them in the opposite order, so inside a visibilitychange
  //    handler on the way to 'visible' the widget still reads as hidden even
  //    though its first frame is already pending.
  //  - With no main frame pending on a visible page, the promise would wait
  //    for whatever frame happens next, which could be arbitrarily far away.
  //    A DOM mutation in this task requests a main frame synchronously, so
  //    this case means the caller mutated nothing (or did so in another task).
  blink::WebFrameWidget* widget = frame->LocalRoot()->FrameWidget();
  const bool page_visible = frame->View()->GetVisibilityState() ==
                            blink::mojom::PageVisibilityState::kVisible;
  if (!page_visible || !widget->RequestedMainFramePending()) {
    resolver->Resolve(context, v8::Null(isolate)).Check();
    args->Return(resolver->GetPromise());
    return;
  }

  // The callback runs when the next frame is presented, or when the compositor
  // gives up on it (e.g. a commit with no visual update, or widget teardown),
  // in which case `presentation_feedback.failed()` is true.
  widget->NotifyPresentationTime(
      base::BindOnce(&ResolvePresentationTime, isolate,
                     v8::Global<v8::Context>(isolate, context),
                     v8::Global<v8::Promise::Resolver>(isolate, resolver)));

  args->Return(resolver->GetPromise());
}

}  // namespace content
