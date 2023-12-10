// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/interceptor.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-util.h"

namespace gin {

class MyInterceptor : public Wrappable<MyInterceptor>,
                      public NamedPropertyInterceptor,
                      public IndexedPropertyInterceptor {
 public:
  MyInterceptor(const MyInterceptor&) = delete;
  MyInterceptor& operator=(const MyInterceptor&) = delete;

  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyInterceptor> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyInterceptor(isolate));
  }

  void Clear() {
    NamedPropertyInterceptor::ClearForTesting();
    IndexedPropertyInterceptor::ClearForTesting();
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override {
    if (property == "value") {
      return ConvertToV8(isolate, value_);
    } else if (property == "func") {
      v8::Local<v8::Context> context = isolate->GetCurrentContext();
      return GetFunctionTemplate(isolate, "func")
          ->GetFunction(context)
          .ToLocalChecked();
    } else {
      return v8::Local<v8::Value>();
    }
  }
  bool SetNamedProperty(v8::Isolate* isolate,
                        const std::string& property,
                        v8::Local<v8::Value> value) override {
    if (property == "value") {
      ConvertFromV8(isolate, value, &value_);
      return true;
    }
    return false;
  }
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override {
    std::vector<std::string> result;
    result.push_back("func");
    result.push_back("value");
    return result;
  }

  // gin::IndexedPropertyInterceptor
  v8::Local<v8::Value> GetIndexedProperty(v8::Isolate* isolate,
                                          uint32_t index) override {
    if (index == 0)
      return ConvertToV8(isolate, value_);
    return v8::Local<v8::Value>();
  }
  bool SetIndexedProperty(v8::Isolate* isolate,
                          uint32_t index,
                          v8::Local<v8::Value> value) override {
    if (index == 0) {
      ConvertFromV8(isolate, value, &value_);
      return true;
    }
    // Don't allow bypassing the interceptor.
    return true;
  }
  std::vector<uint32_t> EnumerateIndexedProperties(
      v8::Isolate* isolate) override {
    std::vector<uint32_t> result;
    result.push_back(0);
    return result;
  }

 private:
  explicit MyInterceptor(v8::Isolate* isolate)
      : NamedPropertyInterceptor(isolate, this),
        IndexedPropertyInterceptor(isolate, this),
        value_(0),
        template_cache_(isolate) {}
  ~MyInterceptor() override = default;

  // gin::Wrappable
  ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return Wrappable<MyInterceptor>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor()
        .AddIndexedPropertyInterceptor();
  }

  int Call(int value) {
    int tmp = value_;
    value_ = value;
    return tmp;
  }

  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(v8::Isolate* isolate,
                                                      const std::string& name) {
    v8::Local<v8::FunctionTemplate> function_template =
        template_cache_.Get(name);
    if (!function_template.IsEmpty())
      return function_template;
    function_template = CreateFunctionTemplate(
        isolate, base::BindRepeating(&MyInterceptor::Call),
        InvokerOptions{true, nullptr});
    template_cache_.Set(name, function_template);
    return function_template;
  }

  int value_;

  v8::StdGlobalValueMap<std::string, v8::FunctionTemplate> template_cache_;
};

WrapperInfo MyInterceptor::kWrapperInfo = {kEmbedderNativeGin};

class InterceptorTest : public V8Test {
 public:
  void RunInterceptorTest(const std::string& script_source) {
    v8::Isolate* isolate = instance_->isolate();
    v8::HandleScope handle_scope(isolate);

    gin::Handle<MyInterceptor> obj = MyInterceptor::Create(isolate);

    obj->set_value(42);
    EXPECT_EQ(42, obj->value());

    v8::Local<v8::String> source = StringToV8(isolate, script_source);
    EXPECT_FALSE(source.IsEmpty());

    gin::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script =
        v8::Script::Compile(context_.Get(isolate), source).ToLocalChecked();
    v8::Local<v8::Value> val =
        script->Run(context_.Get(isolate)).ToLocalChecked();
    EXPECT_FALSE(val.IsEmpty());
    v8::Local<v8::Function> func;
    EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
    v8::Local<v8::Value> argv[] = {
        ConvertToV8(isolate, obj.get()).ToLocalChecked(),
    };
    func->Call(context_.Get(isolate), v8::Undefined(isolate), 1, argv)
        .ToLocalChecked();
    EXPECT_FALSE(try_catch.HasCaught());
    EXPECT_EQ("", try_catch.GetStackTrace());

    EXPECT_EQ(191, obj->value());
    obj->Clear();
  }
};

TEST_F(InterceptorTest, NamedInterceptor) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
}

TEST_F(InterceptorTest, NamedInterceptorCall) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj.func(191) !== 42) throw 'FAIL';"
      "   })");
}

TEST_F(InterceptorTest, IndexedInterceptor) {
  RunInterceptorTest(
      "(function (obj) {"
      "   if (obj[0] !== 42) throw 'FAIL';"
      "   else obj[0] = 191; })");
}

TEST_F(InterceptorTest, BypassInterceptorAllowed) {
  RunInterceptorTest(
      "(function (obj) {"
      "   obj.value = 191 /* make test happy */;"
      "   obj.foo = 23;"
      "   if (obj.foo !== 23) throw 'FAIL'; })");
}

TEST_F(InterceptorTest, BypassInterceptorForbidden) {
  RunInterceptorTest(
      "(function (obj) {"
      "   obj.value = 191 /* make test happy */;"
      "   obj[1] = 23;"
      "   if (obj[1] === 23) throw 'FAIL'; })");
}

}  // namespace gin
