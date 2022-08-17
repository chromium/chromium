// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
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

PendingBeacon::PendingBeacon(ExecutionContext* ec,
                             const String& url,
                             const String& method,
                             int32_t background_timeout,
                             int32_t timeout)
    : ec_(ec),
      remote_(ec),
      url_(url),
      method_(method),
      background_timeout_(base::Milliseconds(background_timeout)),
      timeout_(base::Milliseconds(timeout)) {
  // Creates a corresponding instance of PendingBeacon in the browser process
  // and binds `remote_` to it.
  mojom::blink::BeaconMethod host_method;
  if (method == http_names::kGET) {
    host_method = mojom::blink::BeaconMethod::kGet;
  } else {
    host_method = mojom::blink::BeaconMethod::kPost;
  }

  auto task_runner = ec_->GetTaskRunner(PendingBeaconDispatcher::kTaskType);
  mojo::PendingReceiver<mojom::blink::PendingBeacon> beacon_receiver =
      remote_.BindNewPipeAndPassReceiver(task_runner);
  KURL host_url = ec_->CompleteURL(url);

  PendingBeaconDispatcher& dispatcher =
      PendingBeaconDispatcher::FromOrAttachTo(*ec_);
  dispatcher.CreateHostBeacon(std::move(beacon_receiver), host_url,
                              host_method);
}

void PendingBeacon::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(ec_);
  visitor->Trace(remote_);
}

void PendingBeacon::deactivate() {
  if (pending_) {
    remote_->Deactivate();
    pending_ = false;
  }
}

void PendingBeacon::sendNow() {
  if (pending_) {
    remote_->SendNow();
    pending_ = false;
  }
}

void PendingBeacon::setBackgroundTimeout(int32_t background_timeout) {
  background_timeout_ = base::Milliseconds(background_timeout);
}

void PendingBeacon::setTimeout(int32_t timeout) {
  timeout_ = base::Milliseconds(timeout);
}

void PendingBeacon::SetURLInternal(const String& url) {
  url_ = url;
  KURL host_url = ec_->CompleteURL(url);
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

}  // namespace blink
