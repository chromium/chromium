// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_host_files.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module_ext.h"

namespace media {

namespace {

// TODO(xhwang): Move this to a common place if needed.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory, which is the case for the CDM.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}

}  // namespace

CdmHostFiles::CdmHostFiles() {
  DVLOG(1) << __func__;
}

CdmHostFiles::~CdmHostFiles() {
  DVLOG(1) << __func__;
}

void CdmHostFiles::Initialize(
    const base::FilePath& cdm_path,
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  OpenCdmFile(cdm_path);
  OpenCommonFiles(cdm_host_file_paths);
}

CdmHostFiles::Status CdmHostFiles::InitVerification(
    base::NativeLibrary cdm_library) {
  DVLOG(1) << __func__;
  DCHECK(cdm_library);

  // Get function pointer exported by the CDM.
  // See media/cdm/api/content_decryption_module_ext.h.
  using InitVerificationFunc =
      bool (*)(const cdm::HostFile* cdm_host_files, uint32_t num_files);
  static const char kInitVerificationFuncName[] = "VerifyCdmHost_0";

  InitVerificationFunc init_verification_func =
      reinterpret_cast<InitVerificationFunc>(
          base::GetFunctionPointerFromNativeLibrary(cdm_library,
                                                    kInitVerificationFuncName));
  if (!init_verification_func) {
    LOG(ERROR) << "Function " << kInitVerificationFuncName << " not found.";
    CloseAllFiles();
    return Status::kGetFunctionFailed;
  }

  // Fills |cdm_host_files| with common and CDM specific files.
  std::vector<cdm::HostFile> cdm_host_files;
  TakePlatformFiles(&cdm_host_files);

  // std::vector::data() is not guaranteed to be nullptr when empty().
  const cdm::HostFile* cdm_host_files_ptr =
      cdm_host_files.empty() ? nullptr : cdm_host_files.data();

  // Call |init_verification_func| on the CDM with |cdm_host_files|. Note that
  // the ownership of these files are transferred to the CDM, which will close
  // the files immediately after use.
  VLOG(1) << __func__ << ": Calling " << kInitVerificationFuncName << "() with "
          << cdm_host_files.size() << " files.";
  for (const auto& host_file : cdm_host_files) {
    VLOG(1) << " - File Path: " << host_file.file_path;
    VLOG(1) << " - File: " << host_file.file;
    VLOG(1) << " - Sig File: " << host_file.sig_file;
  }

  if (!init_verification_func(cdm_host_files_ptr, cdm_host_files.size())) {
    LOG(ERROR) << "Failed to verify CDM host.";
    CloseAllFiles();
    return Status::kInitVerificationFailed;
  }

  // Close all files not passed to the CDM.
  CloseAllFiles();
  return Status::kSuccess;
}

void CdmHostFiles::CloseAllFiles() {
  common_files_.clear();
  cdm_specific_files_.clear();
}

void CdmHostFiles::OpenCommonFiles(
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  DCHECK(common_files_.empty());

  for (const auto& value : cdm_host_file_paths) {
    common_files_.push_back(
        CdmHostFile::Create(value.file_path, value.sig_file_path));
  }
}

void CdmHostFiles::OpenCdmFile(const base::FilePath& cdm_path) {
  DCHECK(!cdm_path.empty());
  cdm_specific_files_.push_back(
      CdmHostFile::Create(cdm_path, GetSigFilePath(cdm_path)));
}

void CdmHostFiles::TakePlatformFiles(
    std::vector<cdm::HostFile>* cdm_host_files) {
  DCHECK(cdm_host_files->empty());

  // Populate an array of cdm::HostFile.
  for (const auto& file : common_files_)
    cdm_host_files->push_back(file->TakePlatformFile());
  for (const auto& file : cdm_specific_files_)
    cdm_host_files->push_back(file->TakePlatformFile());
}

}  // namespace media
