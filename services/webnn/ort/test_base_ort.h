// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_
#define SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::ort {

class TestBaseOrt : public testing::Test {
 public:
  void SetUp() override;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_TEST_BASE_ORT_H_
