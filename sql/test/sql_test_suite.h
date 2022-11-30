// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_TEST_SQL_TEST_SUITE_H_
#define SQL_TEST_SQL_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace sql {

class SQLTestSuite : public base::TestSuite {
 public:
  SQLTestSuite(int argc, char** argv);
  SQLTestSuite(const SQLTestSuite&) = delete;
  SQLTestSuite& operator=(const SQLTestSuite&) = delete;
  ~SQLTestSuite() override;

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

}  // namespace sql

#endif  // SQL_TEST_SQL_TEST_SUITE_H_
