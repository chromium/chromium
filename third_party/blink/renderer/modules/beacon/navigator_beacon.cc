// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/beacon/navigator_beacon.h"

#include "third_party/blink/renderer/bindings/modules/v8/array_buffer_view_or_blob_or_string_or_form_data.h"
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

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorBeacon::~NavigatorBeacon() = default;

void NavigatorBeacon::Trace(blink::Visitor* visitor) {
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
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exception_state) {
  return NavigatorBeacon::From(navigator).SendBeaconImpl(
      script_state, urlstring, data, exception_state);
}

bool NavigatorBeacon::SendBeaconImpl(
    ScriptState* script_state,
    const String& urlstring,
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  KURL url = context->CompleteURL(urlstring);
  if (!CanSendBeacon(context, url, exception_state))
    return false;

  bool allowed;

  if (data.IsArrayBufferView()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsArrayBufferView().View());
  } else if (data.IsBlob()) {
    Blob* blob = data.GetAsBlob();
    if (!cors::IsCorsSafelistedContentType(blob->type())) {
      UseCounter::Count(context,
                        WebFeature::kSendBeaconWithNonSimpleContentType);
      if (RuntimeEnabledFeatures::
              SendBeaconThrowForBlobWithNonSimpleTypeEnabled()) {
        exception_state.ThrowSecurityError(
            "sendBeacon() with a Blob whose type is not any of the "
            "CORS-safelisted values for the Content-Type request header is "
            "disabled temporarily. See http://crbug.com/490015 for details.");
        return false;
      }
    }
    allowed =
        PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url, blob);
  } else if (data.IsString()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsString());
  } else if (data.IsFormData()) {
    allowed = PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url,
                                     data.GetAsFormData());
  } else {
    allowed =
        PingLoader::SendBeacon(GetSupplementable()->GetFrame(), url, String());
  }

  if (!allowed) {
    UseCounter::Count(context, WebFeature::kSendBeaconQuotaExceeded);
    return false;
  }

  return true;
}

}  // namespace blink
