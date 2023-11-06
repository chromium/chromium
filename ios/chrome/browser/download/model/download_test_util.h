// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_

#include <string>

namespace testing {

extern const char kCalendarFilePath[];
extern const char kMobileConfigFilePath[];
extern const char kPkPassFilePath[];
extern const char kBundledPkPassFilePath[];
extern const char kSemiValidBundledPkPassFilePath[];
extern const char kUsdzFilePath[];
extern const char kVcardFilePath[];

// Returns contents of the test file at the given relative path in the chrome
// test directory.
std::string GetTestFileContents(const char* file_path);

}  // namespace testing

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_TEST_UTIL_H_
