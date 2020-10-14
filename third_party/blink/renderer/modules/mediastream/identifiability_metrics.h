// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IDENTIFIABILITY_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_IDENTIFIABILITY_METRICS_H_

#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints.h"

namespace blink {

IdentifiableToken TokenFromConstraints(
    const MediaStreamConstraints* constraints);

void RecordIdentifiabilityMetric(const IdentifiableSurface& surface,
                                 ExecutionContext* context,
                                 IdentifiableToken token);

}  // namespace blink

#endif
