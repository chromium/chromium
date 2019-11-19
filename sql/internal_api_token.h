// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_INTERNAL_API_TOKEN_H_
#define SQL_INTERNAL_API_TOKEN_H_

namespace sql {

namespace test {
struct ColumnInfo;
}  // namespace test

// Restricts access to APIs internal to the //sql package.
//
// This implements Java's package-private via the passkey idiom.
class InternalApiToken {
 private:
  // Must NOT be =default to disallow creation by uniform initialization.
  InternalApiToken() {}
  InternalApiToken(const InternalApiToken&) = default;

  friend class DatabaseTestPeer;
  friend class Recovery;
  friend struct test::ColumnInfo;
};

}  // namespace sql

#endif  // SQL_INTERNAL_API_TOKEN_H_
