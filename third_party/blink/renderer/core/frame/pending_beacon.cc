// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"
#include "third_party/blink/renderer/core/loader/beacon_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

// Internally enforces a time limit to send out pending beacons when using
// background timeout.
//
// When the page is in hidden state, beacons will be sent out no later than
// min(Time evicted from back/forward cache,
//     `kDefaultPendingBeaconMaxBackgroundTimeout`).
// Note that this is currently longer than back/forward cache entry's TTL.
// See https://github.com/WICG/pending-beacon/issues/3
constexpr base::TimeDelta kDefaultPendingBeaconMaxBackgroundTimeout =
    base::Minutes(30);  // 30 minutes

// Returns a max possible background timeout for every pending beacon.
base::TimeDelta GetMaxBackgroundTimeout() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      features::kPendingBeaconAPI, "PendingBeaconMaxBackgroundTimeoutInMs",
      base::checked_cast<int32_t>(
          kDefaultPendingBeaconMaxBackgroundTimeout.InMilliseconds())));
}

bool ShouldBlockURL(const KURL& url, ExceptionState* exception_state) {
  if (!url.IsValid()) {
    if (exception_state) {
      exception_state->ThrowTypeError(
          "The URL argument is ill-formed or unsupported.");
    }
    return true;
  }

  if (!url.Protocol().empty() && !url.ProtocolIs(WTF::g_https_atom)) {
    if (exception_state) {
      exception_state->ThrowTypeError(
          "PendingBeacons are only supported over HTTPS.");
    }
    return true;
  }

  return false;
}

}  // namespace

// static
bool PendingBeacon::CanSendBeacon(const String& url,
                                  const ExecutionContext& ec,
                                  ExceptionState& exception_state) {
  return !ShouldBlockURL(ec.CompleteURL(url), &exception_state);
}

PendingBeacon::PendingBeacon(ExecutionContext* ec,
                             const String& url,
                             const String& method,
                             int32_t background_timeout,
                             int32_t timeout)
    : ExecutionContextLifecycleObserver(ec),
      ec_(ec),
      remote_(ec),
      url_(url),
      method_(method),
      background_timeout_(base::Milliseconds(background_timeout)),
      timeout_timer_(GetTaskRunner(), this, &PendingBeacon::TimeoutTimerFired) {
  KURL host_url = ec_->CompleteURL(url);
  // The caller, i.e. JavaScript factory method `Create()`, must ensure `url`
  // is valid.
  CHECK(!ShouldBlockURL(host_url, nullptr));

  // Creates a corresponding instance of PendingBeacon in the browser process
  // and binds `remote_` to it.
  mojom::blink::BeaconMethod host_method;
  if (method == http_names::kGET) {
    host_method = mojom::blink::BeaconMethod::kGet;
  } else {
    host_method = mojom::blink::BeaconMethod::kPost;
  }

  mojo::PendingReceiver<mojom::blink::PendingBeacon> beacon_receiver =
      remote_.BindNewPipeAndPassReceiver(GetTaskRunner());

  PendingBeaconDispatcher& dispatcher =
      PendingBeaconDispatcher::FromOrAttachTo(*ec_);
  dispatcher.CreateHostBeacon(this, std::move(beacon_receiver), host_url,
                              host_method);
  // May trigger beacon sending immediately.
  setTimeout(timeout);
}

void PendingBeacon::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(ec_);
  visitor->Trace(remote_);
  visitor->Trace(timeout_timer_);
}

void PendingBeacon::deactivate() {
  if (pending_) {
    remote_->Deactivate();
    pending_ = false;

    UnregisterFromDispatcher();
  }
}

void PendingBeacon::sendNow() {
  if (pending_) {
    remote_->SendNow();
    pending_ = false;

    UnregisterFromDispatcher();
  }
}

void PendingBeacon::setBackgroundTimeout(int32_t background_timeout) {
  background_timeout_ = base::Milliseconds(background_timeout);
}

void PendingBeacon::setTimeout(int32_t timeout) {
  timeout_ = base::Milliseconds(timeout);
  if (timeout_.is_negative() || !pending_) {
    return;
  }

  // TODO(crbug.com/3774273): Use the nullity of data & url to decide whether
  // beacon should be sent.
  // https://github.com/WICG/pending-beacon/issues/17#issuecomment-1198871880

  // If timeout >= 0, the timer starts immediately after its value is set or
  // updated.
  // https://github.com/WICG/pending-beacon/blob/main/README.md#properties
  timeout_timer_.StartOneShot(timeout_, FROM_HERE);
}

void PendingBeacon::SetURLInternal(const String& url,
                                   ExceptionState& exception_state) {
  KURL host_url = ec_->CompleteURL(url);
  if (ShouldBlockURL(host_url, &exception_state)) {
    return;
  }

  url_ = url;
  remote_->SetRequestURL(host_url);
}

void PendingBeacon::SetDataInternal(const BeaconData& data,
                                    ExceptionState& exception_state) {
  ResourceRequest request;

  data.Serialize(request);
  // `WrappedResourceRequest` only works for POST request.
  request.SetHttpMethod(http_names::kPOST);
  scoped_refptr<network::ResourceRequestBody> request_body =
      GetRequestBodyForWebURLRequest(WrappedResourceRequest(request));
  // TODO(crbug.com/1293679): Support multi-parts request.
  // Current implementation in browser only supports sending single request with
  // single DataElement.
  if (request_body->elements()->size() > 1) {
    exception_state.ThrowRangeError(
        "PendingBeacon only supports single part data.");
    return;
  }

  AtomicString content_type = request.HttpContentType();
  remote_->SetRequestData(std::move(request_body),
                          content_type.IsNull() ? "" : content_type);
}

base::TimeDelta PendingBeacon::GetBackgroundTimeout() const {
  const auto max_background_timeout = GetMaxBackgroundTimeout();
  return (background_timeout_.is_negative() ||
          background_timeout_ > max_background_timeout)
             ? max_background_timeout
             : background_timeout_;
}

void PendingBeacon::Send() {
  sendNow();
}

scoped_refptr<base::SingleThreadTaskRunner> PendingBeacon::GetTaskRunner() {
  return GetExecutionContext()->GetTaskRunner(
      PendingBeaconDispatcher::kTaskType);
}

void PendingBeacon::TimeoutTimerFired(TimerBase*) {
  sendNow();
}

void PendingBeacon::ContextDestroyed() {
  // Updates state to disallow any subsequent actions.
  pending_ = false;
  // Cancels timer task when the Document is destroyed.
  // The browser will take over the responsibility.
  timeout_timer_.Stop();
}

}  // namespace blink
