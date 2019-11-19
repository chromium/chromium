// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/zipfile_installer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

constexpr char kExtensionHandlerTempDirError[] =
    "Could not create temporary directory for zipped extension.";
constexpr char kExtensionHandlerFileUnzipError[] =
    "Could not unzip extension for install.";

constexpr const base::FilePath::CharType* kAllowedThemeFiletypes[] = {
    FILE_PATH_LITERAL(".bmp"),  FILE_PATH_LITERAL(".gif"),
    FILE_PATH_LITERAL(".jpeg"), FILE_PATH_LITERAL(".jpg"),
    FILE_PATH_LITERAL(".json"), FILE_PATH_LITERAL(".png"),
    FILE_PATH_LITERAL(".webp")};

base::Optional<base::FilePath> PrepareAndGetUnzipDir(
    const base::FilePath& zip_file) {
  base::FilePath dir_temp;
  base::PathService::Get(base::DIR_TEMP, &dir_temp);

  base::FilePath::StringType dir_name =
      zip_file.RemoveExtension().BaseName().value() + FILE_PATH_LITERAL("_");

  base::FilePath unzip_dir;
  if (!base::CreateTemporaryDirInDir(dir_temp, dir_name, &unzip_dir))
    return base::Optional<base::FilePath>();

  return unzip_dir;
}

base::Optional<std::string> ReadFileContent(const base::FilePath& path) {
  std::string content;
  return base::ReadFileToString(path, &content) ? content
                                                : base::Optional<std::string>();
}

}  // namespace

// static
scoped_refptr<ZipFileInstaller> ZipFileInstaller::Create(
    DoneCallback done_callback) {
  DCHECK(done_callback);
  return base::WrapRefCounted(new ZipFileInstaller(std::move(done_callback)));
}

void ZipFileInstaller::LoadFromZipFile(const base::FilePath& zip_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LoadFromZipFileImpl(zip_file, base::FilePath());
}

void ZipFileInstaller::LoadFromZipFileInDir(const base::FilePath& zip_file,
                                            const base::FilePath& unzip_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!unzip_dir.empty());
  LoadFromZipFileImpl(zip_file, unzip_dir);
}

void ZipFileInstaller::LoadFromZipFileImpl(const base::FilePath& zip_file,
                                           const base::FilePath& unzip_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!zip_file.empty());

  zip_file_ = zip_file;

  if (!unzip_dir.empty()) {
    Unzip(unzip_dir);
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&PrepareAndGetUnzipDir, zip_file),
      base::BindOnce(&ZipFileInstaller::Unzip, this));
}

ZipFileInstaller::ZipFileInstaller(DoneCallback done_callback)
    : done_callback_(std::move(done_callback)) {}

ZipFileInstaller::~ZipFileInstaller() = default;

void ZipFileInstaller::Unzip(base::Optional<base::FilePath> unzip_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!unzip_dir) {
    ReportFailure(std::string(kExtensionHandlerTempDirError));
    return;
  }

  unzip::UnzipWithFilter(
      unzip::LaunchUnzipper(), zip_file_, *unzip_dir,
      base::BindRepeating(&ZipFileInstaller::IsManifestFile),
      base::BindOnce(&ZipFileInstaller::ManifestUnzipped, this, *unzip_dir));
}

void ZipFileInstaller::ManifestUnzipped(const base::FilePath& unzip_dir,
                                        bool success) {
  if (!success) {
    ReportFailure(kExtensionHandlerFileUnzipError);
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&ReadFileContent, unzip_dir.Append(kManifestFilename)),
      base::BindOnce(&ZipFileInstaller::ManifestRead, this, unzip_dir));
}

void ZipFileInstaller::ManifestRead(
    const base::FilePath& unzip_dir,
    base::Optional<std::string> manifest_content) {
  if (!manifest_content) {
    ReportFailure(std::string(kExtensionHandlerFileUnzipError));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *manifest_content,
      base::BindOnce(&ZipFileInstaller::ManifestParsed, this, unzip_dir));
}

void ZipFileInstaller::ManifestParsed(
    const base::FilePath& unzip_dir,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    ReportFailure(std::string(kExtensionHandlerFileUnzipError));
    return;
  }

  std::unique_ptr<base::DictionaryValue> manifest_dictionary =
      base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(std::move(*result.value)));
  if (!manifest_dictionary) {
    ReportFailure(std::string(kExtensionHandlerFileUnzipError));
    return;
  }

  Manifest manifest(Manifest::INTERNAL, std::move(manifest_dictionary));

  unzip::UnzipFilterCallback filter = base::BindRepeating(
      [](bool is_theme, const base::FilePath& file_path) -> bool {
        // Note that we ignore the manifest as it has already been extracted and
        // would cause the unzipping to fail.
        return ZipFileInstaller::ShouldExtractFile(is_theme, file_path) &&
               !ZipFileInstaller::IsManifestFile(file_path);
      },
      manifest.is_theme());

  // TODO(crbug.com/645263): This silently ignores blocked file types.
  //                         Add install warnings.
  unzip::UnzipWithFilter(
      unzip::LaunchUnzipper(), zip_file_, unzip_dir, filter,
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
    if (extension.empty())
      return true;
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
