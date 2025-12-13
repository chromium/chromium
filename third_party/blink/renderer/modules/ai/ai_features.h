// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Feature flag to relax user activation requirements for AI features.
MODULES_EXPORT BASE_DECLARE_FEATURE(kAIRelaxUserActivationReqs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_FEATURES_H_
