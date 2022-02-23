// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include <stddef.h>

#include "base/values.h"
#include "skia/ext/benchmarking_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace {

testing::AssertionResult HasArg(const base::ListValue* args,
                                const char name[]) {
  for (size_t i = 0; i < args->GetListDeprecated().size(); ++i) {
    const base::Value& arg = args->GetListDeprecated()[i];
    if (!arg.is_dict() || arg.DictSize() != 1) {
      return testing::AssertionFailure() << " malformed argument for index "
                                         << i;
    }

    if (arg.FindKey(name)) {
      return testing::AssertionSuccess() << " argument '" << name
                                         << "' found at index " << i;
    }
  }

  return testing::AssertionFailure() << "argument not found: '" << name << "'";
}
}

namespace content {

TEST(SkiaBenchmarkingExtensionTest, BenchmarkingCanvas) {
  SkGraphics::Init();

  // Prepare canvas and resources.
  SkCanvas canvas(100, 100);
  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);

  SkPaint red_paint;
  red_paint.setColor(SkColorSetARGB(255, 255, 0, 0));
  SkRect fullRect = SkRect::MakeWH(SkIntToScalar(100), SkIntToScalar(100));
  SkRect fillRect = SkRect::MakeXYWH(SkIntToScalar(25), SkIntToScalar(25),
                                     SkIntToScalar(50), SkIntToScalar(50));

  SkMatrix trans;
  trans.setTranslate(SkIntToScalar(10), SkIntToScalar(10));

  // Draw a trivial scene.
  benchmarking_canvas.save();
  benchmarking_canvas.clipRect(fullRect);
  benchmarking_canvas.setMatrix(trans);
  benchmarking_canvas.drawRect(fillRect, red_paint);
  benchmarking_canvas.restore();

  // Verify the recorded commands.
  const base::ListValue& ops = benchmarking_canvas.Commands();
  ASSERT_EQ(ops.GetListDeprecated().size(), static_cast<size_t>(5));

  size_t index = 0;
  const base::Value* value;
  const base::DictionaryValue* op;
  const base::ListValue* op_args;
  std::string op_name;

  value = &ops.GetListDeprecated()[index++];
  ASSERT_TRUE(value->is_dict());
  op = static_cast<const base::DictionaryValue*>(value);
  EXPECT_TRUE(op->GetString("cmd_string", &op_name));
  EXPECT_EQ(op_name, "Save");
  ASSERT_TRUE(op->GetList("info", &op_args));
  EXPECT_EQ(op_args->GetListDeprecated().size(), static_cast<size_t>(0));

  value = &ops.GetListDeprecated()[index++];
  ASSERT_TRUE(value->is_dict());
  op = static_cast<const base::DictionaryValue*>(value);
  EXPECT_TRUE(op->GetString("cmd_string", &op_name));
  EXPECT_EQ(op_name, "ClipRect");
  ASSERT_TRUE(op->GetList("info", &op_args));
  EXPECT_EQ(op_args->GetListDeprecated().size(), static_cast<size_t>(3));
  EXPECT_TRUE(HasArg(op_args, "rect"));
  EXPECT_TRUE(HasArg(op_args, "op"));
  EXPECT_TRUE(HasArg(op_args, "anti-alias"));

  value = &ops.GetListDeprecated()[index++];
  ASSERT_TRUE(value->is_dict());
  op = static_cast<const base::DictionaryValue*>(value);
  EXPECT_TRUE(op->GetString("cmd_string", &op_name));
  EXPECT_EQ(op_name, "SetMatrix");
  ASSERT_TRUE(op->GetList("info", &op_args));
  EXPECT_EQ(op_args->GetListDeprecated().size(), static_cast<size_t>(1));
  EXPECT_TRUE(HasArg(op_args, "matrix"));

  value = &ops.GetListDeprecated()[index++];
  ASSERT_TRUE(value->is_dict());
  op = static_cast<const base::DictionaryValue*>(value);
  EXPECT_TRUE(op->GetString("cmd_string", &op_name));
  EXPECT_EQ(op_name, "DrawRect");
  ASSERT_TRUE(op->GetList("info", &op_args));
  EXPECT_EQ(op_args->GetListDeprecated().size(), static_cast<size_t>(2));
  EXPECT_TRUE(HasArg(op_args, "rect"));
  EXPECT_TRUE(HasArg(op_args, "paint"));

  value = &ops.GetListDeprecated()[index++];
  ASSERT_TRUE(value->is_dict());
  op = static_cast<const base::DictionaryValue*>(value);
  EXPECT_TRUE(op->GetString("cmd_string", &op_name));
  EXPECT_EQ(op_name, "Restore");
  ASSERT_TRUE(op->GetList("info", &op_args));
  EXPECT_EQ(op_args->GetListDeprecated().size(), static_cast<size_t>(0));
}

} // namespace content
