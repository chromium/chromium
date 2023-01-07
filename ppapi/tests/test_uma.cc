// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_uma.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UMA);

bool TestUMA::Init() {
  uma_interface_ = static_cast<const PPB_UMA_Private*>(
      pp::Module::Get()->GetBrowserInterface(PPB_UMA_PRIVATE_INTERFACE));
  return !!uma_interface_;
}

void TestUMA::RunTests(const std::string& filter) {
  RUN_TEST(Count, filter);
  RUN_TEST(Time, filter);
  RUN_TEST(Enum, filter);
}

std::string TestUMA::TestCount() {
  pp::Var name_var = pp::Var("Test.CountHistogram");
  PP_Var name = name_var.pp_var();
  PP_Instance instance = instance_->pp_instance();
  uma_interface_->HistogramCustomCounts(instance, name, 10, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(instance, name, 30, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(instance, name, 20, 1, 100, 50);
  uma_interface_->HistogramCustomCounts(instance, name, 40, 1, 100, 50);
  // Test that passing in different construction arguments for the same
  // histogram name does not crash.
  uma_interface_->HistogramCustomCounts(instance, name, 40, 1, 100, 100);
  uma_interface_->HistogramCustomCounts(instance, name, 40, 1, 90, 50);
  uma_interface_->HistogramCustomCounts(instance, name, 40, 10, 100, 50);
  PASS();
}

std::string TestUMA::TestTime() {
  pp::Var name_var = pp::Var("Test.TimeHistogram");
  PP_Var name = name_var.pp_var();
  PP_Instance instance = instance_->pp_instance();
  uma_interface_->HistogramCustomTimes(instance, name, 100, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(instance, name, 1000, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(instance, name, 5000, 1, 10000, 50);
  uma_interface_->HistogramCustomTimes(instance, name, 10, 1, 10000, 50);
  // Test that passing in different construction arguments for the same
  // histogram name does not crash.
  uma_interface_->HistogramCustomTimes(instance, name, 10, 1, 10000, 100);
  uma_interface_->HistogramCustomTimes(instance, name, 10, 1, 9000, 50);
  uma_interface_->HistogramCustomTimes(instance, name, 10, 100, 10000, 50);
  PASS();
}

std::string TestUMA::TestEnum() {
  pp::Var name_var = pp::Var("Test.EnumHistogram");
  PP_Var name = name_var.pp_var();
  PP_Instance instance = instance_->pp_instance();
  uma_interface_->HistogramEnumeration(instance, name, 0, 5);
  uma_interface_->HistogramEnumeration(instance, name, 3, 5);
  uma_interface_->HistogramEnumeration(instance, name, 3, 5);
  uma_interface_->HistogramEnumeration(instance, name, 1, 5);
  uma_interface_->HistogramEnumeration(instance, name, 2, 5);
  // Test that passing in different construction arguments for the same
  // histogram name does not crash.
  uma_interface_->HistogramEnumeration(instance, name, 2, 6);
  PASS();
}

