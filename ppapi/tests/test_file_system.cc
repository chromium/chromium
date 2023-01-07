// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_system.h"

#include <stdint.h>
#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileSystem);

bool TestFileSystem::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileSystem::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestFileSystem, Open, filter);
  RUN_CALLBACK_TEST(TestFileSystem, MultipleOpens, filter);
  RUN_CALLBACK_TEST(TestFileSystem, ResourceConversion, filter);
}

std::string TestFileSystem::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  // Open.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Open aborted.
  int32_t rv = 0;
  {
    pp::FileSystem fs(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
    rv = fs.Open(1024, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestFileSystem::TestMultipleOpens() {
  // Should not allow multiple opens, regardless of whether or not the first
  // open has completed.
  TestCompletionCallback callback_1(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv_1 = file_system.Open(1024, callback_1.GetCallback());

  TestCompletionCallback callback_2(instance_->pp_instance(), callback_type());
  callback_2.WaitForResult(file_system.Open(1024, callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  // FileSystem should not allow multiple opens.
  ASSERT_NE(PP_OK, callback_2.result());

  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);
  ASSERT_EQ(PP_OK, callback_1.result());

  TestCompletionCallback callback_3(instance_->pp_instance(), callback_type());
  callback_3.WaitForResult(file_system.Open(1024, callback_3.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_3);
  ASSERT_NE(PP_OK, callback_3.result());

  PASS();
}

std::string TestFileSystem::TestResourceConversion() {
  // Test conversion from file system Resource to FileSystem.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::Resource file_system_resource(file_system);
  ASSERT_TRUE(pp::FileSystem::IsFileSystem(file_system_resource));
  pp::FileSystem file_system_resource_file_system(file_system_resource);
  ASSERT_FALSE(file_system_resource_file_system.is_null());

  // Test conversion that a non-file system Resource does not register as a
  // FileSystem. (We cannot test conversion, since this is an assertion on a
  // debug build.)
  pp::FileIO non_file_system(instance_);
  pp::Resource non_file_system_resource(non_file_system);
  ASSERT_FALSE(pp::FileSystem::IsFileSystem(non_file_system_resource));

  PASS();
}
