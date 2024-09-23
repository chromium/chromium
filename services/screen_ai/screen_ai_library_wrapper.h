// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
#define SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_

#include <stdint.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace screen_ai {

// Base wrapper class for Chrome Screen AI library.
class ScreenAILibraryWrapper {
 public:
  ScreenAILibraryWrapper() = default;
  virtual ~ScreenAILibraryWrapper() = default;

  // Loads the library from disk.
  virtual bool Load(const base::FilePath& library_path) = 0;

  virtual void EnableDebugMode() = 0;
  virtual void GetLibraryVersion(uint32_t& major, uint32_t& minor) = 0;
  virtual void SetFileContentFunctions(
      uint32_t (*get_file_content_size)(const char* relative_file_path),
      void (*get_file_content)(const char* relative_file_path,
                               uint32_t buffer_size,
                               char* buffer)) = 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  virtual void SetLogger() = 0;
#endif

  virtual bool InitMainContentExtraction() = 0;
  virtual std::optional<std::vector<int32_t>> ExtractMainContent(
      const std::string& serialized_view_hierarchy) = 0;

  virtual bool InitOCR() = 0;
  virtual std::optional<chrome_screen_ai::VisualAnnotation> PerformOcr(
      const SkBitmap& image) = 0;
};

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
