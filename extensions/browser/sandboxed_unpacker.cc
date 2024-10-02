// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/sandboxed_unpacker.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/install_index_helper.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/install_stage.h"
#include "extensions/browser/ruleset_parse_result.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_resource_path_normalizer.h"
#include "extensions/common/extension_utility_types.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/switches.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

using base::ASCIIToUTF16;
using content::BrowserThread;

namespace extensions {
namespace {

// Normalize the file path. If the call to base::NormalizeFilePath fails then we
// return the original path.
base::FilePath NormalizeFilePath(const base::FilePath& path) {
  base::FilePath normalized;
  if (!base::NormalizeFilePath(path, &normalized)) {
    LOG(WARNING) << path.value() << " couldn't be normalized.";
    return path;
  }
  return normalized;
}

// Work horse for FindWritableTempLocation. Creates a temp file in the folder
// tries to normalize the path.
bool VerifyWritableTempLocation(base::FilePath* temp_dir) {
  if (temp_dir->empty())
    return false;

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(*temp_dir, &temp_file)) {
    LOG(ERROR) << temp_dir->value() << " is not writable";
    return false;
  }

  // NormalizeFilePath requires a non-empty file, so write some data.
  // If you change the exit points of this function please make sure all
  // exit points delete this temp file!
  if (!base::WriteFile(temp_file, ".")) {
    base::DeleteFile(temp_file);
    return false;
  }

  *temp_dir = NormalizeFilePath(temp_file).DirName();
  // Clean up the temp file.
  base::DeleteFile(temp_file);

  return true;
}

// This function tries to find a location for unpacking the extension archive
// that is writable. If no such location exists we can not proceed and should
// fail.
// The result will be written to |temp_dir|. The function will write to this
// parameter even if it returns false.
bool FindWritableTempLocation(const base::FilePath& extensions_dir,
                              base::FilePath* temp_dir) {
// On ChromeOS, we will only attempt to unpack extension in cryptohome (profile)
// directory to provide additional security/privacy and speed up the rest of
// the extension install process.
#if !BUILDFLAG(IS_CHROMEOS)
  base::PathService::Get(base::DIR_TEMP, temp_dir);
  if (VerifyWritableTempLocation(temp_dir)) {
    return true;
  }
#endif

  *temp_dir = file_util::GetInstallTempDir(extensions_dir);
  if (VerifyWritableTempLocation(temp_dir)) {
    return true;
  }
  // Neither paths is link free chances are good installation will fail.
  LOG(ERROR) << "Both the %TEMP% folder and the profile seem to be on "
             << "remote drives or read-only. Installation can not complete!";
  return false;
}

std::set<base::FilePath> GetMessageCatalogPathsToBeSanitized(
    const base::FilePath& locales_path) {
  // Not all folders under _locales have to be valid locales.
  base::FileEnumerator locales(locales_path, /*recursive=*/false,
                               base::FileEnumerator::DIRECTORIES);

  std::set<base::FilePath> message_catalog_paths;
  std::set<std::string> all_locales;
  extension_l10n_util::GetAllLocales(&all_locales);
  base::FilePath locale_path;
  while (!(locale_path = locales.Next()).empty()) {
    if (!extension_l10n_util::ShouldSkipValidation(locales_path, locale_path,
                                                   all_locales)) {
      message_catalog_paths.insert(locale_path.Append(kMessagesFilename));
    }
  }
  return message_catalog_paths;
}

// Callback for ComputedHashes::Create, compute hashes for all files except
// _metadata directory (e.g. computed_hashes.json itself).
bool ShouldComputeHashesForResource(
    const base::FilePath& relative_resource_path) {
  std::vector<base::FilePath::StringType> components =
      relative_resource_path.GetComponents();
  return !components.empty() && components[0] != kMetadataFolder;
}

std::optional<crx_file::VerifierFormat> g_verifier_format_override_for_test;

}  // namespace

class SandboxedUnpacker::IOThreadState {
 public:
  IOThreadState() = default;
  IOThreadState(const IOThreadState&) = delete;
  IOThreadState& operator=(const IOThreadState&) = delete;
  ~IOThreadState() = default;

  void CleanUp() {
    image_sanitizer_.reset();
    json_file_sanitizer_.reset();
    json_parser_.reset();
  }

  data_decoder::DataDecoder* GetDataDecoder() { return &data_decoder_; }

  void CreateImangeSanitizer(
      const extensions::Extension* extension,
      const base::FilePath& extension_root,
      scoped_refptr<ImageSanitizer::Client> client,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner) {
    DCHECK(!image_sanitizer_);
    std::set<base::FilePath> image_paths =
        ExtensionsClient::Get()->GetBrowserImagePaths(extension);
    image_sanitizer_ = ImageSanitizer::CreateAndStart(
        client, extension_root, image_paths, unpacker_io_task_runner);
  }

  void CreateJsonFileSanitizer(
      const std::set<base::FilePath>& message_catalog_paths,
      JsonFileSanitizer::Callback callback,
      const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner) {
    json_file_sanitizer_ = JsonFileSanitizer::CreateAndStart(
        GetDataDecoder(), message_catalog_paths, std::move(callback),
        unpacker_io_task_runner);
  }

  data_decoder::mojom::JsonParser* GetJsonParserPtr(
      SandboxedUnpacker* unpacker) {
    if (!json_parser_) {
      data_decoder_.GetService()->BindJsonParser(
          json_parser_.BindNewPipeAndPassReceiver());
      json_parser_.set_disconnect_handler(base::BindOnce(
          &SandboxedUnpacker::ReportFailure, unpacker,
          SandboxedUnpackerFailureReason::
              UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
              u"UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL") +
              u". " +
              l10n_util::GetStringUTF16(
                  IDS_EXTENSION_INSTALL_PROCESS_CRASHED)));
    }

    return json_parser_.get();
  }

 private:
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
};

SandboxedUnpackerClient::SandboxedUnpackerClient()
    : RefCountedDeleteOnSequence<SandboxedUnpackerClient>(
          content::GetUIThreadTaskRunner({})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void SandboxedUnpackerClient::ShouldComputeHashesForOffWebstoreExtension(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void SandboxedUnpackerClient::GetContentVerifierKey(
    base::OnceCallback<void(ContentVerifierKey)> callback) {
  std::move(callback).Run(ContentVerifierKey(kWebstoreSignaturesPublicKey,
                                             kWebstoreSignaturesPublicKeySize));
}

SandboxedUnpacker::ScopedVerifierFormatOverrideForTest::
    ScopedVerifierFormatOverrideForTest(crx_file::VerifierFormat format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!g_verifier_format_override_for_test.has_value());
  g_verifier_format_override_for_test = format;
}

SandboxedUnpacker::ScopedVerifierFormatOverrideForTest::
    ~ScopedVerifierFormatOverrideForTest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_verifier_format_override_for_test.reset();
}

SandboxedUnpacker::SandboxedUnpacker(
    mojom::ManifestLocation location,
    int creation_flags,
    const base::FilePath& extensions_dir,
    const scoped_refptr<base::SequencedTaskRunner>& unpacker_io_task_runner,
    SandboxedUnpackerClient* client)
    : client_(client),
      extensions_dir_(extensions_dir),
      location_(location),
      creation_flags_(creation_flags),
      format_verifier_override_(g_verifier_format_override_for_test),
      unpacker_io_task_runner_(unpacker_io_task_runner),
      io_thread_state_(std::make_unique<IOThreadState>()) {
  // Tracking for crbug.com/692069. The location must be valid. If it's invalid,
  // the utility process kills itself for a bad IPC.
  CHECK_GT(location, mojom::ManifestLocation::kInvalidLocation);
  CHECK_LE(location, mojom::ManifestLocation::kMaxValue);
}

bool SandboxedUnpacker::CreateTempDirectory() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  base::FilePath temp_dir;
  if (!FindWritableTempLocation(extensions_dir_, &temp_dir)) {
    ReportFailure(
        SandboxedUnpackerFailureReason::COULD_NOT_GET_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"COULD_NOT_GET_TEMP_DIRECTORY"));
    return false;
  }

  if (!temp_dir_.CreateUniqueTempDirUnderPath(temp_dir)) {
    ReportFailure(
        SandboxedUnpackerFailureReason::COULD_NOT_CREATE_TEMP_DIRECTORY,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"COULD_NOT_CREATE_TEMP_DIRECTORY"));
    return false;
  }

  return true;
}

void SandboxedUnpacker::StartWithCrx(const CRXFileInfo& crx_info) {
  // We assume that we are started on the thread that the client wants us
  // to do file IO on.
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  client_->OnStageChanged(InstallationStage::kVerification);
  std::string expected_hash;
  if (!crx_info.expected_hash.empty() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kEnableCrxHashCheck)) {
    expected_hash = base::ToLowerASCII(crx_info.expected_hash);
  }

  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  // Initialize the path that will eventually contain the unpacked extension.
  extension_root_ = temp_dir_.GetPath().AppendASCII(kTempExtensionName);

  // Extract the public key and validate the package.
  if (!ValidateSignature(
          crx_info.path, expected_hash,
          format_verifier_override_.value_or(crx_info.required_format))) {
    return;  // ValidateSignature() already reported the error.
  }

  client_->OnStageChanged(InstallationStage::kCopying);
  // Copy the crx file into our working directory.
  base::FilePath temp_crx_path =
      temp_dir_.GetPath().Append(crx_info.path.BaseName());

  if (!base::CopyFile(crx_info.path, temp_crx_path)) {
    // Failed to copy extension file to temporary directory.
    ReportFailure(SandboxedUnpackerFailureReason::
                      FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      u"FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY"));
    return;
  }

  base::FilePath normalized_crx_path = NormalizeFilePath(temp_crx_path);
  client_->OnStageChanged(InstallationStage::kUnpacking);
  // Make sure to create the directory where the extension will be unzipped, as
  // the unzipper service requires it.
  base::FilePath unzipped_dir =
      normalized_crx_path.DirName().AppendASCII(kTempExtensionName);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(unzipped_dir, &error)) {
    LOG(ERROR) << "Failed to created directory " << unzipped_dir.value()
               << " with error " << error;
    ReportFailure(SandboxedUnpackerFailureReason::UNZIP_FAILED,
                  l10n_util::GetStringUTF16(IDS_EXTENSION_PACKAGE_UNZIP_ERROR));
    return;
  }

  Unzip(normalized_crx_path, unzipped_dir);
}

void SandboxedUnpacker::StartWithDirectory(const ExtensionId& extension_id,
                                           const std::string& public_key,
                                           const base::FilePath& directory) {
  // We assume that we are started on the thread that the client wants us
  // to do file IO on.
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  extension_id_ = extension_id;
  public_key_ = public_key;
  if (!CreateTempDirectory())
    return;  // ReportFailure() already called.

  extension_root_ = temp_dir_.GetPath().AppendASCII(kTempExtensionName);

  if (!base::Move(directory, extension_root_)) {
    LOG(ERROR) << "Could not move " << directory.value() << " to "
               << extension_root_.value();
    ReportFailure(
        SandboxedUnpackerFailureReason::DIRECTORY_MOVE_FAILED,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"DIRECTORY_MOVE_FAILED"));
    return;
  }

  Unpack(extension_root_);
}

SandboxedUnpacker::~SandboxedUnpacker() {
  // To avoid blocking shutdown, don't delete temporary directory here if it
  // hasn't been cleaned up or passed on to another owner yet.
  // This is OK because ExtensionGarbageCollector will take care of the leaked
  // |temp_dir_| eventually.
  temp_dir_.Take();

  unpacker_io_task_runner_->DeleteSoon(FROM_HERE, std::move(io_thread_state_));
}

void SandboxedUnpacker::Unzip(const base::FilePath& crx_path,
                              const base::FilePath& unzipped_dir) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(crx_path.DirName() == temp_dir_.GetPath());

  ZipFileInstaller::Create(unpacker_io_task_runner_,
                           base::BindOnce(&SandboxedUnpacker::UnzipDone, this))
      ->LoadFromZipFileInDir(crx_path, unzipped_dir);
}

void SandboxedUnpacker::UnzipDone(const base::FilePath& zip_file,
                                  const base::FilePath& unzip_dir,
                                  const std::string& error) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  if (!error.empty()) {
    ReportFailure(SandboxedUnpackerFailureReason::UNZIP_FAILED,
                  l10n_util::GetStringUTF16(IDS_EXTENSION_PACKAGE_UNZIP_ERROR));
    return;
  }
  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(extension_root_);
  // If the verified contents are already present in the _metadata folder, we
  // can ignore the verified contents in the header.
  if (compressed_verified_contents_.empty() ||
      base::PathExists(verified_contents_path)) {
    Unpack(unzip_dir);
    return;
  }
  GetDataDecoder()->GzipUncompress(
      compressed_verified_contents_,
      base::BindOnce(&SandboxedUnpacker::OnVerifiedContentsUncompressed, this,
                     unzip_dir));
}

void SandboxedUnpacker::OnVerifiedContentsUncompressed(
    const base::FilePath& unzip_dir,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (!result.has_value()) {
    ReportFailure(SandboxedUnpackerFailureReason::
                      CRX_HEADER_VERIFIED_CONTENTS_UNCOMPRESSING_FAILURE,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      u"CRX_HEADER_VERIFIED_CONTENTS_UNCOMPRESSING_FAILURE"));
    return;
  }
  // Make a copy, since |result| may store data in shared memory, accessible by
  // some other processes.
  std::vector<uint8_t> verified_contents(result->data(),
                                         result->data() + result->size());

  client_->GetContentVerifierKey(
      base::BindOnce(&SandboxedUnpacker::StoreVerifiedContentsInExtensionDir,
                     this, unzip_dir, std::move(verified_contents)));
}

void SandboxedUnpacker::StoreVerifiedContentsInExtensionDir(
    const base::FilePath& unzip_dir,
    base::span<const uint8_t> verified_contents,
    ContentVerifierKey content_verifier_key) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  if (!VerifiedContents::Create(
          content_verifier_key,
          {reinterpret_cast<const char*>(verified_contents.data()),
           verified_contents.size()})) {
    ReportFailure(
        SandboxedUnpackerFailureReason::MALFORMED_VERIFIED_CONTENTS,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"MALFORMED_VERIFIED_CONTENTS"));
    return;
  }

  base::FilePath metadata_path = extension_root_.Append(kMetadataFolder);
  if (!base::CreateDirectory(metadata_path)) {
    ReportFailure(
        SandboxedUnpackerFailureReason::COULD_NOT_CREATE_METADATA_DIRECTORY,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"COULD_NOT_CREATE_METADATA_DIRECTORY"));
    return;
  }

  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(extension_root_);

  // Cannot write the verified contents file.
  if (!base::WriteFile(verified_contents_path, verified_contents)) {
    ReportFailure(SandboxedUnpackerFailureReason::
                      COULD_NOT_WRITE_VERIFIED_CONTENTS_INTO_FILE,
                  l10n_util::GetStringFUTF16(
                      IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                      u"COULD_NOT_WRITE_VERIFIED_CONTENTS_INTO_FILE"));
    return;
  }

  Unpack(unzip_dir);
}

void SandboxedUnpacker::Unpack(const base::FilePath& directory) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(directory.DirName() == temp_dir_.GetPath());

  base::FilePath manifest_path = extension_root_.Append(kManifestFilename);

  ParseJsonFile(manifest_path,
                base::BindOnce(&SandboxedUnpacker::ReadManifestDone, this));
}

void SandboxedUnpacker::ReadManifestDone(
    std::optional<base::Value> manifest,
    const std::optional<std::string>& error) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (error) {
    ReportUnpackExtensionFailed(*error);
    return;
  }
  if (!manifest || !manifest->is_dict()) {
    ReportUnpackExtensionFailed(manifest_errors::kInvalidManifest);
    return;
  }

  std::string error_msg;
  scoped_refptr<Extension> extension(
      Extension::Create(extension_root_, location_, manifest->GetDict(),
                        creation_flags_, extension_id_, &error_msg));
  if (!extension) {
    ReportUnpackExtensionFailed(error_msg);
    return;
  }

  std::vector<InstallWarning> warnings;
  if (!file_util::ValidateExtension(extension.get(), &error_msg, &warnings)) {
    ReportUnpackExtensionFailed(error_msg);
    return;
  }
  extension->AddInstallWarnings(std::move(warnings));

  UnpackExtensionSucceeded(std::move(manifest.value()).TakeDict());
}

void SandboxedUnpacker::UnpackExtensionSucceeded(base::Value::Dict manifest) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  std::optional<base::Value::Dict> final_manifest(
      RewriteManifestFile(manifest));
  if (!final_manifest)
    return;

  // Create an extension object that refers to the temporary location the
  // extension was unpacked to. We use this until the extension is finally
  // installed. For example, the install UI shows images from inside the
  // extension.

  // Localize manifest now, so confirm UI gets correct extension name.

  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with std::u16string
  std::string utf8_error;
  if (!extension_l10n_util::LocalizeExtension(
          extension_root_, &final_manifest.value(),
          extension_l10n_util::GzippedMessagesPermission::kDisallow,
          &utf8_error)) {
    ReportFailure(
        SandboxedUnpackerFailureReason::COULD_NOT_LOCALIZE_EXTENSION,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
                                   base::UTF8ToUTF16(utf8_error)));
    return;
  }

  extension_ =
      Extension::Create(extension_root_, location_, final_manifest.value(),
                        Extension::REQUIRE_KEY | creation_flags_, &utf8_error);

  if (!extension_.get()) {
    ReportFailure(SandboxedUnpackerFailureReason::INVALID_MANIFEST,
                  u"Manifest is invalid: " + ASCIIToUTF16(utf8_error));
    return;
  }

  // The install icon path may be empty, which is OK, but if it is not it should
  // be normalized successfully.
  const std::string& original_install_icon_path =
      IconsInfo::GetIcons(extension_.get())
          .Get(extension_misc::EXTENSION_ICON_LARGE,
               ExtensionIconSet::Match::kBigger);
  if (!original_install_icon_path.empty() &&
      !NormalizeExtensionResourcePath(
          base::FilePath::FromUTF8Unsafe(original_install_icon_path),
          &install_icon_path_)) {
    // Invalid path for browser image.
    ReportFailure(
        SandboxedUnpackerFailureReason::INVALID_PATH_FOR_BROWSER_IMAGE,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"INVALID_PATH_FOR_BROWSER_IMAGE"));
    return;
  }

  manifest_ = std::move(manifest);

  io_thread_state_->CreateImangeSanitizer(extension_.get(), extension_root_,
                                          this, unpacker_io_task_runner_);
}

data_decoder::DataDecoder* SandboxedUnpacker::GetDataDecoder() {
  return io_thread_state_->GetDataDecoder();
}

void SandboxedUnpacker::OnImageDecoded(const base::FilePath& path,
                                       SkBitmap image) {
  if (path == install_icon_path_)
    install_icon_ = image;
}

void SandboxedUnpacker::OnImageSanitizationDone(
    ImageSanitizer::Status status,
    const base::FilePath& file_path_for_error) {
  if (status == ImageSanitizer::Status::kSuccess) {
    // Next step is to sanitize the message catalogs.
    ReadMessageCatalogs();
    return;
  }

  SandboxedUnpackerFailureReason failure_reason =
      SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED;
  std::u16string error;
  switch (status) {
    case ImageSanitizer::Status::kImagePathError:
      failure_reason =
          SandboxedUnpackerFailureReason::INVALID_PATH_FOR_BROWSER_IMAGE;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"INVALID_PATH_FOR_BROWSER_IMAGE");
      break;
    case ImageSanitizer::Status::kFileReadError:
    case ImageSanitizer::Status::kDecodingError:
      error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PACKAGE_IMAGE_ERROR,
          base::i18n::GetDisplayStringInLTRDirectionality(
              file_path_for_error.BaseName().LossyDisplayName()));
      break;
    case ImageSanitizer::Status::kFileDeleteError:
      failure_reason =
          SandboxedUnpackerFailureReason::ERROR_REMOVING_OLD_IMAGE_FILE;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"ERROR_REMOVING_OLD_IMAGE_FILE");
      break;
    case ImageSanitizer::Status::kEncodingError:
      failure_reason =
          SandboxedUnpackerFailureReason::ERROR_RE_ENCODING_THEME_IMAGE;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"ERROR_RE_ENCODING_THEME_IMAGE");
      break;
    case ImageSanitizer::Status::kFileWriteError:
      failure_reason = SandboxedUnpackerFailureReason::ERROR_SAVING_THEME_IMAGE;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"ERROR_SAVING_THEME_IMAGE");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  ReportFailure(failure_reason, error);
}

void SandboxedUnpacker::ReadMessageCatalogs() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (LocaleInfo::GetDefaultLocale(extension_.get()).empty()) {
    MessageCatalogsSanitized(JsonFileSanitizer::Status::kSuccess,
                             std::string());
    return;
  }

  // Get the paths to the message catalogs we should sanitize on the file task
  // runner.
  base::FilePath locales_path = extension_root_.Append(kLocaleFolder);

  extensions::GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetMessageCatalogPathsToBeSanitized, locales_path),
      base::BindOnce(&SandboxedUnpacker::SanitizeMessageCatalogs, this));
}

void SandboxedUnpacker::SanitizeMessageCatalogs(
    const std::set<base::FilePath>& message_catalog_paths) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  io_thread_state_->CreateJsonFileSanitizer(
      message_catalog_paths,
      base::BindOnce(&SandboxedUnpacker::MessageCatalogsSanitized, this),
      unpacker_io_task_runner_);
}

void SandboxedUnpacker::MessageCatalogsSanitized(
    JsonFileSanitizer::Status status,
    const std::string& error_msg) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (status == JsonFileSanitizer::Status::kSuccess) {
    IndexAndPersistJSONRulesetsIfNeeded();
    return;
  }

  SandboxedUnpackerFailureReason failure_reason =
      SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED;
  std::u16string error;
  switch (status) {
    case JsonFileSanitizer::Status::kFileReadError:
    case JsonFileSanitizer::Status::kDecodingError:
      failure_reason = SandboxedUnpackerFailureReason::INVALID_CATALOG_DATA;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"INVALID_CATALOG_DATA");
      break;
    case JsonFileSanitizer::Status::kSerializingError:
      failure_reason =
          SandboxedUnpackerFailureReason::ERROR_SERIALIZING_CATALOG;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"ERROR_SERIALIZING_CATALOG");
      break;
    case JsonFileSanitizer::Status::kFileDeleteError:
    case JsonFileSanitizer::Status::kFileWriteError:
      failure_reason = SandboxedUnpackerFailureReason::ERROR_SAVING_CATALOG;
      error = l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                         u"ERROR_SAVING_CATALOG");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  ReportFailure(failure_reason, error);
}

void SandboxedUnpacker::IndexAndPersistJSONRulesetsIfNeeded() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(extension_);

  // Defer ruleset indexing for disabled rulesets to speed up extension
  // installation.
  auto ruleset_filter = declarative_net_request::FileBackedRulesetSource::
      RulesetFilter::kIncludeManifestEnabled;

  // Ignore rule parsing errors since ruleset indexing (and therefore rule
  // parsing) is deferred until the ruleset is enabled for packed extensions.
  auto parse_flags = declarative_net_request::RulesetSource::kNone;

  declarative_net_request::InstallIndexHelper::IndexStaticRulesets(
      *extension_, ruleset_filter, parse_flags,
      base::BindOnce(&SandboxedUnpacker::OnJSONRulesetsIndexed, this));
}

void SandboxedUnpacker::OnJSONRulesetsIndexed(RulesetParseResult result) {
  if (result.error) {
    ReportFailure(
        SandboxedUnpackerFailureReason::ERROR_INDEXING_DNR_RULESET,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
                                   base::UTF8ToUTF16(*result.error)));
    return;
  }

  if (!result.warnings.empty())
    extension_->AddInstallWarnings(std::move(result.warnings));

  ruleset_install_prefs_ = std::move(result.ruleset_install_prefs);

  CheckComputeHashes();
}

void SandboxedUnpacker::CheckComputeHashes() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  client_->ShouldComputeHashesForOffWebstoreExtension(
      extension_, base::BindOnce(&SandboxedUnpacker::MaybeComputeHashes, this));
}

void SandboxedUnpacker::MaybeComputeHashes(bool should_compute) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (!should_compute) {
    ReportSuccess();
    return;
  }

  base::ElapsedTimer timer;

  std::optional<ComputedHashes::Data> computed_hashes_data =
      ComputedHashes::Compute(
          extension_->path(),
          extension_misc::kContentVerificationDefaultBlockSize,
          IsCancelledCallback(),
          base::BindRepeating(&ShouldComputeHashesForResource));
  bool success =
      computed_hashes_data &&
      ComputedHashes(std::move(*computed_hashes_data))
          .WriteToFile(file_util::GetComputedHashesPath(extension_->path()));
  UMA_HISTOGRAM_BOOLEAN(
      "Extensions.ContentVerification.ComputeHashesOnInstallResult", success);
  if (success) {
    UMA_HISTOGRAM_TIMES(
        "Extensions.ContentVerification.ComputeHashesOnInstallTime",
        timer.Elapsed());
  } else {
    LOG(ERROR) << "[extension " << extension_->id()
               << "] Failed to create computed_hashes.json";
  }

  ReportSuccess();
}

data_decoder::mojom::JsonParser* SandboxedUnpacker::GetJsonParserPtr() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  return io_thread_state_->GetJsonParserPtr(this);
}

void SandboxedUnpacker::ReportUnpackExtensionFailed(std::string_view error) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  ReportFailure(SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED,
                l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_MESSAGE,
                                           base::UTF8ToUTF16(error)));
}

std::u16string SandboxedUnpacker::FailureReasonToString16(
    const SandboxedUnpackerFailureReason reason) {
  switch (reason) {
    case SandboxedUnpackerFailureReason::COULD_NOT_GET_TEMP_DIRECTORY:
      return u"COULD_NOT_GET_TEMP_DIRECTORY";
    case SandboxedUnpackerFailureReason::COULD_NOT_CREATE_TEMP_DIRECTORY:
      return u"COULD_NOT_CREATE_TEMP_DIRECTORY";
    case SandboxedUnpackerFailureReason::
        FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY:
      return u"FAILED_TO_COPY_EXTENSION_FILE_TO_TEMP_DIRECTORY";
    case SandboxedUnpackerFailureReason::COULD_NOT_GET_SANDBOX_FRIENDLY_PATH:
      return u"COULD_NOT_GET_SANDBOX_FRIENDLY_PATH";
    case SandboxedUnpackerFailureReason::COULD_NOT_LOCALIZE_EXTENSION:
      return u"COULD_NOT_LOCALIZE_EXTENSION";
    case SandboxedUnpackerFailureReason::INVALID_MANIFEST:
      return u"INVALID_MANIFEST";
    case SandboxedUnpackerFailureReason::UNPACKER_CLIENT_FAILED:
      return u"UNPACKER_CLIENT_FAILED";
    case SandboxedUnpackerFailureReason::
        UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL:
      return u"UTILITY_PROCESS_CRASHED_WHILE_TRYING_TO_INSTALL";

    case SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE:
      return u"CRX_FILE_NOT_READABLE";
    case SandboxedUnpackerFailureReason::CRX_HEADER_INVALID:
      return u"CRX_HEADER_INVALID";
    case SandboxedUnpackerFailureReason::CRX_MAGIC_NUMBER_INVALID:
      return u"CRX_MAGIC_NUMBER_INVALID";
    case SandboxedUnpackerFailureReason::CRX_VERSION_NUMBER_INVALID:
      return u"CRX_VERSION_NUMBER_INVALID";
    case SandboxedUnpackerFailureReason::CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE:
      return u"CRX_EXCESSIVELY_LARGE_KEY_OR_SIGNATURE";
    case SandboxedUnpackerFailureReason::CRX_ZERO_KEY_LENGTH:
      return u"CRX_ZERO_KEY_LENGTH";
    case SandboxedUnpackerFailureReason::CRX_ZERO_SIGNATURE_LENGTH:
      return u"CRX_ZERO_SIGNATURE_LENGTH";
    case SandboxedUnpackerFailureReason::CRX_PUBLIC_KEY_INVALID:
      return u"CRX_PUBLIC_KEY_INVALID";
    case SandboxedUnpackerFailureReason::CRX_SIGNATURE_INVALID:
      return u"CRX_SIGNATURE_INVALID";
    case SandboxedUnpackerFailureReason::
        CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED:
      return u"CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED";
    case SandboxedUnpackerFailureReason::CRX_SIGNATURE_VERIFICATION_FAILED:
      return u"CRX_SIGNATURE_VERIFICATION_FAILED";
    case SandboxedUnpackerFailureReason::CRX_FILE_IS_DELTA_UPDATE:
      return u"CRX_FILE_IS_DELTA_UPDATE";
    case SandboxedUnpackerFailureReason::CRX_EXPECTED_HASH_INVALID:
      return u"CRX_EXPECTED_HASH_INVALID";

    case SandboxedUnpackerFailureReason::ERROR_SERIALIZING_MANIFEST_JSON:
      return u"ERROR_SERIALIZING_MANIFEST_JSON";
    case SandboxedUnpackerFailureReason::ERROR_SAVING_MANIFEST_JSON:
      return u"ERROR_SAVING_MANIFEST_JSON";

    case SandboxedUnpackerFailureReason::INVALID_PATH_FOR_BROWSER_IMAGE:
      return u"INVALID_PATH_FOR_BROWSER_IMAGE";
    case SandboxedUnpackerFailureReason::ERROR_REMOVING_OLD_IMAGE_FILE:
      return u"ERROR_REMOVING_OLD_IMAGE_FILE";
    case SandboxedUnpackerFailureReason::INVALID_PATH_FOR_BITMAP_IMAGE:
      return u"INVALID_PATH_FOR_BITMAP_IMAGE";
    case SandboxedUnpackerFailureReason::ERROR_RE_ENCODING_THEME_IMAGE:
      return u"ERROR_RE_ENCODING_THEME_IMAGE";
    case SandboxedUnpackerFailureReason::ERROR_SAVING_THEME_IMAGE:
      return u"ERROR_SAVING_THEME_IMAGE";

    case SandboxedUnpackerFailureReason::INVALID_CATALOG_DATA:
      return u"INVALID_CATALOG_DATA";
    case SandboxedUnpackerFailureReason::ERROR_SERIALIZING_CATALOG:
      return u"ERROR_SERIALIZING_CATALOG";
    case SandboxedUnpackerFailureReason::ERROR_SAVING_CATALOG:
      return u"ERROR_SAVING_CATALOG";

    case SandboxedUnpackerFailureReason::CRX_HASH_VERIFICATION_FAILED:
      return u"CRX_HASH_VERIFICATION_FAILED";

    case SandboxedUnpackerFailureReason::UNZIP_FAILED:
      return u"UNZIP_FAILED";
    case SandboxedUnpackerFailureReason::DIRECTORY_MOVE_FAILED:
      return u"DIRECTORY_MOVE_FAILED";

    case SandboxedUnpackerFailureReason::ERROR_INDEXING_DNR_RULESET:
      return u"ERROR_INDEXING_DNR_RULESET";

    case SandboxedUnpackerFailureReason::CRX_REQUIRED_PROOF_MISSING:
      return u"CRX_REQUIRED_PROOF_MISSING";

    case SandboxedUnpackerFailureReason::DEPRECATED_ABORTED_DUE_TO_SHUTDOWN:
    case SandboxedUnpackerFailureReason::DEPRECATED_ERROR_PARSING_DNR_RULESET:
    case SandboxedUnpackerFailureReason::NUM_FAILURE_REASONS:
    default:
      NOTREACHED_IN_MIGRATION();
      return std::u16string();
  }
}

void SandboxedUnpacker::FailWithPackageError(
    const SandboxedUnpackerFailureReason reason) {
  ReportFailure(reason,
                l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_ERROR_CODE,
                                           FailureReasonToString16(reason)));
}

bool SandboxedUnpacker::ValidateSignature(
    const base::FilePath& crx_path,
    const std::string& expected_hash,
    const crx_file::VerifierFormat required_format) {
  std::vector<uint8_t> hash;
  if (!expected_hash.empty()) {
    if (!base::HexStringToBytes(expected_hash, &hash)) {
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_EXPECTED_HASH_INVALID);
      return false;
    }
  }

  const crx_file::VerifierResult result = crx_file::Verify(
      crx_path, required_format, std::vector<std::vector<uint8_t>>(), hash,
      &public_key_, &extension_id_, &compressed_verified_contents_);

  switch (result) {
    case crx_file::VerifierResult::OK_FULL: {
      return true;
    }
    case crx_file::VerifierResult::OK_DELTA:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_FILE_IS_DELTA_UPDATE);
      break;
    case crx_file::VerifierResult::ERROR_FILE_NOT_READABLE:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE);
      break;
    case crx_file::VerifierResult::ERROR_HEADER_INVALID:
      FailWithPackageError(SandboxedUnpackerFailureReason::CRX_HEADER_INVALID);
      break;
    case crx_file::VerifierResult::ERROR_SIGNATURE_INITIALIZATION_FAILED:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::
              CRX_SIGNATURE_VERIFICATION_INITIALIZATION_FAILED);
      break;
    case crx_file::VerifierResult::ERROR_SIGNATURE_VERIFICATION_FAILED:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_SIGNATURE_VERIFICATION_FAILED);
      break;
    case crx_file::VerifierResult::ERROR_EXPECTED_HASH_INVALID:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_EXPECTED_HASH_INVALID);
      break;
    case crx_file::VerifierResult::ERROR_REQUIRED_PROOF_MISSING:
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_REQUIRED_PROOF_MISSING);
      break;
    case crx_file::VerifierResult::ERROR_FILE_HASH_FAILED:
      // We should never get this result unless we had specifically asked for
      // verification of the crx file's hash.
      CHECK(!expected_hash.empty());
      FailWithPackageError(
          SandboxedUnpackerFailureReason::CRX_HASH_VERIFICATION_FAILED);
      break;
  }

  return false;
}

void SandboxedUnpacker::ReportFailure(
    const SandboxedUnpackerFailureReason reason,
    const std::u16string& error) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.SandboxUnpackFailureReason2", reason,
      SandboxedUnpackerFailureReason::NUM_FAILURE_REASONS);
  Cleanup();

  client_->OnUnpackFailure(CrxInstallError(reason, error));
}

void SandboxedUnpacker::ReportSuccess() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());

  UMA_HISTOGRAM_COUNTS_1M("Extensions.SandboxUnpackSuccess", 1);

  DCHECK(!temp_dir_.GetPath().empty());

  // Client takes ownership of temporary directory, manifest, and extension.
  client_->OnUnpackSuccess(
      temp_dir_.Take(), extension_root_,
      std::make_unique<base::Value::Dict>(std::move(manifest_.value())),
      extension_.get(), install_icon_, std::move(ruleset_install_prefs_));

  // Interestingly, the C++ standard doesn't guarantee that a moved-from vector
  // is empty.
  ruleset_install_prefs_.clear();

  extension_.reset();

  Cleanup();
}

std::optional<base::Value::Dict> SandboxedUnpacker::RewriteManifestFile(
    const base::Value::Dict& manifest) {
  constexpr int64_t kMaxFingerprintSize = 1024;

  // Add the public key extracted earlier to the parsed manifest and overwrite
  // the original manifest. We do this to ensure the manifest doesn't contain an
  // exploitable bug that could be used to compromise the browser.
  DCHECK(!public_key_.empty());
  base::Value::Dict final_manifest = manifest.Clone();
  final_manifest.Set(manifest_keys::kPublicKey, public_key_);

  {
    std::string differential_fingerprint;
    if (base::ReadFileToStringWithMaxSize(
            extension_root_.Append(kDifferentialFingerprintFilename),
            &differential_fingerprint, kMaxFingerprintSize)) {
      final_manifest.Set(manifest_keys::kDifferentialFingerprint,
                         std::move(differential_fingerprint));
    }
  }

  std::string manifest_json;
  JSONStringValueSerializer serializer(&manifest_json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(final_manifest)) {
    // Error serializing manifest.json.
    ReportFailure(
        SandboxedUnpackerFailureReason::ERROR_SERIALIZING_MANIFEST_JSON,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"ERROR_SERIALIZING_MANIFEST_JSON"));
    return std::nullopt;
  }

  base::FilePath manifest_path = extension_root_.Append(kManifestFilename);
  if (!base::WriteFile(manifest_path, manifest_json)) {
    // Error saving manifest.json.
    ReportFailure(
        SandboxedUnpackerFailureReason::ERROR_SAVING_MANIFEST_JSON,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_PACKAGE_INSTALL_ERROR,
                                   u"ERROR_SAVING_MANIFEST_JSON"));
    return std::nullopt;
  }

  return std::move(final_manifest);
}

void SandboxedUnpacker::Cleanup() {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  if (temp_dir_.IsValid() && !temp_dir_.Delete()) {
    LOG(WARNING) << "Can not delete temp directory at "
                 << temp_dir_.GetPath().value();
  }

  io_thread_state_->CleanUp();
}

void SandboxedUnpacker::ParseJsonFile(
    const base::FilePath& path,
    data_decoder::mojom::JsonParser::ParseCallback callback) {
  DCHECK(unpacker_io_task_runner_->RunsTasksInCurrentSequence());
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    std::move(callback).Run(
        /*value=*/std::nullopt,
        /*error=*/std::optional<std::string>("File doesn't exist."));
    return;
  }

  GetJsonParserPtr()->Parse(contents, base::JSON_PARSE_CHROMIUM_EXTENSIONS,
                            std::move(callback));
}

}  // namespace extensions
