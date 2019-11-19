// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/extensions_api_permissions.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void CompareExtension(const Extension& extension1,
                      const Extension& extension2) {
  EXPECT_EQ(extension1.name(), extension2.name());
  EXPECT_EQ(extension1.id(), extension2.id());
  EXPECT_EQ(extension1.path(), extension2.path());
  EXPECT_EQ(extension1.permissions_data()->active_permissions(),
            extension2.permissions_data()->active_permissions());
  EXPECT_TRUE(extension1.manifest()->Equals(extension2.manifest()));
  const PermissionsData::TabPermissionsMap& second_tab_permissions =
      extension2.permissions_data()->tab_specific_permissions();
  for (const auto& tab_permissions :
       extension1.permissions_data()->tab_specific_permissions()) {
    ASSERT_NE(0u, second_tab_permissions.count(tab_permissions.first));
    EXPECT_EQ(*tab_permissions.second,
              *(second_tab_permissions.at(tab_permissions.first)))
        << tab_permissions.first;
  }
  EXPECT_EQ(extension1.permissions_data()->policy_blocked_hosts(),
            extension2.permissions_data()->policy_blocked_hosts());
  EXPECT_EQ(extension1.permissions_data()->policy_allowed_hosts(),
            extension2.permissions_data()->policy_allowed_hosts());
}

void AddPattern(const std::string& pattern, URLPatternSet* extent) {
  URLPattern parsed(URLPattern::SCHEME_ALL);
  parsed.Parse(pattern);
  extent->AddPattern(parsed);
}

}  // namespace

TEST(ExtensionMessageTypesTest, TestLoadedParams) {
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "extension")
          .Set("description", "an extension")
          .Set("permissions", ListBuilder().Append("alarms").Build())
          .Set("manifest_version", 2)
          .Set("version", "0.1")
          .Build();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(crx_file::id_util::GenerateId("foo"))
          .Build();
  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension.get());
  LOG(WARNING) << required_permissions.apis().size();
  EXPECT_TRUE(
      extension->permissions_data()->HasAPIPermission(APIPermission::kAlarms));
  {
    APIPermissionSet tab_permissions;
    tab_permissions.insert(APIPermission::kDns);
    extension->permissions_data()->UpdateTabSpecificPermissions(
        1, PermissionSet(std::move(tab_permissions), ManifestPermissionSet(),
                         URLPatternSet(), URLPatternSet()));
  }
  URLPatternSet runtime_blocked_hosts;
  AddPattern("*://*.example.com/*", &runtime_blocked_hosts);
  URLPatternSet runtime_allowed_hosts;
  AddPattern("*://good.example.com/*", &runtime_allowed_hosts);
  extension->permissions_data()->SetPolicyHostRestrictions(
      runtime_blocked_hosts, runtime_allowed_hosts);

  ExtensionMsg_Loaded_Params params_in(extension.get(), true);
  EXPECT_EQ(extension->id(), params_in.id);

  {
    // First, test just converting back to an extension.
    std::string error;
    scoped_refptr<const Extension> extension_out =
        params_in.ConvertToExtension(&error);
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(extension_out);
    CompareExtension(*extension, *extension_out);
  }

  {
    // Second, try bouncing the params and then converting back.
    IPC::Message msg;
    IPC::ParamTraits<ExtensionMsg_Loaded_Params>::Write(&msg, params_in);
    ExtensionMsg_Loaded_Params params_out;
    base::PickleIterator iter(msg);
    EXPECT_TRUE(IPC::ParamTraits<ExtensionMsg_Loaded_Params>::Read(
        &msg, &iter, &params_out));

    EXPECT_EQ(params_in.id, params_out.id);
    EXPECT_TRUE(params_in.manifest.Equals(&params_out.manifest));
    EXPECT_EQ(params_in.location, params_out.location);
    EXPECT_EQ(params_in.path, params_out.path);
    EXPECT_EQ(params_in.creation_flags, params_out.creation_flags);
    // Permission equaliy on the params will be tested through the conversion to
    // an extension.

    std::string error;
    scoped_refptr<const Extension> extension_out =
        params_out.ConvertToExtension(&error);
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(extension_out);
    CompareExtension(*extension, *extension_out);
  }
}

}  // namespace extensions
