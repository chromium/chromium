// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_SESSION_TYPE_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_SESSION_TYPE_H_

#include <memory>

#include "base/auto_reset.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"

namespace extensions {

// Gets the current session type as seen by the Feature system.
mojom::FeatureSessionType GetCurrentFeatureSessionType();

// Sets the current session type as seen by the Feature system. In the browser
// process this should be extensions::util::GetCurrentSessionType(), and in
// the renderer this will need to come from an IPC.
void SetCurrentFeatureSessionType(mojom::FeatureSessionType session_type);

// Scoped session type setter. Use for tests.
std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
ScopedCurrentFeatureSessionType(mojom::FeatureSessionType session_type);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_SESSION_TYPE_H_
