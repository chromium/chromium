// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "mojo/public/cpp/base/uuid_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/work_in_progress.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace {

TEST(MojomWorkInProgress, FeatureIsDisabled) {
  // This feature should never be enabled globally.
  EXPECT_FALSE(base::FeatureList::IsEnabled(mojom::kMojomWorkInProgress));
}

}  // namespace
}  // namespace mojo_base
