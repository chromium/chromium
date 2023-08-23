// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/v8_value_converter_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-date.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-regexp.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-typed-array.h"

namespace content {

// To improve the performance of
// V8ValueConverterImpl::UpdateAndCheckUniqueness, identity hashes of objects
// are used during checking for duplicates. For testing purposes we need to
// ignore the hash sometimes. Create this helper object to avoid using identity
// hashes for the lifetime of the helper.
class ScopedAvoidIdentityHashForTesting {
 public:
  // The hashes will be ignored in |converter|, which must not be NULL and it
  // must outlive the created instance of this helper.
  explicit ScopedAvoidIdentityHashForTesting(
      content::V8ValueConverterImpl* converter);

  ScopedAvoidIdentityHashForTesting(const ScopedAvoidIdentityHashForTesting&) =
      delete;
  ScopedAvoidIdentityHashForTesting& operator=(
      const ScopedAvoidIdentityHashForTesting&) = delete;

  ~ScopedAvoidIdentityHashForTesting();

 private:
  raw_ptr<content::V8ValueConverterImpl> converter_;
};

ScopedAvoidIdentityHashForTesting::ScopedAvoidIdentityHashForTesting(
    content::V8ValueConverterImpl* converter)
    : converter_(converter) {
  CHECK(converter_);
  converter_->avoid_identity_hash_for_testing_ = true;
}

ScopedAvoidIdentityHashForTesting::~ScopedAvoidIdentityHashForTesting() {
  converter_->avoid_identity_hash_for_testing_ = false;
}

class V8ValueConverterImplTest : public testing::Test {
 public:
  V8ValueConverterImplTest()
      : isolate_holder_(task_environment_.GetMainThreadTaskRunner(),
                        gin::IsolateHolder::IsolateType::kTest),
        isolate_scope_(isolate_holder_.isolate()),
        isolate_(isolate_holder_.isolate()) {}

 protected:
  void SetUp() override {
    v8::HandleScope handle_scope(isolate_);
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate_);
    context_.Reset(isolate_, v8::Context::New(isolate_, nullptr, global));
  }

  void TearDown() override { context_.Reset(); }

  std::string GetString(base::Value::Dict* value, const std::string& key) {
    std::string* temp = value->FindString(key);
    if (!temp) {
      ADD_FAILURE();
      return std::string();
    }
    return *temp;
  }

  std::string GetString(v8::Local<v8::Object> value, const std::string& key) {
    v8::Local<v8::Value> temp;
    if (!value
             ->Get(isolate_->GetCurrentContext(),
                   v8::String::NewFromUtf8(isolate_, key.c_str(),
                                           v8::NewStringType::kInternalized)
                       .ToLocalChecked())
             .ToLocal(&temp)) {
      ADD_FAILURE();
      return std::string();
    }
    v8::String::Utf8Value utf8(isolate_, temp.As<v8::String>());
    return std::string(*utf8, utf8.length());
  }

  std::string GetString(const base::Value::List& value_list, uint32_t index) {
    if (index >= value_list.size() || !value_list[index].is_string()) {
      ADD_FAILURE();
      return std::string();
    }
    return value_list[index].GetString();
  }

  std::string GetString(v8::Local<v8::Array> value, uint32_t index) {
    v8::Local<v8::Value> temp;
    if (!value->Get(isolate_->GetCurrentContext(), index).ToLocal(&temp)) {
      ADD_FAILURE();
      return std::string();
    }
    v8::String::Utf8Value utf8(isolate_, temp.As<v8::String>());
    return std::string(*utf8, utf8.length());
  }

  int32_t GetInt(v8::Local<v8::Object> value, const std::string& key) {
    v8::Local<v8::Value> temp;
    if (!value
             ->Get(isolate_->GetCurrentContext(),
                   v8::String::NewFromUtf8(isolate_, key.c_str(),
                                           v8::NewStringType::kInternalized)
                       .ToLocalChecked())
             .ToLocal(&temp)) {
      ADD_FAILURE();
      return -1;
    }
    return temp.As<v8::Int32>()->Value();
  }

  int32_t GetInt(v8::Local<v8::Object> value, uint32_t index) {
    v8::Local<v8::Value> temp;
    if (!value->Get(isolate_->GetCurrentContext(), index).ToLocal(&temp)) {
      ADD_FAILURE();
      return -1;
    }
    return temp.As<v8::Int32>()->Value();
  }

  bool IsNull(base::Value::Dict* value, const std::string& key) {
    base::Value* child = value->Find(key);
    if (!child) {
      ADD_FAILURE();
      return false;
    }
    return child->type() == base::Value::Type::NONE;
  }

  bool IsNull(v8::Local<v8::Object> value, const std::string& key) {
    v8::Local<v8::Value> child;
    if (!value
             ->Get(isolate_->GetCurrentContext(),
                   v8::String::NewFromUtf8(isolate_, key.c_str(),
                                           v8::NewStringType::kInternalized)
                       .ToLocalChecked())
             .ToLocal(&child)) {
      ADD_FAILURE();
      return false;
    }
    return child->IsNull();
  }

  bool IsNull(const base::Value::List& list, uint32_t index) {
    if (list.size() <= index) {
      ADD_FAILURE();
      return false;
    }
    return list[index].type() == base::Value::Type::NONE;
  }

  bool IsNull(v8::Local<v8::Array> value, uint32_t index) {
    v8::Local<v8::Value> child;
    if (!value->Get(isolate_->GetCurrentContext(), index).ToLocal(&child)) {
      ADD_FAILURE();
      return false;
    }
    return child->IsNull();
  }

  void TestWeirdType(V8ValueConverterImpl& converter,
                     v8::Local<v8::Value> val,
                     base::Value::Type expected_type,
                     std::unique_ptr<base::Value> expected_value) {
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    std::unique_ptr<base::Value> raw(converter.FromV8Value(val, context));

    if (expected_value) {
      ASSERT_TRUE(raw.get());
      EXPECT_TRUE(*expected_value == *raw);
      EXPECT_EQ(expected_type, raw->type());
    } else {
      EXPECT_FALSE(raw.get());
    }

    v8::Local<v8::Object> object(v8::Object::New(isolate_));
    object
        ->Set(context,
              v8::String::NewFromUtf8(isolate_, "test",
                                      v8::NewStringType::kInternalized)
                  .ToLocalChecked(),
              val)
        .Check();
    std::unique_ptr<base::Value> dictionary_val(
        converter.FromV8Value(object, context));
    ASSERT_TRUE(dictionary_val.get());
    ASSERT_TRUE(dictionary_val->is_dict());
    const base::Value::Dict& dictionary = dictionary_val->GetDict();

    if (expected_value) {
      const base::Value* temp = dictionary.Find("test");
      ASSERT_TRUE(temp);
      EXPECT_EQ(expected_type, temp->type());
      EXPECT_EQ(*expected_value, *temp);
    } else {
      EXPECT_FALSE(dictionary.contains("test"));
    }

    v8::Local<v8::Array> array(v8::Array::New(isolate_));
    array->Set(context, 0, val).Check();
    std::unique_ptr<base::Value> list(converter.FromV8Value(array, context));
    ASSERT_TRUE(list.get());
    ASSERT_TRUE(list->is_list());
    if (expected_value) {
      ASSERT_FALSE(list->GetList().empty());
      const base::Value& temp = list->GetList()[0];
      EXPECT_EQ(expected_type, temp.type());
      EXPECT_EQ(*expected_value, temp);
    } else {
      // Arrays should preserve their length, and convert unconvertible
      // types into null.
      ASSERT_FALSE(list->GetList().empty());
      const base::Value& temp = list->GetList()[0];
      EXPECT_EQ(base::Value::Type::NONE, temp.type());
    }
  }

  template <typename T>
  v8::Local<T> CompileRun(v8::Local<v8::Context> context, const char* source) {
    return v8::Script::Compile(
               context,
               v8::String::NewFromUtf8(isolate_, source).ToLocalChecked())
        .ToLocalChecked()
        ->Run(context)
        .ToLocalChecked()
        .As<T>();
  }

  base::test::TaskEnvironment task_environment_;
  gin::IsolateHolder isolate_holder_;
  v8::Isolate::Scope isolate_scope_;
  raw_ptr<v8::Isolate> isolate_ = nullptr;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

TEST_F(V8ValueConverterImplTest, BasicRoundTrip) {
  const base::Value original_root = base::test::ParseJson(
      "{ \n"
      "  \"null\": null, \n"
      "  \"true\": true, \n"
      "  \"false\": false, \n"
      "  \"positive-int\": 42, \n"
      "  \"negative-int\": -42, \n"
      "  \"zero\": 0, \n"
      "  \"double\": 88.8, \n"
      "  \"big-integral-double\": 9007199254740992.0, \n"  // 2.0^53
      "  \"string\": \"foobar\", \n"
      "  \"empty-string\": \"\", \n"
      "  \"dictionary\": { \n"
      "    \"foo\": \"bar\",\n"
      "    \"hot\": \"dog\",\n"
      "  }, \n"
      "  \"empty-dictionary\": {}, \n"
      "  \"list\": [ \"bar\", \"foo\" ], \n"
      "  \"empty-list\": [], \n"
      "}");
  ASSERT_TRUE(original_root.is_dict());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  V8ValueConverterImpl converter;
  v8::Local<v8::Object> v8_object =
      converter.ToV8Value(original_root, context).As<v8::Object>();
  ASSERT_FALSE(v8_object.IsEmpty());

  EXPECT_EQ(original_root.GetDict().size(),
            v8_object->GetPropertyNames(context).ToLocalChecked()->Length());
  EXPECT_TRUE(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "null", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked()
          ->IsNull());
  EXPECT_TRUE(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "true", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked()
          ->IsTrue());
  EXPECT_TRUE(v8_object
                  ->Get(context,
                        v8::String::NewFromUtf8(
                            isolate_, "false", v8::NewStringType::kInternalized)
                            .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsFalse());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "positive-int",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsInt32());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "negative-int",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsInt32());
  EXPECT_TRUE(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "zero", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked()
          ->IsInt32());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "double",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsNumber());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "big-integral-double",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsNumber());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "string",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsString());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "empty-string",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsString());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "dictionary",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsObject());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "empty-dictionary",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsObject());
  EXPECT_TRUE(
      v8_object
          ->Get(context, v8::String::NewFromUtf8(
                             isolate_, "list", v8::NewStringType::kInternalized)
                             .ToLocalChecked())
          .ToLocalChecked()
          ->IsArray());
  EXPECT_TRUE(v8_object
                  ->Get(context, v8::String::NewFromUtf8(
                                     isolate_, "empty-list",
                                     v8::NewStringType::kInternalized)
                                     .ToLocalChecked())
                  .ToLocalChecked()
                  ->IsArray());

  std::unique_ptr<base::Value> new_root(
      converter.FromV8Value(v8_object, context));
  EXPECT_EQ(original_root, *new_root);
}

TEST_F(V8ValueConverterImplTest, KeysWithDots) {
  const base::Value original =
      base::test::ParseJson("{ \"foo.bar\": \"baz\" }");
  ASSERT_TRUE(original.is_dict());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> copy(
      converter.FromV8Value(converter.ToV8Value(original, context), context));

  EXPECT_EQ(original, *copy);
}

TEST_F(V8ValueConverterImplTest, ObjectExceptions) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  // Set up objects to throw when reading or writing 'foo'.
  const char source[] =
      "Object.prototype.__defineSetter__('foo', "
      "    function() { throw new Error('muah!'); });"
      "Object.prototype.__defineGetter__('foo', "
      "    function() { throw new Error('muah!'); });";

  CompileRun<v8::Value>(context, source);

  v8::Local<v8::Object> object(v8::Object::New(isolate_));
  v8::Local<v8::String> bar =
      v8::String::NewFromUtf8(isolate_, "bar", v8::NewStringType::kInternalized)
          .ToLocalChecked();
  object->Set(context, bar, bar).Check();

  // Converting from v8 value should replace the foo property with null.
  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> base_value =
      converter.FromV8Value(object, context);
  base::Value::Dict* converted = base_value->GetIfDict();
  ASSERT_TRUE(converted);

  // http://code.google.com/p/v8/issues/detail?id=1342
  // EXPECT_EQ(2u, converted);
  // EXPECT_TRUE(IsNull(*converted, "foo"));
  EXPECT_EQ(1u, converted->size());
  EXPECT_EQ("bar", GetString(converted, "bar"));

  // Converting to v8 value should not trigger the setter.
  converted->Set("foo", "foo");
  v8::Local<v8::Object> copy =
      converter.ToV8Value(*converted, context).As<v8::Object>();
  EXPECT_FALSE(copy.IsEmpty());
  EXPECT_EQ(2u, copy->GetPropertyNames(context).ToLocalChecked()->Length());
  EXPECT_EQ("foo", GetString(copy, "foo"));
  EXPECT_EQ("bar", GetString(copy, "bar"));
}

TEST_F(V8ValueConverterImplTest, ArrayExceptions) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "(function() {"
      "var arr = [];"
      "arr.__defineSetter__(0, "
      "    function() { throw new Error('muah!'); });"
      "arr.__defineGetter__(0, "
      "    function() { throw new Error('muah!'); });"
      "arr[1] = 'bar';"
      "return arr;"
      "})();";

  v8::Local<v8::Array> array = CompileRun<v8::Array>(context, source);

  // Converting from v8 value should replace the first item with null.
  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> converted =
      converter.FromV8Value(array, context);
  ASSERT_TRUE(converted);
  ASSERT_TRUE(converted->is_list());
  // http://code.google.com/p/v8/issues/detail?id=1342
  EXPECT_EQ(2u, converted->GetList().size());
  EXPECT_TRUE(IsNull(converted->GetList(), 0));

  // Converting to v8 value should not be affected by the getter/setter
  // because the setters/getters are defined on the array instance, not
  // on the Array's prototype.
  *converted = base::test::ParseJson("[ \"foo\", \"bar\" ]");
  v8::Local<v8::Array> copy =
      converter.ToV8Value(*converted, context).As<v8::Array>();
  ASSERT_FALSE(copy.IsEmpty());
  EXPECT_EQ(2u, copy->Length());
  EXPECT_EQ("foo", GetString(copy, 0));
  EXPECT_EQ("bar", GetString(copy, 1));
}

TEST_F(V8ValueConverterImplTest, WeirdTypes) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::RegExp> regex(
      v8::RegExp::New(context, v8::String::NewFromUtf8Literal(isolate_, "."),
                      v8::RegExp::kNone)
          .ToLocalChecked());

  V8ValueConverterImpl converter;
  TestWeirdType(converter, v8::Undefined(isolate_),
                base::Value::Type::NONE,  // Arbitrary type, result is NULL.
                std::unique_ptr<base::Value>());
  TestWeirdType(converter, v8::Date::New(context, 1000).ToLocalChecked(),
                base::Value::Type::DICT,
                std::make_unique<base::Value>(base::Value::Type::DICT));
  TestWeirdType(converter, regex, base::Value::Type::DICT,
                std::make_unique<base::Value>(base::Value::Type::DICT));

  converter.SetDateAllowed(true);
  TestWeirdType(converter, v8::Date::New(context, 1000).ToLocalChecked(),
                base::Value::Type::DOUBLE, std::make_unique<base::Value>(1.0));

  converter.SetRegExpAllowed(true);
  TestWeirdType(converter, regex, base::Value::Type::STRING,
                std::make_unique<base::Value>("/./"));
}

TEST_F(V8ValueConverterImplTest, Prototype) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "(function() {"
      "Object.prototype.foo = 'foo';"
      "return {};"
      "})();";

  v8::Local<v8::Object> object = CompileRun<v8::Object>(context, source);

  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> base_value =
      converter.FromV8Value(object, context);
  base::Value::Dict* result = base_value->GetIfDict();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

TEST_F(V8ValueConverterImplTest, ObjectPrototypeSetter) {
  const base::Value original =
      base::test::ParseJson("{ \"foo\": \"good value\" }");
  ASSERT_TRUE(original.is_dict());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "var result = { getters: 0, setters: 0 };"
      "Object.defineProperty(Object.prototype, 'foo', {"
      "  get() { ++result.getters; return 'bogus'; },"
      "  set() { ++result.setters; },"
      "});"
      "result;";

  const char source_sanity[] =
      "({}).foo = 'Trigger setter';"
      "({}).foo;";

  v8::Local<v8::Object> result = CompileRun<v8::Object>(context, source);

  // Sanity checks: the getters/setters are normally triggered.
  CompileRun<v8::Value>(context, source_sanity);
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  V8ValueConverterImpl converter;
  v8::Local<v8::Object> converted =
      converter.ToV8Value(original, context).As<v8::Object>();
  EXPECT_FALSE(converted.IsEmpty());

  // Getters/setters shouldn't be triggered.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  EXPECT_EQ(1u,
            converted->GetPropertyNames(context).ToLocalChecked()->Length());
  EXPECT_EQ("good value", GetString(converted, "foo"));

  // Getters/setters shouldn't be triggered while accessing existing values.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  // Repeat the same exercise with a dictionary without the key.
  base::Value::Dict missing_key_dict;
  missing_key_dict.Set("otherkey", "hello");
  v8::Local<v8::Object> converted2 =
      converter.ToV8Value(missing_key_dict, context).As<v8::Object>();
  EXPECT_FALSE(converted2.IsEmpty());

  // Getters/setters shouldn't be triggered.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  EXPECT_EQ(1u,
            converted2->GetPropertyNames(context).ToLocalChecked()->Length());
  EXPECT_EQ("hello", GetString(converted2, "otherkey"));

  // Missing key = should trigger getter upon access.
  EXPECT_EQ("bogus", GetString(converted2, "foo"));
  EXPECT_EQ(2, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));
}

TEST_F(V8ValueConverterImplTest, ArrayPrototypeSetter) {
  const base::Value original = base::test::ParseJson("[100, 200, 300]");
  ASSERT_TRUE(original.is_list());

  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "var result = { getters: 0, setters: 0 };"
      "Object.defineProperty(Array.prototype, '1', {"
      "  get() { ++result.getters; return 1337; },"
      "  set() { ++result.setters; },"
      "});"
      "result;";

  const char source_sanity[] =
      "[][1] = 'Trigger setter';"
      "[][1];";

  v8::Local<v8::Object> result = CompileRun<v8::Object>(context, source);

  // Sanity checks: the getters/setters are normally triggered.
  CompileRun<v8::Value>(context, source_sanity);
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  V8ValueConverterImpl converter;
  v8::Local<v8::Array> converted =
      converter.ToV8Value(original, context).As<v8::Array>();
  EXPECT_FALSE(converted.IsEmpty());

  // Getters/setters shouldn't be triggered during the conversion.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  EXPECT_EQ(3u, converted->Length());
  EXPECT_EQ(100, GetInt(converted, 0));
  EXPECT_EQ(200, GetInt(converted, 1));
  EXPECT_EQ(300, GetInt(converted, 2));

  // Getters/setters shouldn't be triggered while accessing existing values.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  // Try again, using an array without the index.
  base::Value::List one_item_list;
  one_item_list.Append(123456);
  v8::Local<v8::Array> converted2 =
      converter.ToV8Value(one_item_list, context).As<v8::Array>();
  EXPECT_FALSE(converted2.IsEmpty());

  // Getters/setters shouldn't be triggered during the conversion.
  EXPECT_EQ(1, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));

  EXPECT_EQ(1u, converted2->Length());
  EXPECT_EQ(123456, GetInt(converted2, 0));

  // Accessing missing index 1 triggers the getter.
  EXPECT_EQ(1337, GetInt(converted2, 1));
  EXPECT_EQ(2, GetInt(result, "getters"));
  EXPECT_EQ(1, GetInt(result, "setters"));
}

TEST_F(V8ValueConverterImplTest, StripNullFromObjects) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "(function() {"
      "return { foo: undefined, bar: null };"
      "})();";

  v8::Local<v8::Object> object = CompileRun<v8::Object>(context, source);

  V8ValueConverterImpl converter;
  converter.SetStripNullFromObjects(true);
  std::unique_ptr<base::Value> base_value =
      converter.FromV8Value(object, context);
  base::Value::Dict* result = base_value->GetIfDict();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

TEST_F(V8ValueConverterImplTest, RecursiveObjects) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  V8ValueConverterImpl converter;

  v8::Local<v8::Object> object = v8::Object::New(isolate_).As<v8::Object>();
  ASSERT_FALSE(object.IsEmpty());
  object
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "foo",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            v8::String::NewFromUtf8Literal(isolate_, "bar"))
      .Check();
  object
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, "obj",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            object)
      .Check();

  std::unique_ptr<base::Value> base_value =
      converter.FromV8Value(object, context);
  base::Value::Dict* object_result = base_value->GetIfDict();
  ASSERT_TRUE(object_result);
  EXPECT_EQ(2u, object_result->size());
  EXPECT_TRUE(IsNull(object_result, "obj"));

  v8::Local<v8::Array> array = v8::Array::New(isolate_).As<v8::Array>();
  ASSERT_FALSE(array.IsEmpty());
  array->Set(context, 0, v8::String::NewFromUtf8Literal(isolate_, "1")).Check();
  array->Set(context, 1, array).Check();

  base::Value::List list_result(
      std::move(*converter.FromV8Value(array, context)).TakeList());
  EXPECT_EQ(2u, list_result.size());
  EXPECT_TRUE(IsNull(list_result, 1));
}

TEST_F(V8ValueConverterImplTest, WeirdProperties) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "(function() {"
      "return {"
      "  1: 'foo',"
      "  '2': 'bar',"
      "  true: 'baz',"
      "  false: 'qux',"
      "  null: 'quux',"
      "  undefined: 'oops'"
      "};"
      "})();";

  v8::Local<v8::Object> object = CompileRun<v8::Object>(context, source);

  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> actual(converter.FromV8Value(object, context));

  const base::Value expected = base::test::ParseJson(
      "{ \n"
      "  \"1\": \"foo\", \n"
      "  \"2\": \"bar\", \n"
      "  \"true\": \"baz\", \n"
      "  \"false\": \"qux\", \n"
      "  \"null\": \"quux\", \n"
      "  \"undefined\": \"oops\", \n"
      "}");

  EXPECT_EQ(expected, *actual);
}

TEST_F(V8ValueConverterImplTest, ArrayGetters) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  const char source[] =
      "(function() {"
      "var a = [0];"
      "a.__defineGetter__(1, function() { return 'bar'; });"
      "return a;"
      "})();";

  v8::Local<v8::Array> array = CompileRun<v8::Array>(context, source);

  V8ValueConverterImpl converter;
  base::Value::List result(
      std::move(*converter.FromV8Value(array, context)).TakeList());
  EXPECT_EQ(2u, result.size());
}

TEST_F(V8ValueConverterImplTest, UndefinedValueBehavior) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Local<v8::Object> object;
  {
    const char source[] =
        "(function() {"
        "return { foo: undefined, bar: null, baz: function(){} };"
        "})();";
    object = CompileRun<v8::Object>(context, source);
  }

  v8::Local<v8::Array> array;
  {
    const char source[] =
        "(function() {"
        "return [ undefined, null, function(){} ];"
        "})();";
    array = CompileRun<v8::Array>(context, source);
  }

  v8::Local<v8::Array> sparse_array;
  {
    const char source[] =
        "(function() {"
        "return new Array(3);"
        "})();";
    sparse_array = CompileRun<v8::Array>(context, source);
  }

  V8ValueConverterImpl converter;

  std::unique_ptr<base::Value> actual_object(
      converter.FromV8Value(object, context));
  EXPECT_EQ(base::test::ParseJson("{ \"bar\": null }"), *actual_object);

  // Everything is null because JSON stringification preserves array length.
  std::unique_ptr<base::Value> actual_array(
      converter.FromV8Value(array, context));
  EXPECT_EQ(base::test::ParseJson("[ null, null, null ]"), *actual_array);

  std::unique_ptr<base::Value> actual_sparse_array(
      converter.FromV8Value(sparse_array, context));
  EXPECT_EQ(base::test::ParseJson("[ null, null, null ]"),
            *actual_sparse_array);
}

TEST_F(V8ValueConverterImplTest, ObjectsWithClashingIdentityHash) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8ValueConverterImpl converter;

  // We check that the converter checks identity correctly by disabling the
  // optimization of using identity hashes.
  ScopedAvoidIdentityHashForTesting scoped_hash_avoider(&converter);

  // Create the v8::Object to be converted.
  v8::Local<v8::Array> root(v8::Array::New(isolate_, 4));
  root->Set(context, 0, v8::Local<v8::Object>(v8::Object::New(isolate_)))
      .Check();
  root->Set(context, 1, v8::Local<v8::Object>(v8::Object::New(isolate_)))
      .Check();
  root->Set(context, 2, v8::Local<v8::Object>(v8::Array::New(isolate_, 0)))
      .Check();
  root->Set(context, 3, v8::Local<v8::Object>(v8::Array::New(isolate_, 0)))
      .Check();

  // The expected base::Value result.
  const base::Value expected = base::test::ParseJson("[{},{},[],[]]");
  ASSERT_TRUE(expected.is_list());

  // The actual result.
  std::unique_ptr<base::Value> value(converter.FromV8Value(root, context));
  ASSERT_TRUE(value);

  EXPECT_EQ(expected, *value);
}

TEST_F(V8ValueConverterImplTest, DetectCycles) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8ValueConverterImpl converter;

  // Create a recursive array.
  v8::Local<v8::Array> recursive_array(v8::Array::New(isolate_, 1));
  recursive_array->Set(context, 0, recursive_array).Check();

  // The first repetition should be trimmed and replaced by a null value.
  base::Value::List expected_list;
  expected_list.Append(base::Value());

  // The actual result.
  std::unique_ptr<base::Value> actual_list(
      converter.FromV8Value(recursive_array, context));
  ASSERT_TRUE(actual_list.get());

  EXPECT_TRUE(expected_list == actual_list->GetList());

  // Now create a recursive object
  const std::string key("key");
  v8::Local<v8::Object> recursive_object(v8::Object::New(isolate_));
  recursive_object
      ->Set(context,
            v8::String::NewFromUtf8(isolate_, key.c_str(),
                                    v8::NewStringType::kInternalized,
                                    key.length())
                .ToLocalChecked(),
            recursive_object)
      .Check();

  // The first repetition should be trimmed and replaced by a null value.
  base::Value::Dict expected_dictionary;
  expected_dictionary.Set(key, base::Value());

  // The actual result.
  std::unique_ptr<base::Value> base_value =
      converter.FromV8Value(recursive_object, context);
  base::Value::Dict* actual_dictionary = base_value->GetIfDict();
  ASSERT_TRUE(actual_dictionary);
  EXPECT_EQ(expected_dictionary, *actual_dictionary);
}

// Tests that reused object values with no cycles do not get nullified.
TEST_F(V8ValueConverterImplTest, ReuseObjects) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8ValueConverterImpl converter;

  // Object with reused values in different keys.
  {
    const char source[] =
        "(function() {"
        "var objA = {key: 'another same value'};"
        "var obj = {one: objA, two: objA};"
        "return obj;"
        "})();";
    v8::Local<v8::Object> object = CompileRun<v8::Object>(context, source);

    // The actual result.
    std::unique_ptr<base::Value> base_value =
        converter.FromV8Value(object, context);
    base::Value::Dict* result = base_value->GetIfDict();
    ASSERT_TRUE(result);
    EXPECT_EQ(2u, result->size());

    {
      const char key1[] = "one";
      base::Value::Dict* one_dict = result->FindDict(key1);
      ASSERT_TRUE(one_dict);
      EXPECT_EQ("another same value", GetString(one_dict, "key"));
    }
    {
      const char key2[] = "two";
      base::Value::Dict* two_dict = result->FindDict(key2);
      ASSERT_TRUE(two_dict);
      EXPECT_EQ("another same value", GetString(two_dict, "key"));
    }
  }

  // Array with reused values.
  {
    const char source[] =
        "(function() {"
        "var objA = {key: 'same value'};"
        "var arr = [objA, objA];"
        "return arr;"
        "})();";
    v8::Local<v8::Array> array = CompileRun<v8::Array>(context, source);

    // The actual result.
    base::Value::List list_result(
        std::move(*converter.FromV8Value(array, context)).TakeList());
    ASSERT_EQ(2u, list_result.size());
    for (size_t i = 0; i < list_result.size(); ++i) {
      ASSERT_FALSE(IsNull(list_result, i));
      base::Value::Dict* dict_value = list_result[0].GetIfDict();
      ASSERT_TRUE(dict_value);
      EXPECT_STREQ("same value", dict_value->FindString("key")->c_str());
    }
  }
}

TEST_F(V8ValueConverterImplTest, MaxRecursionDepth) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  // Must larger than kMaxRecursionDepth in v8_value_converter_impl.cc.
  int kDepth = 1000;
  const char kKey[] = "key";

  v8::Local<v8::Object> deep_object = v8::Object::New(isolate_);

  v8::Local<v8::Object> leaf = deep_object;
  for (int i = 0; i < kDepth; ++i) {
    v8::Local<v8::Object> new_object = v8::Object::New(isolate_);
    leaf->Set(context,
              v8::String::NewFromUtf8(isolate_, kKey,
                                      v8::NewStringType::kInternalized)
                  .ToLocalChecked(),
              new_object)
        .Check();
    leaf = new_object;
  }

  V8ValueConverterImpl converter;
  std::unique_ptr<base::Value> value(
      converter.FromV8Value(deep_object, context));
  ASSERT_TRUE(value);

  // Expected depth is kMaxRecursionDepth in v8_value_converter_impl.cc.
  int kExpectedDepth = 100;

  const base::Value* current = value.get();
  for (int i = 1; i < kExpectedDepth; ++i) {
    ASSERT_TRUE(current->is_dict()) << i;
    const base::Value::Dict& current_as_object = current->GetDict();
    current = current_as_object.Find(kKey);
    ASSERT_TRUE(current) << i;
  }

  // The leaf node shouldn't have any properties.
  EXPECT_EQ(base::Value(base::Value::Dict()), *current) << *current;
}

TEST_F(V8ValueConverterImplTest, NegativeZero) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::MicrotasksScope microtasks(context,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Context::Scope context_scope(context);
  const char source[] = "(function() { return -0; })();";

  v8::Local<v8::Value> value = CompileRun<v8::Value>(context, source);

  {
    V8ValueConverterImpl converter;
    std::unique_ptr<base::Value> result = converter.FromV8Value(value, context);
    ASSERT_TRUE(result->is_double())
        << base::Value::GetTypeName(result->type());
    EXPECT_EQ(0, result->GetDouble());
  }
  {
    V8ValueConverterImpl converter;
    converter.SetConvertNegativeZeroToInt(true);
    std::unique_ptr<base::Value> result = converter.FromV8Value(value, context);
    ASSERT_TRUE(result->is_int()) << base::Value::GetTypeName(result->type());
    EXPECT_EQ(0, result->GetInt());
  }
}

class V8ValueConverterOverridingStrategyForTesting
    : public V8ValueConverter::Strategy {
 public:
  V8ValueConverterOverridingStrategyForTesting()
      : reference_value_(NewReferenceValue()) {}
  bool FromV8Object(v8::Local<v8::Object> value,
                    std::unique_ptr<base::Value>* out,
                    v8::Isolate* isolate) override {
    *out = NewReferenceValue();
    return true;
  }
  bool FromV8Array(v8::Local<v8::Array> value,
                   std::unique_ptr<base::Value>* out,
                   v8::Isolate* isolate) override {
    *out = NewReferenceValue();
    return true;
  }
  bool FromV8ArrayBuffer(v8::Local<v8::Object> value,
                         std::unique_ptr<base::Value>* out,
                         v8::Isolate* isolate) override {
    *out = NewReferenceValue();
    return true;
  }
  bool FromV8Number(v8::Local<v8::Number> value,
                    std::unique_ptr<base::Value>* out) override {
    *out = NewReferenceValue();
    return true;
  }
  bool FromV8Undefined(std::unique_ptr<base::Value>* out) override {
    *out = NewReferenceValue();
    return true;
  }
  base::Value* reference_value() const { return reference_value_.get(); }

 private:
  static std::unique_ptr<base::Value> NewReferenceValue() {
    return std::make_unique<base::Value>("strategy");
  }
  std::unique_ptr<base::Value> reference_value_;
};

TEST_F(V8ValueConverterImplTest, StrategyOverrides) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  V8ValueConverterImpl converter;
  V8ValueConverterOverridingStrategyForTesting strategy;
  converter.SetStrategy(&strategy);

  v8::Local<v8::Object> object(v8::Object::New(isolate_));
  std::unique_ptr<base::Value> object_value(
      converter.FromV8Value(object, context));
  ASSERT_TRUE(object_value);
  EXPECT_EQ(*strategy.reference_value(), *object_value);

  v8::Local<v8::Array> array(v8::Array::New(isolate_));
  std::unique_ptr<base::Value> array_value(
      converter.FromV8Value(array, context));
  ASSERT_TRUE(array_value);
  EXPECT_EQ(*strategy.reference_value(), *array_value);

  v8::Local<v8::ArrayBuffer> array_buffer(v8::ArrayBuffer::New(isolate_, 0));
  std::unique_ptr<base::Value> array_buffer_value(
      converter.FromV8Value(array_buffer, context));
  ASSERT_TRUE(array_buffer_value);
  EXPECT_EQ(*strategy.reference_value(), *array_buffer_value);

  v8::Local<v8::ArrayBufferView> array_buffer_view(
      v8::Uint8Array::New(array_buffer, 0, 0));
  std::unique_ptr<base::Value> array_buffer_view_value(
      converter.FromV8Value(array_buffer_view, context));
  ASSERT_TRUE(array_buffer_view_value);
  EXPECT_EQ(*strategy.reference_value(), *array_buffer_view_value);

  v8::Local<v8::Number> number(v8::Number::New(isolate_, 0.0));
  std::unique_ptr<base::Value> number_value(
      converter.FromV8Value(number, context));
  ASSERT_TRUE(number_value);
  EXPECT_EQ(*strategy.reference_value(), *number_value);

  v8::Local<v8::Primitive> undefined(v8::Undefined(isolate_));
  std::unique_ptr<base::Value> undefined_value(
      converter.FromV8Value(undefined, context));
  ASSERT_TRUE(undefined_value);
  EXPECT_EQ(*strategy.reference_value(), *undefined_value);
}

class V8ValueConverterBypassStrategyForTesting
    : public V8ValueConverter::Strategy {
 public:
  bool FromV8Object(v8::Local<v8::Object> value,
                    std::unique_ptr<base::Value>* out,
                    v8::Isolate* isolate) override {
    return false;
  }
  bool FromV8Array(v8::Local<v8::Array> value,
                   std::unique_ptr<base::Value>* out,
                   v8::Isolate* isolate) override {
    return false;
  }
  bool FromV8ArrayBuffer(v8::Local<v8::Object> value,
                         std::unique_ptr<base::Value>* out,
                         v8::Isolate* isolate) override {
    return false;
  }
  bool FromV8Number(v8::Local<v8::Number> value,
                    std::unique_ptr<base::Value>* out) override {
    return false;
  }
  bool FromV8Undefined(std::unique_ptr<base::Value>* out) override {
    return false;
  }
};

// Verify that having a strategy that fallbacks to default behaviour
// actually preserves it.
TEST_F(V8ValueConverterImplTest, StrategyBypass) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);
  v8::Context::Scope context_scope(context);

  V8ValueConverterImpl converter;
  V8ValueConverterBypassStrategyForTesting strategy;
  converter.SetStrategy(&strategy);

  v8::Local<v8::Object> object(v8::Object::New(isolate_));
  std::unique_ptr<base::Value> object_value(
      converter.FromV8Value(object, context));
  ASSERT_TRUE(object_value);
  const base::Value reference_object_value = base::test::ParseJson("{}");
  EXPECT_EQ(reference_object_value, *object_value);

  v8::Local<v8::Array> array(v8::Array::New(isolate_));
  std::unique_ptr<base::Value> array_value(
      converter.FromV8Value(array, context));
  ASSERT_TRUE(array_value);
  const base::Value reference_array_value = base::test::ParseJson("[]");
  EXPECT_EQ(reference_array_value, *array_value);

  const char kExampleData[] = {1, 2, 3, 4, 5};
  v8::Local<v8::ArrayBuffer> array_buffer(
      v8::ArrayBuffer::New(isolate_, sizeof(kExampleData)));
  memcpy(array_buffer->GetBackingStore()->Data(), kExampleData,
         sizeof(kExampleData));
  std::unique_ptr<base::Value> binary_value(
      converter.FromV8Value(array_buffer, context));
  ASSERT_TRUE(binary_value);
  base::Value reference_binary_value(
      base::as_bytes(base::make_span(kExampleData)));
  EXPECT_EQ(reference_binary_value, *binary_value);

  v8::Local<v8::ArrayBufferView> array_buffer_view(
      v8::Uint8Array::New(array_buffer, 1, 3));
  std::unique_ptr<base::Value> binary_view_value(
      converter.FromV8Value(array_buffer_view, context));
  ASSERT_TRUE(binary_view_value);
  base::Value reference_binary_view_value(
      base::as_bytes(base::make_span(kExampleData).subspan(1, 3)));
  EXPECT_EQ(reference_binary_view_value, *binary_view_value);

  v8::Local<v8::Number> number(v8::Number::New(isolate_, 0.0));
  std::unique_ptr<base::Value> number_value(
      converter.FromV8Value(number, context));
  ASSERT_TRUE(number_value);
  const base::Value reference_number_value = base::test::ParseJson("0");
  EXPECT_EQ(reference_number_value, *number_value);

  v8::Local<v8::Primitive> undefined(v8::Undefined(isolate_));
  std::unique_ptr<base::Value> undefined_value(
      converter.FromV8Value(undefined, context));
  EXPECT_FALSE(undefined_value);
}

}  // namespace content
