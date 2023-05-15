// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

enum class TypeToCreate { kExtension, kHostedApp, kPlatformApp };

scoped_refptr<const Extension> CreateExtensionWithFlags(TypeToCreate type,
                                                        const std::string& id) {
  auto manifest_builder = base::Value::Dict()
                              .Set("name", "Test extension")
                              .Set("version", "1.0")
                              .Set("manifest_version", 2);

  switch (type) {
    case TypeToCreate::kExtension:
      manifest_builder.Set(
          "background",
          base::Value::Dict().Set("scripts",
                                  base::Value::List().Append("background.js")));
      break;
    case TypeToCreate::kHostedApp:
      manifest_builder.Set(
          "app", base::Value::Dict().Set(
                     "launch", base::Value::Dict().Set("web_url",
                                                       "https://www.foo.bar")));
      break;
    case TypeToCreate::kPlatformApp:
      manifest_builder.Set(
          "app",
          base::Value::Dict().Set(
              "background",
              base::Value::Dict().Set(
                  "scripts", base::Value::List().Append("background.js"))));
      break;
  }

  return ExtensionBuilder()
      .SetID(id)
      .SetManifest(std::move(manifest_builder))
      .Build();
}

}  // namespace
}  // namespace extensions

using extensions::ProcessMap;

TEST(ExtensionProcessMapTest, Test) {
  ProcessMap map;

  // Test behavior when empty.
  EXPECT_FALSE(map.Contains("a", 1));
  EXPECT_FALSE(map.Remove("a", 1, content::SiteInstanceId(1)));
  EXPECT_EQ(0u, map.size());

  // Test insertion and behavior with one item.
  EXPECT_TRUE(map.Insert("a", 1, content::SiteInstanceId(1)));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_FALSE(map.Contains("a", 2));
  EXPECT_FALSE(map.Contains("b", 1));
  EXPECT_EQ(1u, map.size());

  // Test inserting a duplicate item.
  EXPECT_FALSE(map.Insert("a", 1, content::SiteInstanceId(1)));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_EQ(1u, map.size());

  // Insert some more items.
  EXPECT_TRUE(map.Insert("a", 2, content::SiteInstanceId(2)));
  EXPECT_TRUE(map.Insert("b", 1, content::SiteInstanceId(3)));
  EXPECT_TRUE(map.Insert("b", 2, content::SiteInstanceId(4)));
  EXPECT_EQ(4u, map.size());

  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_TRUE(map.Contains("a", 2));
  EXPECT_TRUE(map.Contains("b", 1));
  EXPECT_TRUE(map.Contains("b", 2));
  EXPECT_FALSE(map.Contains("a", 3));

  // Note that this only differs from an existing item because of the site
  // instance id.
  EXPECT_TRUE(map.Insert("a", 1, content::SiteInstanceId(5)));
  EXPECT_TRUE(map.Contains("a", 1));

  // Test removal.
  EXPECT_TRUE(map.Remove("a", 1, content::SiteInstanceId(1)));
  EXPECT_FALSE(map.Remove("a", 1, content::SiteInstanceId(1)));
  EXPECT_EQ(4u, map.size());

  // Should still return true because there were two site instances for this
  // extension/process pair.
  EXPECT_TRUE(map.Contains("a", 1));

  EXPECT_TRUE(map.Remove("a", 1, content::SiteInstanceId(5)));
  EXPECT_EQ(3u, map.size());
  EXPECT_FALSE(map.Contains("a", 1));

  EXPECT_EQ(2, map.RemoveAllFromProcess(2));
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(0, map.RemoveAllFromProcess(2));
  EXPECT_EQ(1u, map.size());
}

TEST(ExtensionProcessMapTest, GetMostLikelyContextType) {
  ProcessMap map;
  const GURL web_url("https://foo.example");
  const GURL extension_url("chrome-extension://foobar");
  const GURL untrusted_webui_url("chrome-untrusted://foo/index.html");

  EXPECT_EQ(extensions::Feature::WEB_PAGE_CONTEXT,
            map.GetMostLikelyContextType(nullptr, 1, &web_url));

  scoped_refptr<const extensions::Extension> extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "a");

  EXPECT_EQ(extensions::Feature::CONTENT_SCRIPT_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 2, &extension_url));

  EXPECT_EQ(extensions::Feature::CONTENT_SCRIPT_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 2, &web_url));

  EXPECT_EQ(
      extensions::Feature::CONTENT_SCRIPT_CONTEXT,
      map.GetMostLikelyContextType(extension.get(), 2, &untrusted_webui_url));

  map.Insert("b", 3, content::SiteInstanceId(1));
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "b");
  EXPECT_EQ(extensions::Feature::BLESSED_EXTENSION_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 3, &extension_url));

  map.Insert("c", 4, content::SiteInstanceId(2));
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kPlatformApp, "c");
  EXPECT_EQ(extensions::Feature::BLESSED_EXTENSION_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 4, &extension_url));

  map.set_is_lock_screen_context(true);

  map.Insert("d", 5, content::SiteInstanceId(3));
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kPlatformApp, "d");
  EXPECT_EQ(extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 5, &extension_url));

  map.Insert("e", 6, content::SiteInstanceId(4));
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "e");
  EXPECT_EQ(extensions::Feature::LOCK_SCREEN_EXTENSION_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 6, &extension_url));

  map.Insert("f", 7, content::SiteInstanceId(5));
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kHostedApp, "f");
  EXPECT_EQ(extensions::Feature::BLESSED_WEB_PAGE_CONTEXT,
            map.GetMostLikelyContextType(extension.get(), 7, &web_url));

  map.Insert("g", 8, content::SiteInstanceId(6));
  EXPECT_EQ(extensions::Feature::WEBUI_UNTRUSTED_CONTEXT,
            map.GetMostLikelyContextType(/*extension=*/nullptr, 8,
                                         &untrusted_webui_url));

  map.Insert("h", 9, content::SiteInstanceId(7));
  EXPECT_EQ(extensions::Feature::WEB_PAGE_CONTEXT,
            map.GetMostLikelyContextType(/*extension=*/nullptr, 9, &web_url));
}
