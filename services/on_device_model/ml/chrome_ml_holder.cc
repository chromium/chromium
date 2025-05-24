// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml_holder.h"

#include <optional>

#include "base/base_paths.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

namespace ml {

namespace {
constexpr std::string_view kChromeMLLibraryName = "optimization_guide_internal";
}

base::FilePath GetChromeMLPath(const std::optional<std::string>& library_name) {
  // TODO(https://crbug.com/366498630): Clean up the API to introduce dedicated
  // ForTesting() methods for loading the library / querying its path.
  if (library_name.has_value()) {
    // Library name override should only be used in test code.
    CHECK_IS_TEST();
  }

  base::FilePath base_dir;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    base_dir = base::apple::FrameworkBundlePath().Append("Libraries");
  } else {
#endif  // BUILDFLAG(IS_MAC)
    CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
#if BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_FUCHSIA)

  return base_dir.AppendASCII(base::GetNativeLibraryName(
      library_name.value_or(std::string(kChromeMLLibraryName))));
}

ChromeMLHolder::ChromeMLHolder(base::PassKey<ChromeMLHolder>,
                               base::ScopedNativeLibrary library,
                               const ChromeMLAPI* api)
    : library_(std::move(library)), api_(api) {
  CHECK(api_);
}

ChromeMLHolder::~ChromeMLHolder() = default;

// static
DISABLE_CFI_DLSYM
std::unique_ptr<ChromeMLHolder> ChromeMLHolder::Create(
    const std::optional<std::string>& library_name) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(GetChromeMLPath(library_name), &error);
  if (!library) {
    LOG(ERROR) << "Error loading native library: " << error.ToString();
    return {};
  }

  base::ScopedNativeLibrary scoped_library(library);
  auto get_api = reinterpret_cast<ChromeMLAPIGetter>(
      scoped_library.GetFunctionPointer("GetChromeMLAPI"));
  if (!get_api) {
    LOG(ERROR) << "Unable to resolve GetChromeMLAPI() symbol.";
    return {};
  }

  const ChromeMLAPI* api = get_api();
  if (!api) {
    LOG(ERROR) << "GetChromeMLAPI() returned null.";
    return {};
  }

  return std::make_unique<ChromeMLHolder>(base::PassKey<ChromeMLHolder>(),
                                          std::move(scoped_library), api);
}

}  // namespace ml
