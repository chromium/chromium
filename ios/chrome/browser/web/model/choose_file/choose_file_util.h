// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_

#import <string_view>
#import <vector>

// The state of the input when tapped (a.k.a whether it already contains a file
// and whether it would accept multiple files.
// This enum is persisted in log, do not reorder or reuse buckets.
// LINT.IfChange
enum class ChooseFileContentState {
  kSingleEmpty = 0,
  kSingleSelected = 1,
  kMultipleEmpty = 2,
  kMultipleSelected = 3,
  kMaxValue = kMultipleSelected
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Convenience converter between attributes values and ChooseFileContentState.
ChooseFileContentState ContentStateFromAttributes(bool has_multiple,
                                                  bool has_selected_file);

// Returns the list of MIME types contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeMimeTypes(
    std::string_view accept_attribute);

// Returns the list of file extensions contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeFileExtensions(
    std::string_view accept_attribute);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
