// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/data_object_builder.h"

#include <string_view>

#include "base/check_op.h"
#include "base/debug/debugging_buildflags.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "gin/dictionary.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"

namespace gin {
namespace {

using DataObjectBuilderTest = V8Test;

// It should create ordinary data properties.
TEST_F(DataObjectBuilderTest, CreatesDataProperties) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  v8::Local<v8::Object> object =
      DataObjectBuilder(isolate).Set("key", 42).Build();
  ASSERT_TRUE(object->HasOwnProperty(context, StringToSymbol(isolate, "key"))
                  .ToChecked());

  v8::Local<v8::Value> descriptor_object;
  ASSERT_TRUE(
      object->GetOwnPropertyDescriptor(context, StringToSymbol(isolate, "key"))
          .ToLocal(&descriptor_object));
  gin::Dictionary descriptor(isolate, descriptor_object.As<v8::Object>());

  int32_t value = 0;
  ASSERT_TRUE(descriptor.Get("value", &value));
  EXPECT_EQ(42, value);

  bool writable = false;
  ASSERT_TRUE(descriptor.Get("writable", &writable));
  EXPECT_TRUE(writable);

  bool enumerable = false;
  ASSERT_TRUE(descriptor.Get("enumerable", &enumerable));
  EXPECT_TRUE(enumerable);

  bool configurable = false;
  ASSERT_TRUE(descriptor.Get("configurable", &configurable));
  EXPECT_TRUE(configurable);
}

// It should not invoke setters on the prototype chain.
TEST_F(DataObjectBuilderTest, DoesNotInvokeSetters) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  // Install a setter on the object prototype.
  v8::Local<v8::Value> object_constructor;
  ASSERT_TRUE(context->Global()
                  ->Get(context, StringToSymbol(isolate, "Object"))
                  .ToLocal(&object_constructor));
  v8::Local<v8::Value> object_prototype;
  ASSERT_TRUE(object_constructor.As<v8::Function>()
                  ->Get(context, StringToSymbol(isolate, "prototype"))
                  .ToLocal(&object_prototype));
  ASSERT_TRUE(object_prototype.As<v8::Object>()
                  ->SetNativeDataProperty(
                      context, StringToSymbol(isolate, "key"),
                      [](v8::Local<v8::Name>,
                         const v8::PropertyCallbackInfo<v8::Value>&) {},
                      [](v8::Local<v8::Name>, v8::Local<v8::Value>,
                         const v8::PropertyCallbackInfo<void>&) {
                        ADD_FAILURE() << "setter should not be invoked";
                      })
                  .ToChecked());

  // Create an object.
  DataObjectBuilder(isolate).Set("key", 42).Build();
}

// The internal handle is cleared when the builder is finished.
// This makes the class harder to abuse, so that its methods cannot be used
// after something may have modified the object in unexpected ways.
// TODO(pbos): Consider making this a CHECK and test this everywhere.
#if DCHECK_IS_ON() && !BUILDFLAG(DCHECK_IS_CONFIGURABLE)
TEST_F(DataObjectBuilderTest, UnusableAfterBuild) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  DataObjectBuilder builder(isolate);
  EXPECT_FALSE(builder.Build().IsEmpty());

  EXPECT_DEATH_IF_SUPPORTED(builder.Build(),
                            "Check failed: !object_.IsEmpty\\(\\)");
}
#endif  // DCHECK_IS_ON()

// As is the normal behaviour of CreateDataProperty, new data properties should
// replace existing ones. Since no non-configurable ones are present, nor should
// the object be non-extensible, this should work.
TEST_F(DataObjectBuilderTest, ReplacesExistingProperties) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> object =
      DataObjectBuilder(isolate).Set("value", 42).Set("value", 55).Build();

  gin::Dictionary dictionary(isolate, object);
  int32_t value;
  ASSERT_TRUE(dictionary.Get("value", &value));
  EXPECT_EQ(55, value);
}

// It should work for array indices, too.
TEST_F(DataObjectBuilderTest, CreatesDataPropertiesForIndices) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(isolate);

  v8::Local<v8::Object> object =
      DataObjectBuilder(isolate).Set(42, std::string_view("forty-two")).Build();
  ASSERT_TRUE(object->HasOwnProperty(context, 42).ToChecked());

  v8::Local<v8::Value> descriptor_object;
  ASSERT_TRUE(
      object->GetOwnPropertyDescriptor(context, StringToSymbol(isolate, "42"))
          .ToLocal(&descriptor_object));
  gin::Dictionary descriptor(isolate, descriptor_object.As<v8::Object>());

  std::string value;
  ASSERT_TRUE(descriptor.Get("value", &value));
  EXPECT_EQ("forty-two", value);

  bool writable = false;
  ASSERT_TRUE(descriptor.Get("writable", &writable));
  EXPECT_TRUE(writable);

  bool enumerable = false;
  ASSERT_TRUE(descriptor.Get("enumerable", &enumerable));
  EXPECT_TRUE(enumerable);

  bool configurable = false;
  ASSERT_TRUE(descriptor.Get("configurable", &configurable));
  EXPECT_TRUE(configurable);
}

}  // namespace
}  // namespace gin
