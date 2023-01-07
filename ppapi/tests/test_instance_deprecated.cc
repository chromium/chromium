// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_instance_deprecated.h"

#include <assert.h>

#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

static const char kSetValueFunction[] = "SetValue";
static const char kSetExceptionFunction[] = "SetException";
static const char kReturnValueFunction[] = "ReturnValue";

InstanceSO::InstanceSO(TestInstance* i)
    : test_instance_(i),
      testing_interface_(i->testing_interface()) {
}

InstanceSO::~InstanceSO() {
  if (test_instance_)
    test_instance_->clear_instance_so();
}

bool InstanceSO::HasMethod(const pp::Var& name, pp::Var* exception) {
  if (!name.is_string())
    return false;
  return name.AsString() == kSetValueFunction ||
         name.AsString() == kSetExceptionFunction ||
         name.AsString() == kReturnValueFunction;
}

pp::Var InstanceSO::Call(const pp::Var& method_name,
                         const std::vector<pp::Var>& args,
                         pp::Var* exception) {
  if (!method_name.is_string())
    return false;
  std::string name = method_name.AsString();

  if (name == kSetValueFunction) {
    if (args.size() != 1 || !args[0].is_string())
      *exception = pp::Var("Bad argument to SetValue(<string>)");
    else if (test_instance_)
      test_instance_->set_string(args[0].AsString());
  } else if (name == kSetExceptionFunction) {
    if (args.size() != 1 || !args[0].is_string())
      *exception = pp::Var("Bad argument to SetException(<string>)");
    else
      *exception = args[0];
  } else if (name == kReturnValueFunction) {
    if (args.size() != 1)
      *exception = pp::Var("Need single arg to call ReturnValue");
    else
      return args[0];
  } else {
    *exception = pp::Var("Bad function call");
  }

  return pp::Var();
}

REGISTER_TEST_CASE(Instance);

TestInstance::TestInstance(TestingInstance* instance)
    : TestCase(instance),
      instance_so_(NULL) {}

bool TestInstance::Init() {
  return true;
}

TestInstance::~TestInstance() {
  ResetTestObject();
  if (testing_interface_->IsOutOfProcess() == PP_FALSE) {
    // This should cause the instance object's destructor to be called.
    testing_interface_->RunV8GC(instance_->pp_instance());

    // Test a post-condition which ensures the instance objects destructor is
    // called. This only works reliably in-process. Out-of-process, it only
    // can work when the renderer stays alive a short while after the plugin
    // instance is destroyed. If the renderer is being shut down, too much
    // happens asynchronously for the out-of-process case to work reliably. In
    // particular:
    //   - The Var ReleaseObject message is asynchronous.
    //   - The PPB_Var_Deprecated host-side proxy posts a task to actually
    //     release the object when the ReleaseObject message is received.
    //   - The PPP_Class Deallocate message is asynchronous.
    // At time of writing this comment, if you modify the code so that the above
    // happens synchronously, and you remove the restriction that the plugin
    // can't be unblocked by a sync message, then this check actually passes
    // reliably for out-of-process. But we don't want to make any of those
    // changes so we just skip the check.
    PP_DCHECK(!instance_so_);
  } else {
    // Out-of-process, this destructor might not actually get invoked. Clear
    // the InstanceSOs reference to the instance so there is no UAF.
    if (instance_so_)
      instance_so_->clear_test_instance();
  }

  // Save the fact that we were destroyed in sessionStorage. This tests that
  // we can ExecuteScript at instance destruction without crashing. It also
  // allows us to check that ExecuteScript will run and succeed in certain
  // cases. In particular, when the instance is destroyed by normal DOM
  // deletion, ExecuteScript will actually work. See
  // TestExecuteScriptInInstanceShutdown for that test. Note, however, that
  // ExecuteScript will *not* have an effect when the instance is destroyed
  // because the renderer was shut down.
  pp::Var ret = instance()->ExecuteScript(
      "sessionStorage.setItem('instance_destroyed', 'true');");
}

void TestInstance::RunTests(const std::string& filter) {
  RUN_TEST(ExecuteScript, filter);
  RUN_TEST(RecursiveObjects, filter);
  RUN_TEST(LeakedObjectDestructors, filter);
  RUN_TEST(SetupExecuteScriptAtInstanceShutdown, filter);
  RUN_TEST(ExecuteScriptAtInstanceShutdown, filter);
}

void TestInstance::LeakReferenceAndIgnore(const pp::Var& leaked) {
  static const PPB_Var* var_interface = static_cast<const PPB_Var*>(
        pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  var_interface->AddRef(leaked.pp_var());
  IgnoreLeakedVar(leaked.pp_var().value.as_id);
}

pp::deprecated::ScriptableObject* TestInstance::CreateTestObject() {
  if (!instance_so_)
    instance_so_ = new InstanceSO(this);
  return instance_so_;
}

std::string TestInstance::TestExecuteScript() {
  // Simple call back into the plugin.
  pp::Var exception;
  pp::Var ret = instance_->ExecuteScript(
      "document.getElementById('plugin').SetValue('hello, world');",
      &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_TRUE(string_ == "hello, world");

  // Return values from the plugin should be returned.
  ret = instance_->ExecuteScript(
      "document.getElementById('plugin').ReturnValue('return value');",
      &exception);
  ASSERT_TRUE(ret.is_string() && ret.AsString() == "return value");
  ASSERT_TRUE(exception.is_undefined());

  // Exception thrown by the plugin should be caught.
  ret = instance_->ExecuteScript(
      "document.getElementById('plugin').SetException('plugin exception');",
      &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_string());
  // Due to a limitation in the implementation of TryCatch, it doesn't actually
  // pass the strings up. Since this is a trusted only interface, we've decided
  // not to bother fixing this for now.

  // Exception caused by string evaluation should be caught.
  exception = pp::Var();
  ret = instance_->ExecuteScript("document.doesntExist()", &exception);
  ASSERT_TRUE(ret.is_undefined());
  ASSERT_TRUE(exception.is_string());  // Don't know exactly what it will say.

  PASS();
}

// A scriptable object that contains other scriptable objects recursively. This
// is used to help verify that our scriptable object clean-up code works
// properly.
class ObjectWithChildren : public pp::deprecated::ScriptableObject {
 public:
  ObjectWithChildren(TestInstance* i, int num_descendents) {
    if (num_descendents > 0) {
      child_ = pp::VarPrivate(i->instance(),
                              new ObjectWithChildren(i, num_descendents - 1));
    }
  }
  struct IgnoreLeaks {};
  ObjectWithChildren(TestInstance* i, int num_descendents, IgnoreLeaks) {
    if (num_descendents > 0) {
      child_ = pp::VarPrivate(i->instance(),
                              new ObjectWithChildren(i, num_descendents - 1,
                                                     IgnoreLeaks()));
      i->IgnoreLeakedVar(child_.pp_var().value.as_id);
    }
  }
 private:
  pp::VarPrivate child_;
};

std::string TestInstance::TestRecursiveObjects() {
  const int kNumChildren = 20;
  {
    // These should be deleted when we exit scope, so should not leak.
    pp::VarPrivate not_leaked(instance(), new ObjectWithChildren(this,
                                                                 kNumChildren));
  }
  // We need to run the GC multiple times until all of the vars are released.
  // Each GC invocation will result in releasing a var, which will result in its
  // children not having any references, allowing them also to be collected.
  for (int i = 0; i < kNumChildren; ++i)
    testing_interface_->RunV8GC(instance_->pp_instance());

  // Leak some, but tell TestCase to ignore the leaks. This test is run and then
  // reloaded (see ppapi_uitest.cc). If these aren't cleaned up when the first
  // run is torn down, they will show up as leaks in the second run.
  // NOTE: The ScriptableObjects are actually leaked, but they should be removed
  //       from the tracker. See below for a test that verifies that the
  //       destructor is not run.
  pp::VarPrivate leaked(
      instance(),
      new ObjectWithChildren(this, kNumChildren,
                             ObjectWithChildren::IgnoreLeaks()));
  // Now leak a reference to the root object. This should force the root and
  // all its descendents to stay in the tracker.
  LeakReferenceAndIgnore(leaked);

  PASS();
}

// A scriptable object that should cause a crash if its destructor is run. We
// don't run the destructor for objects which the plugin leaks. This is to
// prevent them doing dangerous things at cleanup time, such as executing script
// or creating new objects.
class BadDestructorObject : public pp::deprecated::ScriptableObject {
 public:
  BadDestructorObject() {}
  ~BadDestructorObject() {
    assert(false);
  }
};

std::string TestInstance::TestLeakedObjectDestructors() {
  pp::VarPrivate leaked(instance(), new BadDestructorObject());
  // Leak a reference so it gets deleted on instance shutdown.
  LeakReferenceAndIgnore(leaked);
  PASS();
}

std::string TestInstance::TestSetupExecuteScriptAtInstanceShutdown() {
  // This test only exists so that it can be run before
  // TestExecuteScriptAtInstanceShutdown. See the comment for that test.
  pp::Var exception;
  pp::Var result = instance()->ExecuteScript(
      "sessionStorage.removeItem('instance_destroyed');", &exception);
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_TRUE(result.is_undefined());
  PASS();
}

std::string TestInstance::TestExecuteScriptAtInstanceShutdown() {
  // This test relies on the previous test being run in the same browser
  // session, but in such a way that the instance is destroyed. See
  // chrome/test/ppapi/ppapi_browsertest.cc for how the navigation happens.
  //
  // Given those constraints, ~TestInstance should have been invoked to set
  // instance_destroyed in sessionStorage. So all we have to do is make sure
  // that it was set as expected.
  pp::Var result = instance()->ExecuteScript(
      "sessionStorage.getItem('instance_destroyed');");
  ASSERT_TRUE(result.is_string());
  ASSERT_EQ(std::string("true"), result.AsString());
  instance()->ExecuteScript("sessionStorage.removeItem('instance_destroyed');");

  PASS();
}

