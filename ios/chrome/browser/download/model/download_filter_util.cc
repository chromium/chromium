// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_filter_util.h"

#include <map>

#include "base/strings/string_util.h"

namespace {

// Alias for case-insensitive comparison.
constexpr auto kIgnoreCase = base::CompareCase::INSENSITIVE_ASCII;

// Returns a map to simplify the lookup of MIME type categories.
// Using function-local static to avoid static initialization order issues.
const std::map<std::string_view, DownloadFilterType>& GetMimeTypeMap() {
  static const std::map<std::string_view, DownloadFilterType> kMimeTypeMap = {
      {"video/", DownloadFilterType::kVideo},
      {"audio/", DownloadFilterType::kAudio},
      {"image/", DownloadFilterType::kImage},
      {"text/", DownloadFilterType::kDocument},
  };
  return kMimeTypeMap;
}

// Helper function to determine the category of a MIME type.
DownloadFilterType GetMimeTypeCategory(std::string_view mime_type) {
  if (mime_type.empty()) {
    return DownloadFilterType::kOther;
  }

  // Handle specific case for PDF first, as it's an exact match.
  if (base::EqualsCaseInsensitiveASCII(mime_type, "application/pdf")) {
    return DownloadFilterType::kPDF;
  }

  // Iterate through the map to check for matching prefixes.
  for (const auto& pair : GetMimeTypeMap()) {
    if (base::StartsWith(mime_type, pair.first, kIgnoreCase)) {
      return pair.second;
    }
  }

  return DownloadFilterType::kOther;
}

}  // namespace

bool IsDownloadFilterMatch(std::string_view mime_type,
                           DownloadFilterType filter_type) {
  // Fast path: kAll filter matches everything.
  if (filter_type == DownloadFilterType::kAll) {
    return true;
  }

  // Determine the actual category of the MIME type.
  DownloadFilterType actual_type = GetMimeTypeCategory(mime_type);
  return actual_type == filter_type;
}
