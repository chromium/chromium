// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_hash.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/test/test_url_loader_factory.h"

namespace extensions {

class ContentHashUnittest : public ExtensionsTest {
 protected:
  ContentHashUnittest() = default;

  std::unique_ptr<ContentHashResult> CreateContentHash(
      Extension* extension,
      ContentVerifierDelegate::VerifierSourceType source_type,
      const std::vector<uint8_t>& content_verifier_public_key) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    network::TestURLLoaderFactory().Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());
    ContentHash::FetchKey key(
        extension->id(), extension->path(), extension->version(),
        std::move(url_loader_factory), GURL() /* fetch_url */,
        content_verifier_public_key);
    return ContentHashWaiter().CreateAndWaitForCallback(std::move(key),
                                                        source_type);
  }

  scoped_refptr<Extension> LoadExtension(
      const content_verifier_test_utils::TestExtensionBuilder& builder) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        builder.extension_path(), builder.extension_id(),
        mojom::ManifestLocation::kInternal, 0 /* flags */, &error);
    if (!extension) {
      ADD_FAILURE() << " error:'" << error << "'";
    }
    return extension;
  }

  void CheckContentHashValidityWithOverrideExtensionId(
      const content_verifier_test_utils::TestExtensionBuilder& builder,
      const ExtensionId& override_extension_id,
      bool expected_is_valid) {
    // Copy the extension files to a temp directory as the verification process
    // may delete the verified_contents.json file.
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_TRUE(base::CopyDirectory(builder.extension_path(),
                                    temp_dir.GetPath(), /*recursive=*/true));

    std::string error;
    auto extension = file_util::LoadExtension(
        temp_dir.GetPath().Append(builder.extension_path().BaseName()),
        override_extension_id, mojom::ManifestLocation::kInternal,
        0 /* flags */, &error);
    ASSERT_TRUE(extension) << "Error: " << error;

    auto content_hash = CreateContentHash(
        extension.get(),
        ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
        builder.GetTestContentVerifierPublicKey());
    ASSERT_NE(content_hash, nullptr);

    EXPECT_EQ(content_hash->success, expected_is_valid);
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

// Try to load an extension with the verified_contents.json of a different
// extension and verify that it fails.
TEST_F(ContentHashUnittest, SignedHashesWithIncorrectExtensionId) {
  const ExtensionId signed_extension_id(32, 'a');
  const ExtensionId incorrect_extension_id(32, 'b');

  // Create extension with verified_contents.json for |signed_extension_id|.
  content_verifier_test_utils::TestExtensionBuilder builder(
      signed_extension_id);
  builder.WriteManifest();
  builder.WriteResource(FILE_PATH_LITERAL("background.js"),
                        "console.log('Nothing special');");
  builder.WriteVerifiedContents();

  CheckContentHashValidityWithOverrideExtensionId(builder, signed_extension_id,
                                                  /*expected_is_valid=*/true);
  CheckContentHashValidityWithOverrideExtensionId(builder,
                                                  incorrect_extension_id,
                                                  /*expected_is_valid=*/false);
}

}  // namespace extensions
