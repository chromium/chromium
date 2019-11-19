// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_hash.h"

#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "crypto/signature_creator.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Helper class to create directory with extension files, including signed
// hashes for content verification.
class TestExtensionBuilder {
 public:
  TestExtensionBuilder()
      : test_content_verifier_key_(crypto::RSAPrivateKey::Create(2048)),
        // We have to provide explicit extension id in verified_contents.json.
        extension_id_(32, 'a') {
    base::CreateDirectory(
        extension_dir_.UnpackedPath().Append(kMetadataFolder));
  }

  void WriteManifest() {
    extension_dir_.WriteManifest(DictionaryBuilder()
                                     .Set("manifest_version", 2)
                                     .Set("name", "Test extension")
                                     .Set("version", "1.0")
                                     .ToJSON());
  }

  void WriteResource(base::FilePath::StringType relative_path,
                     std::string contents) {
    extension_dir_.WriteFile(relative_path, contents);
    extension_resources_.emplace_back(base::FilePath(std::move(relative_path)),
                                      std::move(contents));
  }

  void WriteComputedHashes() {
    int block_size = extension_misc::kContentVerificationDefaultBlockSize;
    ComputedHashes::Writer computed_hashes_writer;

    for (const auto& resource : extension_resources_) {
      std::vector<std::string> hashes =
          ComputedHashes::GetHashesForContent(resource.contents, block_size);
      computed_hashes_writer.AddHashes(resource.relative_path, block_size,
                                       hashes);
    }

    ASSERT_TRUE(computed_hashes_writer.WriteToFile(
        file_util::GetComputedHashesPath(extension_dir_.UnpackedPath())));
  }

  void WriteVerifiedContents() {
    std::unique_ptr<base::Value> payload = CreateVerifiedContents();
    std::string payload_value;
    ASSERT_TRUE(base::JSONWriter::Write(*payload, &payload_value));

    std::string payload_b64;
    base::Base64UrlEncode(
        payload_value, base::Base64UrlEncodePolicy::OMIT_PADDING, &payload_b64);

    std::string signature_sha256 = crypto::SHA256HashString("." + payload_b64);
    std::vector<uint8_t> signature_source(signature_sha256.begin(),
                                          signature_sha256.end());
    std::vector<uint8_t> signature_value;
    ASSERT_TRUE(crypto::SignatureCreator::Sign(
        test_content_verifier_key_.get(), crypto::SignatureCreator::SHA256,
        signature_source.data(), signature_source.size(), &signature_value));

    std::string signature_b64;
    base::Base64UrlEncode(
        std::string(signature_value.begin(), signature_value.end()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &signature_b64);

    std::unique_ptr<base::Value> signatures =
        ListBuilder()
            .Append(DictionaryBuilder()
                        .Set("header",
                             DictionaryBuilder().Set("kid", "webstore").Build())
                        .Set("protected", "")
                        .Set("signature", signature_b64)
                        .Build())
            .Build();
    std::unique_ptr<base::Value> verified_contents =
        ListBuilder()
            .Append(DictionaryBuilder()
                        .Set("description", "treehash per file")
                        .Set("signed_content",
                             DictionaryBuilder()
                                 .Set("payload", payload_b64)
                                 .Set("signatures", std::move(signatures))
                                 .Build())
                        .Build())
            .Build();

    std::string json;
    ASSERT_TRUE(base::JSONWriter::Write(*verified_contents, &json));

    base::FilePath verified_contents_path =
        file_util::GetVerifiedContentsPath(extension_dir_.UnpackedPath());
    ASSERT_EQ(
        static_cast<int>(json.size()),
        base::WriteFile(verified_contents_path, json.data(), json.size()));
  }

  std::vector<uint8_t> GetTestContentVerifierPublicKey() {
    std::vector<uint8_t> public_key;
    test_content_verifier_key_->ExportPublicKey(&public_key);
    return public_key;
  }

  base::FilePath extension_path() const {
    return extension_dir_.UnpackedPath();
  }
  const ExtensionId& extension_id() const { return extension_id_; }

 private:
  struct ExtensionResource {
    ExtensionResource(base::FilePath relative_path, std::string contents)
        : relative_path(std::move(relative_path)),
          contents(std::move(contents)) {}

    base::FilePath relative_path;
    std::string contents;
  };

  std::unique_ptr<base::Value> CreateVerifiedContents() {
    int block_size = extension_misc::kContentVerificationDefaultBlockSize;

    ListBuilder files;
    for (const auto& resource : extension_resources_) {
      base::FilePath::StringType path =
          VerifiedContents::NormalizeResourcePath(resource.relative_path);
      std::string tree_hash =
          ContentHash::ComputeTreeHashForContent(resource.contents, block_size);

      std::string tree_hash_b64;
      base::Base64UrlEncode(
          tree_hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &tree_hash_b64);

      files.Append(DictionaryBuilder()
                       .Set("path", path)
                       .Set("root_hash", tree_hash_b64)
                       .Build());
    }

    return DictionaryBuilder()
        .Set("item_id", extension_id_)
        .Set("item_version", "1.0")
        .Set("content_hashes",
             ListBuilder()
                 .Append(DictionaryBuilder()
                             .Set("format", "treehash")
                             .Set("block_size", block_size)
                             .Set("hash_block_size", block_size)
                             .Set("files", files.Build())
                             .Build())
                 .Build())
        .Build();
  }

  std::unique_ptr<crypto::RSAPrivateKey> test_content_verifier_key_;
  ExtensionId extension_id_;
  std::vector<ExtensionResource> extension_resources_;

  TestExtensionDir extension_dir_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionBuilder);
};

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

  scoped_refptr<Extension> LoadExtension(const TestExtensionBuilder& builder) {
    std::string error;
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        builder.extension_path(), builder.extension_id(), Manifest::INTERNAL,
        0 /* flags */, &error);
    if (!extension)
      ADD_FAILURE() << " error:'" << error << "'";
    return extension;
  }
};

TEST_F(ContentHashUnittest, ExtensionWithSignedHashes) {
  TestExtensionBuilder builder;
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
  TestExtensionBuilder builder;
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
  TestExtensionBuilder builder;
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
