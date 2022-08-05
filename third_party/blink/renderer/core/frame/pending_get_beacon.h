// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_GET_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_GET_BEACON_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/frame/pending_beacon.h"

namespace blink {

class PendingBeaconOptions;
class ExecutionContext;

// Implementation of the PendingGetBeacon API.
// https://github.com/WICG/unload-beacon/blob/main/README.md
class CORE_EXPORT PendingGetBeacon : public PendingBeacon {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PendingGetBeacon* Create(ExecutionContext* context,
                                  const String& target_url);

  static PendingGetBeacon* Create(ExecutionContext* context,
                                  const String& target_url,
                                  PendingBeaconOptions* options);

  explicit PendingGetBeacon(ExecutionContext* context,
                            const String& url,
                            int32_t background_timeout,
                            int32_t timeout,
                            base::PassKey<PendingGetBeacon> key);

  void setURL(const String& url);
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_GET_BEACON_H_
