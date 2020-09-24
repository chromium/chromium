// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webshare/navigator_share.h"

#include <stdint.h>
#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_share_data.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

namespace {

constexpr size_t kMaxSharedFileCount = 10;
constexpr uint32_t kMaxSharedFileBytes = 50U * 1024 * 1024;

// Gets the human-friendly error message for a ShareError. |error| must not be
// ShareError::OK.
String ErrorToString(mojom::blink::ShareError error) {
  switch (error) {
    case mojom::blink::ShareError::OK:
      NOTREACHED();
      break;
    case mojom::blink::ShareError::INTERNAL_ERROR:
      return "Share failed";
    case mojom::blink::ShareError::PERMISSION_DENIED:
      return "Permission denied";
    case mojom::blink::ShareError::CANCELED:
      return "Share canceled";
  }
  NOTREACHED();
  return String();
}

bool HasFiles(const ShareData& data) {
  if (!data.hasFiles())
    return false;

  return !data.files().IsEmpty();
}

// Returns true unless |share(data)| would reject with TypeError.
// Populates |url| with the result of running the URL parser on |data.url|.
// If the return value is false and |exception_state| is non null, throws
// TypeError.
//
// https://w3c.github.io/web-share/level-2/#canshare-method
// https://w3c.github.io/web-share/level-2/#share-method
bool CanShareInternal(const LocalDOMWindow& window,
                      const ShareData& data,
                      KURL& url,
                      ExceptionState* exception_state) {
  if (!data.hasTitle() && !data.hasText() && !data.hasUrl() &&
      !HasFiles(data)) {
    if (exception_state) {
      exception_state->ThrowTypeError(
          "No known share data fields supplied. If using only new fields "
          "(other than title, text and url), you must feature-detect "
          "them first.");
    }
    return false;
  }

  if (data.hasUrl()) {
    url = window.CompleteURL(data.url());
    if (!url.IsValid() ||
        (!url.ProtocolIsInHTTPFamily() &&
         url.Protocol() != window.document()->BaseURL().Protocol())) {
      if (exception_state) {
        exception_state->ThrowTypeError("Invalid URL");
      }
      return false;
    }
  }

  return true;
}

}  // namespace

class NavigatorShare::ShareClientImpl final
    : public GarbageCollected<ShareClientImpl> {
 public:
  ShareClientImpl(NavigatorShare*, bool has_files, ScriptPromiseResolver*);

  void Callback(mojom::blink::ShareError);

  void OnConnectionError();

  void Trace(Visitor* visitor) const {
    visitor->Trace(navigator_);
    visitor->Trace(resolver_);
  }

 private:
  WeakMember<NavigatorShare> navigator_;
  bool has_files_;
  Member<ScriptPromiseResolver> resolver_;
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

NavigatorShare::ShareClientImpl::ShareClientImpl(
    NavigatorShare* navigator_share,
    bool has_files,
    ScriptPromiseResolver* resolver)
    : navigator_(navigator_share),
      has_files_(has_files),
      resolver_(resolver),
      feature_handle_for_scheduler_(
          ExecutionContext::From(resolver_->GetScriptState())
              ->GetScheduler()
              ->RegisterFeature(
                  SchedulingPolicy::Feature::kWebShare,
                  {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {}

void NavigatorShare::ShareClientImpl::Callback(mojom::blink::ShareError error) {
  if (navigator_)
    navigator_->clients_.erase(this);

  if (error == mojom::blink::ShareError::OK) {
    UseCounter::Count(ExecutionContext::From(resolver_->GetScriptState()),
                      has_files_
                          ? WebFeature::kWebShareSuccessfulContainingFiles
                          : WebFeature::kWebShareSuccessfulWithoutFiles);
    resolver_->Resolve();
  } else {
    UseCounter::Count(ExecutionContext::From(resolver_->GetScriptState()),
                      has_files_
                          ? WebFeature::kWebShareUnsuccessfulContainingFiles
                          : WebFeature::kWebShareUnsuccessfulWithoutFiles);
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        (error == mojom::blink::ShareError::PERMISSION_DENIED)
            ? DOMExceptionCode::kNotAllowedError
            : DOMExceptionCode::kAbortError,
        ErrorToString(error)));
  }
}

void NavigatorShare::ShareClientImpl::OnConnectionError() {
  resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError,
      "Internal error: could not connect to Web Share interface."));
}

NavigatorShare& NavigatorShare::From(Navigator& navigator) {
  NavigatorShare* supplement =
      Supplement<Navigator>::From<NavigatorShare>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorShare>();
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

void NavigatorShare::Trace(Visitor* visitor) const {
  visitor->Trace(service_remote_);
  visitor->Trace(clients_);
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorShare::kSupplementName[] = "NavigatorShare";

bool NavigatorShare::canShare(ScriptState* script_state,
                              const ShareData* data) {
  if (!script_state->ContextIsValid())
    return false;
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  KURL unused_url;
  return CanShareInternal(*window, *data, unused_url, nullptr);
}

bool NavigatorShare::canShare(ScriptState* script_state,
                              Navigator& navigator,
                              const ShareData* data) {
  return From(navigator).canShare(script_state, data);
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    const ShareData* data,
                                    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kAbortError,
        "Internal error: window frame is missing (the navigator may be "
        "detached).");
    return ScriptPromise();
  }

  // The feature policy is currently not enforced.
  LocalDOMWindow* const window = LocalDOMWindow::From(script_state);
  window->CountUse(
      ExecutionContext::From(script_state)
              ->IsFeatureEnabled(mojom::blink::FeaturePolicyFeature::kWebShare)
          ? WebFeature::kWebSharePolicyAllow
          : WebFeature::kWebSharePolicyDisallow);

  if (!LocalFrame::ConsumeTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to perform a share request.");
    return ScriptPromise();
  }

  KURL url;
  if (!CanShareInternal(*window, *data, url, &exception_state)) {
    DCHECK(exception_state.HadException());
    return ScriptPromise();
  }

  if (!service_remote_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    window->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        service_remote_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    service_remote_.set_disconnect_handler(WTF::Bind(
        &NavigatorShare::OnConnectionError, WrapWeakPersistent(this)));
    DCHECK(service_remote_.is_bound());
  }

  bool has_files = HasFiles(*data);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ShareClientImpl* client =
      MakeGarbageCollected<ShareClientImpl>(this, has_files, resolver);
  clients_.insert(client);
  ScriptPromise promise = resolver->Promise();

  WTF::Vector<mojom::blink::SharedFilePtr> files;
  uint64_t total_bytes = 0;
  if (has_files) {
    files.ReserveInitialCapacity(data->files().size());
    for (const blink::Member<blink::File>& file : data->files()) {
      total_bytes += file->size();
      files.push_back(mojom::blink::SharedFile::New(file->name(),
                                                    file->GetBlobDataHandle()));
    }

    if (files.size() > kMaxSharedFileCount ||
        total_bytes > kMaxSharedFileBytes) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "Permission denied");
      return ScriptPromise();
    }
  }

  service_remote_->Share(
      data->hasTitle() ? data->title() : g_empty_string,
      data->hasText() ? data->text() : g_empty_string, url, std::move(files),
      WTF::Bind(&ShareClientImpl::Callback, WrapPersistent(client)));

  return promise;
}

ScriptPromise NavigatorShare::share(ScriptState* script_state,
                                    Navigator& navigator,
                                    const ShareData* data,
                                    ExceptionState& exception_state) {
  return From(navigator).share(script_state, data, exception_state);
}

void NavigatorShare::OnConnectionError() {
  for (auto& client : clients_) {
    client->OnConnectionError();
  }
  clients_.clear();
  service_remote_.reset();
}

}  // namespace blink
