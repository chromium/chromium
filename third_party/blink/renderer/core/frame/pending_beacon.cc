// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_beacon_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_formdata_readablestream_urlsearchparams_usvstring.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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

// static
PendingBeacon* PendingBeacon::Create(ExecutionContext* ec,
                                     const String& targetURL) {
  BeaconOptions* options = BeaconOptions::Create();
  return PendingBeacon::Create(ec, targetURL, options);
}
// static
PendingBeacon* PendingBeacon::Create(ExecutionContext* ec,
                                     const String& targetURL,
                                     BeaconOptions* options) {
  PendingBeacon* beacon = MakeGarbageCollected<PendingBeacon>(
      ec, targetURL, options->method(), options->pageHideTimeout());
  mojom::blink::BeaconMethod method;
  if (options->method() == V8BeaconMethod::Enum::kGET) {
    method = mojom::blink::BeaconMethod::kGet;
  } else {
    method = mojom::blink::BeaconMethod::kPost;
  }

  // Using the MiscPlatformAPI task type as pending beacons are not yet
  // associated with any specific task runner in the spec.
  auto task_runner = ec->GetTaskRunner(TaskType::kMiscPlatformAPI);

  mojo::PendingReceiver<mojom::blink::PendingBeacon> beacon_receiver =
      beacon->remote_.BindNewPipeAndPassReceiver(task_runner);

  PendingBeaconHostRemote& host_remote = PendingBeaconHostRemote::From(*ec);

  KURL url = ec->CompleteURL(targetURL);

  host_remote.remote_->CreateBeacon(
      std::move(beacon_receiver), url, method,
      base::Milliseconds(beacon->page_hide_timeout_));
  return beacon;
}

PendingBeacon::PendingBeacon(ExecutionContext* context,
                             String url,
                             String method,
                             int32_t page_hide_timeout)
    : remote_(context),
      url_(url),
      method_(method),
      page_hide_timeout_(page_hide_timeout) {}

void PendingBeacon::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(remote_);
}

void PendingBeacon::setData(
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data) {
  switch (data->GetContentType()) {
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
        kUSVString: {
      auto string_data = data->GetAsUSVString();
      remote_->SetData(string_data);
      return;
    }
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
        kArrayBuffer:
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
        kArrayBufferView:
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::kFormData:
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
        kURLSearchParams:
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::kBlob:
    case V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType::
        kReadableStream: {
    }
  }
  NOTIMPLEMENTED();
}

void PendingBeacon::deactivate() {
  remote_->Deactivate();
}

void PendingBeacon::sendNow() {
  if (is_pending_) {
    remote_->SendNow();
    is_pending_ = false;
  }
}

}  // namespace blink
