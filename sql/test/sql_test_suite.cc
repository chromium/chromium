// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/sql_test_suite.h"

#include "base/test/test_suite.h"
#include "sql/test/paths.h"

namespace sql {

SQLTestSuite::SQLTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

SQLTestSuite::~SQLTestSuite() = default;

void SQLTestSuite::Initialize() {
  base::TestSuite::Initialize();
  sql::test::RegisterPathProvider();
}

void SQLTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace sql
