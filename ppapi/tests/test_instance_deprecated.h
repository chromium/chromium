// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_INSTANCE_DEPRECATED_H_
#define PPAPI_TESTS_TEST_INSTANCE_DEPRECATED_H_

#include <string>
#include <vector>

#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/tests/test_case.h"

class TestInstance;

// ScriptableObject used by TestInstance.
class InstanceSO : public pp::deprecated::ScriptableObject {
 public:
  explicit InstanceSO(TestInstance* i);
  virtual ~InstanceSO();

  // pp::deprecated::ScriptableObject overrides.
  bool HasMethod(const pp::Var& name, pp::Var* exception);
  pp::Var Call(const pp::Var& name,
               const std::vector<pp::Var>& args,
               pp::Var* exception);

  // For out-of-process, the InstanceSO might be deleted after the instance was
  // already destroyed, so we can't rely on test_instance_ being valid.
  void clear_test_instance() { test_instance_ = NULL; }

 private:
  TestInstance* test_instance_;
  const PPB_Testing_Private* testing_interface_;
};

class TestInstance : public TestCase {
 public:
  TestInstance(TestingInstance* instance);
  virtual ~TestInstance();

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void set_string(const std::string& s) { string_ = s; }

  // Leak a reference to the given var, but ignore the leak in the leak checking
  // that happens at shutdown. This allows us to test the "ForceFree" that
  // happens on instance shutdown.
  void LeakReferenceAndIgnore(const pp::Var& leaked);

  // This resets the scriptable object if it gets destroyed before the instance
  // which should be the case for in-process tests.
  void clear_instance_so() { instance_so_ = NULL; }

 protected:
  // Test case protected overrides.
  virtual pp::deprecated::ScriptableObject* CreateTestObject();

 private:
  std::string TestExecuteScript();
  std::string TestRecursiveObjects();
  std::string TestLeakedObjectDestructors();
  std::string TestSetupExecuteScriptAtInstanceShutdown();
  std::string TestExecuteScriptAtInstanceShutdown();

  // Value written by set_string which is called by the ScriptableObject. This
  // allows us to keep track of what was called.
  std::string string_;

  // The scriptable object for this test.
  InstanceSO* instance_so_;
};

#endif  // PPAPI_TESTS_TEST_INSTANCE_DEPRECATED_H_
