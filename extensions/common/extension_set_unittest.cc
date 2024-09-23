// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_set.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "build/build_config.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             const std::string& launch_url,
                                             const std::string& extent) {
#if BUILDFLAG(IS_WIN)
  base::FilePath path(FILE_PATH_LITERAL("c:\\"));
#else
  base::FilePath path(FILE_PATH_LITERAL("/"));
#endif
  path = path.AppendASCII(name);

  auto manifest = base::Value::Dict()
                      .Set("name", name)
                      .Set("version", "1")
                      .Set("manifest_version", 2);

  if (!launch_url.empty())
    manifest.SetByDottedPath("app.launch.web_url", launch_url);

  if (!extent.empty()) {
    base::Value::List urls;
    urls.Append(extent);
    manifest.SetByDottedPath("app.urls", std::move(urls));
  }

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, mojom::ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

}  // namespace

TEST(ExtensionSetTest, ExtensionSet) {
  scoped_refptr<Extension> ext1(CreateTestExtension(
      "a", "https://chrome.google.com/launch", "https://chrome.google.com/"));

  scoped_refptr<Extension> ext2(CreateTestExtension(
      "a", "http://code.google.com/p/chromium",
      "http://code.google.com/p/chromium/"));

  scoped_refptr<Extension> ext3(CreateTestExtension(
      "b", "http://dev.chromium.org/", "http://dev.chromium.org/"));

  scoped_refptr<Extension> ext4(
      CreateTestExtension("c", std::string(), std::string()));

  ASSERT_TRUE(ext1.get() && ext2.get() && ext3.get() && ext4.get());

  ExtensionSet extensions;

  // Add an extension.
  EXPECT_TRUE(extensions.Insert(ext1));
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext1.get(), extensions.GetByID(ext1->id()));

  // Since extension2 has same ID, it should overwrite extension1.
  EXPECT_FALSE(extensions.Insert(ext2));
  EXPECT_EQ(1u, extensions.size());
  EXPECT_EQ(ext2.get(), extensions.GetByID(ext1->id()));

  // Add the other extensions.
  EXPECT_TRUE(extensions.Insert(ext3));
  EXPECT_TRUE(extensions.Insert(ext4));
  EXPECT_EQ(3u, extensions.size());

  // Get extension by its chrome-extension:// URL
  EXPECT_EQ(
      ext2.get(),
      extensions.GetExtensionOrAppByURL(ext2->GetResourceURL("test.html")));
  EXPECT_EQ(
      ext3.get(),
      extensions.GetExtensionOrAppByURL(ext3->GetResourceURL("test.html")));
  EXPECT_EQ(
      ext4.get(),
      extensions.GetExtensionOrAppByURL(ext4->GetResourceURL("test.html")));

  // Get extension by a filesystem or blob URL within it.
  GURL ext2_filesystem_url =
      GURL("filesystem:" + ext2->GetResourceURL("test.html").spec());
  EXPECT_EQ(ext2.get(), extensions.GetExtensionOrAppByURL(ext2_filesystem_url));
  EXPECT_EQ(ext2->id(),
            extensions.GetExtensionOrAppIDByURL(ext2_filesystem_url));
  GURL ext3_blob_url = GURL("blob:" + ext3->GetResourceURL("test.html").spec());
  EXPECT_EQ(ext3.get(), extensions.GetExtensionOrAppByURL(ext3_blob_url));
  EXPECT_EQ(ext3->id(), extensions.GetExtensionOrAppIDByURL(ext3_blob_url));

  // Get extension by web extent.
  EXPECT_EQ(ext2.get(),
            extensions.GetExtensionOrAppByURL(
                GURL("http://code.google.com/p/chromium/monkey")));
  EXPECT_EQ(ext3.get(),
            extensions.GetExtensionOrAppByURL(
                GURL("http://dev.chromium.org/design-docs/")));
  EXPECT_FALSE(extensions.GetExtensionOrAppByURL(
      GURL("http://blog.chromium.org/")));

  // Get extension by web extent with filesystem URL. Paths still matter.
  EXPECT_EQ(ext3.get(), extensions.GetExtensionOrAppByURL(
                            GURL("filesystem:http://dev.chromium.org/foo")));
  EXPECT_EQ(ext3->id(), extensions.GetExtensionOrAppIDByURL(
                            GURL("filesystem:http://dev.chromium.org/foo")));
  EXPECT_EQ(nullptr, extensions.GetExtensionOrAppByURL(
                         GURL("filesystem:http://code.google.com/foo")));
  // TODO(crbug.com/41394231): Support blob URLs. This should return ext3.
  EXPECT_EQ(nullptr, extensions.GetExtensionOrAppByURL(
                         GURL("blob:http://dev.chromium.org/abcd")));

  // Test InSameExtent().
  EXPECT_TRUE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/monkey/"),
      GURL("http://code.google.com/p/chromium/")));
  EXPECT_FALSE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/"),
      GURL("https://code.google.com/p/chromium/")));
  EXPECT_FALSE(extensions.InSameExtent(
      GURL("http://code.google.com/p/chromium/"),
      GURL("http://dev.chromium.org/design-docs/")));

  // Both of these should be NULL, which mean true for InSameExtent.
  EXPECT_TRUE(extensions.InSameExtent(
      GURL("http://www.google.com/"),
      GURL("http://blog.chromium.org/")));

  // Remove one of the extensions.
  EXPECT_TRUE(extensions.Remove(ext2->id()));
  EXPECT_EQ(2u, extensions.size());
  EXPECT_FALSE(extensions.GetByID(ext2->id()));

  // Make a union of a set with 3 more extensions (only 2 are new).
  scoped_refptr<Extension> ext5(
      CreateTestExtension("d", std::string(), std::string()));
  scoped_refptr<Extension> ext6(
      CreateTestExtension("e", std::string(), std::string()));
  ASSERT_TRUE(ext5.get() && ext6.get());

  std::unique_ptr<ExtensionSet> to_add(new ExtensionSet());
  // |ext3| is already in |extensions|, should not affect size.
  EXPECT_TRUE(to_add->Insert(ext3));
  EXPECT_TRUE(to_add->Insert(ext5));
  EXPECT_TRUE(to_add->Insert(ext6));

  ASSERT_TRUE(extensions.Contains(ext3->id()));
  ASSERT_TRUE(extensions.InsertAll(*to_add));
  EXPECT_EQ(4u, extensions.size());

  ASSERT_FALSE(extensions.InsertAll(*to_add));  // Re-adding same set no-ops.
  EXPECT_EQ(4u, extensions.size());
}

TEST(ExtensionSetTest, TestInsert) {
  ExtensionSet set;
  std::string id_a(32, 'a');
  std::string id_b(32, 'b');
  scoped_refptr<const Extension> extension_a_v1 =
      ExtensionBuilder("A").SetID(id_a).SetVersion("0.1").Build();
  scoped_refptr<const Extension> extension_a_v2 =
      ExtensionBuilder("A").SetID(id_a).SetVersion("0.2").Build();
  scoped_refptr<const Extension> extension_b =
      ExtensionBuilder("B").SetID(id_b).SetVersion("1").Build();

  // Inserting a new extension should return true.
  EXPECT_TRUE(set.Insert(extension_a_v1));
  EXPECT_EQ(1u, set.size());
  EXPECT_EQ("0.1", set.GetByID(id_a)->version().GetString());
  // Inserting a new version of an extension already in the set should replace
  // the current entry, and return false.
  EXPECT_FALSE(set.Insert(extension_a_v2));
  EXPECT_EQ(1u, set.size());
  // Verify the entry was updated.
  EXPECT_EQ("0.2", set.GetByID(id_a)->version().GetString());

  // Inserting a second new extension should return true.
  EXPECT_TRUE(set.Insert(extension_b));
  EXPECT_EQ(2u, set.size());
}

}  // namespace extensions
