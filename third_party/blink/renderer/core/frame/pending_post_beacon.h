// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_POST_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_POST_BEACON_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/frame/pending_beacon.h"

namespace blink {

class PendingBeaconOptions;
class ExceptionState;
class ExecutionContext;

// Implementation of the PendingPostBeacon API.
// https://github.com/WICG/pending-beacon/blob/main/README.md
class CORE_EXPORT PendingPostBeacon : public PendingBeacon {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PendingPostBeacon* Create(ExecutionContext* context,
                                   const String& target_url,
                                   ExceptionState& exception_state);

  static PendingPostBeacon* Create(ExecutionContext* context,
                                   const String& target_url,
                                   PendingBeaconOptions* options,
                                   ExceptionState& exception_state);

  explicit PendingPostBeacon(ExecutionContext* context,
                             const String& url,
                             int32_t background_timeout,
                             int32_t timeout,
                             base::PassKey<PendingPostBeacon> key);

  void setData(const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data,
               ExceptionState& exception_state);
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_POST_BEACON_H_
