// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_UNITTEST_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_UNITTEST_H_

#include <map>
#include <memory>
#include <string>

#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "v8/include/v8.h"

namespace extensions {
class APIBindingsSystem;

// The base class to test the APIBindingsSystem. This allows subclasses to
// retrieve API schemas differently.
class APIBindingsSystemTest : public APIBindingTest {
 public:
  APIBindingsSystemTest(const APIBindingsSystemTest&) = delete;
  APIBindingsSystemTest& operator=(const APIBindingsSystemTest&) = delete;

 protected:
  // A struct representing a "fake" API, including the name and specification.
  // The specification is expected to be a JSON-serializable string that
  // specifies the types, methods, and events of an API in the same syntax as
  // the real extension APIs.
  struct FakeSpec {
    const char* name;
    const char* spec;
  };

  APIBindingsSystemTest();
  ~APIBindingsSystemTest() override;
  void SetUp() override;
  void TearDown() override;
  void OnWillDisposeContext(v8::Local<v8::Context> context) override;

  // Returns the collection of fake APIs to be used in the test, allowing
  // subclasses to return their own specifications.
  virtual std::vector<FakeSpec> GetAPIs();

  // Returns the object to be used as the parent for the `lastError`, and,
  // optionally, the secondary parent. The default returns an empty JS object
  // and does not populate |secondary_parent| (assumes no last errors will be
  // set).
  virtual v8::Local<v8::Object> GetLastErrorParent(
      v8::Local<v8::Context> context,
      v8::Local<v8::Object>* secondary_parent);

  // Simulates logging an error to the console.
  void AddConsoleError(v8::Local<v8::Context> context,
                       const std::string& error);

  // Returns the base::Value::Dict representing the schema with the given API
  // name.
  const base::Value::Dict& GetAPISchema(const std::string& api_name);

  // Callback for event listeners changing.
  void OnEventListenersChanged(const std::string& event_name,
                               binding::EventListenersChanged changed,
                               const base::Value::Dict* filter,
                               bool was_manual,
                               v8::Local<v8::Context> context);

  // Callback for an API request being made. Stores the request in
  // |last_request_|.
  void OnAPIRequest(std::unique_ptr<APIRequestHandler::Request> request,
                    v8::Local<v8::Context> context);

  // Checks that |last_request_| exists and was provided with the
  // |expected_name| and |expected_arguments|.
  void ValidateLastRequest(const std::string& expected_name,
                           const std::string& expected_arguments);

  // Wraps the given |script source| in (function(obj) { ... }) and executes
  // the result function, passing in |object| for an argument. Returns the
  // result of calling the function.
  v8::Local<v8::Value> CallFunctionOnObject(v8::Local<v8::Context> context,
                                            v8::Local<v8::Object> object,
                                            const std::string& script_source);

  const APIRequestHandler::Request* last_request() const {
    return last_request_.get();
  }
  void reset_last_request() { last_request_.reset(); }
  APIBindingsSystem* bindings_system() { return bindings_system_.get(); }
  const std::vector<std::string>& console_errors() const {
    return console_errors_;
  }

 private:
  // The API schemas for the fake APIs.
  std::map<std::string, base::Value::Dict> api_schemas_;

  // The APIBindingsSystem associated with the test. Safe to use across multiple
  // contexts.
  std::unique_ptr<APIBindingsSystem> bindings_system_;

  // The last request to be received from the APIBindingsSystem, or null if
  // there is none.
  std::unique_ptr<APIRequestHandler::Request> last_request_;

  // A list for keeping track of simulated console errors.
  std::vector<std::string> console_errors_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_BINDINGS_SYSTEM_UNITTEST_H_
