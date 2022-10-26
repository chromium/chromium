// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_post_beacon.h"

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pending_beacon_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_formdata_readablestream_urlsearchparams_usvstring.h"
#include "third_party/blink/renderer/core/loader/beacon_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace blink {

// static
PendingPostBeacon* PendingPostBeacon::Create(ExecutionContext* ec,
                                             const String& target_url,
                                             ExceptionState& exception_state) {
  auto* options = PendingBeaconOptions::Create();
  return PendingPostBeacon::Create(ec, target_url, options, exception_state);
}

// static
PendingPostBeacon* PendingPostBeacon::Create(ExecutionContext* ec,
                                             const String& target_url,
                                             PendingBeaconOptions* options,
                                             ExceptionState& exception_state) {
  if (!CanSendBeacon(target_url, *ec, exception_state)) {
    return nullptr;
  }

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
    const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data,
    ExceptionState& exception_state) {
  using ContentType =
      V8UnionReadableStreamOrXMLHttpRequestBodyInit::ContentType;
  switch (data->GetContentType()) {
    case ContentType::kUSVString: {
      SetDataInternal(BeaconString(data->GetAsUSVString()), exception_state);
      return;
    }
    case ContentType::kArrayBuffer: {
      auto* array_buffer = data->GetAsArrayBuffer();
      if (!base::CheckedNumeric<wtf_size_t>(array_buffer->ByteLength())
               .IsValid()) {
        exception_state.ThrowRangeError(
            "The data provided to setData() exceeds the maximally "
            "possible length, which is 4294967295.");
        return;
      }
      SetDataInternal(BeaconDOMArrayBuffer(array_buffer), exception_state);
      return;
    }
    case ContentType::kArrayBufferView: {
      auto* array_buffer_view = data->GetAsArrayBufferView().Get();
      if (!base::CheckedNumeric<wtf_size_t>(array_buffer_view->byteLength())
               .IsValid()) {
        exception_state.ThrowRangeError(
            "The data provided to setData() exceeds the maximally "
            "possible length, which is 4294967295.");
        return;
      }
      SetDataInternal(BeaconDOMArrayBufferView(array_buffer_view),
                      exception_state);
      return;
    }
    case ContentType::kFormData: {
      SetDataInternal(BeaconFormData(data->GetAsFormData()), exception_state);
      return;
    }
    case ContentType::kURLSearchParams: {
      SetDataInternal(BeaconURLSearchParams(data->GetAsURLSearchParams()),
                      exception_state);
      return;
    }
    case ContentType::kBlob: {
      SetDataInternal(BeaconBlob(data->GetAsBlob()), exception_state);
      return;
    }
    case ContentType::kReadableStream: {
      exception_state.ThrowTypeError(
          "PendingPostBeacon cannot have a ReadableStream body.");
      return;
    }
  }
  NOTIMPLEMENTED();
}

}  // namespace blink
