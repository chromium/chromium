// Copyright 2013 The Chromium Authors
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

testing::AssertionResult HasArg(const base::Value::List& args,
                                const char name[]) {
  for (size_t i = 0; i < args.size(); ++i) {
    const base::Value::Dict* arg = args[i].GetIfDict();
    if (!arg || arg->size() != 1) {
      return testing::AssertionFailure() << " malformed argument for index "
                                         << i;
    }

    if (arg->contains(name)) {
      return testing::AssertionSuccess() << " argument '" << name
                                         << "' found at index " << i;
    }
  }

  return testing::AssertionFailure() << "argument not found: '" << name << "'";
}

}  // namespace

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
  const base::Value::List& ops = benchmarking_canvas.Commands();
  ASSERT_EQ(ops.size(), static_cast<size_t>(5));

  size_t index = 0;
  const base::Value* value;
  const base::Value::Dict* op;
  const base::Value::List* op_args;
  const std::string* op_name;

  value = &ops[index++];
  ASSERT_TRUE(value->is_dict());
  op = &value->GetDict();
  op_name = op->FindString("cmd_string");
  ASSERT_TRUE(op_name);
  EXPECT_EQ(*op_name, "Save");
  op_args = op->FindList("info");
  ASSERT_TRUE(op_args);
  EXPECT_TRUE(op_args->empty());

  value = &ops[index++];
  ASSERT_TRUE(value->is_dict());
  op = &value->GetDict();
  op_name = op->FindString("cmd_string");
  ASSERT_TRUE(op_name);
  EXPECT_EQ(*op_name, "ClipRect");
  op_args = op->FindList("info");
  ASSERT_TRUE(op_args);
  EXPECT_EQ(op_args->size(), static_cast<size_t>(3));
  EXPECT_TRUE(HasArg(*op_args, "rect"));
  EXPECT_TRUE(HasArg(*op_args, "op"));
  EXPECT_TRUE(HasArg(*op_args, "anti-alias"));

  value = &ops[index++];
  ASSERT_TRUE(value->is_dict());
  op = &value->GetDict();
  op_name = op->FindString("cmd_string");
  ASSERT_TRUE(op_name);
  EXPECT_EQ(*op_name, "SetMatrix");
  op_args = op->FindList("info");
  ASSERT_TRUE(op_args);
  EXPECT_EQ(op_args->size(), static_cast<size_t>(1));
  EXPECT_TRUE(HasArg(*op_args, "matrix"));

  value = &ops[index++];
  ASSERT_TRUE(value->is_dict());
  op = &value->GetDict();
  op_name = op->FindString("cmd_string");
  ASSERT_TRUE(op_name);
  EXPECT_EQ(*op_name, "DrawRect");
  op_args = op->FindList("info");
  ASSERT_TRUE(op_args);
  EXPECT_EQ(op_args->size(), static_cast<size_t>(2));
  EXPECT_TRUE(HasArg(*op_args, "rect"));
  EXPECT_TRUE(HasArg(*op_args, "paint"));

  value = &ops[index++];
  ASSERT_TRUE(value->is_dict());
  op = &value->GetDict();
  op_name = op->FindString("cmd_string");
  ASSERT_TRUE(op_name);
  EXPECT_EQ(*op_name, "Restore");
  op_args = op->FindList("info");
  ASSERT_TRUE(op_args);
  EXPECT_TRUE(op_args->empty());
}

}  // namespace content
