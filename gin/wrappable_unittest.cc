// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/check.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-script.h"

namespace gin {

namespace {

// A non-member function to be bound to an ObjectTemplateBuilder.
void NonMemberMethod() {}

// This useless base class ensures that the value of a pointer to a MyObject
// (below) is not the same as the value of that pointer cast to the object's
// WrappableBase base.
class BaseClass {
 public:
  BaseClass() = default;
  BaseClass(const BaseClass&) = delete;
  BaseClass& operator=(const BaseClass&) = delete;
  virtual ~BaseClass() = default;

  // So the compiler doesn't complain that |value_| is unused.
  int value() const { return value_; }

 private:
  int value_ = 23;
};

class MyObject : public Wrappable<MyObject>, public BaseClass {
 public:
  MyObject(const MyObject&) = delete;
  MyObject& operator=(const MyObject&) = delete;

  MyObject() = default;

  static MyObject* Create(v8::Isolate* isolate) {
    return cppgc::MakeGarbageCollected<MyObject>(
        isolate->GetCppHeap()->GetAllocationHandle());
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  void Method() {}

  static constexpr WrapperInfo kWrapperInfo = {{kEmbedderNativeGin},
                                               kTestObject};

  const WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }

  const char* GetHumanReadableName() const final { return "MyObject"; }

 protected:
  ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
    return Wrappable<MyObject>::GetObjectTemplateBuilder(isolate)
        .SetProperty("value", &MyObject::value, &MyObject::set_value)
        .SetMethod("memberMethod", &MyObject::Method)
        .SetMethod("nonMemberMethod", &NonMemberMethod);
  }

 private:
  int value_ = 0;
};

class MyObject2 : public Wrappable<MyObject2> {
 public:
  MyObject2() = default;

  static constexpr WrapperInfo kWrapperInfo = {{kEmbedderNativeGin},
                                               kTestObject2};

  const WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }

  const char* GetHumanReadableName() const final { return "MyObject2"; }

  static MyObject2* Create(v8::Isolate* isolate) {
    return cppgc::MakeGarbageCollected<MyObject2>(
        isolate->GetCppHeap()->GetAllocationHandle());
  }
};

class MyNamedObject : public Wrappable<MyNamedObject> {
 public:
  MyNamedObject(const MyNamedObject&) = delete;
  MyNamedObject& operator=(const MyNamedObject&) = delete;
  MyNamedObject() = default;

  static constexpr WrapperInfo kWrapperInfo = {{kEmbedderNativeGin},
                                               kTestObject2};

  const WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }

  static MyNamedObject* Create(v8::Isolate* isolate) {
    return cppgc::MakeGarbageCollected<MyNamedObject>(
        isolate->GetCppHeap()->GetAllocationHandle());
  }

  void Method() {}

 protected:
  ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
    return Wrappable<MyNamedObject>::GetObjectTemplateBuilder(isolate)
        .SetMethod("memberMethod", &MyNamedObject::Method)
        .SetMethod("nonMemberMethod", &NonMemberMethod);
  }
  const char* GetHumanReadableName() const final { return "MyNamedObject"; }
};

}  // namespace

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  MyObject* obj = MyObject::Create(isolate);

  v8::Local<v8::Value> wrapper = ConvertToV8(isolate, obj).ToLocalChecked();
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = nullptr;
  EXPECT_TRUE(ConvertFromV8(isolate, wrapper, &unwrapped));
  EXPECT_EQ(obj, unwrapped);
}

TEST_F(WrappableTest, UnwrapNull) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  MyObject* obj = nullptr;
  v8::Local<v8::Value> wrapper = ConvertToV8(isolate, obj).ToLocalChecked();
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = nullptr;
  ConvertFromV8(isolate, wrapper, &unwrapped);
  EXPECT_EQ(obj, unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  MyObject* obj = MyObject::Create(isolate);

  obj->set_value(42);
  EXPECT_EQ(42, obj->value());

  v8::Local<v8::String> source = StringToV8(isolate,
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
  EXPECT_FALSE(source.IsEmpty());

  gin::TryCatch try_catch(isolate);
  v8::Local<v8::Script> script =
      v8::Script::Compile(context_.Get(isolate), source).ToLocalChecked();
  v8::Local<v8::Value> val =
      script->Run(context_.Get(isolate)).ToLocalChecked();
  v8::Local<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Local<v8::Value> argv[] = {
      ConvertToV8(isolate, obj).ToLocalChecked(),
  };
  func->Call(context_.Get(isolate), v8::Undefined(isolate), 1, argv)
      .ToLocalChecked();
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ("", try_catch.GetStackTrace());

  EXPECT_EQ(191, obj->value());
}

TEST_F(WrappableTest, MethodInvocationErrorsOnUnnamedObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  MyObject* obj = MyObject::Create(isolate);

  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj).ToLocalChecked().As<v8::Object>();
  v8::Local<v8::Value> member_method =
      v8_object->Get(context, StringToV8(isolate, "memberMethod"))
          .ToLocalChecked();
  ASSERT_TRUE(member_method->IsFunction());
  v8::Local<v8::Value> non_member_method =
      v8_object->Get(context, StringToV8(isolate, "nonMemberMethod"))
          .ToLocalChecked();
  ASSERT_TRUE(non_member_method->IsFunction());

  auto get_error = [isolate, context](v8::Local<v8::Value> function_to_run,
                                      v8::Local<v8::Value> context_object) {
    constexpr char kScript[] =
        "(function(toRun, contextObject) { toRun.apply(contextObject, []); })";
    v8::Local<v8::String> source = StringToV8(isolate, kScript);
    EXPECT_FALSE(source.IsEmpty());

    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    v8::Local<v8::Value> val = script->Run(context).ToLocalChecked();
    v8::Local<v8::Function> func;
    EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
    v8::Local<v8::Value> argv[] = {function_to_run, context_object};
    func->Call(context, v8::Undefined(isolate), std::size(argv), argv)
        .FromMaybe(v8::Local<v8::Value>());
    if (!try_catch.HasCaught())
      return std::string();
    return V8ToString(isolate, try_catch.Message()->Get());
  };

  EXPECT_EQ(std::string(), get_error(member_method, v8_object));
  EXPECT_EQ(std::string(), get_error(non_member_method, v8_object));

  EXPECT_TRUE(get_error(member_method, v8::Null(isolate))
                  .starts_with("Uncaught TypeError: Illegal invocation"));
  // A non-member function shouldn't throw errors for being applied on a
  // null (or invalid) object.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));

  v8::Local<v8::Object> wrong_object = v8::Object::New(isolate);
  // We should get an error for passing the wrong object.
  EXPECT_TRUE(get_error(member_method, wrong_object)
                  .starts_with("Uncaught TypeError: Illegal invocation"));
  // But again, not for a "static" method.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));
}

TEST_F(WrappableTest, MethodInvocationErrorsOnNamedObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  MyNamedObject* obj = MyNamedObject::Create(isolate);

  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj).ToLocalChecked().As<v8::Object>();
  v8::Local<v8::Value> member_method =
      v8_object->Get(context, StringToV8(isolate, "memberMethod"))
          .ToLocalChecked();
  ASSERT_TRUE(member_method->IsFunction());
  v8::Local<v8::Value> non_member_method =
      v8_object->Get(context, StringToV8(isolate, "nonMemberMethod"))
          .ToLocalChecked();
  ASSERT_TRUE(non_member_method->IsFunction());

  auto get_error = [isolate, context](v8::Local<v8::Value> function_to_run,
                                      v8::Local<v8::Value> context_object) {
    constexpr char kScript[] =
        "(function(toRun, contextObject) { toRun.apply(contextObject, []); })";
    v8::Local<v8::String> source = StringToV8(isolate, kScript);
    EXPECT_FALSE(source.IsEmpty());

    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    v8::Local<v8::Value> val = script->Run(context).ToLocalChecked();
    v8::Local<v8::Function> func;
    EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
    v8::Local<v8::Value> argv[] = {function_to_run, context_object};
    func->Call(context, v8::Undefined(isolate), std::size(argv), argv)
        .FromMaybe(v8::Local<v8::Value>());
    if (!try_catch.HasCaught())
      return std::string();
    return V8ToString(isolate, try_catch.Message()->Get());
  };

  EXPECT_EQ(std::string(), get_error(member_method, v8_object));
  EXPECT_EQ(std::string(), get_error(non_member_method, v8_object));

  EXPECT_EQ(
      "Uncaught TypeError: Illegal invocation: Function must be called on "
      "an object of type MyNamedObject",
      get_error(member_method, v8::Null(isolate)));
  // A non-member function shouldn't throw errors for being applied on a
  // null (or invalid) object.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));

  v8::Local<v8::Object> wrong_object = v8::Object::New(isolate);
  // We should get an error for passing the wrong object.
  EXPECT_EQ(
      "Uncaught TypeError: Illegal invocation: Function must be called on "
      "an object of type MyNamedObject",
      get_error(member_method, wrong_object));
  // But again, not for a "static" method.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));
}

class MyObjectWithLazyProperties
    : public Wrappable<MyObjectWithLazyProperties> {
 public:
  MyObjectWithLazyProperties(const MyObjectWithLazyProperties&) = delete;
  MyObjectWithLazyProperties& operator=(const MyObjectWithLazyProperties&) =
      delete;
  MyObjectWithLazyProperties() = default;

  static constexpr WrapperInfo kWrapperInfo = {{kEmbedderNativeGin},
                                               kTestObject};

  const WrapperInfo* wrapper_info() const override { return &kWrapperInfo; }

  const char* GetHumanReadableName() const final {
    return "MyObjectWithLazyProperties";
  }

  static MyObjectWithLazyProperties* Create(v8::Isolate* isolate) {
    return cppgc::MakeGarbageCollected<MyObjectWithLazyProperties>(
        isolate->GetCppHeap()->GetAllocationHandle());
  }

  int access_count() const { return access_count_; }

 private:
  ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
    return Wrappable::GetObjectTemplateBuilder(isolate)
        .SetLazyDataProperty("fortyTwo", &MyObjectWithLazyProperties::FortyTwo)
        .SetLazyDataProperty("self",
                             base::BindRepeating([](gin::Arguments* arguments) {
                               v8::Local<v8::Value> holder;
                               CHECK(arguments->GetHolder(&holder));
                               return holder;
                             }));
  }

  int FortyTwo() {
    access_count_++;
    return 42;
  }

  int access_count_ = 0;
};

TEST_F(WrappableTest, LazyPropertyGetterIsCalledOnce) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  MyObjectWithLazyProperties* obj = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj).ToLocalChecked().As<v8::Object>();
  v8::Local<v8::String> key = StringToSymbol(isolate, "fortyTwo");
  v8::Local<v8::Value> value;

  bool has_own_property = false;
  ASSERT_TRUE(v8_object->HasOwnProperty(context, key).To(&has_own_property));
  EXPECT_TRUE(has_own_property);

  EXPECT_EQ(0, obj->access_count());

  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 42)));
  EXPECT_EQ(1, obj->access_count());

  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 42)));
  EXPECT_EQ(1, obj->access_count());
}

TEST_F(WrappableTest, LazyPropertyGetterCanBeSetFirst) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  auto* obj = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj).ToLocalChecked().As<v8::Object>();
  v8::Local<v8::String> key = StringToSymbol(isolate, "fortyTwo");
  v8::Local<v8::Value> value;

  EXPECT_EQ(0, obj->access_count());

  bool set_ok = false;
  ASSERT_TRUE(
      v8_object->Set(context, key, v8::Int32::New(isolate, 1701)).To(&set_ok));
  ASSERT_TRUE(set_ok);
  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 1701)));
  EXPECT_EQ(0, obj->access_count());
}

TEST_F(WrappableTest, LazyPropertyGetterCanBindSpecialArguments) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  auto* obj = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj).ToLocalChecked().As<v8::Object>();
  v8::Local<v8::Value> value;
  ASSERT_TRUE(
      v8_object->Get(context, StringToSymbol(isolate, "self")).ToLocal(&value));
  EXPECT_TRUE(v8_object == value);
}

TEST_F(WrappableTest, CannotConstruct) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  MyObject* obj = MyObject::Create(isolate);
  v8::Local<v8::Value> wrapper = ConvertToV8(isolate, obj).ToLocalChecked();
  ASSERT_FALSE(wrapper.IsEmpty());

  v8::Local<v8::String> source =
      StringToV8(isolate, "(obj => new obj.constructor())");
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source).ToLocalChecked();
  v8::Local<v8::Function> function =
      script->Run(context).ToLocalChecked().As<v8::Function>();

  {
    TryCatch try_catch(isolate);
    EXPECT_TRUE(function
                    ->Call(context, v8::Undefined(isolate), 1,
                           (v8::Local<v8::Value>[]){wrapper})
                    .IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
}

}  // namespace gin
