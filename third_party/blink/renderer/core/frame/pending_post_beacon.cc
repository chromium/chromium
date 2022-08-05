// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_post_beacon.h"

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pending_beacon_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_formdata_readablestream_urlsearchparams_usvstring.h"
#include "third_party/blink/renderer/core/loader/beacon_data.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace blink {

// static
PendingPostBeacon* PendingPostBeacon::Create(ExecutionContext* ec,
                                             const String& target_url) {
  auto* options = PendingBeaconOptions::Create();
  return PendingPostBeacon::Create(ec, target_url, options);
}

// static
PendingPostBeacon* PendingPostBeacon::Create(ExecutionContext* ec,
                                             const String& target_url,
                                             PendingBeaconOptions* options) {
  return MakeGarbageCollected<PendingPostBeacon>(
      ec, target_url, options->backgroundTimeout(), options->timeout(),
      base::PassKey<PendingPostBeacon>());
}

PendingPostBeacon::PendingPostBeacon(ExecutionContext* context,
                                     const String& url,
                                     int32_t background_timeout,
                                     int32_t timeout,
                                     base::PassKey<PendingPostBeacon> key)
    : PendingBeacon(context,
                    url,
                    http_names::kPOST,
                    background_timeout,
                    timeout) {}

void PendingPostBeacon::setData(
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data) {
  using ContentType =
      V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType;
  switch (data->GetContentType()) {
    case ContentType::kUSVString: {
      SetDataInternal(BeaconString(data->GetAsUSVString()));
      return;
    }
    case ContentType::kArrayBuffer: {
      SetDataInternal(BeaconDOMArrayBuffer(data->GetAsArrayBuffer()));
      return;
    }
    case ContentType::kArrayBufferView: {
      SetDataInternal(
          BeaconDOMArrayBufferView(data->GetAsArrayBufferView().Get()));
      return;
    }
    case ContentType::kFormData: {
      SetDataInternal(BeaconFormData(data->GetAsFormData()));
      return;
    }
    case ContentType::kURLSearchParams: {
      SetDataInternal(BeaconURLSearchParams(data->GetAsURLSearchParams()));
      return;
    }
    case ContentType::kBlob:
      // TODO(crbug.com/1293679): Decide whether to support blob/file.
    case ContentType::kReadableStream: {
      // TODO(crbug.com/1293679): Throw errors.
      break;
    }
  }
  NOTIMPLEMENTED();
}

}  // namespace blink
