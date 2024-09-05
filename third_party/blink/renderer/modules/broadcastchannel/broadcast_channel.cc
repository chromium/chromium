// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// To ensure proper ordering of messages sent to/from multiple BroadcastChannel
// instances in the same thread, this uses one BroadcastChannelProvider
// connection as basis for all connections to channels from the same thread. The
// actual connections used to send/receive messages are then created using
// associated interfaces, ensuring proper message ordering. Note that this
// approach only works in the case of workers, since each worker has it's own
// thread.
mojo::Remote<mojom::blink::BroadcastChannelProvider>&
GetWorkerThreadSpecificProvider(WorkerGlobalScope& worker_global_scope) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojo::Remote<mojom::blink::BroadcastChannelProvider>>,
      provider, ());
  if (!provider.IsSet()) {
    worker_global_scope.GetBrowserInterfaceBroker().GetInterface(
        provider->BindNewPipeAndPassReceiver());
  }
  return *provider;
}

}  // namespace

// static
BroadcastChannel* BroadcastChannel::Create(ExecutionContext* execution_context,
                                           const String& name,
                                           ExceptionState& exception_state) {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (window && window->IsCrossSiteSubframe())
    UseCounter::Count(window, WebFeature::kThirdPartyBroadcastChannel);

  return MakeGarbageCollected<BroadcastChannel>(execution_context, name);
}

BroadcastChannel::~BroadcastChannel() = default;

void BroadcastChannel::Dispose() {
  CloseInternal();
}

void BroadcastChannel::postMessage(const ScriptValue& message,
                                   ExceptionState& exception_state) {
  // If the receiver is not bound because `close` was called on this
  // BroadcastChannel instance, raise an exception per the spec. Otherwise,
  // in cases like the instance being created in an iframe that is now detached,
  // just ignore the postMessage call.
  if (!receiver_.is_bound()) {
    if (explicitly_closed_) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Channel is closed");
    }
    return;
  }

  // Silently ignore the postMessage call if this BroadcastChannel instance is
  // associated with a closing worker. This case needs to be handled explicitly
  // because the mojo connection to the worker won't be torn down until the
  // worker actually goes away.
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context->IsWorkerGlobalScope()) {
    WorkerGlobalScope* worker_global_scope =
        DynamicTo<WorkerGlobalScope>(execution_context);
    DCHECK(worker_global_scope);
    if (worker_global_scope->IsClosing()) {
      return;
    }
  }

  scoped_refptr<SerializedScriptValue> value = SerializedScriptValue::Serialize(
      message.GetIsolate(), message.V8Value(),
      SerializedScriptValue::SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return;

  // Defer postMessage() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-broadcast-channel
  if (execution_context->IsWindow()) {
    Document* document = To<LocalDOMWindow>(execution_context)->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(
          WTF::BindOnce(&BroadcastChannel::PostMessageInternal,
                        WrapWeakPersistent(this), std::move(value),
                        execution_context->GetSecurityOrigin()->IsolatedCopy(),
                        execution_context->GetAgentClusterID()));
      return;
    }
  }

  PostMessageInternal(std::move(value),
                      execution_context->GetSecurityOrigin()->IsolatedCopy(),
                      execution_context->GetAgentClusterID());
}

void BroadcastChannel::PostMessageInternal(
    scoped_refptr<SerializedScriptValue> value,
    scoped_refptr<SecurityOrigin> sender_origin,
    const base::UnguessableToken sender_agent_cluster_id) {
  if (!receiver_.is_bound())
    return;
  BlinkCloneableMessage msg;
  msg.message = std::move(value);
  msg.sender_origin = std::move(sender_origin);
  msg.sender_agent_cluster_id = sender_agent_cluster_id;
  msg.locked_to_sender_agent_cluster = msg.message->IsLockedToAgentCluster();
  remote_client_->OnMessage(std::move(msg));
}

void BroadcastChannel::close() {
  explicitly_closed_ = true;
  CloseInternal();
}

void BroadcastChannel::CloseInternal() {
  remote_client_.reset();
  if (receiver_.is_bound())
    receiver_.reset();
  if (associated_remote_.is_bound())
    associated_remote_.reset();
  feature_handle_for_scheduler_.reset();
}

const AtomicString& BroadcastChannel::InterfaceName() const {
  return event_target_names::kBroadcastChannel;
}

bool BroadcastChannel::HasPendingActivity() const {
  return receiver_.is_bound() && HasEventListeners(event_type_names::kMessage);
}

void BroadcastChannel::ContextDestroyed() {
  CloseInternal();
}

void BroadcastChannel::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
  visitor->Trace(receiver_);
  visitor->Trace(remote_client_);
  visitor->Trace(associated_remote_);
}

void BroadcastChannel::OnMessage(BlinkCloneableMessage message) {
  auto* context = GetExecutionContext();

  // Queue a task to dispatch the event.
  MessageEvent* event;
  if ((!message.locked_to_sender_agent_cluster ||
       context->IsSameAgentCluster(message.sender_agent_cluster_id)) &&
      message.message->CanDeserializeIn(context)) {
    event = MessageEvent::Create(nullptr, std::move(message.message),
                                 context->GetSecurityOrigin()->ToString());
  } else {
    event = MessageEvent::CreateError(context->GetSecurityOrigin()->ToString());
  }

  if (base::FeatureList::IsEnabled(features::kBFCacheOpenBroadcastChannel) &&
      context->is_in_back_forward_cache()) {
    LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
    CHECK(window);
    if (LocalFrame* frame = window->GetFrame()) {
      base::UmaHistogramEnumeration(
          "BackForwardCache.Eviction.Renderer",
          mojom::blink::RendererEvictionReason::kBroadcastChannelOnMessage);
      // We don't need to report the source location of a broadcast channel.
      frame->GetBackForwardCacheControllerHostRemote()
          .EvictFromBackForwardCache(
              mojom::blink::RendererEvictionReason::kBroadcastChannelOnMessage,
              /*source=*/nullptr);
    }
    return;
  }
  // <specdef
  // href="https://html.spec.whatwg.org/multipage/web-messaging.html#dom-broadcastchannel-postmessage">
  // <spec>The tasks must use the DOM manipulation task source, and, for
  // those where the event loop specified by the target BroadcastChannel
  // object's BroadcastChannel settings object is a window event loop,
  // must be associated with the responsible document specified by that
  // target BroadcastChannel object's BroadcastChannel settings object.
  // </spec>
  DispatchEvent(*event);
}

void BroadcastChannel::OnError() {
  CloseInternal();
}

BroadcastChannel::BroadcastChannel(ExecutionContext* execution_context,
                                   const String& name)
    : BroadcastChannel(execution_context,
                       name,
                       mojo::NullAssociatedReceiver(),
                       mojo::NullAssociatedRemote()) {}

BroadcastChannel::BroadcastChannel(
    base::PassKey<StorageAccessHandle>,
    ExecutionContext* execution_context,
    const String& name,
    mojom::blink::BroadcastChannelProvider* provider)
    : ActiveScriptWrappable<BroadcastChannel>({}),
      ExecutionContextLifecycleObserver(execution_context),
      name_(name),
      receiver_(this, execution_context),
      remote_client_(execution_context),
      associated_remote_(execution_context) {
  if (!base::FeatureList::IsEnabled(features::kBFCacheOpenBroadcastChannel)) {
    feature_handle_for_scheduler_ =
        execution_context->GetScheduler()->RegisterFeature(
            SchedulingPolicy::Feature::kBroadcastChannel,
            {SchedulingPolicy::DisableBackForwardCache()});
  }
  provider->ConnectToChannel(
      name_,
      receiver_.BindNewEndpointAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kInternalDefault)),
      remote_client_.BindNewEndpointAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kInternalDefault)));
  SetupDisconnectHandlers();
}

BroadcastChannel::BroadcastChannel(
    base::PassKey<BroadcastChannelTester>,
    ExecutionContext* execution_context,
    const String& name,
    mojo::PendingAssociatedReceiver<mojom::blink::BroadcastChannelClient>
        receiver,
    mojo::PendingAssociatedRemote<mojom::blink::BroadcastChannelClient> remote)
    : BroadcastChannel(execution_context,
                       name,
                       std::move(receiver),
                       std::move(remote)) {}

BroadcastChannel::BroadcastChannel(
    ExecutionContext* execution_context,
    const String& name,
    mojo::PendingAssociatedReceiver<mojom::blink::BroadcastChannelClient>
        receiver,
    mojo::PendingAssociatedRemote<mojom::blink::BroadcastChannelClient> remote)
    : ActiveScriptWrappable<BroadcastChannel>({}),
      ExecutionContextLifecycleObserver(execution_context),
      name_(name),
      receiver_(this, execution_context),
      remote_client_(execution_context),
      associated_remote_(execution_context) {
  if (!base::FeatureList::IsEnabled(features::kBFCacheOpenBroadcastChannel)) {
    feature_handle_for_scheduler_ =
        execution_context->GetScheduler()->RegisterFeature(
            SchedulingPolicy::Feature::kBroadcastChannel,
            {SchedulingPolicy::DisableBackForwardCache()});
  }
  // Note: We cannot associate per-frame task runner here, but postTask
  //       to it manually via EnqueueEvent, since the current expectation
  //       is to receive messages even after close for which queued before
  //       close.
  //       https://github.com/whatwg/html/issues/1319
  //       Relying on Mojo binding will cancel the enqueued messages
  //       at close().

  // The BroadcastChannel spec indicates that messages should be delivered to
  // BroadcastChannel objects in the order in which they were created, so it's
  // important that the ordering of ConnectToChannel messages (used to create
  // the corresponding state in the browser process) is preserved. We accomplish
  // this using two approaches, depending on the context:
  //
  //  - In the frame case, we create a new navigation associated remote for each
  //    BroadcastChannel instance and leverage it to ensure in-order delivery
  //    and delivery to the RenderFrameHostImpl object that corresponds to the
  //    current frame.
  //
  //  - In the worker case, since each worker runs in its own thread, we use a
  //    shared remote for all BroadcastChannel objects created on that thread to
  //    ensure in-order delivery of messages to the appropriate *WorkerHost
  //    object.
  auto receiver_task_runner =
      execution_context->GetTaskRunner(TaskType::kInternalDefault);
  auto client_task_runner =
      execution_context->GetTaskRunner(TaskType::kInternalDefault);
  if (receiver.is_valid() && remote.is_valid()) {
    receiver_.Bind(std::move(receiver), receiver_task_runner);
    remote_client_.Bind(std::move(remote), client_task_runner);
  } else if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame)
      return;

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        associated_remote_.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kInternalDefault)));
    associated_remote_->ConnectToChannel(
        name_, receiver_.BindNewEndpointAndPassRemote(receiver_task_runner),
        remote_client_.BindNewEndpointAndPassReceiver(client_task_runner));
  } else if (auto* worker_global_scope =
                 DynamicTo<WorkerGlobalScope>(execution_context)) {
    if (worker_global_scope->IsClosing())
      return;

    mojo::Remote<mojom::blink::BroadcastChannelProvider>& provider =
        GetWorkerThreadSpecificProvider(*worker_global_scope);
    provider->ConnectToChannel(
        name_, receiver_.BindNewEndpointAndPassRemote(receiver_task_runner),
        remote_client_.BindNewEndpointAndPassReceiver(client_task_runner));
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  SetupDisconnectHandlers();
}

void BroadcastChannel::SetupDisconnectHandlers() {
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
  remote_client_.set_disconnect_handler(
      WTF::BindOnce(&BroadcastChannel::OnError, WrapWeakPersistent(this)));
}

bool BroadcastChannel::IsRemoteClientConnectedForTesting() const {
  return remote_client_.is_connected();
}

}  // namespace blink
