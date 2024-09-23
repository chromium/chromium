// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_TRIGGER_TYPE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_TRIGGER_TYPE_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {

// Returns the PreloadingTriggerType corresponding to the given
// blink::mojom::SpeculationInjectionType.
CONTENT_EXPORT PreloadingTriggerType
PreloadingTriggerTypeFromSpeculationInjectionType(
    blink::mojom::SpeculationInjectionType injection_type);

// Checks if the type is kSpeculationRule*. Recommends to use this function to
// keep the code robust against adding more trigger types in the future.
bool IsSpeculationRuleType(PreloadingTriggerType type);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_TRIGGER_TYPE_IMPL_H_
