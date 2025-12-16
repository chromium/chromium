// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_VECTOR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_VECTOR_H_

#include <vector>

using IntVector = std::vector<int>;

IntVector GetFortyTwoVector();
bool CheckFortyTwoInVector(const IntVector& input);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_VECTOR_H_
