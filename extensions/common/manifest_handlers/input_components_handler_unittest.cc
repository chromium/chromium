// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/input_components_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class InputComponentsManifestTest : public ManifestTest {
 protected:
  std::u16string GetInvalidLayoutError(int component_index, int layout_index) {
    return ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidInputComponentLayoutName,
        base::NumberToString(component_index),
        base::NumberToString(layout_index));
  }
};

TEST_F(InputComponentsManifestTest, ValidLayouts) {
  base::DictValue manifest;
  manifest.Set("name", "test");
  manifest.Set("version", "1");
  manifest.Set("manifest_version", 3);

  base::DictValue component;
  component.Set("name", "test component");
  component.Set("id", "test_id");
  base::ListValue layouts;
  layouts.Append("us");
  layouts.Append("us(intl)");
  layouts.Append("be");
  component.Set("layouts", std::move(layouts));

  base::ListValue input_components;
  input_components.Append(std::move(component));
  manifest.Set("input_components", std::move(input_components));

  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();

  ASSERT_TRUE(extension.get());
  const std::vector<InputComponentInfo>* components =
      InputComponents::GetInputComponents(extension.get());
  ASSERT_TRUE(components);
  ASSERT_EQ(1u, components->size());
  EXPECT_EQ(3u, (*components)[0].layouts.size());
}

TEST_F(InputComponentsManifestTest, InvalidLayouts) {
  const struct {
    const char* layout;
  } kInvalidTestCases[] = {
      {"../../evil"},    {"us/../../evil"}, {"us(../../evil)"},
      {"us-../../evil"}, {"us$"},           {"us(dvo*ak)"},
  };

  for (const auto& test_case : kInvalidTestCases) {
    SCOPED_TRACE(test_case.layout);
    base::DictValue manifest;
    manifest.Set("name", "test");
    manifest.Set("version", "1");
    manifest.Set("manifest_version", 3);

    base::DictValue component;
    component.Set("name", "test component");
    component.Set("id", "test_id");
    base::ListValue layouts;
    layouts.Append(test_case.layout);
    component.Set("layouts", std::move(layouts));

    base::ListValue input_components;
    input_components.Append(std::move(component));
    manifest.Set("input_components", std::move(input_components));

    std::u16string error;
    scoped_refptr<Extension> extension =
        Extension::Create(base::FilePath(), mojom::ManifestLocation::kInternal,
                          manifest, Extension::NO_FLAGS, &error);
    EXPECT_FALSE(extension.get());
    EXPECT_EQ(GetInvalidLayoutError(0, 0), error);
  }
}

}  // namespace extensions
