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
  BaseClass() : value_(23) {}
  BaseClass(const BaseClass&) = delete;
  BaseClass& operator=(const BaseClass&) = delete;
  virtual ~BaseClass() = default;

  // So the compiler doesn't complain that |value_| is unused.
  int value() const { return value_; }

 private:
  int value_;
};

class MyObject : public BaseClass,
                 public Wrappable<MyObject> {
 public:
  MyObject(const MyObject&) = delete;
  MyObject& operator=(const MyObject&) = delete;

  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObject());
  }

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  void Method() {}

 protected:
  MyObject() : value_(0) {}
  ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
    return Wrappable<MyObject>::GetObjectTemplateBuilder(isolate)
        .SetProperty("value", &MyObject::value, &MyObject::set_value)
        .SetMethod("memberMethod", &MyObject::Method)
        .SetMethod("nonMemberMethod", &NonMemberMethod);
  }
  ~MyObject() override = default;

 private:
  int value_;
};

class MyObject2 : public Wrappable<MyObject2> {
 public:
  static WrapperInfo kWrapperInfo;
};

class MyNamedObject : public Wrappable<MyNamedObject> {
 public:
  MyNamedObject(const MyNamedObject&) = delete;
  MyNamedObject& operator=(const MyNamedObject&) = delete;

  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyNamedObject> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyNamedObject());
  }

  void Method() {}

 protected:
  MyNamedObject() = default;
  ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate* isolate) final {
    return Wrappable<MyNamedObject>::GetObjectTemplateBuilder(isolate)
        .SetMethod("memberMethod", &MyNamedObject::Method)
        .SetMethod("nonMemberMethod", &NonMemberMethod);
  }
  const char* GetTypeName() final { return "MyNamedObject"; }
  ~MyNamedObject() override = default;
};

WrapperInfo MyObject::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyObject2::kWrapperInfo = { kEmbedderNativeGin };
WrapperInfo MyNamedObject::kWrapperInfo = {kEmbedderNativeGin};

}  // namespace

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  Handle<MyObject> obj = MyObject::Create(isolate);

  v8::Local<v8::Value> wrapper =
      ConvertToV8(isolate, obj.get()).ToLocalChecked();
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = NULL;
  EXPECT_TRUE(ConvertFromV8(isolate, wrapper, &unwrapped));
  EXPECT_EQ(obj.get(), unwrapped);
}

TEST_F(WrappableTest, UnwrapFailures) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  // Something that isn't an object.
  v8::Local<v8::Value> thing = v8::Number::New(isolate, 42);
  MyObject* unwrapped = NULL;
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's not wrapping anything.
  thing = v8::Object::New(isolate);
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);

  // An object that's wrapping a C++ object of the wrong type.
  thing.Clear();
  thing = ConvertToV8(isolate, new MyObject2()).ToLocalChecked();
  EXPECT_FALSE(ConvertFromV8(isolate, thing, &unwrapped));
  EXPECT_FALSE(unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  gin::Handle<MyObject> obj = MyObject::Create(isolate);

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
      ConvertToV8(isolate, obj.get()).ToLocalChecked(),
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

  gin::Handle<MyObject> obj = MyObject::Create(isolate);

  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj.get()).ToLocalChecked().As<v8::Object>();
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

  EXPECT_EQ("Uncaught TypeError: Illegal invocation",
            get_error(member_method, v8::Null(isolate)));
  // A non-member function shouldn't throw errors for being applied on a
  // null (or invalid) object.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));

  v8::Local<v8::Object> wrong_object = v8::Object::New(isolate);
  // We should get an error for passing the wrong object.
  EXPECT_EQ("Uncaught TypeError: Illegal invocation",
            get_error(member_method, wrong_object));
  // But again, not for a "static" method.
  EXPECT_EQ(std::string(), get_error(non_member_method, v8::Null(isolate)));
}

TEST_F(WrappableTest, MethodInvocationErrorsOnNamedObject) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  gin::Handle<MyNamedObject> obj = MyNamedObject::Create(isolate);

  v8::Local<v8::Object> v8_object =
      ConvertToV8(isolate, obj.get()).ToLocalChecked().As<v8::Object>();
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

  static WrapperInfo kWrapperInfo;

  static gin::Handle<MyObjectWithLazyProperties> Create(v8::Isolate* isolate) {
    return CreateHandle(isolate, new MyObjectWithLazyProperties());
  }

  int access_count() const { return access_count_; }

 private:
  MyObjectWithLazyProperties() = default;

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

WrapperInfo MyObjectWithLazyProperties::kWrapperInfo = {kEmbedderNativeGin};

TEST_F(WrappableTest, LazyPropertyGetterIsCalledOnce) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  auto handle = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object = handle.ToV8().As<v8::Object>();
  v8::Local<v8::String> key = StringToSymbol(isolate, "fortyTwo");
  v8::Local<v8::Value> value;

  bool has_own_property = false;
  ASSERT_TRUE(v8_object->HasOwnProperty(context, key).To(&has_own_property));
  EXPECT_TRUE(has_own_property);

  EXPECT_EQ(0, handle->access_count());

  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 42)));
  EXPECT_EQ(1, handle->access_count());

  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 42)));
  EXPECT_EQ(1, handle->access_count());
}

TEST_F(WrappableTest, LazyPropertyGetterCanBeSetFirst) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  auto handle = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object = handle.ToV8().As<v8::Object>();
  v8::Local<v8::String> key = StringToSymbol(isolate, "fortyTwo");
  v8::Local<v8::Value> value;

  EXPECT_EQ(0, handle->access_count());

  bool set_ok = false;
  ASSERT_TRUE(
      v8_object->Set(context, key, v8::Int32::New(isolate, 1701)).To(&set_ok));
  ASSERT_TRUE(set_ok);
  ASSERT_TRUE(v8_object->Get(context, key).ToLocal(&value));
  EXPECT_TRUE(value->StrictEquals(v8::Int32::New(isolate, 1701)));
  EXPECT_EQ(0, handle->access_count());
}

TEST_F(WrappableTest, LazyPropertyGetterCanBindSpecialArguments) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  auto handle = MyObjectWithLazyProperties::Create(isolate);
  v8::Local<v8::Object> v8_object = handle.ToV8().As<v8::Object>();
  v8::Local<v8::Value> value;
  ASSERT_TRUE(
      v8_object->Get(context, StringToSymbol(isolate, "self")).ToLocal(&value));
  EXPECT_TRUE(v8_object == value);
}

TEST_F(WrappableTest, CannotConstruct) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  Handle<MyObject> obj = MyObject::Create(isolate);
  v8::Local<v8::Value> wrapper =
      ConvertToV8(isolate, obj.get()).ToLocalChecked();
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
