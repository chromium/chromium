// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/api/sockets/sockets_manifest_permission.h"

#include <set>
#include <tuple>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SocketPermissionRequest;

namespace extensions {

namespace {

const char kUdpBindPermission[] =
    "{ \"udp\": { \"bind\": [\"127.0.0.1:3007\", \"a.com:80\"] } }";

const char kUdpSendPermission[] =
    "{ \"udp\": { \"send\": [\"\", \"a.com:80\"] } }";

const char kTcpConnectPermission[] =
    "{ \"tcp\": { \"connect\": [\"127.0.0.1:80\", \"a.com:80\"] } }";

const char kTcpServerListenPermission[] =
    "{ \"tcpServer\": { \"listen\": [\"127.0.0.1:80\", \"a.com:80\"] } }";

static void AssertEmptyPermission(const SocketsManifestPermission* permission) {
  EXPECT_TRUE(permission);
  EXPECT_EQ(std::string(extensions::manifest_keys::kSockets), permission->id());
  EXPECT_EQ(permission->id(), permission->name());
  EXPECT_TRUE(permission->GetPermissions().empty());
  EXPECT_EQ(0u, permission->entries().size());
}

static base::Value ParsePermissionJSON(const std::string& json) {
  std::optional<base::Value> result = base::JSONReader::Read(json);
  EXPECT_TRUE(result) << "Invalid JSON string: " << json;
  return std::move(result.value());
}

static std::unique_ptr<SocketsManifestPermission> PermissionFromValue(
    const base::Value& value) {
  std::u16string error16;
  std::unique_ptr<SocketsManifestPermission> permission(
      SocketsManifestPermission::FromValue(value, &error16));
  EXPECT_TRUE(permission) << "Error parsing Value into permission: " << error16;
  return permission;
}

static std::unique_ptr<SocketsManifestPermission> PermissionFromJSON(
    const std::string& json) {
  base::Value value = ParsePermissionJSON(json);
  return PermissionFromValue(value);
}

struct CheckFormatEntry {
  CheckFormatEntry(SocketPermissionRequest::OperationType operation_type,
                   std::string host_pattern)
      : operation_type(operation_type), host_pattern(host_pattern) {}

  // operators <, == are needed by container std::set and algorithms
  // std::set_includes and std::set_differences.
  bool operator<(const CheckFormatEntry& rhs) const {
    return std::tie(operation_type, host_pattern) <
           std::tie(rhs.operation_type, rhs.host_pattern);
  }

  bool operator==(const CheckFormatEntry& rhs) const {
    return operation_type == rhs.operation_type &&
           host_pattern == rhs.host_pattern;
  }

  SocketPermissionRequest::OperationType operation_type;
  std::string host_pattern;
};

static testing::AssertionResult CheckFormat(
    const std::multiset<CheckFormatEntry>& permissions,
    const std::string& json) {
  std::unique_ptr<SocketsManifestPermission> permission(
      PermissionFromJSON(json));
  if (!permission)
    return testing::AssertionFailure() << "Invalid permission " << json;

  if (permissions.size() != permission->entries().size()) {
    return testing::AssertionFailure()
           << "Incorrect # of entries in json: " << json;
  }

  // Note: We use multiset because SocketsManifestPermission does not have to
  // store entries in the order found in the json message.
  std::multiset<CheckFormatEntry> parsed_permissions;
  for (auto it = permission->entries().cbegin();
       it != permission->entries().cend(); ++it) {
    parsed_permissions.insert(
        CheckFormatEntry(it->pattern().type, it->GetHostPatternAsString()));
  }

  if (!base::ranges::equal(permissions, parsed_permissions))
    return testing::AssertionFailure() << "Incorrect socket operations.";
  return testing::AssertionSuccess();
}

static testing::AssertionResult CheckFormat(const std::string& json) {
  return CheckFormat(std::multiset<CheckFormatEntry>(), json);
}

static testing::AssertionResult CheckFormat(const std::string& json,
                                            const CheckFormatEntry& op1) {
  CheckFormatEntry entries[] = {op1};
  return CheckFormat(
      std::multiset<CheckFormatEntry>(entries, entries + std::size(entries)),
      json);
}

static testing::AssertionResult CheckFormat(const std::string& json,
                                            const CheckFormatEntry& op1,
                                            const CheckFormatEntry& op2) {
  CheckFormatEntry entries[] = {op1, op2};
  return CheckFormat(
      std::multiset<CheckFormatEntry>(entries, entries + std::size(entries)),
      json);
}

static testing::AssertionResult CheckFormat(const std::string& json,
                                            const CheckFormatEntry& op1,
                                            const CheckFormatEntry& op2,
                                            const CheckFormatEntry& op3,
                                            const CheckFormatEntry& op4,
                                            const CheckFormatEntry& op5,
                                            const CheckFormatEntry& op6,
                                            const CheckFormatEntry& op7,
                                            const CheckFormatEntry& op8,
                                            const CheckFormatEntry& op9) {
  CheckFormatEntry entries[] = {op1, op2, op3, op4, op5, op6, op7, op8, op9};
  return CheckFormat(
      std::multiset<CheckFormatEntry>(entries, entries + std::size(entries)),
      json);
}

}  // namespace

TEST(SocketsManifestPermissionTest, Empty) {
  // Construction
  std::unique_ptr<SocketsManifestPermission> permission(
      new SocketsManifestPermission());
  AssertEmptyPermission(permission.get());

  // Clone()/Equal()
  std::unique_ptr<ManifestPermission> manifest_clone = permission->Clone();
  auto* clone = static_cast<SocketsManifestPermission*>(manifest_clone.get());
  AssertEmptyPermission(clone);

  EXPECT_TRUE(permission->Equal(clone));

  // ToValue()/FromValue()
  std::unique_ptr<const base::Value> value(permission->ToValue());
  EXPECT_TRUE(value.get());

  std::unique_ptr<SocketsManifestPermission> permission2(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2->FromValue(value.get()));
  AssertEmptyPermission(permission2.get());

  // Union/Diff/Intersection
  std::unique_ptr<ManifestPermission> manifest_diff = permission->Diff(clone);
  auto* diff = static_cast<SocketsManifestPermission*>(manifest_diff.get());
  AssertEmptyPermission(diff);

  std::unique_ptr<ManifestPermission> manifest_union = permission->Union(clone);
  auto* union_perm =
      static_cast<SocketsManifestPermission*>(manifest_union.get());
  AssertEmptyPermission(union_perm);

  std::unique_ptr<ManifestPermission> manifest_intersect =
      permission->Intersect(clone);
  auto* intersect =
      static_cast<SocketsManifestPermission*>(manifest_intersect.get());
  AssertEmptyPermission(intersect);
}

TEST(SocketsManifestPermissionTest, JSONFormats) {
  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"send\":\"\"}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "*:*")));
  EXPECT_TRUE(CheckFormat("{\"udp\":{\"send\":[]}}"));
  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"send\":[\"\"]}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "*:*")));
  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"send\":[\"a:80\", \"b:10\"]}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "b:10")));

  EXPECT_TRUE(
      CheckFormat("{\"udp\":{\"bind\":\"\"}}",
                  CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "*:*")));
  EXPECT_TRUE(CheckFormat("{\"udp\":{\"bind\":[]}}"));
  EXPECT_TRUE(
      CheckFormat("{\"udp\":{\"bind\":[\"\"]}}",
                  CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "*:*")));
  EXPECT_TRUE(
      CheckFormat("{\"udp\":{\"bind\":[\"a:80\", \"b:10\"]}}",
                  CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "a:80"),
                  CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "b:10")));

  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"multicastMembership\":\"\"}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, "")));
  EXPECT_TRUE(CheckFormat("{\"udp\":{\"multicastMembership\":[]}}"));
  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"multicastMembership\":[\"\"]}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, "")));
  EXPECT_TRUE(CheckFormat(
      "{\"udp\":{\"multicastMembership\":[\"\", \"\"]}}",
      CheckFormatEntry(SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, "")));

  EXPECT_TRUE(CheckFormat(
      "{\"tcp\":{\"connect\":\"\"}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "*:*")));
  EXPECT_TRUE(CheckFormat("{\"tcp\":{\"connect\":[]}}"));
  EXPECT_TRUE(CheckFormat(
      "{\"tcp\":{\"connect\":[\"\"]}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "*:*")));
  EXPECT_TRUE(CheckFormat(
      "{\"tcp\":{\"connect\":[\"a:80\", \"b:10\"]}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "b:10")));

  EXPECT_TRUE(CheckFormat(
      "{\"tcpServer\":{\"listen\":\"\"}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "*:*")));
  EXPECT_TRUE(CheckFormat("{\"tcpServer\":{\"listen\":[]}}"));
  EXPECT_TRUE(CheckFormat(
      "{\"tcpServer\":{\"listen\":[\"\"]}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "*:*")));
  EXPECT_TRUE(CheckFormat(
      "{\"tcpServer\":{\"listen\":[\"a:80\", \"b:10\"]}}",
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "b:10")));

  EXPECT_TRUE(CheckFormat(
      "{"
      "\"udp\":{"
      "\"send\":[\"a:80\", \"b:10\"],"
      "\"bind\":[\"a:80\", \"b:10\"],"
      "\"multicastMembership\":\"\""
      "},"
      "\"tcp\":{\"connect\":[\"a:80\", \"b:10\"]},"
      "\"tcpServer\":{\"listen\":[\"a:80\", \"b:10\"]}"
      "}",
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::UDP_SEND_TO, "b:10"),
      CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::UDP_BIND, "b:10"),
      CheckFormatEntry(SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP, ""),
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::TCP_CONNECT, "b:10"),
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "a:80"),
      CheckFormatEntry(SocketPermissionRequest::TCP_LISTEN, "b:10")));
}

TEST(SocketsManifestPermissionTest, FromToValue) {
  base::Value udp_send = ParsePermissionJSON(kUdpBindPermission);
  base::Value udp_bind = ParsePermissionJSON(kUdpSendPermission);
  base::Value tcp_connect = ParsePermissionJSON(kTcpConnectPermission);
  base::Value tcp_server_listen =
      ParsePermissionJSON(kTcpServerListenPermission);

  // FromValue()
  std::unique_ptr<SocketsManifestPermission> permission1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission1->FromValue(&udp_send));
  EXPECT_EQ(2u, permission1->entries().size());

  std::unique_ptr<SocketsManifestPermission> permission2(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2->FromValue(&udp_bind));
  EXPECT_EQ(2u, permission2->entries().size());

  std::unique_ptr<SocketsManifestPermission> permission3(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission3->FromValue(&tcp_connect));
  EXPECT_EQ(2u, permission3->entries().size());

  std::unique_ptr<SocketsManifestPermission> permission4(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission4->FromValue(&tcp_server_listen));
  EXPECT_EQ(2u, permission4->entries().size());

  // ToValue()
  std::unique_ptr<base::Value> value1 = permission1->ToValue();
  EXPECT_TRUE(value1);
  std::unique_ptr<SocketsManifestPermission> permission1_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission1_1->FromValue(value1.get()));
  EXPECT_TRUE(permission1->Equal(permission1_1.get()));

  std::unique_ptr<base::Value> value2 = permission2->ToValue();
  EXPECT_TRUE(value2);
  std::unique_ptr<SocketsManifestPermission> permission2_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission2_1->FromValue(value2.get()));
  EXPECT_TRUE(permission2->Equal(permission2_1.get()));

  std::unique_ptr<base::Value> value3 = permission3->ToValue();
  EXPECT_TRUE(value3);
  std::unique_ptr<SocketsManifestPermission> permission3_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission3_1->FromValue(value3.get()));
  EXPECT_TRUE(permission3->Equal(permission3_1.get()));

  std::unique_ptr<base::Value> value4 = permission4->ToValue();
  EXPECT_TRUE(value4);
  std::unique_ptr<SocketsManifestPermission> permission4_1(
      new SocketsManifestPermission());
  EXPECT_TRUE(permission4_1->FromValue(value4.get()));
  EXPECT_TRUE(permission4->Equal(permission4_1.get()));
}

TEST(SocketsManifestPermissionTest, SetOperations) {
  std::unique_ptr<SocketsManifestPermission> permission1(
      PermissionFromJSON(kUdpBindPermission));
  std::unique_ptr<SocketsManifestPermission> permission2(
      PermissionFromJSON(kUdpSendPermission));
  std::unique_ptr<SocketsManifestPermission> permission3(
      PermissionFromJSON(kTcpConnectPermission));
  std::unique_ptr<SocketsManifestPermission> permission4(
      PermissionFromJSON(kTcpServerListenPermission));

  // Union
  std::unique_ptr<ManifestPermission> manifest_union =
      permission1->Union(permission2.get());
  auto* union_perm =
      static_cast<SocketsManifestPermission*>(manifest_union.get());

  ASSERT_TRUE(union_perm);
  EXPECT_EQ(4u, union_perm->entries().size());

  EXPECT_TRUE(union_perm->Contains(permission1.get()));
  EXPECT_TRUE(union_perm->Contains(permission2.get()));
  EXPECT_FALSE(union_perm->Contains(permission3.get()));
  EXPECT_FALSE(union_perm->Contains(permission4.get()));

  // Diff
  std::unique_ptr<ManifestPermission> manifest_diff1 =
      permission1->Diff(permission2.get());
  auto* diff1 = static_cast<SocketsManifestPermission*>(manifest_diff1.get());

  ASSERT_TRUE(diff1);
  EXPECT_EQ(2u, diff1->entries().size());

  EXPECT_TRUE(permission1->Equal(diff1));
  EXPECT_TRUE(diff1->Equal(permission1.get()));

  std::unique_ptr<ManifestPermission> manifest_diff2 =
      permission1->Diff(union_perm);
  auto* diff2 = static_cast<SocketsManifestPermission*>(manifest_diff2.get());
  ASSERT_TRUE(diff2);
  AssertEmptyPermission(diff2);

  // Intersection
  std::unique_ptr<ManifestPermission> manifest_intersect1 =
      union_perm->Intersect(permission1.get());
  auto* intersect1 =
      static_cast<SocketsManifestPermission*>(manifest_intersect1.get());
  ASSERT_TRUE(intersect1);
  EXPECT_EQ(2u, intersect1->entries().size());

  EXPECT_TRUE(permission1->Equal(intersect1));
  EXPECT_TRUE(intersect1->Equal(permission1.get()));
}

}  // namespace extensions
