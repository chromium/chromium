// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/test/openxr_mock_helper.h"

#include <optional>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "device/vr/openxr/test/fake_openxr_impl_api.h"
#include "device/vr/test/test_hook.h"

typedef void (*SetMockOpenXrDispatchTableFn)(
    PFN_xrGetInstanceProcAddr get_instance_proc_addr);

bool InitializeOpenXrMockTrampoline() {
  static bool s_initialized = false;
  if (s_initialized) {
    return true;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath library_path;
#if BUILDFLAG(IS_WIN)
  base::FilePath exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir)) {
    LOG(ERROR) << "Failed to get DIR_EXE path for OpenXR mock trampoline";
    return false;
  }
  library_path =
      exe_dir.AppendASCII("mock_vr_clients/bin/openxr/openxrruntime.dll");
#elif BUILDFLAG(IS_ANDROID)
  std::string json_contents;
  if (base::ReadFileToString(
          base::FilePath("/product/etc/openxr/1/active_runtime.json"),
          &json_contents)) {
    std::optional<base::DictValue> parsed_dict =
        base::JSONReader::ReadDict(json_contents, base::JSON_PARSE_RFC);
    if (parsed_dict) {
      const base::DictValue* runtime = parsed_dict->FindDict("runtime");
      if (runtime) {
        const std::string* path_str = runtime->FindString("library_path");
        if (path_str) {
          library_path = base::FilePath(*path_str);
        }
      }
    }
  }
  if (library_path.empty()) {
    library_path = base::FilePath("libmockopenxrruntime.so");
  }
#else
  NOTREACHED() << "Unsupported platform for OpenXR mock trampoline";
#endif

  base::NativeLibraryLoadError error;
  base::NativeLibrary library = base::LoadNativeLibrary(library_path, &error);
  if (!library) {
    LOG(ERROR) << "Failed to load OpenXR mock trampoline library from "
               << library_path.value() << ": " << error.ToString();
    return false;
  }

  auto set_dispatch = reinterpret_cast<SetMockOpenXrDispatchTableFn>(
      base::GetFunctionPointerFromNativeLibrary(library,
                                                "SetMockOpenXrDispatchTable"));
  if (!set_dispatch) {
    LOG(ERROR) << "Failed to find SetMockOpenXrDispatchTable symbol in "
               << library_path.value();
    return false;
  }

  set_dispatch(GetMockXrGetInstanceProcAddr());
  s_initialized = true;
  return true;
}

namespace {
struct TrampolineRegistrar {
  TrampolineRegistrar() {
    device::ServiceTestHook::RegisterInitializeOpenXrMockTrampolineFn(
        &InitializeOpenXrMockTrampoline);
  }
};
TrampolineRegistrar g_trampoline_registrar;
}  // namespace
