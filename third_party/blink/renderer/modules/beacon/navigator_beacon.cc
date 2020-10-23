// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/beacon/navigator_beacon.h"

#include "third_party/blink/renderer/bindings/modules/v8/readable_stream_or_xml_http_request_body_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorBeacon::~NavigatorBeacon() = default;

void NavigatorBeacon::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorBeacon::kSupplementName[] = "NavigatorBeacon";

NavigatorBeacon& NavigatorBeacon::From(Navigator& navigator) {
  NavigatorBeacon* supplement =
      Supplement<Navigator>::From<NavigatorBeacon>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBeacon>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

bool NavigatorBeacon::CanSendBeacon(ExecutionContext* context,
                                    const KURL& url,
                                    ExceptionState& exception_state) {
  if (!url.IsValid()) {
    exception_state.ThrowTypeError(
        "The URL argument is ill-formed or unsupported.");
    return false;
  }
  // For now, only support HTTP and related.
  if (!url.ProtocolIsInHTTPFamily()) {
    exception_state.ThrowTypeError("Beacons are only supported over HTTP(S).");
    return false;
  }

  // If detached from frame, do not allow sending a Beacon.
  if (!GetSupplementable()->GetFrame())
    return false;

  return true;
}

bool NavigatorBeacon::sendBeacon(
    ScriptState* script_state,
    Navigator& navigator,
    const String& urlstring,
    const ReadableStreamOrBlobOrArrayBufferOrArrayBufferViewOrFormDataOrURLSearchParamsOrUSVString&
        data,
    ExceptionState& exception_state) {
  return NavigatorBeacon::From(navigator).SendBeaconImpl(
      script_state, urlstring, data, exception_state);
}

bool NavigatorBeacon::SendBeaconImpl(
    ScriptState* script_state,
    const String& urlstring,
    const ReadableStreamOrBlobOrArrayBufferOrArrayBufferViewOrFormDataOrURLSearchParamsOrUSVString&
        data,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  KURL url = context->CompleteURL(urlstring);
  if (!CanSendBeacon(context, url, exception_state))
    return false;

  bool allowed;

  if (data.IsArrayBuffer()) {
    auto* data_buffer = data.GetAsArrayBuffer();
    if (!base::CheckedNumeric<wtf_size_t>(data_buffer->ByteLengthAsSizeT())
             .IsValid()) {
      // At the moment the PingLoader::SendBeacon implementation cannot deal
      // with huge ArrayBuffers.
      exception_state.ThrowRangeError(
          "The data provided to sendBeacon() exceeds the maximally possible "
          "length, which is 4294967295.");
      return false;
    }
    allowed = PingLoader::SendBeacon(
        *script_state, GetSupplementable()->GetFrame(), url, data_buffer);
  } else if (data.IsArrayBufferView()) {
    auto* data_view = data.GetAsArrayBufferView().View();
    if (!base::CheckedNumeric<wtf_size_t>(data_view->byteLengthAsSizeT())
             .IsValid()) {
      // At the moment the PingLoader::SendBeacon implementation cannot deal
      // with huge ArrayBuffers.
      exception_state.ThrowRangeError(
          "The data provided to sendBeacon() exceeds the maximally possible "
          "length, which is 4294967295.");
      return false;
    }
    allowed = PingLoader::SendBeacon(
        *script_state, GetSupplementable()->GetFrame(), url, data_view);
  } else if (data.IsBlob()) {
    Blob* blob = data.GetAsBlob();
    allowed = PingLoader::SendBeacon(
        *script_state, GetSupplementable()->GetFrame(), url, blob);
  } else if (data.IsUSVString()) {
    allowed =
        PingLoader::SendBeacon(*script_state, GetSupplementable()->GetFrame(),
                               url, data.GetAsUSVString());
  } else if (data.IsFormData()) {
    allowed =
        PingLoader::SendBeacon(*script_state, GetSupplementable()->GetFrame(),
                               url, data.GetAsFormData());
  } else if (data.IsURLSearchParams()) {
    allowed =
        PingLoader::SendBeacon(*script_state, GetSupplementable()->GetFrame(),
                               url, data.GetAsURLSearchParams());
  } else if (data.IsReadableStream()) {
    exception_state.ThrowTypeError(
        "sendBeacon cannot have a ReadableStream body.");
    return false;
  } else {
    allowed = PingLoader::SendBeacon(
        *script_state, GetSupplementable()->GetFrame(), url, String());
  }

  if (!allowed) {
    UseCounter::Count(context, WebFeature::kSendBeaconQuotaExceeded);
    return false;
  }

  return true;
}

}  // namespace blink
