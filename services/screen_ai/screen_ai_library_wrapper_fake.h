// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_FAKE_H_
#define SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_FAKE_H_

#include <stdint.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "services/screen_ai/screen_ai_library_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace screen_ai {

// Wrapper class for Chrome Screen AI library for tests that cannot use the
// actual library.
class ScreenAILibraryWrapperFake : public ScreenAILibraryWrapper {
 public:
  ScreenAILibraryWrapperFake() = default;
  ScreenAILibraryWrapperFake(const ScreenAILibraryWrapperFake&) = delete;
  ScreenAILibraryWrapperFake& operator=(const ScreenAILibraryWrapperFake&) =
      delete;
  ~ScreenAILibraryWrapperFake() override = default;

  bool Load(const base::FilePath& library_path) override;

  void EnableDebugMode() override;
  void GetLibraryVersion(uint32_t& major, uint32_t& minor) override;
  void SetFileContentFunctions(
      uint32_t (*get_file_content_size)(const char* relative_file_path),
      void (*get_file_content)(const char* relative_file_path,
                               uint32_t buffer_size,
                               char* buffer)) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetLogger() override;
#endif

  bool InitMainContentExtraction() override;
  std::optional<std::vector<int32_t>> ExtractMainContent(
      const std::string& serialized_view_hierarchy) override;

  bool InitOCR() override;
  std::optional<chrome_screen_ai::VisualAnnotation> PerformOcr(
      const SkBitmap& image) override;
};

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_FAKE_H_
