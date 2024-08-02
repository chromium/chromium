// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_library_wrapper_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/accessibility_features.h"

namespace screen_ai {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOGGING_VERBOSE:
    case logging::LOGGING_INFO:
      VLOG(2) << message;
      break;
    case logging::LOGGING_WARNING:
      VLOG(1) << message;
      break;
    case logging::LOGGING_ERROR:
    case logging::LOGGING_FATAL:
      VLOG(0) << message;
      break;
  }
}
#endif

}  // namespace

ScreenAILibraryWrapperImpl::ScreenAILibraryWrapperImpl() = default;

template <typename T>
bool ScreenAILibraryWrapperImpl::LoadFunction(T& function_variable,
                                              const char* function_name) {
  function_variable =
      reinterpret_cast<T>(library_.GetFunctionPointer(function_name));
  if (function_variable == nullptr) {
    VLOG(0) << "Could not load function: " << function_name;
    return false;
  }
  return true;
}

bool ScreenAILibraryWrapperImpl::Load(const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);

#if BUILDFLAG(IS_WIN)
  DWORD error = library_.GetError()->code;
  base::UmaHistogramSparse(
      "Accessibility.ScreenAI.LibraryLoadDetailedResultOnWindows",
      static_cast<int>(error));
  if (error != ERROR_SUCCESS) {
    VLOG(0) << "Library load error: " << library_.GetError()->code;
    return false;
  }
#else

  if (!library_.GetError()->message.empty()) {
    VLOG(0) << "Library load error: " << library_.GetError()->message;
    return false;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!LoadFunction(set_logger_, "SetLogger")) {
    return false;
  }
#endif

  // General functions.
  if (!LoadFunction(get_library_version_, "GetLibraryVersion") ||
      !LoadFunction(get_library_version_, "GetLibraryVersion") ||
      !LoadFunction(enable_debug_mode_, "EnableDebugMode") ||
      !LoadFunction(set_file_content_functions_, "SetFileContentFunctions") ||
      !LoadFunction(free_library_allocated_int32_array_,
                    "FreeLibraryAllocatedInt32Array") ||
      !LoadFunction(free_library_allocated_char_array_,
                    "FreeLibraryAllocatedCharArray")) {
    return false;
  }

  if (!LoadFunction(init_ocr_, "InitOCRUsingCallback") ||
      !LoadFunction(perform_ocr_, "PerformOCR")) {
    return false;
  }

  // Main Content Extraction functions.
  if (!LoadFunction(init_main_content_extraction_,
                    "InitMainContentExtractionUsingCallback") ||
      !LoadFunction(extract_main_content_, "ExtractMainContent")) {
    return false;
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapperImpl::ScreenAILibraryWrapperImpl::SetLogger() {
  CHECK(set_logger_);
  set_logger_(&HandleLibraryLogging);
}
#endif

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapperImpl::GetLibraryVersion(uint32_t& major,
                                                   uint32_t& minor) {
  CHECK(get_library_version_);
  get_library_version_(major, minor);
}

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapperImpl::SetFileContentFunctions(
    uint32_t (*get_file_content_size)(const char* /*relative_file_path*/),
    void (*get_file_content)(const char* /*relative_file_path*/,
                             uint32_t /*buffer_size*/,
                             char* /*buffer*/)) {
  CHECK(set_file_content_functions_);
  set_file_content_functions_(get_file_content_size, get_file_content);
}

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapperImpl::EnableDebugMode() {
  CHECK(enable_debug_mode_);
  enable_debug_mode_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapperImpl::InitOCR() {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Accessibility.ScreenAI.OCR.InitializationLatency");
  CHECK(init_ocr_);
  return init_ocr_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapperImpl::InitMainContentExtraction() {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Accessibility.ScreenAI.MainContentExtraction.InitializationLatency");
  CHECK(init_main_content_extraction_);
  return init_main_content_extraction_();
}

NO_SANITIZE("cfi-icall")
std::optional<chrome_screen_ai::VisualAnnotation>
ScreenAILibraryWrapperImpl::PerformOcr(const SkBitmap& image) {
  CHECK(perform_ocr_);
  CHECK(free_library_allocated_char_array_);

  // Report image specifications in case the call crashes.
  static crash_reporter::CrashKeyString<50> image_info("ocr_image_info");
  image_info.Set(base::StringPrintf(
      "W:%5i, H:%5i, CT:%2i, BPP:%2i, RB:%5zu, DN:%i", image.width(),
      image.height(), static_cast<int>(image.colorType()),
      image.bytesPerPixel(), image.rowBytes(), image.drawsNothing()));

  std::optional<chrome_screen_ai::VisualAnnotation> annotation_proto;

  uint32_t annotation_proto_length = 0;
  // Memory allocated in `library_buffer` should be release only using
  // `free_library_allocated_char_array_` function. Using unique_ptr custom
  // deleter results in crash on Linux official build.
  std::unique_ptr<char> library_buffer(
      perform_ocr_(image, annotation_proto_length));

  if (!library_buffer) {
    return annotation_proto;
  }

  annotation_proto = chrome_screen_ai::VisualAnnotation();
  if (!annotation_proto->ParseFromArray(library_buffer.get(),
                                        annotation_proto_length)) {
    annotation_proto.reset();
  }

  free_library_allocated_char_array_(library_buffer.release());
  return annotation_proto;
}

NO_SANITIZE("cfi-icall")
std::optional<std::vector<int32_t>>
ScreenAILibraryWrapperImpl::ExtractMainContent(
    const std::string& serialized_view_hierarchy) {
  CHECK(extract_main_content_);
  CHECK(free_library_allocated_int32_array_);

  std::optional<std::vector<int32_t>> node_ids;

  uint32_t nodes_count = 0;
  // Memory allocated in `library_buffer` should be release only using
  // `free_library_allocated_int32_array_` function.
  std::unique_ptr<int32_t> library_buffer(
      extract_main_content_(serialized_view_hierarchy.data(),
                            serialized_view_hierarchy.length(), nodes_count));

  if (!library_buffer) {
    return node_ids;
  }

  node_ids = std::vector<int32_t>(nodes_count);
  if (nodes_count != 0) {
    memcpy(node_ids->data(), library_buffer.get(),
           nodes_count * sizeof(int32_t));
  }

  free_library_allocated_int32_array_(library_buffer.release());
  return node_ids;
}

}  // namespace screen_ai
