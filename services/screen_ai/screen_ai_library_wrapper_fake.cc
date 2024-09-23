// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_library_wrapper_fake.h"

namespace screen_ai {

bool ScreenAILibraryWrapperFake::Load(const base::FilePath& library_path) {
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ScreenAILibraryWrapperFake::ScreenAILibraryWrapperFake::SetLogger() {}
#endif

void ScreenAILibraryWrapperFake::GetLibraryVersion(uint32_t& major,
                                                   uint32_t& minor) {
  major = 0;
  minor = 0;
}

void ScreenAILibraryWrapperFake::SetFileContentFunctions(
    uint32_t (*get_file_content_size)(const char* /*relative_file_path*/),
    void (*get_file_content)(const char* /*relative_file_path*/,
                             uint32_t /*buffer_size*/,
                             char* /*buffer*/)) {}

void ScreenAILibraryWrapperFake::EnableDebugMode() {}

bool ScreenAILibraryWrapperFake::InitOCR() {
  return true;
}

bool ScreenAILibraryWrapperFake::InitMainContentExtraction() {
  return true;
}

std::optional<chrome_screen_ai::VisualAnnotation>
ScreenAILibraryWrapperFake::PerformOcr(const SkBitmap& image) {
  return std::nullopt;
}

std::optional<std::vector<int32_t>>
ScreenAILibraryWrapperFake::ExtractMainContent(
    const std::string& serialized_view_hierarchy) {
  return std::nullopt;
}

}  // namespace screen_ai
