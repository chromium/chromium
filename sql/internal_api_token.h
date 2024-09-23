// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_INTERNAL_API_TOKEN_H_
#define SQL_INTERNAL_API_TOKEN_H_

namespace base {
class FilePath;
}  // namespace base

namespace sql {

namespace test {
struct ColumnInfo;
bool CorruptSizeInHeader(const base::FilePath&);
}  // namespace test

// Restricts access to APIs internal to the //sql package.
//
// This implements Java's package-private via the passkey idiom.
class InternalApiToken {
 private:
  // Must NOT be =default to disallow creation by uniform initialization.
  InternalApiToken() {}
  InternalApiToken(const InternalApiToken&) = default;

  friend class Database;
  friend class DatabaseTestPeer;
  friend class Recovery;
  friend class Transaction;
  friend struct test::ColumnInfo;
  friend bool test::CorruptSizeInHeader(const base::FilePath&);
};

}  // namespace sql

#endif  // SQL_INTERNAL_API_TOKEN_H_
