// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_UMA_LOGGING_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_UMA_LOGGING_WIN_H_

namespace content {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum class DirectWriteFontLoaderType {
  FILE_SYSTEM_FONT_DIR = 0,
  FILE_OUTSIDE_SANDBOX = 1,
  OTHER_LOADER = 2,
  FONT_WITH_MISSING_REQUIRED_STYLES = 3,

  kMaxValue = FONT_WITH_MISSING_REQUIRED_STYLES
};

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum class MessageFilterError {
  LAST_RESORT_FONT_GET_FONT_FAILED = 0,
  LAST_RESORT_FONT_ADD_FILES_FAILED = 1,
  LAST_RESORT_FONT_GET_FAMILY_FAILED = 2,
  ERROR_NO_COLLECTION = 3,
  MAP_CHARACTERS_NO_FAMILY = 4,
  ADD_FILES_FOR_FONT_CREATE_FACE_FAILED = 5,
  ADD_FILES_FOR_FONT_GET_FILE_COUNT_FAILED = 6,
  ADD_FILES_FOR_FONT_GET_FILES_FAILED = 7,
  ADD_FILES_FOR_FONT_GET_LOADER_FAILED = 8,
  ADD_FILES_FOR_FONT_QI_FAILED = 9,
  ADD_LOCAL_FILE_GET_REFERENCE_KEY_FAILED = 10,
  ADD_LOCAL_FILE_GET_PATH_LENGTH_FAILED = 11,
  ADD_LOCAL_FILE_GET_PATH_FAILED = 12,
  GET_FILE_COUNT_INVALID_NUMBER_OF_FILES = 13,

  kMaxValue = GET_FILE_COUNT_INVALID_NUMBER_OF_FILES
};

void LogLoaderType(DirectWriteFontLoaderType loader_type);

void LogLastResortFontCount(size_t count);

void LogLastResortFontFileCount(size_t count);

void LogMessageFilterError(MessageFilterError error);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_UMA_LOGGING_WIN_H_
