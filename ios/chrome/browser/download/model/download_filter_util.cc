// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_filter_util.h"

#include <map>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"

namespace {

// Alias for case-insensitive comparison.
constexpr auto kIgnoreCase = base::CompareCase::INSENSITIVE_ASCII;

// Map to simplify the lookup of MIME type categories.
constexpr auto kMimeTypeMap =
    base::MakeFixedFlatMap<std::string_view, DownloadFilterType>({
        {"video/", DownloadFilterType::kVideo},
        {"audio/", DownloadFilterType::kAudio},
        {"image/", DownloadFilterType::kImage},
        {"text/", DownloadFilterType::kDocument},
    });

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
  for (const auto& pair : kMimeTypeMap) {
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
