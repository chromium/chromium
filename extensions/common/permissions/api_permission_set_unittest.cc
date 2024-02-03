// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pickle.h"
#include "base/values.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

TEST(APIPermissionSetTest, General) {
  APIPermissionSet apis;
  apis.insert(APIPermissionID::kAudioCapture);
  apis.insert(APIPermissionID::kDns);
  apis.insert(APIPermissionID::kHid);
  apis.insert(APIPermissionID::kPower);
  apis.insert(APIPermissionID::kSerial);

  EXPECT_EQ(apis.find(APIPermissionID::kPower)->id(), APIPermissionID::kPower);
  EXPECT_TRUE(apis.find(APIPermissionID::kSocket) == apis.end());

  EXPECT_EQ(apis.size(), 5u);

  EXPECT_EQ(apis.erase(APIPermissionID::kAudioCapture), 1u);
  EXPECT_EQ(apis.size(), 4u);

  EXPECT_EQ(apis.erase(APIPermissionID::kAudioCapture), 0u);
  EXPECT_EQ(apis.size(), 4u);
}

TEST(APIPermissionSetTest, CreateUnion) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);
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

  // Union with an empty set.
  apis1.insert(APIPermissionID::kAudioCapture);
  apis1.insert(APIPermissionID::kDns);
  apis1.insert(permission->Clone());
  expected_apis.insert(APIPermissionID::kAudioCapture);
  expected_apis.insert(APIPermissionID::kDns);
  expected_apis.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermissionID::kAudioCapture);
  apis2.insert(APIPermissionID::kHid);
  apis2.insert(APIPermissionID::kPower);
  apis2.insert(APIPermissionID::kSerial);

  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kAudioCapture);
  expected_apis.insert(APIPermissionID::kHid);
  expected_apis.insert(APIPermissionID::kPower);
  expected_apis.insert(APIPermissionID::kSerial);

  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  // Insert a new socket permission which will replace the old one.
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Union(apis1, apis2, &result);

  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(apis2.Contains(result));
  EXPECT_TRUE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateIntersection) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);

  // Intersection with an empty set.
  apis1.insert(APIPermissionID::kAudioCapture);
  apis1.insert(APIPermissionID::kDns);
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
  apis1.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Intersection(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_TRUE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_TRUE(result.Contains(apis2));

  EXPECT_TRUE(result.empty());
  EXPECT_EQ(expected_apis, result);

  // Now use a real second set.
  apis2.insert(APIPermissionID::kAudioCapture);
  apis2.insert(APIPermissionID::kHid);
  apis2.insert(APIPermissionID::kPower);
  apis2.insert(APIPermissionID::kSerial);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kAudioCapture);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Intersection(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_TRUE(apis2.Contains(result));
  EXPECT_FALSE(apis1.Contains(apis2));
  EXPECT_FALSE(apis2.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis1));
  EXPECT_FALSE(result.Contains(apis2));

  EXPECT_EQ(expected_apis, result);
}

TEST(APIPermissionSetTest, CreateDifference) {
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;
  APIPermissionSet result;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);

  // Difference with an empty set.
  apis1.insert(APIPermissionID::kAudioCapture);
  apis1.insert(APIPermissionID::kDns);
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
  apis1.insert(std::move(permission));

  ASSERT_TRUE(apis2.empty());
  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_EQ(apis1, result);

  // Now use a real second set.
  apis2.insert(APIPermissionID::kAudioCapture);
  apis2.insert(APIPermissionID::kHid);
  apis2.insert(APIPermissionID::kPower);
  apis2.insert(APIPermissionID::kSerial);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kDns);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  expected_apis.insert(std::move(permission));

  APIPermissionSet::Difference(apis1, apis2, &result);

  EXPECT_TRUE(apis1.Contains(result));
  EXPECT_FALSE(apis2.Contains(result));

  EXPECT_EQ(expected_apis, result);

  // |result| = |apis1| - |apis2| --> |result| intersect |apis2| == empty_set
  APIPermissionSet result2;
  APIPermissionSet::Intersection(result, apis2, &result2);
  EXPECT_TRUE(result2.empty());
}

}  // namespace extensions
