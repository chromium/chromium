// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_PATHS_H_
#define SQL_TEST_PATHS_H_

namespace sql {
namespace test {

enum {
  PATH_START = 10000,

  // Valid only in testing environments.
  DIR_TEST_DATA,
  PATH_END
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace test
}  // namespace sql

#endif  // SQL_TEST_PATHS_H_
