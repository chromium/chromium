// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/info_map.h"

#include "base/path_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extensions::manifest_keys;

namespace extensions {

class InfoMapTest : public testing::Test {
 public:
  InfoMapTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Returns a barebones test Extension object with the given name.
static scoped_refptr<const Extension> CreateExtension(const std::string& name) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);

  return ExtensionBuilder(name).SetPath(path.AppendASCII(name)).Build();
}

// Test that the InfoMap handles refcounting properly.
TEST_F(InfoMapTest, RefCounting) {
  scoped_refptr<InfoMap> info_map(new InfoMap());

  // New extensions should have a single reference holding onto them.
  scoped_refptr<const Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<const Extension> extension2(CreateExtension("extension2"));
  scoped_refptr<const Extension> extension3(CreateExtension("extension3"));
  EXPECT_TRUE(extension1->HasOneRef());
  EXPECT_TRUE(extension2->HasOneRef());
  EXPECT_TRUE(extension3->HasOneRef());

  // Add a ref to each extension and give it to the info map.
  info_map->AddExtension(extension1.get(), base::Time(), false, false);
  info_map->AddExtension(extension2.get(), base::Time(), false, false);
  info_map->AddExtension(extension3.get(), base::Time(), false, false);

  // Release extension1, and the info map should have the only ref.
  const Extension* weak_extension1 = extension1.get();
  extension1.reset();
  EXPECT_TRUE(weak_extension1->HasOneRef());

  // Remove extension2, and the extension2 object should have the only ref.
  info_map->RemoveExtension(extension2->id(),
                            UnloadedExtensionReason::UNINSTALL);
  EXPECT_TRUE(extension2->HasOneRef());

  // Delete the info map, and the extension3 object should have the only ref.
  info_map.reset();
  EXPECT_TRUE(extension3->HasOneRef());
}

// Tests that we can query a few extension properties from the InfoMap.
TEST_F(InfoMapTest, Properties) {
  scoped_refptr<InfoMap> info_map(new InfoMap());

  scoped_refptr<const Extension> extension1(CreateExtension("extension1"));
  scoped_refptr<const Extension> extension2(CreateExtension("extension2"));

  info_map->AddExtension(extension1.get(), base::Time(), false, false);
  info_map->AddExtension(extension2.get(), base::Time(), false, false);

  EXPECT_EQ(2u, info_map->extensions().size());
  EXPECT_EQ(extension1.get(), info_map->extensions().GetByID(extension1->id()));
  EXPECT_EQ(extension2.get(), info_map->extensions().GetByID(extension2->id()));
}

}  // namespace extensions
