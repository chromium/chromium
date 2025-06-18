// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_LIBRARY_HOLDER_H_
#define SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_LIBRARY_HOLDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "services/shape_detection/chrome_shape_detection_api.h"

namespace shape_detection {

base::FilePath GetChromeShapeDetectionPath();

// A ShapeDetectionLibraryHolder object encapsulates a reference to the
// ChromeShapeDetectionAPI shared library, exposing the library's API
// functions to callers and ensuring that the library remains loaded and usable
// throughout the object's lifetime.
class ShapeDetectionLibraryHolder {
 public:
  ShapeDetectionLibraryHolder(base::PassKey<ShapeDetectionLibraryHolder>,
                              base::ScopedNativeLibrary library,
                              const ChromeShapeDetectionAPI* api);
  ShapeDetectionLibraryHolder(const ShapeDetectionLibraryHolder& other) =
      delete;
  ShapeDetectionLibraryHolder& operator=(
      const ShapeDetectionLibraryHolder& other) = delete;
  ShapeDetectionLibraryHolder(ShapeDetectionLibraryHolder&& other) = default;
  ShapeDetectionLibraryHolder& operator=(ShapeDetectionLibraryHolder&& other) =
      default;
  ~ShapeDetectionLibraryHolder();

  // Returns the singleton ShapeDetectionLibraryHolder. Creates it if it does
  // not exist. May return nullopt if the underlying library could not be
  // loaded.
  static ShapeDetectionLibraryHolder* GetInstance();

  // Exposes the raw ChromeShapeDetectionAPI functions defined by the library.
  const ChromeShapeDetectionAPI& api() const { return *api_; }

 private:
  static std::unique_ptr<ShapeDetectionLibraryHolder> Create();

  base::ScopedNativeLibrary library_;
  raw_ptr<const ChromeShapeDetectionAPI> api_;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_SHAPE_DETECTION_LIBRARY_HOLDER_H_
