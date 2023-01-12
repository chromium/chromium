// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_FILE_VALIDATOR_H_
#define STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_FILE_VALIDATOR_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback.h"

namespace base {
class FilePath;
}

namespace storage {

class FileSystemURL;

class COMPONENT_EXPORT(STORAGE_BROWSER) CopyOrMoveFileValidator {
 public:
  // Callback that is invoked when validation completes. A result of
  // base::File::FILE_OK means the file validated.
  using ResultCallback = base::OnceCallback<void(base::File::Error result)>;

  CopyOrMoveFileValidator(const CopyOrMoveFileValidator&) = delete;
  CopyOrMoveFileValidator& operator=(const CopyOrMoveFileValidator&) = delete;
  virtual ~CopyOrMoveFileValidator() = default;

  // Called on a source file before copying or moving to the final
  // destination.
  virtual void StartPreWriteValidation(ResultCallback result_callback) = 0;

  // Called on a destination file after copying or moving to the final
  // destination. Suitable for running Anti-Virus checks.
  virtual void StartPostWriteValidation(
      const base::FilePath& dest_platform_path,
      ResultCallback result_callback) = 0;

 protected:
  CopyOrMoveFileValidator() = default;
};

class CopyOrMoveFileValidatorFactory {
 public:
  CopyOrMoveFileValidatorFactory(const CopyOrMoveFileValidatorFactory&) =
      delete;
  CopyOrMoveFileValidatorFactory& operator=(
      const CopyOrMoveFileValidatorFactory&) = delete;
  virtual ~CopyOrMoveFileValidatorFactory() = default;

  // This method must always return a non-null validator. |src_url| is needed
  // in addition to |platform_path| because in the obfuscated file system
  // case, |platform_path| will be an obfuscated filename and extension.
  virtual CopyOrMoveFileValidator* CreateCopyOrMoveFileValidator(
      const FileSystemURL& src_url,
      const base::FilePath& platform_path) = 0;

 protected:
  CopyOrMoveFileValidatorFactory() = default;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_COPY_OR_MOVE_FILE_VALIDATOR_H_
