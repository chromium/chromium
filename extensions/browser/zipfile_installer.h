// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
#define EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"

namespace extensions {

using ZipResultVariant = absl::variant<base::FilePath, std::string>;

// ZipFileInstaller unzips an extension safely using the Unzipper and
// SafeJSONParser services.
// This class is not thread-safe: it is bound to the sequence it is created on.
class ZipFileInstaller : public base::RefCountedThreadSafe<ZipFileInstaller> {
 public:
  // The callback invoked when the ZIP file installation is finished.
  // On success, |unzip_dir| points to the directory the ZIP file was installed
  // and |error| is empty. On failure, |unzip_dir| is empty and |error| contains
  // an error message describing the failure.
  using DoneCallback = base::OnceCallback<void(const base::FilePath& zip_file,
                                               const base::FilePath& unzip_dir,
                                               const std::string& error)>;

  ZipFileInstaller(const ZipFileInstaller&) = delete;
  ZipFileInstaller& operator=(const ZipFileInstaller&) = delete;

  // Creates a ZipFileInstaller that invokes |done_callback| when done.
  static scoped_refptr<ZipFileInstaller> Create(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      DoneCallback done_callback);

  // First attempts to create `unpacked_extensions_dir` and does not load the
  // extension if unsuccessful. If successful, then unzips the extension into a
  // unique directory within `unpacked_extensions_dir`.
  // `unpacked_extensions_dir` should be the unpacked extensions directory from
  // the extensions service. The directory name will have the format of
  // "hello-world.zip" -> "hello-world_XXXXXX/" in the style of mkdtemp().
  void InstallZipFileToUnpackedExtensionsDir(
      const base::FilePath& zip_file,
      const base::FilePath& unpacked_extensions_dir);

  // Unzips the extension in `unzip_dir`. If `unzip_dir` is empty, the extension
  // will not be unzipped.
  void LoadFromZipFileInDir(const base::FilePath& zip_file,
                            const base::FilePath& unzip_dir);

 private:
  friend class base::RefCountedThreadSafe<ZipFileInstaller>;
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerFilterTest,
                           NonTheme_FileExtractionFilter);
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerFilterTest,
                           Theme_FileExtractionFilter);
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerFilterTest,
                           ManifestExtractionFilter);

  explicit ZipFileInstaller(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
      DoneCallback done_callback);
  ~ZipFileInstaller();

  // Unzip `zip_file` into `unzip_dir`. `create_unzip_dir` indicates that
  // `unzip_dir` might need to be created before installing the .zip file to the
  // dir. extensions_features::kExtensionsZipFileInstalledInProfileDir being
  // enabled causes `create_unzip_dir` to create a
  void LoadFromZipFileImpl(const base::FilePath& zip_file,
                           const base::FilePath& unzip_dir,
                           bool create_unzip_dir = false);

  // Unzip an extension the `base::FilePath` provided by the result and load it
  // with an UnpackedInstaller. String in the result is an error explaining why
  // the path couldn't be created.
  void Unzip(ZipResultVariant result);

  void ManifestUnzipped(const base::FilePath& unzip_dir, bool success);
  void ManifestRead(const base::FilePath& unzip_dir,
                    std::optional<std::string> manifest_content);
  void ManifestParsed(const base::FilePath& unzip_dir,
                      std::optional<base::Value> result,
                      const std::optional<std::string>& error);
  void UnzipDone(const base::FilePath& unzip_dir, bool success);

  // On failure, report the |error| reason.
  void ReportFailure(const std::string& error);

  // Callback invoked when unzipping has finished.
  DoneCallback done_callback_;

  // Whether a file should be extracted as part of installing an
  // extension/theme. Protects against unused or potentially hamrful files.
  static bool ShouldExtractFile(bool is_theme, const base::FilePath& file_path);

  // Returns true if |file_path| points to an extension manifest.
  static bool IsManifestFile(const base::FilePath& file_path);

  // File containing the extension to unzip.
  base::FilePath zip_file_;

  // Task runner for file I/O.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
