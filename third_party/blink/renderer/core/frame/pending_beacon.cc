// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request_util.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/beacon_data.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// Helper class to wrap a shared mojo remote to the frame's pending beacon host,
// such that all beacons can share the same remote to make calls to that host.
class PendingBeaconHostRemote
    : public GarbageCollected<PendingBeaconHostRemote>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];
  explicit PendingBeaconHostRemote(ExecutionContext& ec)
      : Supplement<ExecutionContext>(ec), remote_(&ec) {
    // Using the MiscPlatformAPI task type as pending beacons are not yet
    // associated with any specific task runner in the spec.
    auto task_runner = ec.GetTaskRunner(TaskType::kMiscPlatformAPI);

    mojo::PendingReceiver<mojom::blink::PendingBeaconHost> host_receiver =
        remote_.BindNewPipeAndPassReceiver(task_runner);
    ec.GetBrowserInterfaceBroker().GetInterface(std::move(host_receiver));
  }

  static PendingBeaconHostRemote& From(ExecutionContext& ec) {
    PendingBeaconHostRemote* remote =
        Supplement<ExecutionContext>::From<PendingBeaconHostRemote>(ec);
    if (!remote) {
      remote = MakeGarbageCollected<PendingBeaconHostRemote>(ec);
      ProvideTo(ec, remote);
    }
    return *remote;
  }

  void Trace(Visitor* visitor) const override {
    Supplement::Trace(visitor);
    visitor->Trace(remote_);
  }

  HeapMojoRemote<mojom::blink::PendingBeaconHost> remote_;
};

const char PendingBeaconHostRemote::kSupplementName[] =
    "PendingBeaconHostRemote";

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
  mojom::blink::BeaconMethod host_method;
  if (method == http_names::kGET) {
    host_method = mojom::blink::BeaconMethod::kGet;
  } else {
    host_method = mojom::blink::BeaconMethod::kPost;
  }

  // Using the MiscPlatformAPI task type as pending beacons are not yet
  // associated with any specific task runner in the spec.
  auto task_runner = ec->GetTaskRunner(TaskType::kMiscPlatformAPI);
  mojo::PendingReceiver<mojom::blink::PendingBeacon> beacon_receiver =
      remote_.BindNewPipeAndPassReceiver(task_runner);
  KURL host_url = ec->CompleteURL(url);

  PendingBeaconHostRemote& host_remote = PendingBeaconHostRemote::From(*ec);
  host_remote.remote_->CreateBeacon(std::move(beacon_receiver), host_url,
                                    host_method, background_timeout_);
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

void PendingBeacon::SetDataInternal(const BeaconData& data) {
  ResourceRequest request;

  data.Serialize(request);
  // `WrappedResourceRequest` only works for POST request.
  request.SetHttpMethod(http_names::kPOST);
  scoped_refptr<network::ResourceRequestBody> request_body =
      GetRequestBodyForWebURLRequest(WrappedResourceRequest(request));
  AtomicString content_type = request.HttpContentType();
  remote_->SetRequestData(std::move(request_body),
                          content_type.IsNull() ? "" : content_type);
}

}  // namespace blink
