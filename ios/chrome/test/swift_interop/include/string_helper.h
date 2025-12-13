// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_STRING_HELPER_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_STRING_HELPER_H_

#include <string>

// Avoid using string.h as a file name. It will conflict with the one in the
// standard library.

std::string addStringFromCxx(std::string a);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_STRING_HELPER_H_
