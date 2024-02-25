// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/utils/extension_types_utils.h"

#include "extensions/common/api/extension_types.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionTypesUtils, ConvertRunLocation) {
  const std::vector<api::extension_types::RunAt> values({
      api::extension_types::RunAt::kDocumentStart,
      api::extension_types::RunAt::kDocumentEnd,
      api::extension_types::RunAt::kDocumentIdle,
  });
  for (const api::extension_types::RunAt value : values) {
    mojom::RunLocation mojo = ConvertRunLocation(value);
    api::extension_types::RunAt api = ConvertRunLocationForAPI(mojo);
    EXPECT_EQ(value, api) << "One-to-one constraint fails for RunAt";

    EXPECT_LE(mojom::RunLocation::kMinValue, mojo);
    EXPECT_GE(mojom::RunLocation::kMaxValue, mojo);
    EXPECT_NE(mojom::RunLocation::kUndefined, mojo);

    EXPECT_LT(api::extension_types::RunAt::kNone, api);
    EXPECT_GE(api::extension_types::RunAt::kMaxValue, api);
  }
}

}  // namespace extensions
