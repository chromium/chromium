// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_STD_VECTOR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_STD_VECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/apple/swift_interop_util.h"

using IntVector = std::vector<int>;
using StringVector = std::vector<std::unique_ptr<std::string>>;

IntVector GetFortyTwoVector();
bool CheckFortyTwoInVector(const IntVector& input);

// Create a wrapper when a type inside a container is non-copyable to prevent
// the Swift compiler from treating the vector as a value type.
SWIFT_DECLARE_MOVE_ONLY_INTEROP_WRAPPER(CxxStringVector, StringVector)
CxxStringVector GetStringVector();

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_STD_VECTOR_H_
