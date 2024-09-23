// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/arguments.h"

#include <string_view>

#include "base/functional/bind.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"
#include "v8/include/v8-template.h"

namespace gin {

using ArgumentsTest = V8Test;

// Test that Arguments::GetHolderCreationContext returns the proper context.
TEST_F(ArgumentsTest, TestArgumentsHolderCreationContext) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> creation_context = context_.Get(instance_->isolate());

  auto check_creation_context = [](v8::Local<v8::Context> expected_context,
                                   gin::Arguments* arguments) {
    EXPECT_EQ(expected_context, arguments->GetHolderCreationContext());
  };

  // Create an object that will compare GetHolderCreationContext() with
  // |creation_context|.
  v8::Local<v8::ObjectTemplate> object_template =
      ObjectTemplateBuilder(isolate)
          .SetMethod(
              "checkCreationContext",
              base::BindRepeating(check_creation_context, creation_context))
          .Build();

  v8::Local<v8::Object> object =
      object_template->NewInstance(creation_context).ToLocalChecked();

  // Call checkCreationContext() on the generated object using the passed-in
  // context as the current context.
  auto test_context = [object, isolate](v8::Local<v8::Context> context) {
    v8::Context::Scope context_scope(context);
    const char kCallFunction[] = "(function(o) { o.checkCreationContext(); })";
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, StringToV8(isolate, kCallFunction))
            .ToLocalChecked();
    v8::Local<v8::Function> function;
    ASSERT_TRUE(ConvertFromV8(isolate, script->Run(context).ToLocalChecked(),
                              &function));
    v8::Local<v8::Value> args[] = {object};
    function->Call(context, v8::Undefined(isolate), std::size(args), args)
        .ToLocalChecked();
  };

  // Test calling in the creation context.
  test_context(creation_context);

  {
    // Create a second context, and test calling in that. The creation context
    // should be the same (even though the current context has changed).
    v8::Local<v8::Context> second_context =
        v8::Context::New(isolate, nullptr, v8::Local<v8::ObjectTemplate>());
    test_context(second_context);
  }
}

TEST_F(ArgumentsTest, TestGetAll) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = context_.Get(instance_->isolate());

  using V8List = v8::LocalVector<v8::Value>;

  V8List list1(isolate,
               {
                   gin::ConvertToV8(isolate, 1),
                   gin::StringToV8(isolate, "some string"),
                   gin::ConvertToV8(isolate, std::vector<double>({2.0, 3.0})),
               });
  bool called1 = false;

  V8List list2(isolate, {
                            gin::StringToV8(isolate, "some other string"),
                            gin::ConvertToV8(isolate, 42),
                        });
  bool called2 = false;

  V8List list3(isolate);  // Empty list.
  bool called3 = false;

  auto check_arguments = [](V8List* expected, bool* called,
                            gin::Arguments* arguments) {
    *called = true;
    V8List actual = arguments->GetAll();
    ASSERT_EQ(expected->size(), actual.size());
    for (size_t i = 0; i < expected->size(); ++i)
      EXPECT_EQ(expected->at(i), actual[i]) << i;
  };

  // Create an object that will compare GetHolderCreationContext() with
  // |creation_context|.
  v8::Local<v8::ObjectTemplate> object_template =
      ObjectTemplateBuilder(isolate)
          .SetMethod("check1",
                     base::BindRepeating(check_arguments, &list1, &called1))
          .SetMethod("check2",
                     base::BindRepeating(check_arguments, &list2, &called2))
          .SetMethod("check3",
                     base::BindRepeating(check_arguments, &list3, &called3))
          .Build();

  v8::Local<v8::Object> object =
      object_template->NewInstance(context).ToLocalChecked();

  auto do_check = [object, context](V8List& args, std::string_view key) {
    v8::Local<v8::Value> val;
    ASSERT_TRUE(
        object->Get(context, gin::StringToSymbol(context->GetIsolate(), key))
            .ToLocal(&val));
    ASSERT_TRUE(val->IsFunction());
    val.As<v8::Function>()
        ->Call(context, object, static_cast<int>(args.size()), args.data())
        .ToLocalChecked();
  };

  do_check(list1, "check1");
  EXPECT_TRUE(called1);
  do_check(list2, "check2");
  EXPECT_TRUE(called2);
  do_check(list3, "check3");
  EXPECT_TRUE(called3);
}

}  // namespace gin
