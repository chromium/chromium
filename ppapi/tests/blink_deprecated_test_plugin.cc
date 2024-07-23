// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple C++ Pepper plugin for exercising deprecated PPAPI interfaces in
// Blink layout tests.
//
// Most layout tests should prefer to use the normal Blink test plugin, with the
// MIME type application/x-blink-test-plugin. For layout tests that absolutely
// need to test deprecated synchronous scripting interfaces, this plugin can be
// instantiated using the application/x-blink-deprecated-test-plugin MIME type.
//
// The plugin exposes the following interface:
//
// Attributes:
// testwindowopen: if set, the plugin will synchronously attempt to open a
// window from DidCreateInstance, and log a message if successful.
//
// keydownscript: if set, the plugin will execute the value of the attribute as
// a script on a key down.
//
// mousedownscript: if set, the plugin will execute the value of the attribute
// as a script on a mouse button down.
//
//
// Functions:
// * plugin.normalize(): synchronously calls window.pluginCallback.
//
// * plugin.remember(value): keeps a reference on |value| in the plugin.
//
// * plugin.testCloneObject(): creates and returns another instance of the
// plugin object.
//
// * plugin.testCreateTestObject(): creates and returns a new TestObject
// instance (see below).
//
// * plugin.testExecuteScript(script): synchronously evaluates |script| and
// returns the result.
//
// * plugin.testGetProperty(property): returns the property named |property|
// from the window object.
//
// * plugin.testPassTestObject(function, object): synchronously calls the
// function named |function| on the window object, passing it |object| as a
// parameter, and returns its result.
//
// * plugin.testScriptObjectInvoke(function, value): synchronously calls the
// function named |function| on the window object, passing it |value| as a
// parameter, and returns its result.
//
//
// Properties:
// * plugin.testObject (read-only): a TestObject instance (see below).
//
// * plugin.testObjectCount (read-only): the number of TestObject instance
// created.
//
// * plugin.testGetUndefined (read-only): returns undefined.
//
//
// TestObject exposes the following interface:
// Properties:
// * object.testObject (read-only: another TestObject instance.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <map>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/instance_private.h"
#include "ppapi/cpp/private/var_private.h"
#include "ppapi/cpp/var.h"

namespace {

class ScriptableBase : public pp::deprecated::ScriptableObject {
 public:
  explicit ScriptableBase(pp::InstancePrivate* instance)
      : instance_(instance) {}
  ~ScriptableBase() override {}

  // pp::deprecated::ScriptableObject overrides:
  bool HasMethod(const pp::Var& name, pp::Var* exception) override {
    return FindMethod(name) != methods_.end();
  }

  bool HasProperty(const pp::Var& name, pp::Var* exception) override {
    return FindProperty(name) != properties_.end();
  }

  pp::Var Call(const pp::Var& method_name,
               const std::vector<pp::Var>& args,
               pp::Var* exception) override {
    auto method = FindMethod(method_name);
    if (method != methods_.end()) {
      return method->second.Run(args, exception);
    }

    return ScriptableObject::Call(method_name, args, exception);
  }

  pp::Var GetProperty(const pp::Var& name, pp::Var* exception) override {
    auto accessor = FindProperty(name);
    if (accessor != properties_.end()) {
      pp::Var value;
      accessor->second.Run(false, &value);
      return value;
    }
    return ScriptableObject::GetProperty(name, exception);
  }

  void SetProperty(const pp::Var& name,
                   const pp::Var& value,
                   pp::Var* exception) override {
    auto accessor = FindProperty(name);
    if (accessor != properties_.end())
      accessor->second.Run(true, const_cast<pp::Var*>(&value));
    else
      ScriptableObject::SetProperty(name, value, exception);
  }

 protected:
  using MethodMap = std::map<
      std::string,
      base::RepeatingCallback<pp::Var(const std::vector<pp::Var>&, pp::Var*)>>;
  using PropertyMap =
      std::map<std::string, base::RepeatingCallback<void(bool, pp::Var*)>>;

  MethodMap::iterator FindMethod(const pp::Var& name) {
    if (!name.is_string())
      return methods_.end();
    return methods_.find(name.AsString());
  }

  PropertyMap::iterator FindProperty(const pp::Var& name) {
    if (!name.is_string())
      return properties_.end();
    return properties_.find(name.AsString());
  }

  pp::InstancePrivate* const instance_;
  MethodMap methods_;
  PropertyMap properties_;
};

class TestObjectSO : public ScriptableBase {
 public:
  explicit TestObjectSO(pp::InstancePrivate* instance)
      : ScriptableBase(instance) {
    ++count_;
    properties_.insert(std::make_pair(
        "testObject", base::BindRepeating(&TestObjectSO::TestObjectAccessor,
                                          base::Unretained(this))));
  }
  ~TestObjectSO() override {
    --count_;
  }

  static int32_t count() { return count_; }

 private:
  void TestObjectAccessor(bool set, pp::Var* var) {
    if (set)
      return;
    if (test_object_.is_undefined())
      test_object_ = pp::VarPrivate(instance_, new TestObjectSO(instance_));
    *var = test_object_;
  }

  static int32_t count_;

  pp::VarPrivate test_object_;
};

int32_t TestObjectSO::count_ = 0;

class InstanceSO : public ScriptableBase {
 public:
  explicit InstanceSO(pp::InstancePrivate* instance)
      : ScriptableBase(instance) {
    methods_.insert(std::make_pair(
        "normalize",
        base::BindRepeating(&InstanceSO::Normalize, base::Unretained(this))));
    methods_.insert(std::make_pair(
        "remember",
        base::BindRepeating(&InstanceSO::Remember, base::Unretained(this))));
    methods_.insert(std::make_pair(
        "testCloneObject", base::BindRepeating(&InstanceSO::TestCloneObject,
                                               base::Unretained(this))));
    methods_.insert(
        std::make_pair("testCreateTestObject",
                       base::BindRepeating(&InstanceSO::TestCreateTestObject,
                                           base::Unretained(this))));
    methods_.insert(std::make_pair(
        "testExecuteScript", base::BindRepeating(&InstanceSO::TestExecuteScript,
                                                 base::Unretained(this))));
    methods_.insert(std::make_pair(
        "testGetProperty", base::BindRepeating(&InstanceSO::TestGetProperty,
                                               base::Unretained(this))));
    methods_.insert(
        std::make_pair("testPassTestObject",
                       base::BindRepeating(&InstanceSO::TestPassTestObject,
                                           base::Unretained(this))));
    // Note: the semantics of testScriptObjectInvoke are identical to the
    // semantics of testPassTestObject: call args[0] with args[1] as a
    // parameter.
    methods_.insert(
        std::make_pair("testScriptObjectInvoke",
                       base::BindRepeating(&InstanceSO::TestPassTestObject,
                                           base::Unretained(this))));
    properties_.insert(std::make_pair(
        "testObject", base::BindRepeating(&InstanceSO::TestObjectAccessor,
                                          base::Unretained(this))));
    properties_.insert(
        std::make_pair("testObjectCount",
                       base::BindRepeating(&InstanceSO::TestObjectCountAccessor,
                                           base::Unretained(this))));
    properties_.insert(std::make_pair(
        "testGetUndefined",
        base::BindRepeating(&InstanceSO::TestGetUndefinedAccessor,
                            base::Unretained(this))));
  }
  ~InstanceSO() override = default;

 private:
  // Requires no argument.
  pp::Var Normalize(const std::vector<pp::Var>& args, pp::Var* exception) {
    pp::VarPrivate object = instance_->GetWindowObject();
    return object.Call(pp::Var("pluginCallback"), exception);
  }

  // Requires 1 argument. The argument is retained into remembered_
  pp::Var Remember(const std::vector<pp::Var>& args, pp::Var* exception) {
    if (args.size() != 1) {
      *exception = pp::Var("remember requires one argument");
      return pp::Var();
    }
    remembered_ = args[0];
    return pp::Var();
  }

  // Requires no argument.
  pp::Var TestCloneObject(const std::vector<pp::Var>& args,
                          pp::Var* exception) {
    return pp::VarPrivate(instance_, new InstanceSO(instance_));
  }

  // Requires no argument.
  pp::Var TestCreateTestObject(const std::vector<pp::Var>& args,
                               pp::Var* exception) {
    return pp::VarPrivate(instance_, new TestObjectSO(instance_));
  }

  // Requires one argument. The argument is passed through as-is to
  // pp::InstancePrivate::ExecuteScript().
  pp::Var TestExecuteScript(const std::vector<pp::Var>& args,
                            pp::Var* exception) {
    if (args.size() != 1) {
      *exception = pp::Var("testExecuteScript requires one argument");
      return pp::Var();
    }
    return instance_->ExecuteScript(args[0], exception);
  }

  // Requires one or more arguments. Roughly analogous to NPN_GetProperty.
  // The arguments are the chain of properties to traverse, starting with the
  // global context.
  pp::Var TestGetProperty(const std::vector<pp::Var>& args,
                          pp::Var* exception) {
    if (args.size() < 1) {
      *exception = pp::Var("testGetProperty requires at least one argument");
      return pp::Var();
    }
    pp::VarPrivate object = instance_->GetWindowObject();
    for (const auto& arg : args) {
      if (!object.HasProperty(arg, exception))
        return pp::Var();
      object = object.GetProperty(arg, exception);
    }
    return object;
  }

  // Requires 2 or more arguments. The first argument is the name of a function
  // to invoke, and the second argument is a value to pass to that function.
  pp::Var TestPassTestObject(const std::vector<pp::Var>& args,
                             pp::Var* exception) {
    if (args.size() < 2) {
      *exception = pp::Var("testPassTestObject requires at least 2 arguments");
      return pp::Var();
    }
    pp::VarPrivate object = instance_->GetWindowObject();
    return object.Call(args[0], args[1], exception);
  }

  void TestObjectAccessor(bool set, pp::Var* var) {
    if (set)
      return;
    if (test_object_.is_undefined())
      test_object_ = pp::VarPrivate(instance_, new TestObjectSO(instance_));
    *var = test_object_;
  }

  void TestObjectCountAccessor(bool set, pp::Var* var) {
    if (set)
      return;
    *var = pp::Var(TestObjectSO::count());
  }

  void TestGetUndefinedAccessor(bool set, pp::Var* var) {
    if (set)
      return;
    *var = pp::Var();
  }

  pp::VarPrivate test_object_;
  pp::Var remembered_;
};

class BlinkDeprecatedTestInstance : public pp::InstancePrivate {
 public:
  explicit BlinkDeprecatedTestInstance(PP_Instance instance)
      : pp::InstancePrivate(instance) {}
  ~BlinkDeprecatedTestInstance() override {
    LogMessage("%s", "Destroying");
  }

  // pp::Instance overrides
  bool Init(uint32_t argc, const char* argn[], const char* argv[]) override {
    for (uint32_t i = 0; i < argc; ++i)
      attributes_[argn[i]] = argv[i];

    if (HasAttribute("testwindowopen"))
      return TestWindowOpen();

    if (HasAttribute("initscript"))
      ExecuteScript(attributes_["initscript"]);

    uint32_t event_classes = 0;
    if (HasAttribute("keydownscript"))
        event_classes |= PP_INPUTEVENT_CLASS_KEYBOARD;
    if (HasAttribute("mousedownscript"))
        event_classes |= PP_INPUTEVENT_CLASS_MOUSE;
    RequestFilteringInputEvents(event_classes);

    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) override {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        if (HasAttribute("mousedownscript"))
          ExecuteScript(attributes_["mousedownscript"]);
        return true;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        if (HasAttribute("keydownscript"))
          ExecuteScript(attributes_["keydownscript"]);
        return true;
      default:
        return false;
    }
  }

  // pp::InstancePrivate overrides:
  pp::Var GetInstanceObject() override {
    if (instance_var_.is_undefined()) {
      instance_so_ = new InstanceSO(this);
      instance_var_ = pp::VarPrivate(this, instance_so_);
    }
    return instance_var_;
  }

  void NotifyTestCompletion() {
    ExecuteScript("window.testRunner.notifyDone()");
  }

  bool TestWindowOpen() {
    pp::Var result = GetWindowObject().Call(
        pp::Var("open"), pp::Var("about:blank"), pp::Var("_blank"));
    if (result.is_object())
      LogMessage("PLUGIN: WINDOW OPEN SUCCESS");
    NotifyTestCompletion();
    return true;
  }

  void LogMessage(const char* format...) {
    va_list args;
    va_start(args, format);
    LogToConsoleWithSource(PP_LOGLEVEL_LOG,
                           pp::Var("Blink Deprecated Test Plugin"),
                           pp::Var(base::StringPrintV(format, args)));
    va_end(args);
  }

 private:
  bool HasAttribute(const std::string& name) {
    return attributes_.find(name) != attributes_.end();
  }

  std::unordered_map<std::string, std::string> attributes_;
  pp::VarPrivate instance_var_;
  // Owned by |instance_var_|.
  InstanceSO* instance_so_;
};

class BlinkDeprecatedTestModule : public pp::Module {
 public:
  BlinkDeprecatedTestModule() {}
  ~BlinkDeprecatedTestModule() override {}

  pp::Instance* CreateInstance(PP_Instance instance) override {
    return new BlinkDeprecatedTestInstance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new BlinkDeprecatedTestModule();
}

}  // namespace pp
