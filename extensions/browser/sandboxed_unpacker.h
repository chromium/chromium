// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
#define EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/install_index_helper.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/image_sanitizer.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/json_file_sanitizer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
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
enum class InstallationStage;
struct RulesetParseResult;

namespace declarative_net_request {
struct IndexAndPersistJSONRulesetResult;
}

class SandboxedUnpackerClient
    : public base::RefCountedDeleteOnSequence<SandboxedUnpackerClient> {
 public:
  // Initialize the ref-counted base to always delete on the UI thread. Note
  // the constructor call must also happen on the UI thread.
  SandboxedUnpackerClient();

  // Determines whether |extension| requires computing and storing
  // computed_hashes.json and returns the result through |callback|.
  // Currently we do this only for force-installed extensions outside of Chrome
  // Web Store, and that is reflected in method's name.
  virtual void ShouldComputeHashesForOffWebstoreExtension(
      scoped_refptr<const Extension> extension,
      base::OnceCallback<void(bool)> callback);

  // Since data for content verification (verifier_contents.json) may be present
  // in the CRX header, we need to verify it against public key. Normally it is
  // Chrome Web Store public key, but may be overridden for tests.
  virtual void GetContentVerifierKey(
      base::OnceCallback<void(ContentVerifierKey)> callback);

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
  // ruleset_install_prefs - Install prefs needed for the Declarative Net
  // Request API.
  //
  // Note: OnUnpackSuccess/Failure may be called either synchronously or
  // asynchronously from SandboxedUnpacker::StartWithCrx/Directory.
  virtual void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::Value::Dict> original_manifest,
      const Extension* extension,
      const SkBitmap& install_icon,
      base::Value::Dict ruleset_install_prefs) = 0;
  virtual void OnUnpackFailure(const CrxInstallError& error) = 0;

  // Called after stage of installation is changed.
  virtual void OnStageChanged(InstallationStage stage) {}

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
class SandboxedUnpacker : public ImageSanitizer::Client {
 public:
  // Overrides the required verifier format for testing purposes. Only one
  // ScopedVerifierFormatOverrideForTest may exist at a time.
  class ScopedVerifierFormatOverrideForTest {
   public:
    explicit ScopedVerifierFormatOverrideForTest(
        crx_file::VerifierFormat format);
    ~ScopedVerifierFormatOverrideForTest();

   private:
    THREAD_CHECKER(thread_checker_);
  };

  // Creates a SandboxedUnpacker that will do work to unpack an extension,
  // passing the |location| and |creation_flags| to Extension::Create. The
  // |extensions_dir| parameter should specify the directory under which we'll
  // create a subdirectory to write the unpacked extension contents.
  // Note: Because this requires disk I/O, the task runner passed should use
  // TaskShutdownBehavior::SKIP_ON_SHUTDOWN to ensure that either the task is
  // fully run (if initiated before shutdown) or not run at all (if shutdown is
  // initiated first). See crbug.com/235525.
  // TODO(devlin): SKIP_ON_SHUTDOWN is also not quite sufficient for this. We
  // should probably instead be using base::ImportantFileWriter or similar.
  SandboxedUnpacker(
      mojom::ManifestLocation location,
      int creation_flags,
      const base::FilePath& extensions_dir,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
      SandboxedUnpackerClient* client);

  SandboxedUnpacker(const SandboxedUnpacker&) = delete;
  SandboxedUnpacker& operator=(const SandboxedUnpacker&) = delete;

  // Start processing the extension, either from a CRX file or already unzipped
  // in a directory. The client is called with the results. The directory form
  // requires the id and base64-encoded public key (for insertion into the
  // 'key' field of the manifest.json file).
  void StartWithCrx(const CRXFileInfo& crx_info);
  void StartWithDirectory(const ExtensionId& extension_id,
                          const std::string& public_key_base64,
                          const base::FilePath& directory);

 private:
  friend class SandboxedUnpackerTest;
  class IOThreadState;

  ~SandboxedUnpacker() override;

  // Create |temp_dir_| used to unzip or unpack the extension in.
  bool CreateTempDirectory();

  // Helper functions to simplify calling ReportFailure.
  std::u16string FailureReasonToString16(
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

  // Callback which is called after the verified contents are uncompressed.
  void OnVerifiedContentsUncompressed(
      const base::FilePath& unzip_dir,
      base::expected<mojo_base::BigBuffer, std::string> result);

  // Verifies the decompressed verified contents fetched from the header of CRX
  // and stores them if the verification of these contents is successful.
  void StoreVerifiedContentsInExtensionDir(
      const base::FilePath& unzip_dir,
      base::span<const uint8_t> verified_contents,
      ContentVerifierKey content_verifier_key);

  // Unpacks the extension in directory and returns the manifest.
  void Unpack(const base::FilePath& directory);
  void ReadManifestDone(std::optional<base::Value> manifest,
                        const std::optional<std::string>& error);
  void UnpackExtensionSucceeded(base::Value::Dict manifest);

  // Helper which calls ReportFailure.
  void ReportUnpackExtensionFailed(std::string_view error);

  // Implementation of ImageSanitizer::Client:
  data_decoder::DataDecoder* GetDataDecoder() override;
  void OnImageSanitizationDone(ImageSanitizer::Status status,
                               const base::FilePath& path) override;
  void OnImageDecoded(const base::FilePath& path, SkBitmap image) override;

  void ReadMessageCatalogs();

  void SanitizeMessageCatalogs(
      const std::set<base::FilePath>& message_catalog_paths);

  void MessageCatalogsSanitized(JsonFileSanitizer::Status status,
                                const std::string& error_msg);

  // Reports unpack success or failure, or unzip failure.
  void ReportSuccess();

  // Puts a sanboxed unpacker failure in histogram
  // Extensions.SandboxUnpackFailureReason.
  void ReportFailure(const SandboxedUnpackerFailureReason reason,
                     const std::u16string& error);

  // Overwrites original manifest with safe result from utility process.
  // Returns nullopt on error.
  std::optional<base::Value::Dict> RewriteManifestFile(
      const base::Value::Dict& manifest);

  // Cleans up temp directory artifacts.
  void Cleanup();

  // If a Declarative Net Request JSON ruleset is present, parses the JSON
  // rulesets for the Declarative Net Request API and persists the indexed
  // rulesets.
  void IndexAndPersistJSONRulesetsIfNeeded();

  void OnJSONRulesetsIndexed(RulesetParseResult result);

  // Computed hashes: if requested (via ShouldComputeHashes callback in
  // SandbloxedUnpackerClient), calculate hashes of all extensions' resources
  // and writes them in _metadata/computed_hashes.json. This is used by content
  // verification system for extensions outside of Chrome Web Store.
  void CheckComputeHashes();

  void MaybeComputeHashes(bool should_compute_hashes);

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

  // Parsed original manifest of the extension. Set after unpacking the
  // extension and working with its manifest, so after UnpackExtensionSucceeded
  // is called.
  std::optional<base::Value::Dict> manifest_;

  // Install prefs needed for the Declarative Net Request API.
  base::Value::Dict ruleset_install_prefs_;

  // Represents the extension we're unpacking.
  scoped_refptr<Extension> extension_;

  // The compressed verified contents extracted from the CRX header.
  std::vector<uint8_t> compressed_verified_contents_;

  // The public key that was extracted from the CRX header.
  std::string public_key_;

  // The extension's ID. This will be calculated from the public key
  // in the CRX header.
  ExtensionId extension_id_;

  // Location to use for the unpacked extension.
  mojom::ManifestLocation location_;

  // Creation flags to use for the extension. These flags will be used
  // when calling Extension::Create() by the CRX installer.
  int creation_flags_;

  // Overridden value of VerifierFormat that is used from StartWithCrx().
  std::optional<crx_file::VerifierFormat> format_verifier_override_;

  // Sequenced task runner where file I/O operations will be performed.
  scoped_refptr<base::SequencedTaskRunner> unpacker_io_task_runner_;

  // The normalized path of the install icon path, retrieved from the manifest.
  base::FilePath install_icon_path_;

  // The decoded install icon.
  SkBitmap install_icon_;

  // TODO(crbug.com/40232388): Consider to wrap it in base::SequenceBound
  std::unique_ptr<IOThreadState> io_thread_state_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SANDBOXED_UNPACKER_H_
