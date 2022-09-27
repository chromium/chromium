// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

ContextFeatureSettings::ContextFeatureSettings(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

// static
const char ContextFeatureSettings::kSupplementName[] = "ContextFeatureSettings";

// static
ContextFeatureSettings* ContextFeatureSettings::From(
    ExecutionContext* context,
    CreationMode creation_mode) {
  ContextFeatureSettings* settings =
      Supplement<ExecutionContext>::From<ContextFeatureSettings>(context);
  if (!settings && creation_mode == CreationMode::kCreateIfNotExists) {
    settings = MakeGarbageCollected<ContextFeatureSettings>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, settings);
  }
  return settings;
}

void ContextFeatureSettings::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
