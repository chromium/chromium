// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler_permission.h"

#include <initializer_list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using ::testing::ElementsAre;

constexpr char kPdf[] = "application/pdf";
constexpr char kText[] = "text/plain";

std::unique_ptr<MimeTypesHandlerPermission> MakePermission(
    std::initializer_list<std::string> mime_types) {
  return std::make_unique<MimeTypesHandlerPermission>(
      MimeTypesHandlerPermission::MimeTypeSet(mime_types));
}

const MimeTypesHandlerPermission* AsMimePermission(
    const ManifestPermission* permission) {
  return static_cast<const MimeTypesHandlerPermission*>(permission);
}

void AssertEmptyPermission(const MimeTypesHandlerPermission* permission) {
  ASSERT_TRUE(permission);
  EXPECT_EQ(manifest_keys::kMimeTypesHandler, permission->id());
  EXPECT_EQ(permission->id(), permission->name());
  EXPECT_TRUE(permission->GetPermissions().empty());
  EXPECT_TRUE(permission->mime_types().empty());
  EXPECT_FALSE(permission->RequiresManagementUIWarning());
}

}  // namespace

TEST(MimeTypesHandlerPermissionTest, Empty) {
  auto permission = std::make_unique<MimeTypesHandlerPermission>();
  AssertEmptyPermission(permission.get());

  std::unique_ptr<base::Value> value = permission->ToValue();
  ASSERT_TRUE(value);
  auto restored = std::make_unique<MimeTypesHandlerPermission>();
  EXPECT_TRUE(restored->FromValue(value.get()));
  AssertEmptyPermission(restored.get());
}

TEST(MimeTypesHandlerPermissionTest, FromToValue) {
  auto permission = MakePermission({kText, kPdf});

  std::unique_ptr<base::Value> value = permission->ToValue();
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_list());
  EXPECT_THAT(value->GetList(), ElementsAre(kPdf, kText));

  auto restored = std::make_unique<MimeTypesHandlerPermission>();
  EXPECT_TRUE(restored->FromValue(value.get()));
  EXPECT_TRUE(permission->Equal(restored.get()));
  EXPECT_THAT(restored->mime_types(), ElementsAre(kPdf, kText));
}

TEST(MimeTypesHandlerPermissionTest, FromValueRejectsMalformedInput) {
  const base::Value kNotAList("application/pdf");
  base::ListValue mixed;
  mixed.Append(kPdf);
  mixed.Append(7);
  const base::Value kMixedList(std::move(mixed));

  const struct {
    const char* name;
    raw_ptr<const base::Value> value;
  } kTestCases[] = {
      {"null", nullptr},
      {"not a list", &kNotAList},
      {"non-string entry", &kMixedList},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    MimeTypesHandlerPermission permission;
    EXPECT_FALSE(permission.FromValue(test_case.value));
  }
}

TEST(MimeTypesHandlerPermissionTest, SetOperations) {
  auto pdf = MakePermission({kPdf});
  auto text = MakePermission({kText});
  auto both = MakePermission({kPdf, kText});

  std::unique_ptr<ManifestPermission> union_perm = pdf->Union(text.get());
  EXPECT_THAT(AsMimePermission(union_perm.get())->mime_types(),
              ElementsAre(kPdf, kText));
  EXPECT_TRUE(union_perm->Contains(pdf.get()));
  EXPECT_TRUE(union_perm->Contains(text.get()));
  EXPECT_TRUE(union_perm->Equal(both.get()));

  // Claiming an extra type is a delta; dropping one is not.
  EXPECT_TRUE(both->Diff(pdf.get())->Equal(text.get()));
  AssertEmptyPermission(AsMimePermission(pdf->Diff(both.get()).get()));

  EXPECT_TRUE(both->Intersect(pdf.get())->Equal(pdf.get()));
  AssertEmptyPermission(AsMimePermission(pdf->Intersect(text.get()).get()));
}

TEST(MimeTypesHandlerPermissionTest, GetPermissionsMapsOnlySupportedTypes) {
  PermissionIDSet ids = MakePermission({kPdf, kText})->GetPermissions();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(ids.ContainsID(mojom::APIPermissionID::kMimeTypesHandlerPdf));

  EXPECT_TRUE(MakePermission({kText})->GetPermissions().empty());
}

TEST(MimeTypesHandlerPermissionTest, RequiresManagementUIWarningWhenClaimed) {
  EXPECT_TRUE(MakePermission({kPdf})->RequiresManagementUIWarning());
}

}  // namespace extensions
