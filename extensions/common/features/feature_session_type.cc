// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_session_type.h"

#include "base/check_op.h"

namespace extensions {

namespace {

const mojom::FeatureSessionType kDefaultSessionType =
    mojom::FeatureSessionType::kInitial;
mojom::FeatureSessionType g_current_session_type = kDefaultSessionType;

}  // namespace

mojom::FeatureSessionType GetCurrentFeatureSessionType() {
  return g_current_session_type;
}

void SetCurrentFeatureSessionType(mojom::FeatureSessionType session_type) {
  // Make sure that session type stays constant after it's been initialized.
  // Note that this requirement can be bypassed in tests by using
  // |ScopedCurrentFeatureSessionType|.
  CHECK(g_current_session_type == kDefaultSessionType ||
        session_type == g_current_session_type);
  // In certain unit tests, SetCurrentFeatureSessionType() can be called
  // within the same process (where e.g. utility processes run as a separate
  // thread). Don't write if the value is the same to avoid TSAN failures.
  if (session_type != g_current_session_type)
    g_current_session_type = session_type;
}

std::unique_ptr<base::AutoReset<mojom::FeatureSessionType>>
ScopedCurrentFeatureSessionType(mojom::FeatureSessionType type) {
  CHECK_EQ(g_current_session_type, kDefaultSessionType);
  return std::make_unique<base::AutoReset<mojom::FeatureSessionType>>(
      &g_current_session_type, type);
}

}  // namespace extensions
