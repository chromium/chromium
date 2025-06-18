// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/shape_detection_library_holder.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"

namespace shape_detection {

namespace {
constexpr std::string_view kChromeShapeDetectionLibraryName =
    "shape_detection_internal";
}  // namespace

base::FilePath GetChromeShapeDetectionPath() {
  base::FilePath base_dir;
  CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
  return base_dir.AppendASCII(
      base::GetNativeLibraryName(kChromeShapeDetectionLibraryName));
}

ShapeDetectionLibraryHolder::ShapeDetectionLibraryHolder(
    base::PassKey<ShapeDetectionLibraryHolder>,
    base::ScopedNativeLibrary library,
    const ChromeShapeDetectionAPI* api)
    : library_(std::move(library)), api_(api) {}

ShapeDetectionLibraryHolder::~ShapeDetectionLibraryHolder() = default;

// static
ShapeDetectionLibraryHolder* ShapeDetectionLibraryHolder::GetInstance() {
  static base::NoDestructor<std::unique_ptr<ShapeDetectionLibraryHolder>>
      holder{Create()};
  return holder->get();
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<ShapeDetectionLibraryHolder>
ShapeDetectionLibraryHolder::Create() {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(GetChromeShapeDetectionPath(), &error);
  if (!library) {
    LOG(ERROR) << "Error loading native library: " << error.ToString();
    return {};
  }

  base::ScopedNativeLibrary scoped_library(library);
  auto get_api = reinterpret_cast<ChromeShapeDetectionAPIGetter>(
      scoped_library.GetFunctionPointer("GetChromeShapeDetectionAPI"));
  if (!get_api) {
    LOG(ERROR) << "Unable to resolve GetChromeShapeDetectionAPI symbol.";
    return {};
  }

  const ChromeShapeDetectionAPI* api = get_api();
  if (!api) {
    LOG(ERROR) << "GetChromeShapeDetectionAPI() returned null.";
    return {};
  }

  return std::make_unique<ShapeDetectionLibraryHolder>(
      base::PassKey<ShapeDetectionLibraryHolder>(), std::move(scoped_library),
      api);
}

}  // namespace shape_detection
