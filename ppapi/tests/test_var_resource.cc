// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_var_resource.h"

#include <stdint.h>

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(VarResource);

bool TestVarResource::Init() {
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  file_system_interface_ = static_cast<const PPB_FileSystem*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILESYSTEM_INTERFACE));
  var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  return core_interface_ && file_system_interface_ && var_interface_ &&
         CheckTestingInterface();
}

void TestVarResource::RunTests(const std::string& filter) {
  RUN_TEST(BasicResource, filter);
  RUN_TEST(InvalidAndEmpty, filter);
  RUN_TEST(WrongType, filter);
}

std::string TestVarResource::TestBasicResource() {
  uint32_t before_object = testing_interface_->GetLiveObjectsForInstance(
      instance_->pp_instance());
  {
    // Create an unopened FileSystem resource.
    PP_Resource file_system = file_system_interface_->Create(
        instance_->pp_instance(), PP_FILESYSTEMTYPE_LOCALTEMPORARY);
    ASSERT_NE(0, file_system);

    // Build a var to wrap the resource.
    PP_Var var = var_interface_->VarFromResource(file_system);
    ASSERT_EQ(PP_VARTYPE_RESOURCE, var.type);

    // Reading back the resource should work. This will increment the reference
    // on the resource, so we must release it afterwards.
    PP_Resource result = var_interface_->VarToResource(var);
    ASSERT_EQ(file_system, result);
    core_interface_->ReleaseResource(result);

    // Destroy the var, readback should now fail.
    var_interface_->Release(var);
    result = var_interface_->VarToResource(var);
    ASSERT_EQ(0, result);

    // Release the resource. There should be no more references to it.
    core_interface_->ReleaseResource(file_system);
  }

  // Make sure nothing leaked. This checks for both var and resource leaks.
  ASSERT_EQ(
      before_object,
      testing_interface_->GetLiveObjectsForInstance(instance_->pp_instance()));

  PASS();
}

std::string TestVarResource::TestInvalidAndEmpty() {
  uint32_t before_object = testing_interface_->GetLiveObjectsForInstance(
      instance_->pp_instance());
  {
    PP_Var invalid_resource;
    invalid_resource.type = PP_VARTYPE_RESOURCE;
    invalid_resource.value.as_id = 31415926;

    // Invalid resource vars should give 0 as the return value.
    PP_Resource result = var_interface_->VarToResource(invalid_resource);
    ASSERT_EQ(0, result);

    // Test writing and reading a non-existant resource.
    PP_Resource fake_resource = 27182818;
    PP_Var var = var_interface_->VarFromResource(fake_resource);
    if (testing_interface()->IsOutOfProcess()) {
      // An out-of-process plugin is expected to generate null in this case.
      ASSERT_EQ(PP_VARTYPE_NULL, var.type);
      result = var_interface_->VarToResource(var);
      ASSERT_EQ(0, result);
    } else {
      // An in-process plugin is expected to generate a valid resource var
      // (because it does not validate the resource).
      ASSERT_EQ(PP_VARTYPE_RESOURCE, var.type);
      result = var_interface_->VarToResource(var);
      ASSERT_EQ(fake_resource, result);
      var_interface_->Release(var);
    }
    // Note: Not necessary to release the resource, since it does not exist.

    // Write the resource 0; expect a valid resource var with 0.
    var = var_interface_->VarFromResource(0);
    ASSERT_EQ(PP_VARTYPE_RESOURCE, var.type);
    result = var_interface_->VarToResource(var);
    ASSERT_EQ(0, result);
    var_interface_->Release(var);
  }

  // Make sure nothing leaked. This checks for both var and resource leaks.
  ASSERT_EQ(
      before_object,
      testing_interface_->GetLiveObjectsForInstance(instance_->pp_instance()));

  PASS();
}

std::string TestVarResource::TestWrongType() {
  PP_Resource result = var_interface_->VarToResource(PP_MakeUndefined());
  ASSERT_EQ(0, result);

  result = var_interface_->VarToResource(PP_MakeNull());
  ASSERT_EQ(0, result);

  result = var_interface_->VarToResource(PP_MakeBool(PP_TRUE));
  ASSERT_EQ(0, result);

  result = var_interface_->VarToResource(PP_MakeInt32(42));
  ASSERT_EQ(0, result);

  result = var_interface_->VarToResource(PP_MakeDouble(1.0));
  ASSERT_EQ(0, result);

  PASS();
}
