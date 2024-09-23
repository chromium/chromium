// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/isolated_world_manager.h"
#include "extensions/renderer/scripts_run_info.h"
#include "extensions/renderer/trace_util.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/public/web/web_script_source.h"

using perfetto::protos::pbzero::ChromeTrackEvent;

namespace extensions {

namespace {

const int64_t kInvalidRequestId = -1;

// The id of the next pending injection.
int64_t g_next_pending_id = 0;

}  // namespace

// Watches for the deletion of a RenderFrame, after which is_valid will return
// false.
class ScriptInjection::FrameWatcher : public content::RenderFrameObserver {
 public:
  FrameWatcher(content::RenderFrame* render_frame,
               ScriptInjection* injection)
      : content::RenderFrameObserver(render_frame),
        injection_(injection) {}

  FrameWatcher(const FrameWatcher&) = delete;
  FrameWatcher& operator=(const FrameWatcher&) = delete;

  ~FrameWatcher() override {}

 private:
  void WillDetach(blink::DetachReason detach_reason) override {
    injection_->invalidate_render_frame();
  }
  void OnDestruct() override { injection_->invalidate_render_frame(); }

  raw_ptr<ScriptInjection> injection_;
};

ScriptInjection::ScriptInjection(
    std::unique_ptr<ScriptInjector> injector,
    content::RenderFrame* render_frame,
    std::unique_ptr<const InjectionHost> injection_host,
    mojom::RunLocation run_location,
    bool log_activity)
    : injector_(std::move(injector)),
      render_frame_(render_frame),
      injection_host_(std::move(injection_host)),
      run_location_(run_location),
      request_id_(kInvalidRequestId),
      complete_(false),
      did_inject_js_(false),
      log_activity_(log_activity),
      frame_watcher_(new FrameWatcher(render_frame, this)) {
  CHECK(injection_host_.get());
  TRACE_EVENT_BEGIN(
      "extensions", "ScriptInjection", perfetto::Track::FromPointer(this),
      ChromeTrackEvent::kRenderProcessHost, content::RenderThread::Get(),
      ChromeTrackEvent::kChromeExtensionId,
      ExtensionIdForTracing(host_id().id));
}

ScriptInjection::~ScriptInjection() {
  if (!complete_)
    NotifyWillNotInject(ScriptInjector::WONT_INJECT);

  TRACE_EVENT_END("extensions", perfetto::Track::FromPointer(this),
                  ChromeTrackEvent::kRenderProcessHost,
                  content::RenderThread::Get(),
                  ChromeTrackEvent::kChromeExtensionId,
                  ExtensionIdForTracing(host_id().id));
}

ScriptInjection::InjectionResult ScriptInjection::TryToInject(
    mojom::RunLocation current_location,
    ScriptsRunInfo* scripts_run_info,
    StatusUpdatedCallback async_updated_callback) {
  if (current_location == mojom::RunLocation::kUndefined &&
      render_frame_->IsInFencedFrameTree() && render_frame_->IsMainFrame()) {
    // Fenced frames do not navigate to about:blank by default the way iframes
    // do. They cannot accept script injections until they perform an initial
    // navigation.
    NotifyWillNotInject(ScriptInjector::NOT_ALLOWED);
    return INJECTION_FINISHED;  // We're done.
  }

  if (current_location < run_location_)
    return INJECTION_WAITING;  // Wait for the right location.

  if (request_id_ != kInvalidRequestId) {
    // We're waiting for permission right now, try again later.
    return INJECTION_WAITING;
  }

  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;  // We're done.
  }

  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();
  switch (injector_->CanExecuteOnFrame(
      injection_host_.get(), web_frame,
      ExtensionFrameHelper::Get(render_frame_)->tab_id())) {
    case PermissionsData::PageAccess::kDenied:
      NotifyWillNotInject(ScriptInjector::NOT_ALLOWED);
      return INJECTION_FINISHED;  // We're done.
    case PermissionsData::PageAccess::kWithheld:
      RequestPermissionFromBrowser(std::move(async_updated_callback));
      return INJECTION_WAITING;  // Wait around for permission.
    case PermissionsData::PageAccess::kAllowed:
      InjectionResult result = Inject(scripts_run_info);
      // If the injection is blocked, we need to set the manager so we can
      // notify it upon completion.
      if (result == INJECTION_BLOCKED)
        async_completion_callback_ = std::move(async_updated_callback);
      return result;
  }

  NOTREACHED_IN_MIGRATION();
  return INJECTION_FINISHED;
}

ScriptInjection::InjectionResult ScriptInjection::OnPermissionGranted(
    ScriptsRunInfo* scripts_run_info) {
  if (!injection_host_) {
    NotifyWillNotInject(ScriptInjector::EXTENSION_REMOVED);
    return INJECTION_FINISHED;
  }

  return Inject(scripts_run_info);
}

void ScriptInjection::OnHostRemoved() {
  injection_host_.reset(nullptr);
}

void ScriptInjection::RequestPermissionFromBrowser(
    StatusUpdatedCallback async_updated_callback) {
  // If we are just notifying the browser of the injection, then send an
  // invalid request (which is treated like a notification).
  request_id_ = g_next_pending_id++;
  ExtensionFrameHelper::Get(render_frame_)
      ->GetLocalFrameHost()
      ->RequestScriptInjectionPermission(
          host_id().id, injector_->script_type(), run_location_,
          base::BindOnce(&ScriptInjection::HandlePermission,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(async_updated_callback)));
}

void ScriptInjection::NotifyWillNotInject(
    ScriptInjector::InjectFailureReason reason) {
  complete_ = true;
  injector_->OnWillNotInject(reason);
}

void ScriptInjection::HandlePermission(
    StatusUpdatedCallback async_updated_callback,
    bool granted) {
  if (!granted)
    return;
  std::move(async_updated_callback).Run(InjectionStatus::kPermitted, this);
}

ScriptInjection::InjectionResult ScriptInjection::Inject(
    ScriptsRunInfo* scripts_run_info) {
  DCHECK(injection_host_);
  DCHECK(scripts_run_info);
  DCHECK(!complete_);
  bool should_inject_js = injector_->ShouldInjectJs(
      run_location_, scripts_run_info->executing_scripts[host_id().id]);
  bool should_inject_or_remove_css = injector_->ShouldInjectOrRemoveCss(
      run_location_, scripts_run_info->injected_stylesheets[host_id().id]);

  // This can happen if the extension specified a script to
  // be run in multiple rules, and the script has already run.
  // See crbug.com/631247.
  if (!should_inject_js && !should_inject_or_remove_css) {
    return INJECTION_FINISHED;
  }

  if (should_inject_js)
    InjectJs(&(scripts_run_info->executing_scripts[host_id().id]),
             &(scripts_run_info->num_js));
  if (should_inject_or_remove_css)
    InjectOrRemoveCss(&(scripts_run_info->injected_stylesheets[host_id().id]),
                      &(scripts_run_info->num_css));

  complete_ = did_inject_js_ || !should_inject_js;

  if (complete_) {
    injector_->OnInjectionComplete(std::move(execution_result_), run_location_);
  } else {
    ++scripts_run_info->num_blocking_js;
  }

  return complete_ ? INJECTION_FINISHED : INJECTION_BLOCKED;
}

void ScriptInjection::InjectJs(std::set<std::string>* executing_scripts,
                               size_t* num_injected_js_scripts) {
  TRACE_RENDERER_EXTENSION_EVENT("ScriptInjection::InjectJs", host_id().id);

  DCHECK(!did_inject_js_);
  std::vector<blink::WebScriptSource> sources = injector_->GetJsSources(
      run_location_, executing_scripts, num_injected_js_scripts);
  DCHECK(!sources.empty());

  base::ElapsedTimer exec_timer;

  // For content scripts executing during page load, we run them asynchronously
  // in order to reduce UI jank experienced by the user. (We don't do this for
  // kDocumentStart scripts, because there's no UI to jank until after those
  // run, so we run them as soon as we can.)
  // Note: We could potentially also run deferred and browser-driven scripts
  // asynchronously; however, these are rare enough that there probably isn't
  // UI jank. If this changes, we can update this.
  bool should_execute_asynchronously =
      injector_->script_type() == mojom::InjectionType::kContentScript &&
      (run_location_ == mojom::RunLocation::kDocumentEnd ||
       run_location_ == mojom::RunLocation::kDocumentIdle);
  blink::mojom::EvaluationTiming execution_option =
      should_execute_asynchronously
          ? blink::mojom::EvaluationTiming::kAsynchronous
          : blink::mojom::EvaluationTiming::kSynchronous;

  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame_);
  CHECK(frame_helper);

  std::optional<std::string> world_id = injector_->GetExecutionWorldId();
  const std::string& host_string_id = injection_host_->id().id;
  const mojom::ExecutionWorld execution_world = injector_->GetExecutionWorld();

  // We limit the number of user script worlds that may be active on a given
  // document. Check if this is within bounds.
  if (execution_world == mojom::ExecutionWorld::kUserScript) {
    const std::set<std::optional<std::string>>* active_user_script_worlds =
        frame_helper->GetActiveUserScriptWorlds(host_string_id);

    // TODO(devlin): It'd be nice to isolate this logic into
    // IsolatedWorldManager instead of having it shared with
    // ExtensionFrameHelper and this class, but ExtensionFrameHelper is the
    // one that's able to track this information (as a per-frame object that
    // can be cleared on a new document). If we had something like
    // DocumentUserData on the renderer, that would be a better fit.
    constexpr size_t kMaxActiveUserScriptWorldCount = 10;
    if (active_user_script_worlds &&
        active_user_script_worlds->size() >= kMaxActiveUserScriptWorldCount &&
        !base::Contains(*active_user_script_worlds, world_id)) {
      // If there are 10 or more active user script worlds, we use the default
      // world for future injections.
      // Note: This *can* mean that up to 11 user script worlds for this
      // exist on the document, since the first ten can correspond to "named"
      // worlds and then we'll create a new one for the default world. However,
      // that's better than needing to choose a new world "at random" to inject
      // in.
      world_id = std::nullopt;
    }

    // Register the world as active. This is a no-op if it's already registered.
    frame_helper->AddActiveUserScriptWorld(host_string_id, world_id);
  }

  int32_t blink_world_id = blink::kMainDOMWorldId;
  switch (execution_world) {
    case mojom::ExecutionWorld::kIsolated:
    case mojom::ExecutionWorld::kUserScript:
      blink_world_id =
          IsolatedWorldManager::GetInstance().GetOrCreateIsolatedWorldForHost(
              *injection_host_, execution_world, world_id);
      if (injection_host_->id().type == mojom::HostID::HostType::kExtensions &&
          log_activity_) {
        DOMActivityLogger::AttachToWorld(blink_world_id, host_string_id);
      }

      break;
    case mojom::ExecutionWorld::kMain:
      blink_world_id = blink::kMainDOMWorldId;
      break;
  }

  render_frame_->GetWebFrame()->RequestExecuteScript(
      blink_world_id, sources, injector_->IsUserGesture(), execution_option,
      blink::mojom::LoadEventBlockingOption::kBlock,
      base::BindOnce(&ScriptInjection::OnJsInjectionCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      blink::BackForwardCacheAware::kPossiblyDisallow,
      injector_->ExpectsResults(), injector_->ShouldWaitForPromise());
}

void ScriptInjection::OnJsInjectionCompleted(std::optional<base::Value> value,
                                             base::TimeTicks start_time) {
  DCHECK(!did_inject_js_);

  base::TimeTicks timestamp(base::TimeTicks::Now());
  std::optional<base::TimeDelta> elapsed;
  // If the script will never execute (such as if the context is destroyed),
  // `start_time` is null. Only log a time for execution if the script, in fact,
  // executed.
  if (!start_time.is_null())
    elapsed = timestamp - start_time;

  if (injection_host_->id().type == mojom::HostID::HostType::kExtensions &&
      elapsed) {
    UMA_HISTOGRAM_TIMES("Extensions.InjectedScriptExecutionTime", *elapsed);
    switch (run_location_) {
      case mojom::RunLocation::kDocumentStart:
        UMA_HISTOGRAM_TIMES(
            "Extensions.InjectedScriptExecutionTime.DocumentStart", *elapsed);
        break;
      case mojom::RunLocation::kDocumentEnd:
        UMA_HISTOGRAM_TIMES(
            "Extensions.InjectedScriptExecutionTime.DocumentEnd", *elapsed);
        break;
      case mojom::RunLocation::kDocumentIdle:
        UMA_HISTOGRAM_TIMES(
            "Extensions.InjectedScriptExecutionTime.DocumentIdle", *elapsed);
        break;
      default:
        break;
    }
  }

  execution_result_ = std::move(value);
  did_inject_js_ = true;

  // If |async_completion_callback_| is set, it means the script finished
  // asynchronously, and we should run it.
  if (!async_completion_callback_.is_null()) {
    complete_ = true;
    injector_->OnInjectionComplete(std::move(execution_result_), run_location_);
    // Warning: this object can be destroyed after this line!
    std::move(async_completion_callback_).Run(InjectionStatus::kFinished, this);
  }
}

void ScriptInjection::InjectOrRemoveCss(
    std::set<std::string>* injected_stylesheets,
    size_t* num_injected_stylesheets) {
  std::vector<ScriptInjector::CSSSource> css_sources = injector_->GetCssSources(
      run_location_, injected_stylesheets, num_injected_stylesheets);
  blink::WebLocalFrame* web_frame = render_frame_->GetWebFrame();

  auto blink_css_origin = blink::WebCssOrigin::kAuthor;
  switch (injector_->GetCssOrigin()) {
    case mojom::CSSOrigin::kUser:
      blink_css_origin = blink::WebCssOrigin::kUser;
      break;
    case mojom::CSSOrigin::kAuthor:
      blink_css_origin = blink::WebCssOrigin::kAuthor;
      break;
  }

  mojom::CSSInjection::Operation operation =
      injector_->GetCSSInjectionOperation();
  for (const auto& source : css_sources) {
    switch (operation) {
      case mojom::CSSInjection::Operation::kRemove:
        DCHECK(!source.key.IsEmpty())
            << "An injection key is required to remove CSS.";
        // CSS deletion can be thought of as the inverse of CSS injection
        // (i.e. x - y = x + -y and x | y = ~(~x & ~y)), so it is handled here
        // in the injection function.
        //
        // TODO(crbug.com/40144586): Extend this API's capabilities to
        // also remove CSS added by content scripts?
        web_frame->GetDocument().RemoveInsertedStyleSheet(source.key,
                                                          blink_css_origin);
        break;
      case mojom::CSSInjection::Operation::kAdd:
        web_frame->GetDocument().InsertStyleSheet(
            source.code, &source.key, blink_css_origin,
            blink::BackForwardCacheAware::kPossiblyDisallow);
        break;
    }
  }
}

}  // namespace extensions
