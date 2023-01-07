// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_TEST_SUPPORT_IMPL_H_
#define MOJO_CORE_TEST_TEST_SUPPORT_IMPL_H_

#include <stdio.h>

#include "mojo/public/tests/test_support_private.h"

namespace mojo {
namespace core {
namespace test {

class TestSupportImpl : public mojo::test::TestSupport {
 public:
  TestSupportImpl();

  TestSupportImpl(const TestSupportImpl&) = delete;
  TestSupportImpl& operator=(const TestSupportImpl&) = delete;

  ~TestSupportImpl() override;

  void LogPerfResult(const char* test_name,
                     const char* sub_test_name,
                     double value,
                     const char* units) override;
  FILE* OpenSourceRootRelativeFile(const char* relative_path) override;
  char** EnumerateSourceRootRelativeDirectory(
      const char* relative_path) override;
};

}  // namespace test
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_TEST_TEST_SUPPORT_IMPL_H_
