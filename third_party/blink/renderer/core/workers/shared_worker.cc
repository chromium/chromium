/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/workers/shared_worker.h"

#include <optional>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shared_worker_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_sharedworkeroptions_string.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/shared_worker_client_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

void RecordSharedWorkerUsage(LocalDOMWindow* window) {
  UseCounter::Count(window, WebFeature::kSharedWorkerStart);

  if (window->IsCrossSiteSubframe())
    UseCounter::Count(window, WebFeature::kThirdPartySharedWorker);
}

}  // namespace

SharedWorker::SharedWorker(ExecutionContext* context)
    : AbstractWorker(context),
      ActiveScriptWrappable<SharedWorker>({}),
      is_being_connected_(false),
      feature_handle_for_scheduler_(context->GetScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kSharedWorker,
          {SchedulingPolicy::DisableBackForwardCache()})) {}

SharedWorker* SharedWorker::Create(
    ExecutionContext* context,
    const String& url,
    const V8UnionSharedWorkerOptionsOrString* name_or_options,
    ExceptionState& exception_state) {
  return CreateImpl(context, url, name_or_options, exception_state,
                    &To<LocalDOMWindow>(context)->GetPublicURLManager(),
                    /*connector_override=*/nullptr);
}

SharedWorker* SharedWorker::Create(
    base::PassKey<StorageAccessHandle>,
    ExecutionContext* context,
    const String& url,
    const V8UnionSharedWorkerOptionsOrString* name_or_options,
    ExceptionState& exception_state,
    PublicURLManager* public_url_manager,
    const HeapMojoRemote<mojom::blink::SharedWorkerConnector>*
        connector_override) {
  return CreateImpl(context, url, name_or_options, exception_state,
                    public_url_manager, connector_override);
}

SharedWorker* SharedWorker::CreateImpl(
    ExecutionContext* context,
    const String& url,
    const V8UnionSharedWorkerOptionsOrString* name_or_options,
    ExceptionState& exception_state,
    PublicURLManager* public_url_manager,
    const HeapMojoRemote<mojom::blink::SharedWorkerConnector>*
        connector_override) {
  DCHECK(IsMainThread());

  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }

  // We don't currently support nested workers, so workers can only be created
  // from windows.
  LocalDOMWindow* window = To<LocalDOMWindow>(context);

  RecordSharedWorkerUsage(window);

  SharedWorker* worker = MakeGarbageCollected<SharedWorker>(context);
  worker->UpdateStateIfNeeded();

  auto* channel = MakeGarbageCollected<MessageChannel>(context);
  worker->port_ = channel->port1();
  MessagePortChannel remote_port = channel->port2()->Disentangle();

  if (!window->GetSecurityOrigin()->CanAccessSharedWorkers()) {
    exception_state.ThrowSecurityError(
        "Access to shared workers is denied to origin '" +
        window->GetSecurityOrigin()->ToString() + "'.");
    return nullptr;
  } else if (window->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(window, WebFeature::kFileAccessedSharedWorker);
  }

  KURL script_url = ResolveURL(context, url, exception_state);
  if (script_url.IsEmpty())
    return nullptr;

  mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token;
  if (script_url.ProtocolIs("blob")) {
    public_url_manager->Resolve(
        script_url, blob_url_token.InitWithNewPipeAndPassReceiver());
  }

  auto options = mojom::blink::WorkerOptions::New();
  // The same_site_cookies setting defaults to kAll for first-party contexts
  // (allowing access to SameSite Lax and String cookies) and kNone in
  // third-party contexts (allowing access to just SameSite None cookies).
  mojom::blink::SharedWorkerSameSiteCookies same_site_cookies =
      window->GetStorageKey().IsFirstPartyContext()
          ? mojom::blink::SharedWorkerSameSiteCookies::kAll
          : mojom::blink::SharedWorkerSameSiteCookies::kNone;
  switch (name_or_options->GetContentType()) {
    case V8UnionSharedWorkerOptionsOrString::ContentType::kString:
      options->name = name_or_options->GetAsString();
      break;
    case V8UnionSharedWorkerOptionsOrString::ContentType::
        kSharedWorkerOptions: {
      SharedWorkerOptions* worker_options =
          name_or_options->GetAsSharedWorkerOptions();
      options->name = worker_options->name();
      options->type =
          Script::V8WorkerTypeToScriptType(worker_options->type().AsEnum());
      options->credentials = Request::V8RequestCredentialsToCredentialsMode(
          worker_options->credentials().AsEnum());
      if (worker_options->hasSameSiteCookies()) {
        switch (worker_options->sameSiteCookies().AsEnum()) {
          case V8SharedWorkerSameSiteCookies::Enum::kAll:
            same_site_cookies = mojom::blink::SharedWorkerSameSiteCookies::kAll;
            if (window->GetStorageKey().IsThirdPartyContext()) {
              // Third-party contexts cannot request SameSite Strict or Lax
              // cookies so no worker can be returned.
              exception_state.ThrowSecurityError(
                  "SharedWorkers in third-party contexts cannot request "
                  "SameSite Strict or Lax cookies via the `sameSiteCookies: "
                  "\"all\"` option.");
              return nullptr;
            }
            break;
          case V8SharedWorkerSameSiteCookies::Enum::kNone:
            same_site_cookies =
                mojom::blink::SharedWorkerSameSiteCookies::kNone;
            if (window->GetStorageKey().IsFirstPartyContext()) {
              // We want to note when `none` is specifically requested in a
              // first-party context to gauge usage of this feature.
              UseCounter::Count(
                  window,
                  WebFeature::kFirstPartySharedWorkerSameSiteCookiesNone);
            }
            break;
        }
      }
      break;
    }
  }
  DCHECK(!options->name.IsNull());
  if (options->type == mojom::blink::ScriptType::kClassic)
    UseCounter::Count(window, WebFeature::kClassicSharedWorker);
  else if (options->type == mojom::blink::ScriptType::kModule)
    UseCounter::Count(window, WebFeature::kModuleSharedWorker);

  SharedWorkerClientHolder::From(*window)->Connect(
      worker, std::move(remote_port), script_url, std::move(blob_url_token),
      std::move(options), same_site_cookies, context->UkmSourceID(),
      connector_override);

  return worker;
}

SharedWorker::~SharedWorker() = default;

const AtomicString& SharedWorker::InterfaceName() const {
  return event_target_names::kSharedWorker;
}

bool SharedWorker::HasPendingActivity() const {
  return is_being_connected_;
}

void SharedWorker::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {}

void SharedWorker::Trace(Visitor* visitor) const {
  visitor->Trace(port_);
  AbstractWorker::Trace(visitor);
  Supplementable<SharedWorker>::Trace(visitor);
}

}  // namespace blink
