// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/external_clear_key_test_helper.h"

#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_paths.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// INITIALIZE_CDM_MODULE is a macro in api/content_decryption_module.h.
// However, we need to pass it as a string to GetFunctionPointer() once it
// is expanded.
#define STRINGIFY(X) #X
#define MAKE_STRING(X) STRINGIFY(X)

ExternalClearKeyTestHelper::ExternalClearKeyTestHelper() {
  LoadLibrary();
}

ExternalClearKeyTestHelper::~ExternalClearKeyTestHelper() {
  UnloadLibrary();
}

void ExternalClearKeyTestHelper::LoadLibrary() {
  // Determine the location of the CDM. It is expected to be in the same
  // directory as the current module.
  base::FilePath cdm_base_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &cdm_base_path));
  cdm_base_path = cdm_base_path.Append(
      GetPlatformSpecificDirectory(kClearKeyCdmBaseDirectory));
  library_path_ = cdm_base_path.AppendASCII(
      base::GetLoadableModuleName(kClearKeyCdmLibraryName));
  ASSERT_TRUE(base::PathExists(library_path_)) << library_path_.value();

  // Now load the CDM library.
  library_ = base::ScopedNativeLibrary(library_path_);
  ASSERT_TRUE(library_.is_valid()) << library_.GetError()->ToString();

  // Call INITIALIZE_CDM_MODULE()
  typedef void (*InitializeCdmFunc)();
  InitializeCdmFunc initialize_cdm_func = reinterpret_cast<InitializeCdmFunc>(
      library_.GetFunctionPointer(MAKE_STRING(INITIALIZE_CDM_MODULE)));
  ASSERT_TRUE(initialize_cdm_func) << "No INITIALIZE_CDM_MODULE in library";

  // Loading and unloading this library leaks all static allocations; previously
  // these were suppressed by a similar annotation in base::LazyInstance. With
  // the switch to thread-safe statics, we lost the annotation.
  //
  // TODO(xhwang): Investigate if we are actually leaking memory during the
  // normal process by which Chrome uses this library. http://crbug.com/691132.
  ANNOTATE_SCOPED_MEMORY_LEAK;
  initialize_cdm_func();
}

void ExternalClearKeyTestHelper::UnloadLibrary() {
  // Call DeinitializeCdmModule()
  typedef void (*DeinitializeCdmFunc)();
  DeinitializeCdmFunc deinitialize_cdm_func =
      reinterpret_cast<DeinitializeCdmFunc>(
          library_.GetFunctionPointer("DeinitializeCdmModule"));
  ASSERT_TRUE(deinitialize_cdm_func) << "No DeinitializeCdmModule() in library";
  deinitialize_cdm_func();
}

}  // namespace media
