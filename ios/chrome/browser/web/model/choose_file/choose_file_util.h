// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

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

// The capture type of the input.
// This enum is persisted in log, do not reorder or reuse buckets.
// LINT.IfChange
enum class ChooseFileCaptureType {
  kNone = 0,
  kUser = 1,
  kEnvironment = 2,
  kMaxValue = kEnvironment
};
// LINT.ThenChange(
//     /ios/chrome/browser/web/model/choose_file/resources/choose_file_utils.ts)

// Convenience converter between attributes values and ChooseFileContentState.
ChooseFileContentState ContentStateFromAttributes(bool has_multiple,
                                                  bool has_selected_file);

// Returns the list of MIME types contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeMimeTypes(
    std::string_view accept_attribute);

// Returns the list of file extensions contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeFileExtensions(
    std::string_view accept_attribute);

// Returns whether `type_identifiers` contains a type conforming to
// `target_type`.
template <typename TypeIdentifiers>
UTType* FindTypeConformingToTarget(TypeIdentifiers type_identifiers,
                                   UTType* target_type) {
  for (NSString* type_identifier in type_identifiers) {
    UTType* type = [UTType typeWithIdentifier:type_identifier];
    if ([type conformsToType:target_type]) {
      return type;
    }
  }
  return nil;
}

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
