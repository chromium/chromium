// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/install_prompt_permissions.h"

#include <string>
#include <vector>

#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(InstallPromptPermissionsTest, NoSubmessages) {
  PermissionMessages messages;
  messages.emplace_back(u"Main permission", PermissionIDSet());

  InstallPromptPermissions prompt_permissions;
  prompt_permissions.AddPermissionMessages(messages);

  ASSERT_EQ(1u, prompt_permissions.permissions.size());
  EXPECT_EQ(u"Main permission", prompt_permissions.permissions[0]);
  EXPECT_TRUE(prompt_permissions.details[0].empty());
  EXPECT_TRUE(prompt_permissions.visible_details[0].empty());
  EXPECT_TRUE(prompt_permissions.collapsed_details[0].empty());
}

TEST(InstallPromptPermissionsTest, ThreeOrFewerSubmessagesAllVisible) {
  PermissionMessages messages;
  messages.emplace_back(
      u"Main permission", PermissionIDSet(),
      std::vector<std::u16string>{u"Detail 1", u"Detail 2", u"Detail 3"});

  InstallPromptPermissions prompt_permissions;
  prompt_permissions.AddPermissionMessages(messages);

  ASSERT_EQ(1u, prompt_permissions.permissions.size());
  EXPECT_EQ(u"Main permission", prompt_permissions.permissions[0]);
  EXPECT_EQ(u"• Detail 1\n• Detail 2\n• Detail 3",
            prompt_permissions.details[0]);
  EXPECT_EQ(u"• Detail 1\n• Detail 2\n• Detail 3",
            prompt_permissions.visible_details[0]);
  EXPECT_TRUE(prompt_permissions.collapsed_details[0].empty());
}

TEST(InstallPromptPermissionsTest, MoreThanThreeSubmessagesSplit) {
  PermissionMessages messages;
  messages.emplace_back(
      u"Main permission", PermissionIDSet(),
      std::vector<std::u16string>{u"Detail 1", u"Detail 2", u"Detail 3",
                                  u"Detail 4", u"Detail 5"});

  InstallPromptPermissions prompt_permissions;
  prompt_permissions.AddPermissionMessages(messages);

  ASSERT_EQ(1u, prompt_permissions.permissions.size());
  EXPECT_EQ(u"Main permission", prompt_permissions.permissions[0]);
  EXPECT_EQ(u"• Detail 1\n• Detail 2\n• Detail 3\n• Detail 4\n• Detail 5",
            prompt_permissions.details[0]);
  EXPECT_EQ(u"• Detail 1\n• Detail 2\n• Detail 3",
            prompt_permissions.visible_details[0]);
  EXPECT_EQ(u"• Detail 4\n• Detail 5", prompt_permissions.collapsed_details[0]);
}

}  // namespace extensions
