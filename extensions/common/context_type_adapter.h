// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONTEXT_TYPE_ADAPTER_H_
#define EXTENSIONS_COMMON_CONTEXT_TYPE_ADAPTER_H_

#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

namespace extensions {

// Converts a Feature::Context into a mojom::ContextType.
// NOTE: Feature::UNSPECIFIED_CONTEXT is unsupported and will crash.
mojom::ContextType FeatureContextToMojomContext(
    Feature::Context feature_context);

// Converts a mojom::ContextType into a Feature::Context.
Feature::Context MojomContextToFeatureContext(mojom::ContextType mojom_context);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CONTEXT_TYPE_ADAPTER_H_
