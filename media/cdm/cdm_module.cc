// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_module.h"

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "media/cdm/cdm_host_files.h"
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

// INITIALIZE_CDM_MODULE is a macro in api/content_decryption_module.h.
// However, we need to pass it as a string to GetFunctionPointer(). The follow
// macro helps expanding it into a string.
#define STRINGIFY(X) #X
#define MAKE_STRING(X) STRINGIFY(X)

namespace media {

namespace {

static CdmModule* g_cdm_module = nullptr;

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
void InitCdmHostVerification(
    base::NativeLibrary cdm_library,
    const base::FilePath& cdm_path,
    const std::vector<CdmHostFilePath>& cdm_host_file_paths) {
  DCHECK(cdm_library);

  CdmHostFiles cdm_host_files;
  cdm_host_files.Initialize(cdm_path, cdm_host_file_paths);

  auto status = cdm_host_files.InitVerification(cdm_library);

  UMA_HISTOGRAM_ENUMERATION("Media.EME.CdmHostVerificationStatus", status,
                            CdmHostFiles::Status::kStatusCount);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

// These enums are reported to UMA so values should not be renumbered or reused.
enum class LoadResult {
  kLoadSuccess,
  kFileMissing,        // The CDM does not exist.
  kLoadFailed,         // CDM exists but LoadNativeLibrary() failed.
  kEntryPointMissing,  // CDM loaded but somce required entry point missing.
  // NOTE: Add new values only immediately above this line.
  kLoadResultCount  // Boundary value for UMA_HISTOGRAM_ENUMERATION.
};

void ReportLoadResult(LoadResult load_result) {
  DCHECK_LT(load_result, LoadResult::kLoadResultCount);
  UMA_HISTOGRAM_ENUMERATION("Media.EME.CdmLoadResult", load_result,
                            LoadResult::kLoadResultCount);
}

void ReportLoadErrorCode(const base::NativeLibraryLoadError* error) {
// Only report load error code on Windows because that's the only platform that
// has a numerical error value.
#if defined(OS_WIN)
  base::UmaHistogramSparse("Media.EME.CdmLoadErrorCode", error->code);
#endif
}

void ReportLoadTime(const base::TimeDelta load_time) {
  UMA_HISTOGRAM_TIMES("Media.EME.CdmLoadTime", load_time);
}

}  // namespace

// static
CdmModule* CdmModule::GetInstance() {
  // The |cdm_module| will be leaked and we will not be able to call
  // |deinitialize_cdm_module_func_|. This is fine since it's never guaranteed
  // to be called, e.g. in the fast shutdown case.
  // TODO(xhwang): Find a better ownership model to make sure |cdm_module| is
  // destructed properly whenever possible (e.g. in non-fast-shutdown case).
  if (!g_cdm_module)
    g_cdm_module = new CdmModule();

  return g_cdm_module;
}

// static
void CdmModule::ResetInstanceForTesting() {
  if (!g_cdm_module)
    return;

  delete g_cdm_module;
  g_cdm_module = nullptr;
}

CdmModule::CdmModule() = default;

CdmModule::~CdmModule() {
  if (deinitialize_cdm_module_func_)
    deinitialize_cdm_module_func_();
}

CdmModule::CreateCdmFunc CdmModule::GetCreateCdmFunc() {
  if (!was_initialize_called_) {
    NOTREACHED() << __func__ << " called before CdmModule is initialized.";
    return nullptr;
  }

  // If initialization failed, nullptr will be returned.
  return create_cdm_func_;
}

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
bool CdmModule::Initialize(const base::FilePath& cdm_path,
                           std::vector<CdmHostFilePath> cdm_host_file_paths) {
#else
bool CdmModule::Initialize(const base::FilePath& cdm_path) {
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();

  DCHECK(!was_initialize_called_);
  was_initialize_called_ = true;

  cdm_path_ = cdm_path;

  // Load the CDM.
  base::TimeTicks start = base::TimeTicks::Now();
  library_ = base::ScopedNativeLibrary(cdm_path);
  base::TimeDelta load_time = base::TimeTicks::Now() - start;
  if (!library_.is_valid()) {
    LOG(ERROR) << "CDM at " << cdm_path.value() << " could not be loaded.";
    LOG(ERROR) << "Error: " << library_.GetError()->ToString();
    ReportLoadResult(base::PathExists(cdm_path) ? LoadResult::kLoadFailed
                                                : LoadResult::kFileMissing);
    ReportLoadErrorCode(library_.GetError());
    return false;
  }

  // Only report load time for success loads.
  ReportLoadTime(load_time);

  // Get function pointers.
  // TODO(xhwang): Define function names in macros to avoid typo errors.
  initialize_cdm_module_func_ = reinterpret_cast<InitializeCdmModuleFunc>(
      library_.GetFunctionPointer(MAKE_STRING(INITIALIZE_CDM_MODULE)));
  deinitialize_cdm_module_func_ = reinterpret_cast<DeinitializeCdmModuleFunc>(
      library_.GetFunctionPointer("DeinitializeCdmModule"));
  create_cdm_func_ = reinterpret_cast<CreateCdmFunc>(
      library_.GetFunctionPointer("CreateCdmInstance"));
  get_cdm_version_func_ = reinterpret_cast<GetCdmVersionFunc>(
      library_.GetFunctionPointer("GetCdmVersion"));

  if (!initialize_cdm_module_func_ || !deinitialize_cdm_module_func_ ||
      !create_cdm_func_ || !get_cdm_version_func_) {
    LOG(ERROR) << "Missing entry function in CDM at " << cdm_path.value();
    initialize_cdm_module_func_ = nullptr;
    deinitialize_cdm_module_func_ = nullptr;
    create_cdm_func_ = nullptr;
    get_cdm_version_func_ = nullptr;
    library_.reset();
    ReportLoadResult(LoadResult::kEntryPointMissing);
    return false;
  }

  // In case of crashes, provide CDM version to facilitate investigation.
  std::string cdm_version = get_cdm_version_func_();
  DVLOG(2) << __func__ << ": cdm_version = " << cdm_version;

  static crash_reporter::CrashKeyString<32> cdm_version_key("cdm-version");
  cdm_version_key.Set(cdm_version);

#if defined(OS_WIN)
  // Load DXVA before sandbox lockdown to give CDM access to Output Protection
  // Manager (OPM).
  LoadLibraryA("dxva2.dll");
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  InitCdmHostVerification(library_.get(), cdm_path_, cdm_host_file_paths);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  ReportLoadResult(LoadResult::kLoadSuccess);
  return true;
}

void CdmModule::InitializeCdmModule() {
  DCHECK(was_initialize_called_);
  DCHECK(initialize_cdm_module_func_);
  initialize_cdm_module_func_();
}

base::FilePath CdmModule::GetCdmPath() const {
  DCHECK(was_initialize_called_);
  return cdm_path_;
}

}  // namespace media
