// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_PROTO_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_PROTO_UTIL_H_

#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"

// Re-open ios::proto namespace to define comparison operator for protobuf
// message because `EXPECT_EQ` and similar macros requires that the operator
// are found using depends name lookup.
namespace ios::proto {

// Equality operator for `WebStateListStorage` for use in tests.
bool operator==(const WebStateListStorage& lhs, const WebStateListStorage& rhs);

// Inequality operator for `WebStateListStorage` for use in tests.
bool operator!=(const WebStateListStorage& lhs, const WebStateListStorage& rhs);

// Equality operator for `WebStateListItemStorage` for use in tests.
bool operator==(const WebStateListItemStorage& lhs,
                const WebStateListItemStorage& rhs);

// Inequality operator for `WebStateListItemStorage` for use in tests.
bool operator!=(const WebStateListItemStorage& lhs,
                const WebStateListItemStorage& rhs);

// Equality operator for `OpenerStorage` for use in tests.
bool operator==(const OpenerStorage& lhs, const OpenerStorage& rhs);

// Inequality operator for `OpenerStorage` for use in tests.
bool operator!=(const OpenerStorage& lhs, const OpenerStorage& rhs);

}  // namespace ios::proto

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_PROTO_UTIL_H_
