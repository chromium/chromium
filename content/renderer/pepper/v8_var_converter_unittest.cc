// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/v8_var_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <unordered_map>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/renderer/pepper/resource_converter.h"
#include "gin/public/isolate_holder.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/test_utils.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-template.h"

using ppapi::ArrayBufferVar;
using ppapi::ArrayVar;
using ppapi::DictionaryVar;
using ppapi::PpapiGlobals;
using ppapi::ProxyLock;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using ppapi::TestGlobals;
using ppapi::TestEqual;
using ppapi::VarTracker;

namespace content {

namespace {

void FromV8ValueComplete(const ScopedPPVar& scoped_var,
                         bool success) {
  NOTREACHED_IN_MIGRATION();
}

class MockResourceConverter : public content::ResourceConverter {
 public:
  ~MockResourceConverter() override {}
  void Reset() override {}
  bool NeedsFlush() override { return false; }
  void Flush(base::OnceCallback<void(bool)> callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  bool FromV8Value(v8::Local<v8::Object> val,
                   v8::Local<v8::Context> context,
                   PP_Var* result,
                   bool* was_resource) override {
    *was_resource = false;
    return true;
  }
  bool ToV8Value(const PP_Var& var,
                 v8::Local<v8::Context> context,
                 v8::Local<v8::Value>* result) override {
    return false;
  }
};

// Maps PP_Var IDs to the V8 value handle they correspond to.
typedef std::unordered_map<int64_t, v8::Local<v8::Value>> VarHandleMap;

bool Equals(const PP_Var& var,
            v8::Local<v8::Value> val,
            v8::Isolate* isolate,
            VarHandleMap* visited_ids) {
  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type)) {
    auto it = visited_ids->find(var.value.as_id);
    if (it != visited_ids->end())
      return it->second == val;
    (*visited_ids)[var.value.as_id] = val;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (val->IsUndefined()) {
    return var.type == PP_VARTYPE_UNDEFINED;
  } else if (val->IsNull()) {
    return var.type == PP_VARTYPE_NULL;
  } else if (val->IsBoolean() || val->IsBooleanObject()) {
    return var.type == PP_VARTYPE_BOOL &&
           PP_FromBool(val->ToBoolean(isolate)->Value()) == var.value.as_bool;
  } else if (val->IsInt32()) {
    return var.type == PP_VARTYPE_INT32 &&
           val.As<v8::Int32>()->Value() == var.value.as_int;
  } else if (val->IsNumber() || val->IsNumberObject()) {
    return var.type == PP_VARTYPE_DOUBLE &&
           fabs(val->NumberValue(context).ToChecked() - var.value.as_double) <=
               1.0e-4;
  } else if (val->IsString() || val->IsStringObject()) {
    if (var.type != PP_VARTYPE_STRING)
      return false;
    StringVar* string_var = StringVar::FromPPVar(var);
    DCHECK(string_var);
    v8::String::Utf8Value utf8(isolate, val);
    return std::string(*utf8, utf8.length()) == string_var->value();
  } else if (val->IsArray()) {
    if (var.type != PP_VARTYPE_ARRAY)
      return false;
    ArrayVar* array_var = ArrayVar::FromPPVar(var);
    DCHECK(array_var);
    v8::Local<v8::Array> v8_array = val.As<v8::Array>();
    if (v8_array->Length() != array_var->elements().size())
      return false;
    for (uint32_t i = 0; i < v8_array->Length(); ++i) {
      v8::Local<v8::Value> child_v8 =
          v8_array->Get(context, i).ToLocalChecked();
      if (!Equals(array_var->elements()[i].get(), child_v8, isolate,
                  visited_ids)) {
        return false;
      }
    }
    return true;
  } else if (val->IsObject()) {
    if (var.type == PP_VARTYPE_ARRAY_BUFFER) {
      // TODO(raymes): Implement this when we have tests for array buffers.
      NOTIMPLEMENTED();
      return false;
    } else {
      v8::Local<v8::Object> v8_object = val.As<v8::Object>();
      if (var.type != PP_VARTYPE_DICTIONARY)
        return false;
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(var);
      DCHECK(dict_var);
      v8::Local<v8::Array> property_names(
          v8_object->GetOwnPropertyNames(context).ToLocalChecked());
      if (property_names->Length() != dict_var->key_value_map().size())
        return false;
      for (uint32_t i = 0; i < property_names->Length(); ++i) {
        v8::Local<v8::Value> key(
            property_names->Get(context, i).ToLocalChecked());

        if (!key->IsString() && !key->IsNumber())
          return false;
        v8::Local<v8::Value> child_v8 =
            v8_object->Get(context, key).ToLocalChecked();

        v8::String::Utf8Value name_utf8(isolate, key);
        ScopedPPVar release_key(ScopedPPVar::PassRef(),
                                StringVar::StringToPPVar(std::string(
                                    *name_utf8, name_utf8.length())));
        if (!dict_var->HasKey(release_key.get()))
          return false;
        ScopedPPVar release_value(ScopedPPVar::PassRef(),
                                  dict_var->Get(release_key.get()));
        if (!Equals(release_value.get(), child_v8, isolate, visited_ids)) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

bool Equals(const PP_Var& var, v8::Local<v8::Value> val, v8::Isolate* isolate) {
  VarHandleMap var_handle_map;
  return Equals(var, val, isolate, &var_handle_map);
}

class V8VarConverterTest : public testing::Test {
 public:
  V8VarConverterTest()
      : isolate_holder_(task_environment_.GetMainThreadTaskRunner(),
                        gin::IsolateHolder::IsolateType::kTest),
        isolate_scope_(isolate_holder_.isolate()) {
    isolate_ = isolate_holder_.isolate();
    PP_Instance dummy = 1234;
    converter_ = std::make_unique<V8VarConverter>(
        dummy, std::unique_ptr<ResourceConverter>(new MockResourceConverter));
  }
  ~V8VarConverterTest() override {}

  // testing::Test implementation.
  void SetUp() override {
    ProxyLock::Acquire();
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_, nullptr, global));
  }
  void TearDown() override {
    isolate_ = nullptr;
    context_.Reset();
    ASSERT_TRUE(PpapiGlobals::Get()->GetVarTracker()->GetLiveVars().empty());
    ProxyLock::Release();
  }

 protected:
  bool FromV8ValueSync(v8::Local<v8::Value> val,
                       v8::Local<v8::Context> context,
                       PP_Var* result) {
    V8VarConverter::VarResult conversion_result = converter_->FromV8Value(
        val, context, base::BindOnce(&FromV8ValueComplete));
    DCHECK(conversion_result.completed_synchronously);
    if (conversion_result.success)
      *result = conversion_result.var.Release();

    return conversion_result.success;
  }

  bool RoundTrip(const PP_Var& var, PP_Var* result) {
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks(context,
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Value> v8_result;
    if (!converter_->ToV8Value(var, context, &v8_result))
      return false;
    if (!Equals(var, v8_result, isolate_)) {
      return false;
    }
    if (!FromV8ValueSync(v8_result, context, result))
      return false;
    return true;
  }

  // Assumes a ref for var.
  bool RoundTripAndCompare(const PP_Var& var) {
    ScopedPPVar expected(ScopedPPVar::PassRef(), var);
    PP_Var actual_var;
    if (!RoundTrip(expected.get(), &actual_var))
      return false;
    ScopedPPVar actual(ScopedPPVar::PassRef(), actual_var);
    return TestEqual(expected.get(), actual.get(), false);
  }

  raw_ptr<v8::Isolate> isolate_;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;

  std::unique_ptr<V8VarConverter> converter_;

 private:
  // Required to receive callbacks.
  base::test::TaskEnvironment task_environment_;
  gin::IsolateHolder isolate_holder_;
  v8::Isolate::Scope isolate_scope_;

  TestGlobals globals_;
};

}  // namespace

TEST_F(V8VarConverterTest, SimpleRoundTripTest) {
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeUndefined()));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeNull()));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeInt32(100)));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeBool(PP_TRUE)));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeDouble(53.75)));
}

TEST_F(V8VarConverterTest, StringRoundTripTest) {
  EXPECT_TRUE(RoundTripAndCompare(StringVar::StringToPPVar("")));
  EXPECT_TRUE(RoundTripAndCompare(StringVar::StringToPPVar("hello world!")));
}

TEST_F(V8VarConverterTest, ArrayBufferRoundTripTest) {
  // TODO(raymes): Testing this here requires spinning up some of WebKit.
  // Work out how to do this.
}

TEST_F(V8VarConverterTest, DictionaryArrayRoundTripTest) {
  // Empty array.
  scoped_refptr<ArrayVar> array(new ArrayVar);
  ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  size_t index = 0;

  // Array with primitives.
  array->Set(index++, PP_MakeUndefined());
  array->Set(index++, PP_MakeNull());
  array->Set(index++, PP_MakeInt32(100));
  array->Set(index++, PP_MakeBool(PP_FALSE));
  array->Set(index++, PP_MakeDouble(0.123));
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with 2 references to the same string.
  ScopedPPVar release_string(ScopedPPVar::PassRef(),
                             StringVar::StringToPPVar("abc"));
  array->Set(index++, release_string.get());
  array->Set(index++, release_string.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with nested array that references the same string.
  scoped_refptr<ArrayVar> array2(new ArrayVar);
  ScopedPPVar release_array2(ScopedPPVar::PassRef(), array2->GetPPVar());
  array2->Set(0, release_string.get());
  array->Set(index++, release_array2.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Empty dictionary.
  scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
  ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                 dictionary->GetPPVar());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with primitives.
  dictionary->SetWithStringKey("1", PP_MakeUndefined());
  dictionary->SetWithStringKey("2", PP_MakeNull());
  dictionary->SetWithStringKey("3", PP_MakeInt32(-100));
  dictionary->SetWithStringKey("4", PP_MakeBool(PP_TRUE));
  dictionary->SetWithStringKey("5", PP_MakeDouble(-103.52));
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with 2 references to the same string.
  dictionary->SetWithStringKey("6", release_string.get());
  dictionary->SetWithStringKey("7", release_string.get());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with nested dictionary that references the same string.
  scoped_refptr<DictionaryVar> dictionary2(new DictionaryVar);
  ScopedPPVar release_dictionary2(ScopedPPVar::PassRef(),
                                  dictionary2->GetPPVar());
  dictionary2->SetWithStringKey("abc", release_string.get());
  dictionary->SetWithStringKey("8", release_dictionary2.get());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Array with dictionary.
  array->Set(index++, release_dictionary.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with dictionary with array.
  array2->Set(0, PP_MakeInt32(100));
  dictionary->SetWithStringKey("9", release_array2.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));
}

TEST_F(V8VarConverterTest, Cycles) {
  // Check that cycles aren't converted.
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  // Var->V8 conversion.
  {
    scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
    ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                   dictionary->GetPPVar());
    scoped_refptr<ArrayVar> array(new ArrayVar);
    ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());

    dictionary->SetWithStringKey("1", release_array.get());
    array->Set(0, release_dictionary.get());

    v8::Local<v8::Value> v8_result;

    // Array <-> dictionary cycle.
    dictionary->SetWithStringKey("1", release_array.get());
    ASSERT_FALSE(
        converter_->ToV8Value(release_dictionary.get(), context, &v8_result));
    // Break the cycle.
    // TODO(raymes): We need some better machinery for releasing vars with
    // cycles. Remove the code below once we have that.
    dictionary->DeleteWithStringKey("1");

    // Array with self reference.
    array->Set(0, release_array.get());
    ASSERT_FALSE(
        converter_->ToV8Value(release_array.get(), context, &v8_result));
    // Break the self reference.
    array->Set(0, PP_MakeUndefined());
  }

  // V8->Var conversion.
  {
    v8::Local<v8::Object> object = v8::Object::New(isolate_);
    v8::Local<v8::Array> array = v8::Array::New(isolate_);

    PP_Var var_result;

    // Array <-> dictionary cycle.
    std::string key = "1";
    object
        ->Set(context,
              v8::String::NewFromUtf8(isolate_, key.c_str(),
                                      v8::NewStringType::kInternalized,
                                      key.length())
                  .ToLocalChecked(),
              array)
        .ToChecked();
    array->Set(context, 0, object).ToChecked();

    ASSERT_FALSE(FromV8ValueSync(object, context, &var_result));

    // Array with self reference.
    array->Set(context, 0, array).Check();
    ASSERT_FALSE(FromV8ValueSync(array, context, &var_result));
  }
}

TEST_F(V8VarConverterTest, StrangeDictionaryKeyTest) {
  {
    // Test keys with '.'.
    scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
    dictionary->SetWithStringKey(".", PP_MakeUndefined());
    dictionary->SetWithStringKey("x.y", PP_MakeUndefined());
    EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));
  }

  {
    // Test non-string key types. They should be cast to strings.
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks(context,
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);

    const char* source =
        "(function() {"
        "return {"
        "1: 'foo',"
        "'2': 'bar',"
        "true: 'baz',"
        "false: 'qux',"
        "null: 'quux',"
        "undefined: 'oops'"
        "};"
        "})();";

    v8::Local<v8::Script> script(
        v8::Script::Compile(
            context, v8::String::NewFromUtf8(isolate_, source).ToLocalChecked())
            .ToLocalChecked());
    v8::Local<v8::Object> object =
        script->Run(context).ToLocalChecked().As<v8::Object>();

    PP_Var actual;
    ASSERT_TRUE(FromV8ValueSync(
        object, v8::Local<v8::Context>::New(isolate_, context_), &actual));
    ScopedPPVar release_actual(ScopedPPVar::PassRef(), actual);

    scoped_refptr<DictionaryVar> expected(new DictionaryVar);
    ScopedPPVar foo(ScopedPPVar::PassRef(), StringVar::StringToPPVar("foo"));
    expected->SetWithStringKey("1", foo.get());
    ScopedPPVar bar(ScopedPPVar::PassRef(), StringVar::StringToPPVar("bar"));
    expected->SetWithStringKey("2", bar.get());
    ScopedPPVar baz(ScopedPPVar::PassRef(), StringVar::StringToPPVar("baz"));
    expected->SetWithStringKey("true", baz.get());
    ScopedPPVar qux(ScopedPPVar::PassRef(), StringVar::StringToPPVar("qux"));
    expected->SetWithStringKey("false", qux.get());
    ScopedPPVar quux(ScopedPPVar::PassRef(), StringVar::StringToPPVar("quux"));
    expected->SetWithStringKey("null", quux.get());
    ScopedPPVar oops(ScopedPPVar::PassRef(), StringVar::StringToPPVar("oops"));
    expected->SetWithStringKey("undefined", oops.get());
    ScopedPPVar release_expected(ScopedPPVar::PassRef(), expected->GetPPVar());

    ASSERT_TRUE(TestEqual(release_expected.get(), release_actual.get(), true));
  }
}

}  // namespace content
