// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/pending_get_beacon.h"

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pending_beacon_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace blink {

// static
PendingGetBeacon* PendingGetBeacon::Create(ExecutionContext* ec,
                                           const String& target_url,
                                           ExceptionState& exception_state) {
  auto* options = PendingBeaconOptions::Create();
  return PendingGetBeacon::Create(ec, target_url, options, exception_state);
}

// static
PendingGetBeacon* PendingGetBeacon::Create(ExecutionContext* ec,
                                           const String& target_url,
                                           PendingBeaconOptions* options,
                                           ExceptionState& exception_state) {
  if (!CanSendBeacon(target_url, *ec, exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<PendingGetBeacon>(
      ec, target_url, options->backgroundTimeout(), options->timeout(),
      base::PassKey<PendingGetBeacon>());
}

PendingGetBeacon::PendingGetBeacon(ExecutionContext* context,
                                   const String& url,
                                   int32_t background_timeout,
                                   int32_t timeout,
                                   base::PassKey<PendingGetBeacon> key)
    : PendingBeacon(context,
                    url,
                    http_names::kGET,
                    background_timeout,
                    timeout) {}

void PendingGetBeacon::setURL(const String& url,
                              ExceptionState& exception_state) {
  SetURLInternal(url, exception_state);
}

}  // namespace blink
