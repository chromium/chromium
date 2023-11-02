// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_ATTRIBUTION_REPORTING_AUTOMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_ATTRIBUTION_REPORTING_AUTOMATION_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Internals;
class ScriptPromise;
class ScriptState;

class AttributionReportingAutomation {
  STATIC_ONLY(AttributionReportingAutomation);

 public:
  static ScriptPromise resetAttributionReporting(ScriptState* script_state,
                                                 Internals& internals);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_ATTRIBUTION_REPORTING_AUTOMATION_H_
