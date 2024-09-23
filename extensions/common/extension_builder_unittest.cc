// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"

#include "base/values.h"
#include "base/version.h"
#include "components/version_info/channel.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionBuilderTest, Basic) {
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("some name").Build();
    ASSERT_TRUE(extension);
    EXPECT_EQ("some name", extension->name());
    EXPECT_TRUE(extension->is_extension());
    EXPECT_EQ(3, extension->manifest_version());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("some app", ExtensionBuilder::Type::PLATFORM_APP)
            .Build();
    ASSERT_TRUE(extension);
    EXPECT_EQ("some app", extension->name());
    EXPECT_TRUE(extension->is_platform_app());
    EXPECT_EQ(2, extension->manifest_version());
  }
}

TEST(ExtensionBuilderTest, AddAPIPermission) {
  // MV2 API permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(2).Build();
    EXPECT_TRUE(extension->permissions_data()->active_permissions().IsEmpty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(2)
            .AddAPIPermission("storage")
            .AddAPIPermissions({"alarms", "idle"})
            .Build();
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("storage"));
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("alarms"));
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("idle"));
  }

  // MV3 API permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(3).Build();
    EXPECT_TRUE(extension->permissions_data()->active_permissions().IsEmpty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(3)
            .AddAPIPermission("storage")
            .AddAPIPermissions({"alarms", "idle"})
            .Build();
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("storage"));
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("alarms"));
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission("idle"));
  }
}

TEST(ExtensionBuilderTest, AddOptionalAPIPermission) {
  // MV2 optional API permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(2).Build();
    EXPECT_TRUE(
        PermissionsParser::GetOptionalPermissions(extension.get()).IsEmpty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(2)
            .AddOptionalAPIPermission("storage")
            .AddOptionalAPIPermissions({"alarms", "idle"})
            .Build();
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("storage"));
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("alarms"));
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("idle"));
  }

  // MV3 optional API permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(3).Build();
    EXPECT_TRUE(
        PermissionsParser::GetOptionalPermissions(extension.get()).IsEmpty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(3)
            .AddOptionalAPIPermission("storage")
            .AddOptionalAPIPermissions({"alarms", "idle"})
            .Build();
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("storage"));
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("alarms"));
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .HasAPIPermission("idle"));
  }
}

TEST(ExtensionBuilderTest, AddHostPermission) {
  // MV2 host permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(2).Build();
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .effective_hosts()
                    .is_empty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(2)
            .AddHostPermission("*://one.example/*")
            .AddHostPermissions({"*://two.example/*", "*://three.example/*"})
            .Build();
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://one.example")));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://two.example")));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://three.example")));
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(GURL("http://four.example")));
  }

  // MV3 host permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(3).Build();
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .effective_hosts()
                    .is_empty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(3)
            .AddHostPermission("*://one.example/*")
            .AddHostPermissions({"*://two.example/*", "*://three.example/*"})
            .Build();
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://one.example")));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://two.example")));
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .HasExplicitAccessToOrigin(GURL("http://three.example")));
    EXPECT_FALSE(extension->permissions_data()
                     ->active_permissions()
                     .HasExplicitAccessToOrigin(GURL("http://four.example")));
  }
}

TEST(ExtensionBuilderTest, AddOptionalHostPermission) {
  // MV2 optional host permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(2).Build();
    EXPECT_TRUE(PermissionsParser::GetOptionalPermissions(extension.get())
                    .effective_hosts()
                    .is_empty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(2)
            .AddOptionalHostPermission("*://one.example/*")
            .AddOptionalHostPermissions(
                {"*://two.example/*", "*://three.example/*"})
            .Build();
    const PermissionSet& optional_permissions =
        PermissionsParser::GetOptionalPermissions(extension.get());
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://one.example")));
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://two.example")));
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://three.example")));
    EXPECT_FALSE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://four.example")));
  }

  // MV3 optional host permissions.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no permissions").SetManifestVersion(3).Build();
    EXPECT_TRUE(extension->permissions_data()
                    ->active_permissions()
                    .effective_hosts()
                    .is_empty());
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("api permissions")
            .SetManifestVersion(3)
            .AddOptionalHostPermission("*://one.example/*")
            .AddOptionalHostPermissions(
                {"*://two.example/*", "*://three.example/*"})
            .Build();
    const PermissionSet& optional_permissions =
        PermissionsParser::GetOptionalPermissions(extension.get());
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://one.example")));
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://two.example")));
    EXPECT_TRUE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://three.example")));
    EXPECT_FALSE(optional_permissions.HasExplicitAccessToOrigin(
        GURL("http://four.example")));
  }
}

TEST(ExtensionBuilderTest, Actions) {
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no action").Build();
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kPageAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kBrowserAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kAction));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("page action")
            .SetManifestVersion(2)
            .SetAction(ActionInfo::Type::kPage)
            .Build();
    EXPECT_TRUE(extension->manifest()->FindKey(manifest_keys::kPageAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kBrowserAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kAction));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("browser action")
            .SetManifestVersion(2)
            .SetAction(ActionInfo::Type::kBrowser)
            .Build();
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kPageAction));
    EXPECT_TRUE(extension->manifest()->FindKey(manifest_keys::kBrowserAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kAction));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("action").SetAction(ActionInfo::Type::kAction).Build();
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kPageAction));
    EXPECT_FALSE(extension->manifest()->FindKey(manifest_keys::kBrowserAction));
    EXPECT_TRUE(extension->manifest()->FindKey(manifest_keys::kAction));
  }
}

TEST(ExtensionBuilderTest, Background) {
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("no background").Build();
    EXPECT_FALSE(BackgroundInfo::HasBackgroundPage(extension.get()));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("persistent background page")
            .SetManifestVersion(2)
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::BACKGROUND_PAGE)
            .Build();
    EXPECT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
    EXPECT_TRUE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));
    EXPECT_FALSE(BackgroundInfo::IsServiceWorkerBased(extension.get()));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("event page")
            .SetManifestVersion(2)
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::EVENT_PAGE)
            .Build();
    EXPECT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
    EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));
    EXPECT_FALSE(BackgroundInfo::IsServiceWorkerBased(extension.get()));
  }
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("service worker")
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
            .Build();
    EXPECT_FALSE(BackgroundInfo::HasBackgroundPage(extension.get()));
    EXPECT_FALSE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));
    EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));
    EXPECT_TRUE(BackgroundInfo::IsServiceWorkerBased(extension.get()));
    EXPECT_EQ(
        ExtensionBuilder::kServiceWorkerScriptFile,
        BackgroundInfo::GetBackgroundServiceWorkerScript(extension.get()));
  }
}

TEST(ExtensionBuilderTest, MergeManifest) {
  auto connectable = base::Value::Dict().Set(
      "matches", base::Value::List().Append("*://example.com/*"));
  base::Value::Dict connectable_value =
      base::Value::Dict().Set("externally_connectable", std::move(connectable));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extra")
          .MergeManifest(std::move(connectable_value))
          .Build();
  EXPECT_TRUE(ExternallyConnectableInfo::Get(extension.get()));
}

TEST(ExtensionBuilderTest, IDUniqueness) {
  scoped_refptr<const Extension> a = ExtensionBuilder("a").Build();
  scoped_refptr<const Extension> b = ExtensionBuilder("b").Build();
  scoped_refptr<const Extension> c = ExtensionBuilder("c").Build();

  std::set<ExtensionId> ids = {a->id(), b->id(), c->id()};
  EXPECT_EQ(3u, ids.size());
}

TEST(ExtensionBuilderTest, SetManifestAndMergeManifest) {
  auto manifest = base::Value::Dict()
                      .Set("name", "some name")
                      .Set("manifest_version", 2)
                      .Set("description", "some description");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .MergeManifest(base::Value::Dict().Set("version", "0.1"))
          .Build();
  EXPECT_EQ("some name", extension->name());
  EXPECT_EQ(2, extension->manifest_version());
  EXPECT_EQ("some description", extension->description());
  EXPECT_EQ("0.1", extension->version().GetString());
}

TEST(ExtensionBuilderTest, MergeManifestOverridesValues) {
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("foo")
            .MergeManifest(base::Value::Dict().Set("version", "52.0.9"))
            .Build();
    // MergeManifest() should have overwritten the default 0.1 value for
    // version.
    EXPECT_EQ("52.0.9", extension->version().GetString());
  }

  {
    auto manifest = base::Value::Dict()
                        .Set("name", "some name")
                        .Set("manifest_version", 2)
                        .Set("description", "some description")
                        .Set("version", "0.1");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .MergeManifest(base::Value::Dict().Set("version", "42.1"))
            .Build();
    EXPECT_EQ("42.1", extension->version().GetString());
  }
}

TEST(ExtensionBuilderTest, SetManifestKey) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .SetManifestKey("short_name", "short name")
          .Build();
  EXPECT_EQ("short name", extension->short_name());
}

TEST(ExtensionBuilderTest, AddContentScript) {
  constexpr char kScriptOne[] = "one.js";
  const std::vector<std::string> script_one_patterns = {
      "https://example.com/*", "https://chromium.org/foo"};
  constexpr char kScriptTwo[] = "two.js";
  const std::vector<std::string> script_two_patterns = {"https://google.com/*"};
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo")
          .AddContentScript(kScriptOne, script_one_patterns)
          .AddContentScript(kScriptTwo, script_two_patterns)
          .Build();

  const UserScriptList& content_scripts =
      ContentScriptsInfo::GetContentScripts(extension.get());
  auto script_by_name =
      [&content_scripts](const char* name) -> const extensions::UserScript* {
    for (const auto& script : content_scripts) {
      if (script->js_scripts()[0]->relative_path().AsUTF8Unsafe() == name) {
        return script.get();
      }
    }
    return nullptr;
  };

  auto patterns_as_strings = [](const URLPatternSet& patterns) {
    std::vector<std::string> strings;
    for (const auto& pattern : patterns)
      strings.push_back(pattern.GetAsString());
    return strings;
  };

  {
    const UserScript* script_one = script_by_name(kScriptOne);
    ASSERT_TRUE(script_one);
    EXPECT_THAT(patterns_as_strings(script_one->url_patterns()),
                testing::UnorderedElementsAreArray(script_one_patterns));
  }

  {
    const UserScript* script_two = script_by_name(kScriptTwo);
    ASSERT_TRUE(script_two);
    EXPECT_THAT(patterns_as_strings(script_two->url_patterns()),
                testing::UnorderedElementsAreArray(script_two_patterns));
  }
}

TEST(ExtensionBuilderTest, SetVersion) {
  constexpr char kVersion[] = "42.0.99.1";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo").SetVersion(kVersion).Build();
  EXPECT_EQ(kVersion, extension->VersionString());
}

}  // namespace extensions
