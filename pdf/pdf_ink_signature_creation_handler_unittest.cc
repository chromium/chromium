// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_signature_creation_handler.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "pdf/pdf_features.h"
#include "pdf/test/fake_pdf_ink_module_client.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkPathTypes.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

using ::testing::InSequence;
using ::testing::NiceMock;

base::DictValue CreateSignatureStrokePointDict(float x,
                                               float y,
                                               float elapsed_time) {
  return base::DictValue().Set("x", x).Set("y", y).Set("elapsedTime",
                                                       elapsed_time);
}

// Renders `[x0, y0, x1, y1, ...]` polygon loops in `outlines` as a black
// winding-filled `SkPath` on a 50x50 white surface.
sk_sp<SkImage> RenderSignatureOutlinesToImage(const base::ListValue& outlines) {
  static constexpr gfx::Size kCanvasSize(50, 50);
  sk_sp<SkSurface> surface =
      CreateSkiaSurfaceForTesting(kCanvasSize, SK_ColorWHITE);
  CHECK(surface);

  SkPathBuilder path_builder(SkPathFillType::kWinding);
  for (const base::Value& outline_val : outlines) {
    const base::ListValue& coords = outline_val.GetList();
    // Each loop represents a closed polygon of [x0, y0, x1, y1, ...] vertex
    // coordinate pairs, flattened for IPC efficiency. An even coordinate count
    // and at least three vertices (6 numbers) are required to form an enclosed
    // 2D shape.
    CHECK_GE(coords.size(), 6u);
    CHECK_EQ(coords.size() % 2, 0u);
    path_builder.moveTo(static_cast<float>(coords[0].GetDouble()),
                        static_cast<float>(coords[1].GetDouble()));
    for (size_t i = 2; i < coords.size(); i += 2) {
      path_builder.lineTo(static_cast<float>(coords[i].GetDouble()),
                          static_cast<float>(coords[i + 1].GetDouble()));
    }
    path_builder.close();
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);
  surface->getCanvas()->drawPath(path_builder.detach(), paint);
  return surface->makeImageSnapshot();
}

class PdfInkSignatureCreationHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome_pdf::features::kPdfInk2,
        {
            {features::kPdfInk2Signatures.name, "true"},
        });
    signature_creation_handler_ =
        std::make_unique<PdfInkSignatureCreationHandler>(client_);
  }

 protected:
  void ExpectSignatureOutlineMessage(
      std::string_view expected_type,
      base::FilePath::StringViewType expected_png_filename) {
    EXPECT_CALL(client(), PostMessage)
        .WillOnce([expected_type,
                   expected_png_filename](const base::DictValue& message) {
          const std::string* type = message.FindString("type");
          ASSERT_TRUE(type);
          EXPECT_EQ(expected_type, *type);
          const std::string* message_id = message.FindString("messageId");
          ASSERT_TRUE(message_id);
          EXPECT_EQ("foo", *message_id);
          const base::ListValue* outlines = message.FindList("outlines");
          ASSERT_TRUE(outlines);
          sk_sp<SkImage> image = RenderSignatureOutlinesToImage(*outlines);
          ASSERT_TRUE(image);
          EXPECT_TRUE(MatchesPngFile(
              *image, GetInkTestDataFilePath(expected_png_filename)));
        });
  }

  FakePdfInkModuleClient& client() { return client_; }
  PdfInkSignatureCreationHandler& signature_creation_handler() {
    return *signature_creation_handler_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  NiceMock<FakePdfInkModuleClient> client_;
  std::unique_ptr<PdfInkSignatureCreationHandler> signature_creation_handler_;
};

}  // namespace

// Verify that starting, continuing, and finishing a signature stroke replies
// with incremental and final stroke outlines.
TEST_F(PdfInkSignatureCreationHandlerTest, SignatureStrokeMessages) {
  {
    InSequence seq;
    ExpectSignatureOutlineMessage(
        "startSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_start.png"));
    ExpectSignatureOutlineMessage(
        "continueSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_continue.png"));
    ExpectSignatureOutlineMessage(
        "finishSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_finish.png"));
  }

  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "startSignatureStroke")
          .Set("messageId", "foo")
          .Set("point", CreateSignatureStrokePointDict(10.0f, 20.0f, 0.0f))));

  base::ListValue points;
  points.Append(CreateSignatureStrokePointDict(25.0f, 20.0f, 5.0f));
  points.Append(CreateSignatureStrokePointDict(25.0f, 35.0f, 10.0f));
  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "continueSignatureStroke")
          .Set("messageId", "foo")
          .Set("points", std::move(points))));

  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "finishSignatureStroke")
          .Set("messageId", "foo")
          .Set("point", CreateSignatureStrokePointDict(40.0f, 35.0f, 20.0f))));
}

// Verify that continuing a signature stroke skips points with negative
// `elapsedTime` without crashing or altering the stroke.
TEST_F(PdfInkSignatureCreationHandlerTest, SignatureStrokeInvalidElapsedTime) {
  {
    InSequence seq;
    ExpectSignatureOutlineMessage(
        "startSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_start.png"));
    ExpectSignatureOutlineMessage(
        "continueSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_continue.png"));
  }

  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "startSignatureStroke")
          .Set("messageId", "foo")
          .Set("point", CreateSignatureStrokePointDict(10.0f, 20.0f, 0.0f))));

  // Negative `elapsedTime` is skipped without crashing or extending the stroke
  // to (45, 45).
  base::ListValue points;
  points.Append(CreateSignatureStrokePointDict(45.0f, 45.0f, -5.0f));
  points.Append(CreateSignatureStrokePointDict(25.0f, 20.0f, 5.0f));
  points.Append(CreateSignatureStrokePointDict(25.0f, 35.0f, 10.0f));
  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "continueSignatureStroke")
          .Set("messageId", "foo")
          .Set("points", std::move(points))));
}

// Verify that continuing a signature stroke skips adjacent duplicate coalesced
// points without crashing or altering the stroke.
TEST_F(PdfInkSignatureCreationHandlerTest, SignatureStrokeDuplicatePoint) {
  {
    InSequence seq;
    ExpectSignatureOutlineMessage(
        "startSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_start.png"));
    ExpectSignatureOutlineMessage(
        "continueSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_continue.png"));
  }

  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "startSignatureStroke")
          .Set("messageId", "foo")
          .Set("point", CreateSignatureStrokePointDict(10.0f, 20.0f, 0.0f))));

  // Adjacent duplicate coalesced points are skipped without crashing.
  base::ListValue points;
  points.Append(CreateSignatureStrokePointDict(25.0f, 20.0f, 5.0f));
  points.Append(CreateSignatureStrokePointDict(25.0f, 20.0f, 5.0f));
  points.Append(CreateSignatureStrokePointDict(25.0f, 35.0f, 10.0f));
  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "continueSignatureStroke")
          .Set("messageId", "foo")
          .Set("points", std::move(points))));
}

// Verify that finishing a signature stroke without a final `"point"` completes
// the stroke using the existing inputs.
TEST_F(PdfInkSignatureCreationHandlerTest, SignatureStrokeFinishWithoutPoint) {
  {
    InSequence seq;
    ExpectSignatureOutlineMessage(
        "startSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_start.png"));
    ExpectSignatureOutlineMessage(
        "finishSignatureStrokeReply",
        FILE_PATH_LITERAL("signature_stroke_start.png"));
  }

  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "startSignatureStroke")
          .Set("messageId", "foo")
          .Set("point", CreateSignatureStrokePointDict(10.0f, 20.0f, 0.0f))));
  EXPECT_TRUE(signature_creation_handler().OnMessage(
      base::DictValue()
          .Set("type", "finishSignatureStroke")
          .Set("messageId", "foo")));
}

}  // namespace chrome_pdf
