// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTERVENTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTERVENTION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Intervention {
  DISALLOW_NEW();

 public:
  Intervention() = default;
  ~Intervention() = default;

  // Generates a intervention report, to be routed to the Reporting API and any
  // ReportingObservers. Also sends the intervention message to the console.
  static void GenerateReport(const LocalFrame*,
                             const String& id,
                             const String& message);

  DISALLOW_COPY_AND_ASSIGN(Intervention);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTERVENTION_H_
