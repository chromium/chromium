// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/api_permission.h"

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "extensions/common/alias.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Tests that the ExtensionPermission3 enum in enums.xml exactly matches the
// mojom::APIPermissionID enum in Mojom.
TEST(ExtensionAPIPermissionTest, CheckEnums) {
  std::optional<base::HistogramEnumEntryMap> enums = base::ReadEnumFromEnumsXml(
      "ExtensionPermission3", /*subdirectory=*/"extensions");
  ASSERT_TRUE(enums);
  // The number of enums in the histogram entry should be equal to the number of
  // enums in the C++ file.
  EXPECT_EQ(enums->size(),
            static_cast<size_t>(mojom::APIPermissionID::kMaxValue) + 1);

  base::FilePath src_root;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  base::FilePath permission_histogram_value =
      src_root.AppendASCII("extensions")
          .AppendASCII("common")
          .AppendASCII("mojom")
          .AppendASCII("api_permission_id.mojom");
  ASSERT_TRUE(base::PathExists(permission_histogram_value));

  std::string file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(permission_histogram_value, &file_contents));

  for (const auto& entry : *enums) {
    // Check that the Mojo file has a definition equal to the histogram file.
    // For now, we do this in a simple, but reasonably effective, manner:
    // expecting to find the string "ENTRY = <value>" somewhere in the file.
    std::string expected_string =
        base::StringPrintf("%s = %d", entry.second.c_str(), entry.first);
    EXPECT_TRUE(file_contents.contains(expected_string))
        << "Failed to find entry " << entry.second << " with value "
        << entry.first;
  }
}

TEST(ExtensionAPIPermissionTest, ManagedSessionLoginWarningFlag) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  constexpr APIPermissionInfo::InitInfo init_info[] = {
      {mojom::APIPermissionID::kUnknown, "test permission",
       APIPermissionInfo::kFlagImpliesFullURLAccess |
           APIPermissionInfo::
               kFlagDoesNotRequireManagedSessionFullLoginWarning}};

  info->RegisterPermissions(base::span(init_info),
                            base::span<const extensions::Alias>());

  const APIPermissionInfo* permission =
      info->GetByID(mojom::APIPermissionID::kUnknown);
  ASSERT_TRUE(permission);
  EXPECT_FALSE(permission->requires_managed_session_full_login_warning());
}

TEST(ExtensionAPIPermissionTest, GetByNameWithNonNullTerminatedStringView) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  constexpr APIPermissionInfo::InitInfo init_info[] = {
      {mojom::APIPermissionID::kInvalid, "test lookup permission", 0}};

  info->RegisterPermissions(base::span(init_info),
                            base::span<const extensions::Alias>());

  const APIPermissionInfo* permission =
      info->GetByID(mojom::APIPermissionID::kInvalid);
  ASSERT_TRUE(permission);

  const std::string name_with_suffix = "test lookup permission suffix";
  EXPECT_EQ(
      permission,
      info->GetByName(
          std::string_view(name_with_suffix)
              .substr(0, std::string_view("test lookup permission").size())));
}

}  // namespace extensions
