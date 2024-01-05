// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_last_error.h"

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "extensions/renderer/bindings/api_binding_test.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "gin/converter.h"
#include "gin/public/context_holder.h"

namespace extensions {

namespace {

// Returns the v8 object for the lastError.
v8::Local<v8::Value> GetLastError(v8::Local<v8::Object> parent,
                                  v8::Local<v8::Context> context) {
  return GetPropertyFromObject(parent, context, "lastError");
}

// Returns a stringified version of the lastError message, if one exists, and
// otherwise a stringified version of whatever the lastError property is (e.g.
// undefined).
std::string GetLastErrorMessage(v8::Local<v8::Object> parent,
                                v8::Local<v8::Context> context) {
  v8::Local<v8::Value> last_error = GetLastError(parent, context);
  if (last_error.IsEmpty() || !last_error->IsObject())
    return V8ToString(last_error, context);
  v8::Local<v8::Value> message =
      GetPropertyFromObject(last_error.As<v8::Object>(), context, "message");
  return V8ToString(message, context);
}

using ContextParentPair =
    std::pair<v8::Local<v8::Context>, v8::Local<v8::Object>>;
using ParentList = v8::MemorySpan<ContextParentPair>;
v8::Local<v8::Object> GetParent(const ParentList& parents,
                                v8::Local<v8::Context> context,
                                v8::Local<v8::Object>* secondary_parent) {
  // This would be simpler with a map<context, object>, but Local<> doesn't
  // define an operator<.
  for (const auto& parent : parents) {
    if (parent.first == context)
      return parent.second;
  }
  return v8::Local<v8::Object>();
}

}  // namespace

using APILastErrorTest = APIBindingTest;

// Test basic functionality of the lastError object.
TEST_F(APILastErrorTest, TestLastError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  auto parents = v8::to_array<ContextParentPair>({{context, parent_object}});
  APILastError last_error(base::BindRepeating(&GetParent, ParentList(parents)),
                          base::DoNothing());

  EXPECT_FALSE(last_error.HasError(context));
  EXPECT_FALSE(last_error.GetErrorMessage(context));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_object, context));
  // Check that the key isn't present on the object (as opposed to simply being
  // undefined).
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());

  last_error.SetError(context, "Some last error");
  EXPECT_TRUE(last_error.HasError(context));
  EXPECT_EQ(R"("Some last error")",
            GetLastErrorMessage(parent_object, context));
  std::optional<std::string> error_message =
      last_error.GetErrorMessage(context);
  EXPECT_TRUE(error_message);
  EXPECT_EQ("Some last error", error_message);

  last_error.ClearError(context, false);
  EXPECT_FALSE(last_error.HasError(context));
  EXPECT_FALSE(last_error.GetErrorMessage(context));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_object, context));
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());
}

// Test throwing an error if the last error wasn't checked.
TEST_F(APILastErrorTest, ReportIfUnchecked) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  std::optional<std::string> console_error;
  auto log_error = [](std::optional<std::string>* console_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *console_error = error; };

  auto parents = v8::to_array<ContextParentPair>({{context, parent_object}});
  APILastError last_error(base::BindRepeating(&GetParent, ParentList(parents)),
                          base::BindRepeating(log_error, &console_error));
  {
    v8::TryCatch try_catch(isolate());
    last_error.SetError(context, "foo");
    // GetLastErrorMessage() will count as accessing the error property, so we
    // shouldn't throw an exception.
    EXPECT_EQ("\"foo\"", GetLastErrorMessage(parent_object, context));
    last_error.ClearError(context, true);
    EXPECT_FALSE(console_error);
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    last_error.SetError(context, "foo");
    // GetLastError() only accesses the error object, and not the message
    // directly (e.g. chrome.runtime.lastError vs
    // chrome.runtime.lastError.message), but should still count as access and
    // shouldn't throw an exception.
    v8::Local<v8::Value> v8_error = GetLastError(parent_object, context);
    ASSERT_FALSE(v8_error.IsEmpty());
    EXPECT_TRUE(v8_error->IsObject());
    last_error.ClearError(context, true);
    EXPECT_FALSE(console_error);
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    // This time, we should log an error.
    last_error.SetError(context, "A last error");
    last_error.ClearError(context, true);
    ASSERT_TRUE(console_error);
    EXPECT_EQ("Unchecked runtime.lastError: A last error", *console_error);
    // We shouldn't have thrown an exception in order to prevent disrupting
    // JS execution.
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    last_error.SetError(context, "A last error");
    // Access through the internal HasError() should not count as access.
    EXPECT_TRUE(last_error.HasError(context));
    last_error.ClearError(context, true);
    ASSERT_TRUE(console_error);
    EXPECT_EQ("Unchecked runtime.lastError: A last error", *console_error);
    EXPECT_FALSE(try_catch.HasCaught());
  }

  {
    v8::TryCatch try_catch(isolate());
    last_error.SetError(context, "A last error");
    // Access through the internal GetErrorMessage() should not count as access.
    std::optional<std::string> error_message =
        last_error.GetErrorMessage(context);
    EXPECT_TRUE(error_message);
    EXPECT_EQ("A last error", error_message);
    last_error.ClearError(context, true);
    ASSERT_TRUE(console_error);
    EXPECT_EQ("Unchecked runtime.lastError: A last error", *console_error);
    EXPECT_FALSE(try_catch.HasCaught());
  }
}

TEST_F(APILastErrorTest, ReportUncheckedError) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  std::optional<std::string> console_error;
  auto log_error = [](std::optional<std::string>* console_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *console_error = error; };

  auto parents = v8::to_array<ContextParentPair>({{context, parent_object}});
  APILastError last_error(base::BindRepeating(&GetParent, ParentList(parents)),
                          base::BindRepeating(log_error, &console_error));

  // lastError should start unset.
  EXPECT_FALSE(last_error.HasError(context));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_object, context));
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());

  {
    v8::TryCatch try_catch(isolate());
    // Report an unchecked error. We should log the error, but not throw an
    // exception to avoid disrupting JS execution.
    last_error.ReportUncheckedError(context, "A last error");
    ASSERT_TRUE(console_error);
    EXPECT_EQ("Unchecked runtime.lastError: A last error", *console_error);
    EXPECT_FALSE(try_catch.HasCaught());
  }

  // lastError should remain unset.
  EXPECT_FALSE(last_error.HasError(context));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_object, context));
  EXPECT_FALSE(
      parent_object->Has(context, gin::StringToV8(isolate(), "lastError"))
          .ToChecked());
}

// Test behavior when something else sets a lastError property on the parent
// object.
TEST_F(APILastErrorTest, NonLastErrorObject) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> parent_object = v8::Object::New(isolate());

  auto parents = v8::to_array<ContextParentPair>({{context, parent_object}});
  APILastError last_error(base::BindRepeating(&GetParent, ParentList(parents)),
                          base::DoNothing());

  auto checked_set = [context](v8::Local<v8::Object> object,
                               std::string_view key,
                               v8::Local<v8::Value> value) {
    v8::Maybe<bool> success = object->Set(
        context, gin::StringToSymbol(context->GetIsolate(), key), value);
    ASSERT_TRUE(success.IsJust());
    ASSERT_TRUE(success.FromJust());
  };

  // Set a "fake" lastError property on the parent.
  v8::Local<v8::Object> fake_last_error = v8::Object::New(isolate());
  checked_set(fake_last_error, "message",
              gin::StringToV8(isolate(), "fake error"));
  checked_set(parent_object, "lastError", fake_last_error);

  EXPECT_EQ("\"fake error\"", GetLastErrorMessage(parent_object, context));

  // The bindings shouldn't mangle an existing property (or maybe we should -
  // see the TODO in api_last_error.cc).
  last_error.SetError(context, "Real last error");
  EXPECT_EQ("\"fake error\"", GetLastErrorMessage(parent_object, context));
  last_error.ClearError(context, false);
  EXPECT_EQ("\"fake error\"", GetLastErrorMessage(parent_object, context));

  checked_set(parent_object, "lastError", v8::Undefined(isolate()));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_object, context));
  last_error.SetError(context, "a last error");
  EXPECT_EQ("\"a last error\"", GetLastErrorMessage(parent_object, context));
  checked_set(parent_object, "lastError", fake_last_error);
  EXPECT_EQ("\"fake error\"", GetLastErrorMessage(parent_object, context));
}

// Test lastError in multiple different contexts.
TEST_F(APILastErrorTest, MultipleContexts) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context_a = MainContext();
  v8::Local<v8::Context> context_b = AddContext();

  v8::Local<v8::Object> parent_a = v8::Object::New(isolate());
  v8::Local<v8::Object> parent_b = v8::Object::New(isolate());
  auto parents = v8::to_array<ContextParentPair>(
      {{context_a, parent_a}, {context_b, parent_b}});
  APILastError last_error(base::BindRepeating(&GetParent, ParentList(parents)),
                          base::DoNothing());

  last_error.SetError(context_a, "Last error a");
  EXPECT_EQ("\"Last error a\"", GetLastErrorMessage(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_b, context_b));

  last_error.SetError(context_b, "Last error b");
  EXPECT_EQ("\"Last error a\"", GetLastErrorMessage(parent_a, context_a));
  EXPECT_EQ("\"Last error b\"", GetLastErrorMessage(parent_b, context_b));

  last_error.ClearError(context_b, false);
  EXPECT_EQ("\"Last error a\"", GetLastErrorMessage(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_b, context_b));

  last_error.ClearError(context_a, false);
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_a, context_a));
  EXPECT_EQ("undefined", GetLastErrorMessage(parent_b, context_b));
}

TEST_F(APILastErrorTest, SecondaryParent) {
  auto get_parents = [](v8::Local<v8::Object> primary_parent,
                        v8::Local<v8::Object> secondary_parent,
                        v8::Local<v8::Context> context,
                        v8::Local<v8::Object>* secondary_parent_out) {
    if (secondary_parent_out)
      *secondary_parent_out = secondary_parent;
    return primary_parent;
  };

  std::optional<std::string> console_error;
  auto log_error = [](std::optional<std::string>* console_error,
                      v8::Local<v8::Context> context,
                      const std::string& error) { *console_error = error; };

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();
  v8::Local<v8::Object> primary_parent = v8::Object::New(isolate());
  v8::Local<v8::Object> secondary_parent = v8::Object::New(isolate());

  APILastError last_error(
      base::BindRepeating(get_parents, primary_parent, secondary_parent),
      base::BindRepeating(log_error, &console_error));

  last_error.SetError(context, "error");
  EXPECT_TRUE(last_error.HasError(context));
  EXPECT_EQ("\"error\"", GetLastErrorMessage(primary_parent, context));
  EXPECT_EQ("\"error\"", GetLastErrorMessage(secondary_parent, context));
  EXPECT_FALSE(console_error);

  last_error.ClearError(context, true);
  EXPECT_FALSE(console_error);
  EXPECT_EQ("undefined", GetLastErrorMessage(primary_parent, context));
  EXPECT_EQ("undefined", GetLastErrorMessage(secondary_parent, context));

  // Accessing the primary parent's error should be sufficient to not log the
  // error in the console.
  last_error.SetError(context, "error");
  EXPECT_EQ("\"error\"", GetLastErrorMessage(primary_parent, context));
  last_error.ClearError(context, true);
  EXPECT_FALSE(console_error);

  // Accessing only the secondary parent's error shouldn't count as access on
  // the main error, and we should log it.
  last_error.SetError(context, "error");
  EXPECT_EQ("\"error\"", GetLastErrorMessage(secondary_parent, context));
  last_error.ClearError(context, true);
  ASSERT_TRUE(console_error);
  EXPECT_EQ("Unchecked runtime.lastError: error", *console_error);
}

}  // namespace extensions
