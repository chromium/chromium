// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_MOJO_TEST_SUITE_BASE_H_
#define MOJO_CORE_TEST_MOJO_TEST_SUITE_BASE_H_

#include "base/test/test_suite.h"
#include "build/build_config.h"

namespace mojo {
namespace core {
namespace test {

class MojoTestSuiteBase : public base::TestSuite {
 public:
  MojoTestSuiteBase(int argc, char** argv);
#if BUILDFLAG(IS_WIN)
  MojoTestSuiteBase(int argc, wchar_t** argv);
#endif  // BUILDFLAG(IS_WIN)

  MojoTestSuiteBase(const MojoTestSuiteBase&) = delete;
  MojoTestSuiteBase& operator=(const MojoTestSuiteBase&) = delete;

 protected:
  void Initialize() override;
};

}  // namespace test
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_TEST_MOJO_TEST_SUITE_BASE_H_
