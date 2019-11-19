// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
#define EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/image_sanitizer.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/json_file_sanitizer.h"
#include "extensions/common/manifest.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

class SkBitmap;

namespace base {
class SequencedTaskRunner;
}

namespace crx_file {
enum class VerifierFormat;
}

namespace extensions {
class Extension;
enum class SandboxedUnpackerFailureReason;

namespace declarative_net_request {
struct IndexAndPersistJSONRulesetResult;
}

class SandboxedUnpackerClient
    : public base::RefCountedDeleteOnSequence<SandboxedUnpackerClient> {
 public:
  // Initialize the ref-counted base to always delete on the UI thread. Note
  // the constructor call must also happen on the UI thread.
  SandboxedUnpackerClient();

  // temp_dir - A temporary directory containing the results of the extension
  // unpacking. The client is responsible for deleting this directory.
  //
  // extension_root - The path to the extension root inside of temp_dir.
  //
  // original_manifest - The parsed but unmodified version of the manifest,
  // with no modifications such as localization, etc.
  //
  // extension - The extension that was unpacked. The client is responsible
  // for deleting this memory.
  //
  // install_icon - The icon we will display in the installation UI, if any.
  //
  // dnr_ruleset_checksum - Checksum for the indexed ruleset corresponding to
  // the Declarative Net Request API. Optional since it's only valid for
  // extensions which provide a declarative ruleset.
  //
  // Note: OnUnpackSuccess/Failure may be called either synchronously or
  // asynchronously from SandboxedUnpacker::StartWithCrx/Directory.
  virtual void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::DictionaryValue> original_manifest,
      const Extension* extension,
      const SkBitmap& install_icon,
      const base::Optional<int>& dnr_ruleset_checksum) = 0;
  virtual void OnUnpackFailure(const CrxInstallError& error) = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<SandboxedUnpackerClient>;
  friend class base::DeleteHelper<SandboxedUnpackerClient>;

  virtual ~SandboxedUnpackerClient() = default;
};

// SandboxedUnpacker does work to optionally unpack and then validate/sanitize
// an extension, either starting from a crx file, or else an already unzipped
// directory (eg., from a differential update). The parsing of complex data
// formats like JPEG or JSON is performed in specific, sandboxed services.
//
// Unpacking an extension using this class makes changes to its source, such as
// transcoding all images to PNG, parsing all message catalogs, and rewriting
// the manifest JSON. As such, it should not be used when the output is not
// intended to be given back to the author.
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread.
//
// Additionally, we hold a reference to our own client so that the client lives
// long enough to receive the result of unpacking.
//
// NOTE: This class should only be used on the FILE thread.
//
class SandboxedUnpacker : public base::RefCountedThreadSafe<SandboxedUnpacker> {
 public:
  // Overrides the required verifier format for testing purposes. Only one
  // ScopedVerifierFormatOverrideForTest may exist at a time.
  class ScopedVerifierFormatOverrideForTest {
   public:
    explicit ScopedVerifierFormatOverrideForTest(
        crx_file::VerifierFormat format);
    ~ScopedVerifierFormatOverrideForTest();
  };

  // Creates a SandboxedUnpacker that will do work to unpack an extension,
  // passing the |location| and |creation_flags| to Extension::Create. The
  // |extensions_dir| parameter should specify the directory under which we'll
  // create a subdirectory to write the unpacked extension contents.
  // Note: Because this requires disk I/O, the task runner passed should use
  // TaskShutdownBehavior::SKIP_ON_SHUTDOWN to ensure that either the task is
  // fully run (if initiated before shutdown) or not run at all (if shutdown is
  // initiated first). See crbug.com/235525.
  // TODO(devlin): We should probably just have SandboxedUnpacker use the common
  // ExtensionFileTaskRunner, and not pass in a separate one.
  // TODO(devlin): SKIP_ON_SHUTDOWN is also not quite sufficient for this. We
  // should probably instead be using base::ImportantFileWriter or similar.
  SandboxedUnpacker(
      Manifest::Location location,
      int creation_flags,
      const base::FilePath& extensions_dir,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
      SandboxedUnpackerClient* client);

  // Start processing the extension, either from a CRX file or already unzipped
  // in a directory. The client is called with the results. The directory form
  // requires the id and base64-encoded public key (for insertion into the
  // 'key' field of the manifest.json file).
  void StartWithCrx(const CRXFileInfo& crx_info);
  void StartWithDirectory(const std::string& extension_id,
                          const std::string& public_key_base64,
                          const base::FilePath& directory);

 private:
  friend class base::RefCountedThreadSafe<SandboxedUnpacker>;

  friend class SandboxedUnpackerTest;

  ~SandboxedUnpacker();

  // Create |temp_dir_| used to unzip or unpack the extension in.
  bool CreateTempDirectory();

  // Helper functions to simplify calling ReportFailure.
  base::string16 FailureReasonToString16(
      const SandboxedUnpackerFailureReason reason);
  void FailWithPackageError(const SandboxedUnpackerFailureReason reason);

  // Validates the signature of the extension and extract the key to
  // |public_key_|. True if the signature validates, false otherwise.
  bool ValidateSignature(const base::FilePath& crx_path,
                         const std::string& expected_hash,
                         const crx_file::VerifierFormat required_format);

  // Unzips the extension into directory.
  void Unzip(const base::FilePath& crx_path,
             const base::FilePath& unzipped_dir);
  void UnzipDone(const base::FilePath& zip_file,
                 const base::FilePath& unzip_dir,
                 const std::string& error);

  // Unpacks the extension in directory and returns the manifest.
  void Unpack(const base::FilePath& directory);
  void ReadManifestDone(base::Optional<base::Value> manifest,
                        const base::Optional<std::string>& error);
  void UnpackExtensionSucceeded(
      std::unique_ptr<base::DictionaryValue> manifest);

  // Helper which calls ReportFailure.
  void ReportUnpackExtensionFailed(base::StringPiece error);

  void ImageSanitizationDone(std::unique_ptr<base::DictionaryValue> manifest,
                             ImageSanitizer::Status status,
                             const base::FilePath& path);
  void ImageSanitizerDecodedImage(const base::FilePath& path, SkBitmap image);

  void ReadMessageCatalogs(std::unique_ptr<base::DictionaryValue> manifest);

  void SanitizeMessageCatalogs(
      std::unique_ptr<base::DictionaryValue> manifest,
      const std::set<base::FilePath>& message_catalog_paths);

  void MessageCatalogsSanitized(std::unique_ptr<base::DictionaryValue> manifest,
                                JsonFileSanitizer::Status status,
                                const std::string& error_msg);

  // Reports unpack success or failure, or unzip failure.
  void ReportSuccess(std::unique_ptr<base::DictionaryValue> original_manifest,
                     const base::Optional<int>& dnr_ruleset_checksum);

  // Puts a sanboxed unpacker failure in histogram
  // Extensions.SandboxUnpackFailureReason.
  void ReportFailure(const SandboxedUnpackerFailureReason reason,
                     const base::string16& error);

  // Overwrites original manifest with safe result from utility process.
  // Returns NULL on error. Caller owns the returned object.
  base::DictionaryValue* RewriteManifestFile(
      const base::DictionaryValue& manifest);

  // Cleans up temp directory artifacts.
  void Cleanup();

  // If a Declarative Net Request JSON ruleset is present, parses the JSON
  // ruleset for the Declarative Net Request API and persists the indexed
  // ruleset.
  void IndexAndPersistJSONRulesetIfNeeded(
      std::unique_ptr<base::DictionaryValue> manifest);

  void OnJSONRulesetIndexed(
      std::unique_ptr<base::DictionaryValue> manifest,
      declarative_net_request::IndexAndPersistJSONRulesetResult result);

  // Returns a JsonParser that can be used on the |unpacker_io_task_runner|.
  data_decoder::mojom::JsonParser* GetJsonParserPtr();

  // Parses the JSON file at |path| and invokes |callback| when done. |callback|
  // is called with a null parameter if parsing failed.
  // This must be called from the |unpacker_io_task_runner_|.
  void ParseJsonFile(const base::FilePath& path,
                     data_decoder::mojom::JsonParser::ParseCallback callback);

  // If we unpacked a CRX file, we hold on to the path name for use
  // in various histograms.
  base::FilePath crx_path_for_histograms_;

  // Our unpacker client.
  scoped_refptr<SandboxedUnpackerClient> client_;

  // The Extensions directory inside the profile.
  base::FilePath extensions_dir_;

  // Temporary directory to use for unpacking.
  base::ScopedTempDir temp_dir_;

  // Root directory of the unpacked extension (a child of temp_dir_).
  base::FilePath extension_root_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;

  // The extension's ID. This will be calculated from the public key
  // in the CRX header.
  std::string extension_id_;

  // If we unpacked a CRX file, the time at which unpacking started.
  // Used to compute the time unpacking takes.
  base::TimeTicks crx_unpack_start_time_;

  // Location to use for the unpacked extension.
  Manifest::Location location_;

  // Creation flags to use for the extension. These flags will be used
  // when calling Extension::Create() by the CRX installer.
  int creation_flags_;

  // Sequenced task runner where file I/O operations will be performed.
  scoped_refptr<base::SequencedTaskRunner> unpacker_io_task_runner_;

  // The normalized path of the install icon path, retrieved from the manifest.
  base::FilePath install_icon_path_;

  // The decoded install icon.
  SkBitmap install_icon_;

  // Controls our own lazily started, isolated instance of the Data Decoder
  // service so that multiple decode operations related to this
  // SandboxedUnpacker can share a single instance.
  data_decoder::DataDecoder data_decoder_;

  // The JSONParser remote from the data decoder service.
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser_;

  // The ImageSanitizer used to clean-up images.
  std::unique_ptr<ImageSanitizer> image_sanitizer_;

  // Used during the message catalog rewriting phase to sanitize the extension
  // provided message catalogs.
  std::unique_ptr<JsonFileSanitizer> json_file_sanitizer_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedUnpacker);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
