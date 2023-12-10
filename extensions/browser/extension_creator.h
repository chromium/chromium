// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_CREATOR_H_
#define EXTENSIONS_BROWSER_EXTENSION_CREATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace crypto {
class RSAPrivateKey;
}

namespace extensions {

// This class create an installable extension (.crx file) given an input
// directory that contains a valid manifest.json and the extension's resources
// contained within that directory. The output .crx file is always signed with a
// private key that is either provided in |private_key_path| or is internal
// generated randomly (and optionally written to |output_private_key_path|.
class ExtensionCreator {
 public:
  ExtensionCreator();

  ExtensionCreator(const ExtensionCreator&) = delete;
  ExtensionCreator& operator=(const ExtensionCreator&) = delete;

  // Settings to specify treatment of special or ignorable error conditions.
  // TODO(tjudkins): We should get rid of these flags and instead use explicit
  // boolean parameters in their place if they are still needed.
  enum RunFlags {
    kNoRunFlags = 0,
    kOverwriteCRX = 1 << 0,
    kRequireModernManifestVersion = 1 << 1,
  };

  // Categories of error that may need special handling on the UI end.
  enum ErrorType { kOtherError, kCRXExists };

  bool Run(const base::FilePath& extension_dir,
           const base::FilePath& crx_path,
           const base::FilePath& private_key_path,
           const base::FilePath& private_key_output_path,
           int run_flags);

  // Returns the error message that will be present if Run(...) returned false.
  std::string error_message() { return error_message_; }

  ErrorType error_type() { return error_type_; }

 private:
  friend class ExtensionCreatorTest;
  friend class ContentVerifierTest;

  // Verifies input directory's existence. |extension_dir| is the source
  // directory that should contain all the extension resources. |crx_path| is
  // the path to which final crx will be written.
  // |private_key_path| is the optional path to an existing private key to sign
  // the extension. If not provided, a random key will be created (in which case
  // it is written to |private_key_output_path| -- if provided).
  // |flags| is a bitset of RunFlags values.
  bool InitializeInput(const base::FilePath& extension_dir,
                       const base::FilePath& crx_path,
                       const base::FilePath& private_key_path,
                       const base::FilePath& private_key_output_path,
                       int run_flags);

  // Validates the extension by trying to load it and checking language files.
  bool ValidateExtension(const base::FilePath& extension_dir, int run_flags);

  // Reads private key from |private_key_path|.
  std::unique_ptr<crypto::RSAPrivateKey> ReadInputKey(
      const base::FilePath& private_key_path);

  // Generates a key pair and writes the private key to |private_key_path|
  // if provided.
  std::unique_ptr<crypto::RSAPrivateKey> GenerateKey(
      const base::FilePath& private_key_path);

  // Creates temporary zip file for the extension.
  bool CreateZip(const base::FilePath& extension_dir,
                 const base::FilePath& temp_path,
                 base::FilePath* zip_path);

  // Creates a CRX file at |crx_path|, signed with |private_key| and with the
  // contents of the archive at |zip_path|. Injects
  // |compressed_verified_contents| in the header if it not equal to
  // std::nullopt.
  bool CreateCrx(
      const base::FilePath& zip_path,
      crypto::RSAPrivateKey* private_key,
      const base::FilePath& crx_path,
      const std::optional<std::string>& compressed_verified_contents);

  // Creates a temporary directory to store zipped extension and then creates
  // CRX using the zipped extension.
  bool CreateCrxAndPerformCleanup(
      const base::FilePath& extension_dir,
      const base::FilePath& crx_path,
      crypto::RSAPrivateKey* private_key,
      const std::optional<std::string>& compressed_verified_contents);

  // Holds a message for any error that is raised during Run(...).
  std::string error_message_;

  // Type of error that was raised, if any.
  ErrorType error_type_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_CREATOR_H_
