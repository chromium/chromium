// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/renderer_resource_coordinator_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using performance_manager::mojom::blink::IframeAttributionData;
using performance_manager::mojom::blink::IframeAttributionDataPtr;
using performance_manager::mojom::blink::ProcessCoordinationUnit;
using performance_manager::mojom::blink::V8ContextDescription;
using performance_manager::mojom::blink::V8ContextDescriptionPtr;
using performance_manager::mojom::blink::V8ContextWorldType;

namespace WTF {

// Copies the data by move.
template <>
struct CrossThreadCopier<V8ContextDescriptionPtr>
    : public WTF::CrossThreadCopierByValuePassThrough<V8ContextDescriptionPtr> {
};

// Copies the data by move.
template <>
struct CrossThreadCopier<IframeAttributionDataPtr>
    : public WTF::CrossThreadCopierByValuePassThrough<
          IframeAttributionDataPtr> {};

// Copies the data using the copy constructor.
template <>
struct CrossThreadCopier<blink::V8ContextToken>
    : public WTF::CrossThreadCopierPassThrough<blink::V8ContextToken> {};

}  // namespace WTF

namespace blink {

namespace {

// Determines if the given stable world ID is an extension world ID.
// Extensions IDs are 32-character strings containing characters in the range of
// 'a' to 'p', inclusive.
// TODO(chrisha): Lift this somewhere public and common in components/extensions
// and reuse it from there.
bool IsExtensionStableWorldId(const String& stable_world_id) {
  if (stable_world_id.IsNull() || stable_world_id.empty())
    return false;
  if (stable_world_id.length() != 32)
    return false;
  for (unsigned i = 0; i < stable_world_id.length(); ++i) {
    if (stable_world_id[i] < 'a' || stable_world_id[i] > 'p')
      return false;
  }
  return true;
}

// Returns true if |owner| is an iframe, false otherwise.
// This will also return true for custom elements built on iframe, like
// <webview> and <guestview>. Since the renderer has no knowledge of these they
// must be filtered out on the browser side.
bool ShouldSendIframeNotificationsFor(const HTMLFrameOwnerElement& owner) {
  return owner.OwnerType() == FrameOwnerElementType::kIframe;
}

// If |frame| is a RemoteFrame with a local parent, returns the parent.
// Otherwise returns nullptr.
LocalFrame* GetLocalParentOfRemoteFrame(const Frame& frame) {
  if (IsA<RemoteFrame>(frame)) {
    if (Frame* parent = frame.Tree().Parent()) {
      return DynamicTo<LocalFrame>(parent);
    }
  }
  return nullptr;
}

IframeAttributionDataPtr AttributionDataForOwner(
    const HTMLFrameOwnerElement& owner) {
  auto attribution_data = IframeAttributionData::New();
  attribution_data->id = owner.FastGetAttribute(html_names::kIdAttr);
  attribution_data->src = owner.FastGetAttribute(html_names::kSrcAttr);
  return attribution_data;
}

}  // namespace

RendererResourceCoordinatorImpl::~RendererResourceCoordinatorImpl() = default;

// static
void RendererResourceCoordinatorImpl::MaybeInitialize() {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled())
    return;

  blink::Platform* platform = Platform::Current();
  DCHECK(IsMainThread());
  DCHECK(platform);

  mojo::PendingRemote<ProcessCoordinationUnit> remote;
  platform->GetBrowserInterfaceBroker()->GetInterface(
      remote.InitWithNewPipeAndPassReceiver());
  RendererResourceCoordinator::Set(
      new RendererResourceCoordinatorImpl(std::move(remote)));
}

void RendererResourceCoordinatorImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  DCHECK(service_);
  service_->SetMainThreadTaskLoadIsLow(main_thread_task_load_is_low);
}

void RendererResourceCoordinatorImpl::OnScriptStateCreated(
    ScriptState* script_state,
    ExecutionContext* execution_context) {
  DCHECK(script_state);
  DCHECK(service_);

  auto v8_desc = V8ContextDescription::New();
  v8_desc->token = script_state->GetToken();

  IframeAttributionDataPtr iframe_attribution_data;

  // Default the world name to being empty.

  auto& dom_wrapper = script_state->World();
  switch (dom_wrapper.GetWorldType()) {
    case DOMWrapperWorld::WorldType::kMain: {
      v8_desc->world_type = V8ContextWorldType::kMain;
    } break;
    case DOMWrapperWorld::WorldType::kIsolated: {
      auto stable_world_id = dom_wrapper.NonMainWorldStableId();
      if (IsExtensionStableWorldId(stable_world_id)) {
        v8_desc->world_type = V8ContextWorldType::kExtension;
        v8_desc->world_name = stable_world_id;
      } else {
        v8_desc->world_type = V8ContextWorldType::kIsolated;
        v8_desc->world_name = dom_wrapper.NonMainWorldHumanReadableName();
      }
    } break;
    case DOMWrapperWorld::WorldType::kInspectorIsolated: {
      v8_desc->world_type = V8ContextWorldType::kInspector;
    } break;
    case DOMWrapperWorld::WorldType::kRegExp: {
      v8_desc->world_type = V8ContextWorldType::kRegExp;
    } break;
    case DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain: {
      // This should not happen in the production browser.
      NOTREACHED_IN_MIGRATION();
    } break;
    case DOMWrapperWorld::WorldType::kWorkerOrWorklet: {
      v8_desc->world_type = V8ContextWorldType::kWorkerOrWorklet;
    } break;
    case DOMWrapperWorld::WorldType::kShadowRealm: {
      v8_desc->world_type = V8ContextWorldType::kShadowRealm;
    } break;
  }

  if (execution_context) {
    // This should never happen for a regexp world.
    DCHECK_NE(DOMWrapperWorld::WorldType::kRegExp, dom_wrapper.GetWorldType());

    v8_desc->execution_context_token =
        execution_context->GetExecutionContextToken();

    // Only report the iframe data alongside the main world.
    // If this is the main world (so also a LocalDOMWindow) ...
    if (v8_desc->world_type == V8ContextWorldType::kMain) {
      auto* local_dom_window = To<LocalDOMWindow>(execution_context);
      // ... with a parent ...
      auto* local_frame = local_dom_window->GetFrame();
      DCHECK(local_frame);
      if (auto* parent_frame = local_frame->Parent()) {
        // ... that is also local ...
        if (IsA<LocalFrame>(parent_frame)) {
          // ... then we want to grab the iframe data associated with this
          // frame.
          auto* owner = To<HTMLFrameOwnerElement>(local_frame->Owner());
          DCHECK(owner);
          iframe_attribution_data = AttributionDataForOwner(*owner);
        }
      }
    }
  }

  DispatchOnV8ContextCreated(std::move(v8_desc),
                             std::move(iframe_attribution_data));
}

void RendererResourceCoordinatorImpl::OnScriptStateDetached(
    ScriptState* script_state) {
  DCHECK(script_state);
  DispatchOnV8ContextDetached(script_state->GetToken());
}

void RendererResourceCoordinatorImpl::OnScriptStateDestroyed(
    ScriptState* script_state) {
  DCHECK(script_state);
  DispatchOnV8ContextDestroyed(script_state->GetToken());
}

void RendererResourceCoordinatorImpl::OnBeforeContentFrameAttached(
    const Frame& frame,
    const HTMLFrameOwnerElement& owner) {
  DCHECK(service_);
  if (!ShouldSendIframeNotificationsFor(owner))
    return;
  LocalFrame* parent = GetLocalParentOfRemoteFrame(frame);
  if (!parent)
    return;
  service_->OnRemoteIframeAttached(
      parent->GetLocalFrameToken(),
      frame.GetFrameToken().GetAs<RemoteFrameToken>(),
      AttributionDataForOwner(owner));
}

void RendererResourceCoordinatorImpl::OnBeforeContentFrameDetached(
    const Frame& frame,
    const HTMLFrameOwnerElement& owner) {
  DCHECK(service_);
  if (!ShouldSendIframeNotificationsFor(owner))
    return;
  LocalFrame* parent = GetLocalParentOfRemoteFrame(frame);
  if (!parent)
    return;
  service_->OnRemoteIframeDetached(
      parent->GetLocalFrameToken(),
      frame.GetFrameToken().GetAs<RemoteFrameToken>());
}

RendererResourceCoordinatorImpl::RendererResourceCoordinatorImpl(
    mojo::PendingRemote<ProcessCoordinationUnit> remote) {
  service_task_runner_ =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
  service_.Bind(std::move(remote));
}

void RendererResourceCoordinatorImpl::DispatchOnV8ContextCreated(
    V8ContextDescriptionPtr v8_desc,
    IframeAttributionDataPtr iframe_attribution_data) {
  DCHECK(service_);
  // Calls to this can arrive on any thread (due to workers, etc), but the
  // interface itself is bound to the main thread. In this case, once we've
  // collated the necessary data we bounce over to the main thread. Note that
  // posting "this" unretained is safe because the renderer resource coordinator
  // is a singleton that leaks at process shutdown.

  if (!service_task_runner_->RunsTasksInCurrentSequence()) {
    blink::PostCrossThreadTask(
        *service_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RendererResourceCoordinatorImpl::DispatchOnV8ContextCreated,
            WTF::CrossThreadUnretained(this), std::move(v8_desc),
            std::move(iframe_attribution_data)));
  } else {
    service_->OnV8ContextCreated(std::move(v8_desc),
                                 std::move(iframe_attribution_data));
  }
}

void RendererResourceCoordinatorImpl::DispatchOnV8ContextDetached(
    const blink::V8ContextToken& token) {
  DCHECK(service_);
  // See DispatchOnV8ContextCreated for why this is both needed and safe.
  if (!service_task_runner_->RunsTasksInCurrentSequence()) {
    blink::PostCrossThreadTask(
        *service_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RendererResourceCoordinatorImpl::DispatchOnV8ContextDetached,
            WTF::CrossThreadUnretained(this), token));
  } else {
    service_->OnV8ContextDetached(token);
  }
}
void RendererResourceCoordinatorImpl::DispatchOnV8ContextDestroyed(
    const blink::V8ContextToken& token) {
  DCHECK(service_);
  // See DispatchOnV8ContextCreated for why this is both needed and safe.
  if (!service_task_runner_->RunsTasksInCurrentSequence()) {
    blink::PostCrossThreadTask(
        *service_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(
            &RendererResourceCoordinatorImpl::DispatchOnV8ContextDestroyed,
            WTF::CrossThreadUnretained(this), token));
  } else {
    service_->OnV8ContextDestroyed(token);
  }
}

}  // namespace blink
