// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_TEST_UTIL_H_

#include "ios/web/public/session/proto/common.pb.h"
#include "ios/web/public/session/proto/navigation.pb.h"
#include "ios/web/public/session/proto/session.pb.h"
#include "ios/web/public/session/proto/storage.pb.h"

// Re-open web::proto namespace to define comparison operator for protobuf
// message because `EXPECT_EQ` and similar macros requires that the operator
// are found using depends name lookup.
namespace web::proto {

// Equality operator for `Timestamp` for use in tests.
bool operator==(const Timestamp& lhs, const Timestamp& rhs);

// Inequality operator for `Timestamp` for use in tests.
bool operator!=(const Timestamp& lhs, const Timestamp& rhs);

// Equality operator for `ReferrerStorage` for use in tests.
bool operator==(const ReferrerStorage& lhs, const ReferrerStorage& rhs);

// Inequality operator for `ReferrerStorage` for use in tests.
bool operator!=(const ReferrerStorage& lhs, const ReferrerStorage& rhs);

// Equality operator for `HttpHeaderStorage` for use in tests.
bool operator==(const HttpHeaderStorage& lhs, const HttpHeaderStorage& rhs);

// Inequality operator for `HttpHeaderStorage` for use in tests.
bool operator!=(const HttpHeaderStorage& lhs, const HttpHeaderStorage& rhs);

// Equality operator for `HttpHeaderListStorage` for use in tests.
bool operator==(const HttpHeaderListStorage& lhs,
                const HttpHeaderListStorage& rhs);

// Inequality operator for `HttpHeaderListStorage` for use in tests.
bool operator!=(const HttpHeaderListStorage& lhs,
                const HttpHeaderListStorage& rhs);

// Equality operator for `NavigationItemStorage` for use in tests.
bool operator==(const NavigationItemStorage& lhs,
                const NavigationItemStorage& rhs);

// Inequality operator for `NavigationItemStorage` for use in tests.
bool operator!=(const NavigationItemStorage& lhs,
                const NavigationItemStorage& rhs);

// Equality operator for `NavigationStorage` for use in tests.
bool operator==(const NavigationStorage& lhs, const NavigationStorage& rhs);

// Inequality operator for `NavigationStorage` for use in tests.
bool operator!=(const NavigationStorage& lhs, const NavigationStorage& rhs);

// Equality operator for `CertificateStorage` for use in tests.
bool operator==(const CertificateStorage& lhs, const CertificateStorage& rhs);

// Inequality operator for `CertificateStorage` for use in tests.
bool operator!=(const CertificateStorage& lhs, const CertificateStorage& rhs);

// Equality operator for `CertificatesCacheStorage` for use in tests.
bool operator==(const CertificatesCacheStorage& lhs,
                const CertificatesCacheStorage& rhs);

// Inequality operator for `CertificateStorage` for use in tests.
bool operator!=(const CertificatesCacheStorage& lhs,
                const CertificatesCacheStorage& rhs);

// Equality operator for `WebStateStorage` for use in tests.
bool operator==(const WebStateStorage& lhs, const WebStateStorage& rhs);

// Inequality operator for `WebStateStorage` for use in tests.
bool operator!=(const WebStateStorage& lhs, const WebStateStorage& rhs);

}  // namespace web::proto

#endif  // IOS_WEB_PUBLIC_SESSION_PROTO_PROTO_TEST_UTIL_H_
