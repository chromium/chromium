// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_TRIALS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_TRIALS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class WebDocument;
class WebString;

// A class with tests for experimental features which can be enabled by the
// origin trials framework via origin trial tokens.
class WebOriginTrials {
 public:
  // This function returns true if the passed trial name is valid and ALL of the
  // features with that trial name are enabled.
  CORE_EXPORT static bool isTrialEnabled(const WebDocument*, const WebString&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_ORIGIN_TRIALS_H_
