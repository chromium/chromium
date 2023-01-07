// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_hash.h"

#include "base/files/scoped_temp_dir.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"

namespace extensions {

class ContentHashUnittest : public ExtensionsTest {
 protected:
  ContentHashUnittest() = default;

  std::unique_ptr<ContentHashResult> CreateContentHash(
      Extension* extension,
      ContentVerifierDelegate::VerifierSourceType source_type,
      const std::vector<uint8_t>& content_verifier_public_key) {
    ContentHash::FetchKey key(
        extension->id(), extension->path(), extension->version(),
        mojo::NullRemote() /* url_loader_factory_remote */,
        GURL() /* fetch_url */, content_verifier_public_key);
    return ContentHashWaiter().CreateAndWaitForCallback(std::move(key),
                                                        source_type);
  }

  scoped_refptr<Extension> LoadExtension(
      const content_verifier_test_utils::TestExtensionBuilder& builder) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        builder.extension_path(), builder.extension_id(),
        mojom::ManifestLocation::kInternal, 0 /* flags */, &error);
    if (!extension)
      ADD_FAILURE() << " error:'" << error << "'";
    return extension;
  }
};

TEST_F(ContentHashUnittest, ExtensionWithSignedHashes) {
  content_verifier_test_utils::TestExtensionBuilder builder;
  builder.WriteManifest();
  builder.WriteResource(FILE_PATH_LITERAL("background.js"),
                        "console.log('Nothing special');");
  builder.WriteVerifiedContents();

  scoped_refptr<Extension> extension = LoadExtension(builder);
  ASSERT_NE(nullptr, extension);

  std::unique_ptr<ContentHashResult> result = CreateContentHash(
      extension.get(),
      ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
      builder.GetTestContentVerifierPublicKey());
  DCHECK(result);

  EXPECT_TRUE(result->success);
}

TEST_F(ContentHashUnittest, ExtensionWithUnsignedHashes) {
  content_verifier_test_utils::TestExtensionBuilder builder;
  builder.WriteManifest();
  builder.WriteResource(FILE_PATH_LITERAL("background.js"),
                        "console.log('Nothing special');");
  builder.WriteComputedHashes();

  scoped_refptr<Extension> extension = LoadExtension(builder);
  ASSERT_NE(nullptr, extension);

  std::unique_ptr<ContentHashResult> result = CreateContentHash(
      extension.get(),
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES,
      builder.GetTestContentVerifierPublicKey());
  DCHECK(result);

  EXPECT_TRUE(result->success);
}

TEST_F(ContentHashUnittest, ExtensionWithoutHashes) {
  content_verifier_test_utils::TestExtensionBuilder builder;
  builder.WriteManifest();
  builder.WriteResource(FILE_PATH_LITERAL("background.js"),
                        "console.log('Nothing special');");

  scoped_refptr<Extension> extension = LoadExtension(builder);
  ASSERT_NE(nullptr, extension);

  std::unique_ptr<ContentHashResult> result = CreateContentHash(
      extension.get(),
      ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES,
      builder.GetTestContentVerifierPublicKey());
  DCHECK(result);

  EXPECT_FALSE(result->success);
}

}  // namespace extensions
