// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_MODULE_H_
#define MEDIA_CDM_CDM_MODULE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "media/base/media_export.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/media_buildflags.h"

#if !BUILDFLAG(ENABLE_LIBRARY_CDMS)
#error This file only applies to builds that enable_library_cdms.
#endif

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_file.h"
#include "media/cdm/cdm_host_files.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

namespace media {

class MEDIA_EXPORT CdmModule {
 public:
  static CdmModule* GetInstance();

  // Reset the CdmModule instance so that each test have it's own instance.
  static void ResetInstanceForTesting();

  CdmModule(const CdmModule&) = delete;
  CdmModule& operator=(const CdmModule&) = delete;

  ~CdmModule();

  using CreateCdmFunc = decltype(&::CreateCdmInstance);

  CreateCdmFunc GetCreateCdmFunc();

// Loads the CDM, initialize function pointers and initialize the CDM module.
// This must only be called only once.
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  bool Initialize(const base::FilePath& cdm_path,
                  std::vector<CdmHostFilePath> cdm_host_file_paths);
#else
  bool Initialize(const base::FilePath& cdm_path);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // Calls INITIALIZE_CDM_MODULE on the actually library CDM. Must be called
  // within the sandbox!
  void InitializeCdmModule();

 private:
  using InitializeCdmModuleFunc = decltype(&::INITIALIZE_CDM_MODULE);
  using DeinitializeCdmModuleFunc = decltype(&::DeinitializeCdmModule);
  using GetCdmVersionFunc = decltype(&::GetCdmVersion);

  CdmModule();

  bool initialized_ = false;
  base::FilePath cdm_path_;
  base::ScopedNativeLibrary library_;
  CreateCdmFunc create_cdm_func_ = nullptr;
  InitializeCdmModuleFunc initialize_cdm_module_func_ = nullptr;
  DeinitializeCdmModuleFunc deinitialize_cdm_module_func_ = nullptr;
  GetCdmVersionFunc get_cdm_version_func_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_MODULE_H_
