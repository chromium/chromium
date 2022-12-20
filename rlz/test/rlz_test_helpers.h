// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions used by the tests.

#ifndef RLZ_TEST_RLZ_TEST_HELPERS_H_
#define RLZ_TEST_RLZ_TEST_HELPERS_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_temp_dir.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

// A test helper class that constructs and destructs platform dependent machine
// state. It's used by src/components/rlz/rlz_tracker_unittest.cc and
// src/rlz/lib/rlz_lib_test.cc
class RlzLibTestNoMachineStateHelper {
 public:
  void SetUp();
  void TearDown();
  void Reset();

#if BUILDFLAG(IS_POSIX)
  base::ScopedTempDir temp_dir_;
#endif

#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager override_manager_;
#endif
};

class RlzLibTestNoMachineState : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  RlzLibTestNoMachineStateHelper m_rlz_test_helper_;
};

class RlzLibTestBase : public RlzLibTestNoMachineState {
 public:
  RlzLibTestBase();
  ~RlzLibTestBase() override;

 protected:
  void SetUp() override;
  void TearDown() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::system::FakeStatisticsProvider> statistics_provider_;
#endif
};

#endif  // RLZ_TEST_RLZ_TEST_HELPERS_H_
