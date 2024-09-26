// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/zipfile_installer.h"

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

constexpr char kExtensionHandlerUnpackedDirCreationError[] =
    "Failed to create root unpacked directory * for "
    "zip file: *. Encountered error: *.";
constexpr char kExtensionHandlerZippedDirError[] =
    "Could not create directory * for zipped extension.";
constexpr char kExtensionHandlerFileUnzipError[] =
    "Could not unzip extension for install.";

constexpr const base::FilePath::CharType* kAllowedThemeFiletypes[] = {
    FILE_PATH_LITERAL(".bmp"),  FILE_PATH_LITERAL(".gif"),
    FILE_PATH_LITERAL(".jpeg"), FILE_PATH_LITERAL(".jpg"),
    FILE_PATH_LITERAL(".json"), FILE_PATH_LITERAL(".png"),
    FILE_PATH_LITERAL(".webp")};

// Creates a unique directory based on `zip_file` inside `root_unzip_dir`.
// Directory format is (`zip_file` == "myzip.zip"):
//   <`root_unzip_dir`>/myzip_XXXXXX
// XXXXXX is populated with mkdtemp() logic.
ZipResultVariant PrepareAndGetUnzipDir(const base::FilePath& zip_file,
                                       const base::FilePath& root_unzip_dir) {
  // Create `root_unzip_dir`. This should only occur once per profile as
  // CreateDirectoryAndGetError check for `root_unzip_dir` to exist first.
  base::File::Error root_unzip_dir_creation_error;
  if (!base::CreateDirectoryAndGetError(root_unzip_dir,
                                        &root_unzip_dir_creation_error)) {
    return ZipResultVariant{ErrorUtils::FormatErrorMessage(
        kExtensionHandlerUnpackedDirCreationError,
        base::UTF16ToUTF8(root_unzip_dir.LossyDisplayName()),
        base::UTF16ToUTF8(zip_file.LossyDisplayName()),
        base::File::ErrorToString(root_unzip_dir_creation_error))};
  }

  // Create the root of the unique directory for the .zip file.
  base::FilePath::StringType dir_name =
      zip_file.RemoveExtension().BaseName().value() + FILE_PATH_LITERAL("_");

  // Creates the full unique directory path as unzip_dir.
  base::FilePath unzip_dir;
  if (!base::CreateTemporaryDirInDir(root_unzip_dir, dir_name, &unzip_dir)) {
    return ZipResultVariant{ErrorUtils::FormatErrorMessage(
        kExtensionHandlerZippedDirError,
        base::UTF16ToUTF8(unzip_dir.LossyDisplayName()))};
  }

  return ZipResultVariant{unzip_dir};
}

std::optional<std::string> ReadFileContent(const base::FilePath& path) {
  std::string content;
  return base::ReadFileToString(path, &content) ? content
                                                : std::optional<std::string>();
}

}  // namespace

// static
scoped_refptr<ZipFileInstaller> ZipFileInstaller::Create(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    DoneCallback done_callback) {
  DCHECK(done_callback);
  return base::WrapRefCounted(
      new ZipFileInstaller(io_task_runner, std::move(done_callback)));
}

void ZipFileInstaller::InstallZipFileToUnpackedExtensionsDir(
    const base::FilePath& zip_file,
    const base::FilePath& unpacked_extensions_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!unpacked_extensions_dir.empty());
  LoadFromZipFileImpl(zip_file, unpacked_extensions_dir,
                      /*create_unzip_dir=*/true);
}

void ZipFileInstaller::LoadFromZipFileInDir(const base::FilePath& zip_file,
                                            const base::FilePath& unzip_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!unzip_dir.empty());
  LoadFromZipFileImpl(zip_file, unzip_dir);
}

void ZipFileInstaller::LoadFromZipFileImpl(const base::FilePath& zip_file,
                                           const base::FilePath& unzip_dir,
                                           bool create_unzip_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!zip_file.empty());

  zip_file_ = zip_file;

  if (create_unzip_dir) {
      io_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PrepareAndGetUnzipDir, zip_file, unzip_dir),
          base::BindOnce(&ZipFileInstaller::Unzip, this));
    return;
  }

  // Unzip dir should exist so unzip directly there.
  // ZipResultVariant result = ZipResultVariant{unzip_dir};
  Unzip(ZipResultVariant{unzip_dir});
}

ZipFileInstaller::ZipFileInstaller(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    DoneCallback done_callback)
    : done_callback_(std::move(done_callback)),
      io_task_runner_(io_task_runner) {}

ZipFileInstaller::~ZipFileInstaller() = default;

void ZipFileInstaller::Unzip(ZipResultVariant unzip_dir_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (absl::holds_alternative<std::string>(unzip_dir_or_error)) {
    ReportFailure(absl::get<std::string>(unzip_dir_or_error));
    return;
  }

  base::FilePath unzip_dir = absl::get<base::FilePath>(unzip_dir_or_error);
  unzip::Unzip(
      unzip::LaunchUnzipper(), zip_file_, unzip_dir,
      unzip::mojom::UnzipOptions::New(),
      base::BindRepeating(&ZipFileInstaller::IsManifestFile), base::DoNothing(),
      base::BindOnce(&ZipFileInstaller::ManifestUnzipped, this, unzip_dir));
}

void ZipFileInstaller::ManifestUnzipped(const base::FilePath& unzip_dir,
                                        bool success) {
  if (!success) {
    ReportFailure(kExtensionHandlerFileUnzipError);
    return;
  }

  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadFileContent, unzip_dir.Append(kManifestFilename)),
      base::BindOnce(&ZipFileInstaller::ManifestRead, this, unzip_dir));
}

void ZipFileInstaller::ManifestRead(
    const base::FilePath& unzip_dir,
    std::optional<std::string> manifest_content) {
  if (!manifest_content) {
    ReportFailure(std::string(kExtensionHandlerFileUnzipError));
    return;
  }

  // Create a DataDecoder to specify custom parse options to the JSON
  // parser. The ownership of the |data_decoder| and |json_parser|
  // transfer to the response callback and are deleted after it runs.
  auto data_decoder = std::make_unique<data_decoder::DataDecoder>();
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser;
  data_decoder->GetService()->BindJsonParser(
      json_parser.BindNewPipeAndPassReceiver());
  json_parser.set_disconnect_handler(
      base::BindOnce(&ZipFileInstaller::ManifestParsed, this, unzip_dir,
                     std::nullopt, "Data Decoder terminated unexpectedly"));
  auto* json_parser_ptr = json_parser.get();
  json_parser_ptr->Parse(
      *manifest_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS,
      base::BindOnce(
          [](std::unique_ptr<data_decoder::DataDecoder>,
             mojo::Remote<data_decoder::mojom::JsonParser>,
             scoped_refptr<ZipFileInstaller> installer,
             const base::FilePath& unzip_dir, std::optional<base::Value> value,
             const std::optional<std::string>& error) {
            installer->ManifestParsed(unzip_dir, std::move(value), error);
          },
          std::move(data_decoder), std::move(json_parser),
          base::WrapRefCounted(this), unzip_dir));
}

void ZipFileInstaller::ManifestParsed(const base::FilePath& unzip_dir,
                                      std::optional<base::Value> result,
                                      const std::optional<std::string>& error) {
  if (!result || !result->is_dict()) {
    ReportFailure(std::string(kExtensionHandlerFileUnzipError));
    return;
  }

  Manifest::Type manifest_type =
      Manifest::GetTypeFromManifestValue(result->GetDict());

  unzip::UnzipFilterCallback filter = base::BindRepeating(
      [](bool is_theme, const base::FilePath& file_path) -> bool {
        // Note that we ignore the manifest as it has already been extracted and
        // would cause the unzipping to fail.
        return ZipFileInstaller::ShouldExtractFile(is_theme, file_path) &&
               !ZipFileInstaller::IsManifestFile(file_path);
      },
      manifest_type == Manifest::TYPE_THEME);

  // TODO(crbug.com/41274425): This silently ignores blocked file types.
  //                         Add install warnings.
  unzip::Unzip(unzip::LaunchUnzipper(), zip_file_, unzip_dir,
               unzip::mojom::UnzipOptions::New(), filter, base::DoNothing(),
               base::BindOnce(&ZipFileInstaller::UnzipDone, this, unzip_dir));
}

void ZipFileInstaller::UnzipDone(const base::FilePath& unzip_dir,
                                 bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    ReportFailure(kExtensionHandlerFileUnzipError);
    return;
  }

  std::move(done_callback_).Run(zip_file_, unzip_dir, std::string());
}

void ZipFileInstaller::ReportFailure(const std::string& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(done_callback_).Run(zip_file_, base::FilePath(), error);
}

// static
bool ZipFileInstaller::ShouldExtractFile(bool is_theme,
                                         const base::FilePath& file_path) {
  if (is_theme) {
    const base::FilePath::StringType extension =
        base::ToLowerASCII(file_path.FinalExtension());
    // Allow filenames with no extension.
    if (extension.empty()) {
      return true;
    }
    return base::Contains(kAllowedThemeFiletypes, extension);
  }
  return !base::FilePath::CompareEqualIgnoreCase(file_path.FinalExtension(),
                                                 FILE_PATH_LITERAL(".exe"));
}

// static
bool ZipFileInstaller::IsManifestFile(const base::FilePath& file_path) {
  CHECK(!file_path.IsAbsolute());
  return base::FilePath::CompareEqualIgnoreCase(file_path.value(),
                                                kManifestFilename);
}

}  // namespace extensions
