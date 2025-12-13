// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILTER_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILTER_UTIL_H_

#include <string_view>

// Enum class for download filter types.
enum class DownloadFilterType {
  kAll = 0,       // Show all downloads
  kPDF = 1,       // PDF files
  kImage = 2,     // Image files
  kVideo = 3,     // Video files
  kAudio = 4,     // Audio files
  kDocument = 5,  // Text/Document files
  kOther = 6,     // Other file types
  kMaxValue = kOther,
};

// Checks if a MIME type matches the given download filter type.
// Returns true if the mime_type belongs to the specified filter_type category.
bool IsDownloadFilterMatch(std::string_view mime_type,
                           DownloadFilterType filter_type);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILTER_UTIL_H_
