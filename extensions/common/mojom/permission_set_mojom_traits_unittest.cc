// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/permission_set_mojom_traits.h"

#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handler_registry.h"
#include "extensions/common/mojom/permission_set.mojom.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/mock_manifest_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using mojo::test::SerializeAndDeserialize;

class MockManifestHandler : public ManifestHandler {
 public:
  MockManifestHandler() = default;
  MockManifestHandler(const MockManifestHandler&) = delete;
  MockManifestHandler& operator=(const MockManifestHandler&) = delete;
  ~MockManifestHandler() override = default;

  bool Parse(Extension* extension, std::u16string* error) override {
    return true;
  }

  ManifestPermission* CreatePermission() override {
    return new MockManifestPermission("mock_keys::key");
  }

  base::span<const char* const> Keys() const override {
    static constexpr const char* kKeys[] = {"mock_keys::key"};
    return kKeys;
  }
};

TEST(PermissionSetMojomTraitsTest, BasicAPIPermission) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(mojom::APIPermissionID::kSocket);
  std::unique_ptr<APIPermission> input = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(input->FromValue(&value, nullptr, nullptr));
  }

  std::unique_ptr<APIPermission> output = nullptr;
  EXPECT_TRUE(
      SerializeAndDeserialize<extensions::mojom::APIPermission>(input, output));
  EXPECT_TRUE(input->Equal(output.get()));
}

TEST(PermissionSetMojomTraitsTest, BasicAPIPermissionSet) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(mojom::APIPermissionID::kSocket);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }

  APIPermissionSet input;
  input.insert(mojom::APIPermissionID::kAudioCapture);
  input.insert(mojom::APIPermissionID::kDns);
  input.insert(mojom::APIPermissionID::kHid);
  input.insert(mojom::APIPermissionID::kPower);
  input.insert(mojom::APIPermissionID::kSerial);
  input.insert(std::move(permission));

  APIPermissionSet output;
  EXPECT_TRUE(SerializeAndDeserialize<extensions::mojom::APIPermissionSet>(
      input, output));
  EXPECT_EQ(input, output);
}

TEST(PermissionSetMojomTraitsTest, BasicManifestPermission) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  ManifestHandlerRegistry::Get()->RegisterHandler(
      std::make_unique<MockManifestHandler>());
  ManifestHandler::FinalizeRegistration();

  std::unique_ptr<extensions::ManifestPermission> input(
      extensions::ManifestHandler::CreatePermission("mock_keys::key"));
  ASSERT_TRUE(input);
  base::Value value("value");
  input->FromValue(&value);

  std::unique_ptr<extensions::ManifestPermission> output = nullptr;
  EXPECT_TRUE(SerializeAndDeserialize<extensions::mojom::ManifestPermission>(
      input, output));
  EXPECT_EQ(input->id(), output->id());
  EXPECT_TRUE(input->Equal(output.get()));
}

TEST(PermissionSetMojomTraitsTest, BasicManifestPermissionSet) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  ManifestHandlerRegistry::Get()->RegisterHandler(
      std::make_unique<MockManifestHandler>());
  ManifestHandler::FinalizeRegistration();
  std::unique_ptr<extensions::ManifestPermission> permission1(
      extensions::ManifestHandler::CreatePermission("mock_keys::key"));
  std::unique_ptr<extensions::ManifestPermission> permission2(
      extensions::ManifestHandler::CreatePermission("mock_keys::key"));
  base::Value value("value");
  permission2->FromValue(&value);
  ManifestPermissionSet input;
  input.insert(std::move(permission1));
  input.insert(std::move(permission2));

  ManifestPermissionSet output;
  EXPECT_TRUE(SerializeAndDeserialize<extensions::mojom::ManifestPermissionSet>(
      input, output));
  EXPECT_EQ(input, output);
}

TEST(PermissionSetMojomTraitsTest, BasicPermissionSet) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(mojom::APIPermissionID::kSocket);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }

  APIPermissionSet apis;
  apis.insert(mojom::APIPermissionID::kAudioCapture);
  apis.insert(mojom::APIPermissionID::kDns);
  apis.insert(std::move(permission));

  ScopedTestingManifestHandlerRegistry scoped_registry;
  ManifestHandlerRegistry::Get()->RegisterHandler(
      std::make_unique<MockManifestHandler>());
  ManifestHandler::FinalizeRegistration();
  std::unique_ptr<extensions::ManifestPermission> permission1(
      extensions::ManifestHandler::CreatePermission("mock_keys::key"));
  std::unique_ptr<extensions::ManifestPermission> permission2(
      extensions::ManifestHandler::CreatePermission("mock_keys::key"));
  {
    base::Value value("value");
    permission2->FromValue(&value);
  }
  ManifestPermissionSet manifest_permissions;
  manifest_permissions.insert(std::move(permission1));
  manifest_permissions.insert(std::move(permission2));

  URLPattern pattern1(URLPattern::SCHEME_ALL);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern1.Parse("http://*.foo:1234/bar"))
      << "Got unexpected error in the URLPattern parsing";
  URLPattern pattern2(URLPattern::SCHEME_HTTPS);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern2.Parse("https://www.google.com/foobar"))
      << "Got unexpected error in the URLPattern parsing";
  URLPatternSet hosts;
  hosts.AddPattern(pattern1);
  hosts.AddPattern(pattern2);

  URLPattern pattern3(URLPattern::SCHEME_HTTP);
  EXPECT_EQ(URLPattern::ParseResult::kSuccess,
            pattern3.Parse("http://127.0.0.1/*"))
      << "Got unexpected error in the URLPattern parsing";
  URLPatternSet user_script_hosts;
  user_script_hosts.AddPattern(pattern3);

  PermissionSet input(std::move(apis), std::move(manifest_permissions),
                      std::move(hosts), std::move(user_script_hosts));

  PermissionSet output;
  EXPECT_TRUE(
      SerializeAndDeserialize<extensions::mojom::PermissionSet>(input, output));
  EXPECT_EQ(input, output);
}

}  // namespace extensions
