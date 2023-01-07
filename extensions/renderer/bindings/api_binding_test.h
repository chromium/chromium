// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_H_

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "extensions/renderer/bindings/test_js_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace gin {
class ContextHolder;
class IsolateHolder;
}

namespace extensions {

// A common unit test class for testing API bindings. Creates an isolate and an
// initial v8 context, and checks for v8 leaks at the end of the test.
class APIBindingTest : public testing::Test {
 public:
  APIBindingTest(const APIBindingTest&) = delete;
  APIBindingTest& operator=(const APIBindingTest&) = delete;

 protected:
  APIBindingTest();
  ~APIBindingTest() override;

  // Returns the V8 ExtensionConfiguration to use for contexts. The default
  // implementation returns null.
  virtual v8::ExtensionConfiguration* GetV8ExtensionConfiguration();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Returns a local to the main context.
  v8::Local<v8::Context> MainContext();

  // Creates a new context (maintaining it with a holder in
  // |additional_context_holders_| and returns a local.
  v8::Local<v8::Context> AddContext();

  // Disposes the context pointed to by |context|.
  //
  // When the main context is disposed, it will be exited. This balances the
  // call to Context::Enter in APIBindingTest::SetUp. This will not work
  // if either you have already exited the main context, or you have nested
  // another Context::Enter or Context::Scope, since context entry must be
  // properly nested. A test may re-enter the context by holding a Local
  // to it and then entering a Context::Scope after disposal:
  //
  //   v8::Local<v8::Context> main_context = MainContext();
  //   DisposeContext(main_context);
  //   v8::Context::Scope context_scope(main_context);
  //
  // This can be used to simulate cases where the context has been disposed
  // (e.g. frame detached), but it is still used because references to its
  // objects are held by another context.
  void DisposeContext(v8::Local<v8::Context> context);

  // Disposes all contexts and checks for leaks.
  void DisposeAllContexts();

  // Allows subclasses to perform context disposal cleanup.
  virtual void OnWillDisposeContext(v8::Local<v8::Context> context) {}

  // Runs V8 garbage collection.
  void RunGarbageCollection();

  // Returns the TestJSRunner::Scope to use in the test, or null.
  virtual std::unique_ptr<TestJSRunner::Scope> CreateTestJSRunner();

  // Returns the associated isolate. Defined out-of-line to avoid the include
  // for IsolateHolder in the header.
  v8::Isolate* isolate();

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> main_context_holder_;
  std::unique_ptr<TestJSRunner::Scope> test_js_runner_;
  std::vector<std::unique_ptr<gin::ContextHolder>> additional_context_holders_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDING_TEST_H_
