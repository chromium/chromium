// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_console.h"

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Console);

TestConsole::TestConsole(TestingInstance* instance)
    : TestCase(instance),
      console_interface_(NULL) {
}

bool TestConsole::Init() {
  console_interface_ = static_cast<const PPB_Console*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
  return !!console_interface_;
}

void TestConsole::RunTests(const std::string& filter) {
  RUN_TEST(Smoke, filter);
}

namespace {

void TestConsoleSub(const PPB_Console* console_interface_,
                    PP_Instance instance,
                    pp::Var source,
                    pp::Var message) {

  console_interface_->Log(instance, PP_LOGLEVEL_ERROR,
                          message.pp_var());
  console_interface_->LogWithSource(instance, PP_LOGLEVEL_LOG,
                                    source.pp_var(), message.pp_var());
}

} // anonymous namespace

std::string TestConsole::TestSmoke() {
  // This test does not verify the log message actually reaches the console, but
  // it does test that the interface exists and that it can be called without
  // crashing.
  pp::Var source(std::string("somewhere"));
  const PPB_Console* interface = console_interface_;
  PP_Instance pp_instance = instance()->pp_instance();

  TestConsoleSub(interface, pp_instance, source, pp::Var());
  TestConsoleSub(interface, pp_instance, source, pp::Var(pp::Var::Null()));
  TestConsoleSub(interface, pp_instance, source, pp::Var(false));
  TestConsoleSub(interface, pp_instance, source, pp::Var(12345678));
  TestConsoleSub(interface, pp_instance, source, pp::Var(-0.0));
  TestConsoleSub(interface, pp_instance, source, pp::Var("Hello World!"));
  TestConsoleSub(interface, pp_instance, source, pp::VarArray());
  TestConsoleSub(interface, pp_instance, source, pp::VarArrayBuffer());
  TestConsoleSub(interface, pp_instance, source, pp::VarDictionary());
  PASS();
}
