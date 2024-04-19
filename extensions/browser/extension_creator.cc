// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_creator.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "components/crx_file/crx_creator.h"
#include "components/crx_file/id_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "extensions/browser/extension_creator_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kRSAKeySize = 2048;
}

namespace extensions {

ExtensionCreator::ExtensionCreator() : error_type_(kOtherError) {}

bool ExtensionCreator::InitializeInput(
    const base::FilePath& extension_dir,
    const base::FilePath& crx_path,
    const base::FilePath& private_key_path,
    const base::FilePath& private_key_output_path,
    int run_flags) {
  // Validate input |extension_dir|.
  if (extension_dir.value().empty() || !base::DirectoryExists(extension_dir)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_DIRECTORY_NO_EXISTS);
    return false;
  }

  base::FilePath absolute_extension_dir =
      base::MakeAbsoluteFilePath(extension_dir);
  if (absolute_extension_dir.empty()) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_CANT_GET_ABSOLUTE_PATH);
    return false;
  }

  // Validate input |private_key| (if provided).
  if (!private_key_path.value().empty() &&
      !base::PathExists(private_key_path)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID_PATH);
    return false;
  }

  // If an |output_private_key| path is given, make sure it doesn't over-write
  // an existing private key.
  if (private_key_path.value().empty() &&
      !private_key_output_path.value().empty() &&
      base::PathExists(private_key_output_path)) {
    error_message_ = l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_EXISTS);
    return false;
  }

  // Check whether crx file already exists. Should be last check, as this is
  // a warning only.
  if (!(run_flags & kOverwriteCRX) && base::PathExists(crx_path)) {
    error_message_ = l10n_util::GetStringUTF8(IDS_EXTENSION_CRX_EXISTS);
    error_type_ = kCRXExists;

    return false;
  }

  return true;
}

bool ExtensionCreator::ValidateExtension(const base::FilePath& extension_dir,
                                         int run_flags) {
  int create_flags =
      Extension::FOLLOW_SYMLINKS_ANYWHERE | Extension::ERROR_ON_PRIVATE_KEY;
  if (run_flags & kRequireModernManifestVersion)
    create_flags |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;

  // Loading the extension does a lot of useful validation of the structure.
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kInternal,
      create_flags, &error_message_));

  return !!extension.get() && extension_l10n_util::ValidateExtensionLocales(
      extension_dir, *extension.get()->manifest()->value(), &error_message_);
}

std::unique_ptr<crypto::RSAPrivateKey> ExtensionCreator::ReadInputKey(
    const base::FilePath& private_key_path) {
  if (!base::PathExists(private_key_path)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_NO_EXISTS);
    return nullptr;
  }

  std::string private_key_contents;
  if (!base::ReadFileToString(private_key_path, &private_key_contents)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_READ);
    return nullptr;
  }

  std::string private_key_bytes;
  if (!Extension::ParsePEMKeyBytes(private_key_contents, &private_key_bytes)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID);
    return nullptr;
  }

  std::unique_ptr<crypto::RSAPrivateKey> private_key =
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_bytes.begin(), private_key_bytes.end()));
  if (!private_key) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_INVALID_FORMAT);
    return nullptr;
  }

  return private_key;
}

std::unique_ptr<crypto::RSAPrivateKey> ExtensionCreator::GenerateKey(
    const base::FilePath& output_private_key_path) {
  std::unique_ptr<crypto::RSAPrivateKey> key_pair(
      crypto::RSAPrivateKey::Create(kRSAKeySize));
  if (!key_pair) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_GENERATE);
    return nullptr;
  }

  std::vector<uint8_t> private_key_vector;
  if (!key_pair->ExportPrivateKey(&private_key_vector)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_EXPORT);
    return nullptr;
  }
  std::string private_key_bytes(
      reinterpret_cast<char*>(&private_key_vector.front()),
      private_key_vector.size());

  std::string private_key;
  if (!Extension::ProducePEM(private_key_bytes, &private_key)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
    return nullptr;
  }
  std::string pem_output;
  if (!Extension::FormatPEMForFileOutput(private_key, &pem_output, false)) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
    return nullptr;
  }

  if (!output_private_key_path.empty()) {
    if (!base::WriteFile(output_private_key_path, pem_output)) {
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_PRIVATE_KEY_FAILED_TO_OUTPUT);
      return nullptr;
    }
  }

  return key_pair;
}

bool ExtensionCreator::CreateZip(const base::FilePath& extension_dir,
                                 const base::FilePath& temp_path,
                                 base::FilePath* zip_path) {
  *zip_path = temp_path.Append(FILE_PATH_LITERAL("extension.zip"));

  scoped_refptr<ExtensionCreatorFilter> filter =
      base::MakeRefCounted<ExtensionCreatorFilter>(extension_dir);
  zip::FilterCallback filter_cb =
      base::BindRepeating(&ExtensionCreatorFilter::ShouldPackageFile, filter);

  // TODO(crbug.com/40584446): Surface a warning to the user for files excluded
  // from being packed.
  if (!zip::ZipWithFilterCallback(extension_dir, *zip_path,
                                  std::move(filter_cb))) {
    error_message_ =
        l10n_util::GetStringUTF8(IDS_EXTENSION_FAILED_DURING_PACKAGING);
    return false;
  }

  return true;
}

bool ExtensionCreator::CreateCrx(
    const base::FilePath& zip_path,
    crypto::RSAPrivateKey* private_key,
    const base::FilePath& crx_path,
    const std::optional<std::string>& compressed_verified_contents) {
  crx_file::CreatorResult result;
  if (compressed_verified_contents.has_value()) {
    result = crx_file::CreateCrxWithVerifiedContentsInHeader(
        crx_path, zip_path, private_key, compressed_verified_contents.value());
  } else {
    result = crx_file::Create(crx_path, zip_path, private_key);
  }
  switch (result) {
    case crx_file::CreatorResult::OK:
      return true;
    case crx_file::CreatorResult::ERROR_SIGNING_FAILURE:
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_ERROR_WHILE_SIGNING);
      return false;
    case crx_file::CreatorResult::ERROR_FILE_NOT_WRITABLE:
      error_message_ =
          l10n_util::GetStringUTF8(IDS_EXTENSION_SHARING_VIOLATION);
      return false;
    case crx_file::CreatorResult::ERROR_FILE_NOT_READABLE:
    case crx_file::CreatorResult::ERROR_FILE_WRITE_FAILURE:
      return false;
  }
  return false;
}

bool ExtensionCreator::CreateCrxAndPerformCleanup(
    const base::FilePath& extension_dir,
    const base::FilePath& crx_path,
    crypto::RSAPrivateKey* private_key,
    const std::optional<std::string>& compressed_verified_contents) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  base::FilePath zip_path;
  bool result =
      CreateZip(extension_dir, temp_dir.GetPath(), &zip_path) &&
      CreateCrx(zip_path, private_key, crx_path, compressed_verified_contents);
  base::DeleteFile(zip_path);
  return result;
}

bool ExtensionCreator::Run(const base::FilePath& extension_dir,
                           const base::FilePath& crx_path,
                           const base::FilePath& private_key_path,
                           const base::FilePath& output_private_key_path,
                           int run_flags) {
  // Check input diretory and read manifest.
  if (!InitializeInput(extension_dir, crx_path, private_key_path,
                       output_private_key_path, run_flags)) {
    return false;
  }

  if (!ValidateExtension(extension_dir, run_flags)) {
    return false;
  }

  // Initialize Key Pair
  std::unique_ptr<crypto::RSAPrivateKey> key_pair;
  if (!private_key_path.value().empty())
    key_pair = ReadInputKey(private_key_path);
  else
    key_pair = GenerateKey(output_private_key_path);
  if (!key_pair) {
    DCHECK(!error_message_.empty()) << "Set proper error message.";
    return false;
  }

  return CreateCrxAndPerformCleanup(extension_dir, crx_path, key_pair.get(),
                                    std::nullopt);
}

}  // namespace extensions
