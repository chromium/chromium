// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PROBE_PLATFORM_PROBES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PROBE_PLATFORM_PROBES_H_

#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class FetchContext;
class PlatformProbeSink;

namespace probe {

class PLATFORM_EXPORT ProbeBase {
  STACK_ALLOCATED();

 public:
  TimeTicks CaptureStartTime() const;
  TimeTicks CaptureEndTime() const;
  TimeDelta Duration() const;

 private:
  mutable TimeTicks start_time_;
  mutable TimeTicks end_time_;
};

inline PlatformProbeSink* ToPlatformProbeSink(FetchContext* context) {
  return context->GetPlatformProbeSink();
}

}  // namespace probe
}  // namespace blink

#include "third_party/blink/renderer/platform/platform_probes_inl.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PROBE_PLATFORM_PROBES_H_
