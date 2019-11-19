// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
#define EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {

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

  // Creates a ZipFileInstaller that invokes |done_callback| when done.
  static scoped_refptr<ZipFileInstaller> Create(DoneCallback done_callback);

  // Creates a temporary directory and unzips the extension in it.
  void LoadFromZipFile(const base::FilePath& zip_file);

  // Unzips the extension in |unzip_dir|.
  void LoadFromZipFileInDir(const base::FilePath& zip_file,
                            const base::FilePath& unzip_dir);

 private:
  friend class base::RefCountedThreadSafe<ZipFileInstaller>;
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerTest, NonTheme_FileExtractionFilter);
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerTest, Theme_FileExtractionFilter);
  FRIEND_TEST_ALL_PREFIXES(ZipFileInstallerTest, ManifestExtractionFilter);

  explicit ZipFileInstaller(DoneCallback done_callback);
  ~ZipFileInstaller();

  void LoadFromZipFileImpl(const base::FilePath& zip_file,
                           const base::FilePath& unzip_dir);

  // Unzip an extension into |unzip_dir| and load it with an UnpackedInstaller.
  void Unzip(base::Optional<base::FilePath> unzip_dir);
  void ManifestUnzipped(const base::FilePath& unzip_dir, bool success);
  void ManifestRead(const base::FilePath& unzip_dir,
                    base::Optional<std::string> manifest_content);
  void ManifestParsed(const base::FilePath& unzip_dir,
                      data_decoder::DataDecoder::ValueOrError result);
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

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ZipFileInstaller);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ZIPFILE_INSTALLER_H_
