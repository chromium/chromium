// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_TEST_UTIL_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_TEST_UTIL_H_

#include <string>
#include <vector>

#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class PermissionsData;
class PermissionSet;

testing::AssertionResult VerifyHasPermissionMessage(
    const PermissionsData* permissions_data,
    const std::u16string& expected_message);
testing::AssertionResult VerifyHasPermissionMessage(
    const PermissionSet& permissions,
    Manifest::Type extension_type,
    const std::string& expected_message);

testing::AssertionResult VerifyNoPermissionMessages(
    const PermissionsData* permissions_data);

testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionsData* permissions_data,
    const std::string& expected_message);
testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionsData* permissions_data,
    const std::u16string& expected_message);
testing::AssertionResult VerifyOnePermissionMessage(
    const PermissionSet& permissions,
    Manifest::Type extension_type,
    const std::u16string& expected_message);

testing::AssertionResult VerifyOnePermissionMessageWithSubmessages(
    const PermissionsData* permissions_data,
    const std::string& expected_message,
    const std::vector<std::string>& expected_submessages);

testing::AssertionResult VerifyTwoPermissionMessages(
    const PermissionsData* permissions_data,
    const std::string& expected_message_1,
    const std::string& expected_message_2,
    bool check_order);

testing::AssertionResult VerifyPermissionMessages(
    const PermissionsData* permissions_data,
    const std::vector<std::string>& expected_messages,
    bool check_order);
testing::AssertionResult VerifyPermissionMessages(
    const PermissionsData* permissions_data,
    const std::vector<std::u16string>& expected_messages,
    bool check_order);

testing::AssertionResult VerifyPermissionMessagesWithSubmessages(
    const PermissionsData* permissions_data,
    const std::vector<std::string>& expected_messages,
    const std::vector<std::vector<std::string>>& expected_submessages,
    bool check_order);
testing::AssertionResult VerifyPermissionMessagesWithSubmessages(
    const PermissionsData* permissions_data,
    const std::vector<std::u16string>& expected_messages,
    const std::vector<std::vector<std::u16string>>& expected_submessages,
    bool check_order);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSION_MESSAGE_TEST_UTIL_H_
