// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_module.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "load_cdm_uma_helper.h"

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
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

// UMA report prefix
const char kUmaPrefix[] = "Media.EME.Cdm";

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
  if (!initialized_) {
    DLOG(ERROR) << __func__ << " called before CdmModule is initialized.";
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
  CHECK(!initialized_) << "CdmModule can only be initialized once!";

  initialized_ = true;
  cdm_path_ = cdm_path;

  // Load the CDM.
  base::TimeTicks start = base::TimeTicks::Now();
  library_ = base::ScopedNativeLibrary(cdm_path);
  base::TimeDelta load_time = base::TimeTicks::Now() - start;
  if (!library_.is_valid()) {
    LOG(ERROR) << "CDM at " << cdm_path.value() << " could not be loaded.";
    LOG(ERROR) << "Error: " << library_.GetError()->ToString();
    ReportLoadResult(kUmaPrefix, base::PathExists(cdm_path)
                                     ? CdmLoadResult::kLoadFailed
                                     : CdmLoadResult::kFileMissing);
    ReportLoadErrorCode(kUmaPrefix, library_.GetError());
    return false;
  }

  // Only report load time for success loads.
  ReportLoadTime(kUmaPrefix, load_time);

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
    ReportLoadResult(kUmaPrefix, CdmLoadResult::kEntryPointMissing);
    return false;
  }

  // In case of crashes, provide CDM version to facilitate investigation.
  std::string cdm_version = get_cdm_version_func_();
  DVLOG(2) << __func__ << ": cdm_version = " << cdm_version;
  TRACE_EVENT1("media", "CdmModule::Initialize", "cdm_version", cdm_version);

  static crash_reporter::CrashKeyString<32> cdm_version_key("cdm-version");
  cdm_version_key.Set(cdm_version);

#if BUILDFLAG(IS_WIN)
  // Load DXVA before sandbox lockdown to give CDM access to Output Protection
  // Manager (OPM).
  LoadLibraryA("dxva2.dll");
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (base::FeatureList::IsEnabled(media::kCdmHostVerification))
    InitCdmHostVerification(library_.get(), cdm_path_, cdm_host_file_paths);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  ReportLoadResult(kUmaPrefix, CdmLoadResult::kLoadSuccess);
  return true;
}

void CdmModule::InitializeCdmModule() {
  DCHECK(initialized_);
  DCHECK(initialize_cdm_module_func_);
  TRACE_EVENT0("media", "CdmModule::InitializeCdmModule");
  initialize_cdm_module_func_();
}

}  // namespace media
