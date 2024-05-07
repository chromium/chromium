// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
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
  ProcessMap map(/*browser_context=*/nullptr);

  // Test behavior when empty.
  EXPECT_FALSE(map.Contains("a", 1));
  EXPECT_FALSE(map.Remove(1));
  EXPECT_EQ(0u, map.size());

  // Test insertion and behavior with one item.
  EXPECT_TRUE(map.Insert("a", 1));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_FALSE(map.Contains("a", 2));
  EXPECT_FALSE(map.Contains("b", 1));
  EXPECT_EQ(1u, map.size());

  // Test inserting a duplicate item.
  EXPECT_FALSE(map.Insert("a", 1));
  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_EQ(1u, map.size());

  // Insert some more items.
  EXPECT_TRUE(map.Insert("a", 2));
  EXPECT_TRUE(map.Insert("b", 3));
  EXPECT_TRUE(map.Insert("b", 4));
  EXPECT_EQ(4u, map.size());

  EXPECT_TRUE(map.Contains("a", 1));
  EXPECT_TRUE(map.Contains("a", 2));
  EXPECT_TRUE(map.Contains("b", 3));
  EXPECT_TRUE(map.Contains("b", 4));

  EXPECT_FALSE(map.Contains("a", 3));
  EXPECT_FALSE(map.Contains("b", 2));
  EXPECT_FALSE(map.Contains("a", 5));
  EXPECT_FALSE(map.Contains("c", 3));

  // At this point we have {a,1}, {a,2}, {b,3}, and {b,4} in the map. Test
  // removal of these processes.
  EXPECT_EQ(1, map.Remove(1));
  EXPECT_EQ(3u, map.size());
  EXPECT_FALSE(map.Contains("a", 1));
  EXPECT_TRUE(map.Contains("a", 2));

  EXPECT_EQ(1, map.Remove(2));
  EXPECT_EQ(2u, map.size());
  EXPECT_EQ(0, map.Remove(2));
  EXPECT_EQ(2u, map.size());
  EXPECT_EQ(1, map.Remove(3));
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(0, map.Remove(3));
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(1, map.Remove(4));
  EXPECT_EQ(0u, map.size());
  EXPECT_EQ(0, map.Remove(4));
  EXPECT_EQ(0u, map.size());
}

TEST(ExtensionProcessMapTest, GetMostLikelyContextType) {
  ProcessMap map(/*browser_context=*/nullptr);
  const GURL web_url("https://foo.example");
  const GURL extension_url("chrome-extension://foobar");
  const GURL untrusted_webui_url("chrome-untrusted://foo/index.html");

  EXPECT_EQ(extensions::mojom::ContextType::kWebPage,
            map.GetMostLikelyContextType(nullptr, 1, &web_url));

  scoped_refptr<const extensions::Extension> extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "a");

  EXPECT_EQ(extensions::mojom::ContextType::kContentScript,
            map.GetMostLikelyContextType(extension.get(), 2, &extension_url));

  EXPECT_EQ(extensions::mojom::ContextType::kContentScript,
            map.GetMostLikelyContextType(extension.get(), 2, &web_url));

  EXPECT_EQ(
      extensions::mojom::ContextType::kContentScript,
      map.GetMostLikelyContextType(extension.get(), 2, &untrusted_webui_url));

  map.Insert("b", 3);
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "b");
  EXPECT_EQ(extensions::mojom::ContextType::kPrivilegedExtension,
            map.GetMostLikelyContextType(extension.get(), 3, &extension_url));

  map.Insert("c", 4);
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kPlatformApp, "c");
  EXPECT_EQ(extensions::mojom::ContextType::kPrivilegedExtension,
            map.GetMostLikelyContextType(extension.get(), 4, &extension_url));

  map.set_is_lock_screen_context(true);

  map.Insert("d", 5);
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kPlatformApp, "d");
  EXPECT_EQ(extensions::mojom::ContextType::kLockscreenExtension,
            map.GetMostLikelyContextType(extension.get(), 5, &extension_url));

  map.Insert("e", 6);
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kExtension, "e");
  EXPECT_EQ(extensions::mojom::ContextType::kLockscreenExtension,
            map.GetMostLikelyContextType(extension.get(), 6, &extension_url));

  map.Insert("f", 7);
  extension =
      CreateExtensionWithFlags(extensions::TypeToCreate::kHostedApp, "f");
  EXPECT_EQ(extensions::mojom::ContextType::kPrivilegedWebPage,
            map.GetMostLikelyContextType(extension.get(), 7, &web_url));

  map.Insert("g", 8);
  EXPECT_EQ(extensions::mojom::ContextType::kUntrustedWebUi,
            map.GetMostLikelyContextType(/*extension=*/nullptr, 8,
                                         &untrusted_webui_url));

  map.Insert("h", 9);
  EXPECT_EQ(extensions::mojom::ContextType::kWebPage,
            map.GetMostLikelyContextType(/*extension=*/nullptr, 9, &web_url));
}
