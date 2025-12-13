// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_TEST_BASE_H_
#define SERVICES_WEBNN_DML_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::dml {

class TestBase : public testing::Test {
 public:
  void SetUp() override;

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_TEST_BASE_H_
