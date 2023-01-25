// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_HOST_FILES_H_
#define MEDIA_CDM_CDM_HOST_FILES_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module_ext.h"
#include "media/cdm/cdm_host_file.h"

namespace base {
class FilePath;
}

namespace media {

// Manages all CDM host files.
class MEDIA_EXPORT CdmHostFiles {
 public:
  CdmHostFiles();

  CdmHostFiles(const CdmHostFiles&) = delete;
  CdmHostFiles& operator=(const CdmHostFiles&) = delete;

  ~CdmHostFiles();

  // Opens all common files and CDM specific files for the CDM at |cdm_path|.
  void Initialize(const base::FilePath& cdm_path,
                  const std::vector<CdmHostFilePath>& cdm_host_file_paths);

  // Status of CDM host verification.
  // Note: Reported to UMA. Do not change the values.
  enum class Status {
    kNotCalled = 0,
    kSuccess = 1,
    kCdmLoadFailed = 2,
    kGetFunctionFailed = 3,
    kInitVerificationFailed = 4,
    kStatusCount
  };

  // Initializes the verification of CDM files by calling the function exported
  // by the CDM. If unexpected error happens, all files will be closed.
  // Otherwise, the PlatformFiles are passed to the CDM which will close the
  // files later.
  // NOTE: Initialize() must be called before calling this.
  Status InitVerification(base::NativeLibrary cdm_library);

  void CloseAllFiles();

 private:
  // Opens common CDM host files shared by all CDMs.
  void OpenCommonFiles(const std::vector<CdmHostFilePath>& cdm_host_file_paths);

  // Opens the CDM file.
  void OpenCdmFile(const base::FilePath& cdm_path);

  // Fills |cdm_host_files| with common and CDM specific files. The ownership
  // of those files are also transferred.
  void TakePlatformFiles(std::vector<cdm::HostFile>* cdm_host_files);

  using ScopedFileVector = std::vector<std::unique_ptr<CdmHostFile>>;

  // Files common to all CDM types, e.g. main executable.
  ScopedFileVector common_files_;

  // Files specific to each CDM type, e.g. the CDM binary.
  ScopedFileVector cdm_specific_files_;
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_HOST_FILES_H_
