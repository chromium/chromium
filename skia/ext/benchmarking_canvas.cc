// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/benchmarking_canvas.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace {

class FlagsBuilder {
public:
  FlagsBuilder(char separator)
      : separator_(separator) {}

  void addFlag(bool flag_val, const char flag_name[]) {
    if (!flag_val)
      return;
    if (!oss_.str().empty())
      oss_ << separator_;

    oss_ << flag_name;
  }

  std::string str() const {
    return oss_.str();
  }

private:
  char separator_;
  std::ostringstream oss_;
};

std::unique_ptr<base::Value> AsValue(bool b) {
  std::unique_ptr<base::Value> val(new base::Value(b));

  return val;
}

std::unique_ptr<base::Value> AsValue(SkScalar scalar) {
  std::unique_ptr<base::Value> val(new base::Value(scalar));

  return val;
}

std::unique_ptr<base::Value> AsValue(const SkSize& size) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("width",  AsValue(size.width()));
  val->Set("height", AsValue(size.height()));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkPoint& point) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("x", AsValue(point.x()));
  val->Set("y", AsValue(point.y()));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkRect& rect) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("left", AsValue(rect.fLeft));
  val->Set("top", AsValue(rect.fTop));
  val->Set("right", AsValue(rect.fRight));
  val->Set("bottom", AsValue(rect.fBottom));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkRRect& rrect) {
  std::unique_ptr<base::DictionaryValue> radii_val(new base::DictionaryValue());
  radii_val->Set("upper-left", AsValue(rrect.radii(SkRRect::kUpperLeft_Corner)));
  radii_val->Set("upper-right", AsValue(rrect.radii(SkRRect::kUpperRight_Corner)));
  radii_val->Set("lower-right", AsValue(rrect.radii(SkRRect::kLowerRight_Corner)));
  radii_val->Set("lower-left", AsValue(rrect.radii(SkRRect::kLowerLeft_Corner)));

  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("rect", AsValue(rrect.rect()));
  val->Set("radii", std::move(radii_val));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkMatrix& matrix) {
  std::unique_ptr<base::ListValue> val(new base::ListValue());
  for (int i = 0; i < 9; ++i)
    val->Append(AsValue(matrix[i]));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(SkColor color) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->SetInteger("a", SkColorGetA(color));
  val->SetInteger("r", SkColorGetR(color));
  val->SetInteger("g", SkColorGetG(color));
  val->SetInteger("b", SkColorGetB(color));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(SkBlendMode mode) {
  std::unique_ptr<base::Value> val(new base::Value(SkBlendMode_Name(mode)));

  return val;
}

std::unique_ptr<base::Value> AsValue(SkCanvas::PointMode mode) {
  static const char* gModeStrings[] = { "Points", "Lines", "Polygon" };
  DCHECK_LT(static_cast<size_t>(mode), SK_ARRAY_COUNT(gModeStrings));

  std::unique_ptr<base::Value> val(new base::Value(gModeStrings[mode]));

  return val;
}

std::unique_ptr<base::Value> AsValue(const SkColorFilter& filter) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());

  if (unsigned flags = filter.getFlags()) {
    FlagsBuilder builder('|');
    builder.addFlag(flags & SkColorFilter::kAlphaUnchanged_Flag,
                    "kAlphaUnchanged_Flag");

    val->SetString("flags", builder.str());
  }

  SkScalar color_matrix[20];
  if (filter.asAColorMatrix(color_matrix)) {
    std::unique_ptr<base::ListValue> color_matrix_val(new base::ListValue());
    for (unsigned i = 0; i < 20; ++i)
      color_matrix_val->Append(AsValue(color_matrix[i]));

    val->Set("color_matrix", std::move(color_matrix_val));
  }

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkImageFilter& filter) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->SetInteger("inputs", filter.countInputs());

  SkColorFilter* color_filter;
  if (filter.asColorFilter(&color_filter)) {
    val->Set("color_filter", AsValue(*color_filter));
    SkSafeUnref(color_filter); // ref'd in asColorFilter
  }

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkPaint& paint) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  SkPaint default_paint;

  if (paint.getColor() != default_paint.getColor())
    val->Set("Color", AsValue(paint.getColor()));

  if (paint.getStyle() != default_paint.getStyle()) {
    static const char* gStyleStrings[] = { "Fill", "Stroke", "StrokeFill" };
    DCHECK_LT(static_cast<size_t>(paint.getStyle()),
              SK_ARRAY_COUNT(gStyleStrings));
    val->SetString("Style", gStyleStrings[paint.getStyle()]);
  }

  if (paint.getBlendMode() != default_paint.getBlendMode()) {
    val->Set("Xfermode", AsValue(paint.getBlendMode()));
  }

  if (paint.isAntiAlias() || paint.isDither()) {
    FlagsBuilder builder('|');
    builder.addFlag(paint.isAntiAlias(), "AntiAlias");
    builder.addFlag(paint.isDither(), "Dither");

    val->SetString("Flags", builder.str());
  }

  if (paint.getFilterQuality() != default_paint.getFilterQuality()) {
    static const char* gFilterQualityStrings[] = {
        "None", "Low", "Medium", "High"};
    DCHECK_LT(static_cast<size_t>(paint.getFilterQuality()),
              SK_ARRAY_COUNT(gFilterQualityStrings));
    val->SetString("FilterLevel",
                   gFilterQualityStrings[paint.getFilterQuality()]);
  }

  if (paint.getColorFilter())
    val->Set("ColorFilter", AsValue(*paint.getColorFilter()));

  if (paint.getImageFilter())
    val->Set("ImageFilter", AsValue(*paint.getImageFilter()));

  return std::move(val);
}

std::unique_ptr<base::Value> SaveLayerFlagsAsValue(
    SkCanvas::SaveLayerFlags flags) {
  std::unique_ptr<base::Value> val(new base::Value(static_cast<int>(flags)));

  return val;
}

std::unique_ptr<base::Value> AsValue(SkClipOp op) {
  static const char* gOpStrings[] = { "Difference",
                                      "Intersect",
                                      "Union",
                                      "XOR",
                                      "ReverseDifference",
                                      "Replace"
                                    };
  size_t index = static_cast<size_t>(op);
  DCHECK_LT(index, SK_ARRAY_COUNT(gOpStrings));
  std::unique_ptr<base::Value> val(new base::Value(gOpStrings[index]));
  return val;
}

std::unique_ptr<base::Value> AsValue(const SkRegion& region) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("bounds", AsValue(SkRect::Make(region.getBounds())));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkBitmap& bitmap) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("size", AsValue(SkSize::Make(bitmap.width(), bitmap.height())));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkImage& image) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("size", AsValue(SkSize::Make(image.width(), image.height())));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkTextBlob& blob) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());
  val->Set("bounds", AsValue(blob.bounds()));

  return std::move(val);
}

std::unique_ptr<base::Value> AsValue(const SkPath& path) {
  std::unique_ptr<base::DictionaryValue> val(new base::DictionaryValue());

  static const char* gFillStrings[] =
      { "winding", "even-odd", "inverse-winding", "inverse-even-odd" };
  DCHECK_LT(static_cast<size_t>(path.getFillType()),
      SK_ARRAY_COUNT(gFillStrings));
  val->SetString("fill-type", gFillStrings[path.getFillType()]);

  static const char* gConvexityStrings[] = { "Unknown", "Convex", "Concave" };
  DCHECK_LT(static_cast<size_t>(path.getConvexity()),
      SK_ARRAY_COUNT(gConvexityStrings));
  val->SetString("convexity", gConvexityStrings[path.getConvexity()]);

  val->SetBoolean("is-rect", path.isRect(nullptr));
  val->Set("bounds", AsValue(path.getBounds()));

  static const char* gVerbStrings[] =
      { "move", "line", "quad", "conic", "cubic", "close", "done" };
  static const int gPtsPerVerb[] = { 1, 1, 2, 2, 3, 0, 0 };
  static const int gPtOffsetPerVerb[] = { 0, 1, 1, 1, 1, 0, 0 };
  static_assert(
      SK_ARRAY_COUNT(gVerbStrings) == static_cast<size_t>(SkPath::kDone_Verb + 1),
      "gVerbStrings size mismatch");
  static_assert(
      SK_ARRAY_COUNT(gVerbStrings) == SK_ARRAY_COUNT(gPtsPerVerb),
      "gPtsPerVerb size mismatch");
  static_assert(
      SK_ARRAY_COUNT(gVerbStrings) == SK_ARRAY_COUNT(gPtOffsetPerVerb),
      "gPtOffsetPerVerb size mismatch");

  std::unique_ptr<base::ListValue> verbs_val(new base::ListValue());
  SkPath::Iter iter(const_cast<SkPath&>(path), false);
  SkPoint points[4];

  for(SkPath::Verb verb = iter.next(points, false);
      verb != SkPath::kDone_Verb; verb = iter.next(points, false)) {
      DCHECK_LT(static_cast<size_t>(verb), SK_ARRAY_COUNT(gVerbStrings));

      std::unique_ptr<base::DictionaryValue> verb_val(
          new base::DictionaryValue());
      std::unique_ptr<base::ListValue> pts_val(new base::ListValue());

      for (int i = 0; i < gPtsPerVerb[verb]; ++i)
        pts_val->Append(AsValue(points[i + gPtOffsetPerVerb[verb]]));

      verb_val->Set(gVerbStrings[verb], std::move(pts_val));

      if (SkPath::kConic_Verb == verb)
        verb_val->Set("weight", AsValue(iter.conicWeight()));

      verbs_val->Append(std::move(verb_val));
  }
  val->Set("verbs", std::move(verbs_val));

  return std::move(val);
}

template <typename T>
std::unique_ptr<base::Value> AsListValue(const T array[], size_t count) {
  std::unique_ptr<base::ListValue> val(new base::ListValue());

  for (size_t i = 0; i < count; ++i)
    val->Append(AsValue(array[i]));

  return std::move(val);
}

} // namespace

namespace skia {

class BenchmarkingCanvas::AutoOp {
public:
  // AutoOp objects are always scoped within draw call frames,
  // so the paint is guaranteed to be valid for their lifetime.
 AutoOp(BenchmarkingCanvas* canvas,
        const char op_name[],
        const SkPaint* paint = nullptr)
     : canvas_(canvas), op_record_(new base::DictionaryValue()) {
   DCHECK(canvas);
   DCHECK(op_name);

   op_record_->SetString("cmd_string", op_name);
   op_params_ =
       op_record_->SetList("info", std::make_unique<base::ListValue>());

   if (paint) {
     this->addParam("paint", AsValue(*paint));
     filtered_paint_ = *paint;
   }

   start_ticks_ = base::TimeTicks::Now();
  }

  ~AutoOp() {
    base::TimeDelta ticks = base::TimeTicks::Now() - start_ticks_;
    op_record_->SetDouble("cmd_time", ticks.InMillisecondsF());

    canvas_->op_records_.Append(std::move(op_record_));
  }

  void addParam(const char name[], std::unique_ptr<base::Value> value) {
    std::unique_ptr<base::DictionaryValue> param(new base::DictionaryValue());
    param->Set(name, std::move(value));

    op_params_->Append(std::move(param));
  }

  const SkPaint* paint() const { return &filtered_paint_; }

private:
  BenchmarkingCanvas* canvas_;
  std::unique_ptr<base::DictionaryValue> op_record_;
  base::ListValue* op_params_;
  base::TimeTicks start_ticks_;

  SkPaint filtered_paint_;
};

BenchmarkingCanvas::BenchmarkingCanvas(SkCanvas* canvas)
    : INHERITED(canvas->imageInfo().width(),
                canvas->imageInfo().height()) {
  addCanvas(canvas);
}

BenchmarkingCanvas::~BenchmarkingCanvas() = default;

size_t BenchmarkingCanvas::CommandCount() const {
  return op_records_.GetSize();
}

const base::ListValue& BenchmarkingCanvas::Commands() const {
  return op_records_;
}

double BenchmarkingCanvas::GetTime(size_t index) {
  const base::DictionaryValue* op;
  if (!op_records_.GetDictionary(index, &op))
    return 0;

  double t;
  if (!op->GetDouble("cmd_time", &t))
    return 0;

  return t;
}

void BenchmarkingCanvas::willSave() {
  AutoOp op(this, "Save");

  INHERITED::willSave();
}

SkCanvas::SaveLayerStrategy BenchmarkingCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  AutoOp op(this, "SaveLayer", rec.fPaint);
  if (rec.fBounds)
    op.addParam("bounds", AsValue(*rec.fBounds));
  if (rec.fSaveLayerFlags)
    op.addParam("flags", SaveLayerFlagsAsValue(rec.fSaveLayerFlags));

  return INHERITED::getSaveLayerStrategy(rec);
}

void BenchmarkingCanvas::willRestore() {
  AutoOp op(this, "Restore");

  INHERITED::willRestore();
}

void BenchmarkingCanvas::didConcat(const SkMatrix& m) {
  AutoOp op(this, "Concat");
  op.addParam("matrix", AsValue(m));

  INHERITED::didConcat(m);
}

void BenchmarkingCanvas::didSetMatrix(const SkMatrix& m) {
  AutoOp op(this, "SetMatrix");
  op.addParam("matrix", AsValue(m));

  INHERITED::didSetMatrix(m);
}

void BenchmarkingCanvas::onClipRect(const SkRect& rect,
                                    SkClipOp region_op,
                                    SkCanvas::ClipEdgeStyle style) {
  AutoOp op(this, "ClipRect");
  op.addParam("rect", AsValue(rect));
  op.addParam("op", AsValue(region_op));
  op.addParam("anti-alias", AsValue(style == kSoft_ClipEdgeStyle));

  INHERITED::onClipRect(rect, region_op, style);
}

void BenchmarkingCanvas::onClipRRect(const SkRRect& rrect,
                                     SkClipOp region_op,
                                     SkCanvas::ClipEdgeStyle style) {
  AutoOp op(this, "ClipRRect");
  op.addParam("rrect", AsValue(rrect));
  op.addParam("op", AsValue(region_op));
  op.addParam("anti-alias", AsValue(style == kSoft_ClipEdgeStyle));

  INHERITED::onClipRRect(rrect, region_op, style);
}

void BenchmarkingCanvas::onClipPath(const SkPath& path,
                                    SkClipOp region_op,
                                    SkCanvas::ClipEdgeStyle style) {
  AutoOp op(this, "ClipPath");
  op.addParam("path", AsValue(path));
  op.addParam("op", AsValue(region_op));
  op.addParam("anti-alias", AsValue(style == kSoft_ClipEdgeStyle));

  INHERITED::onClipPath(path, region_op, style);
}

void BenchmarkingCanvas::onClipRegion(const SkRegion& region,
                                      SkClipOp region_op) {
  AutoOp op(this, "ClipRegion");
  op.addParam("region", AsValue(region));
  op.addParam("op", AsValue(region_op));

  INHERITED::onClipRegion(region, region_op);
}

void BenchmarkingCanvas::onDrawPaint(const SkPaint& paint) {
  AutoOp op(this, "DrawPaint", &paint);

  INHERITED::onDrawPaint(*op.paint());
}

void BenchmarkingCanvas::onDrawPoints(PointMode mode, size_t count,
                                      const SkPoint pts[], const SkPaint& paint) {
  AutoOp op(this, "DrawPoints", &paint);
  op.addParam("mode", AsValue(mode));
  op.addParam("points", AsListValue(pts, count));

  INHERITED::onDrawPoints(mode, count, pts, *op.paint());
}

void BenchmarkingCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  AutoOp op(this, "DrawRect", &paint);
  op.addParam("rect", AsValue(rect));

  INHERITED::onDrawRect(rect, *op.paint());
}

void BenchmarkingCanvas::onDrawOval(const SkRect& rect, const SkPaint& paint) {
  AutoOp op(this, "DrawOval", &paint);
  op.addParam("rect", AsValue(rect));

  INHERITED::onDrawOval(rect, *op.paint());
}

void BenchmarkingCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
  AutoOp op(this, "DrawRRect", &paint);
  op.addParam("rrect", AsValue(rrect));

  INHERITED::onDrawRRect(rrect, *op.paint());
}

void BenchmarkingCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner,
                                      const SkPaint& paint) {
  AutoOp op(this, "DrawDRRect", &paint);
  op.addParam("outer", AsValue(outer));
  op.addParam("inner", AsValue(inner));

  INHERITED::onDrawDRRect(outer, inner, *op.paint());
}

void BenchmarkingCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  AutoOp op(this, "DrawPath", &paint);
  op.addParam("path", AsValue(path));

  INHERITED::onDrawPath(path, *op.paint());
}

void BenchmarkingCanvas::onDrawPicture(const SkPicture* picture,
                                       const SkMatrix* matrix,
                                       const SkPaint* paint) {
  DCHECK(picture);
  AutoOp op(this, "DrawPicture", paint);
  op.addParam("picture", AsValue(picture));
  if (matrix)
    op.addParam("matrix", AsValue(*matrix));

  INHERITED::onDrawPicture(picture, matrix, op.paint());
}

void BenchmarkingCanvas::onDrawBitmap(const SkBitmap& bitmap,
                                      SkScalar left,
                                      SkScalar top,
                                      const SkPaint* paint) {
  AutoOp op(this, "DrawBitmap", paint);
  op.addParam("bitmap", AsValue(bitmap));
  op.addParam("left", AsValue(left));
  op.addParam("top", AsValue(top));

  INHERITED::onDrawBitmap(bitmap, left, top, op.paint());
}

void BenchmarkingCanvas::onDrawBitmapRect(const SkBitmap& bitmap,
                                          const SkRect* src,
                                          const SkRect& dst,
                                          const SkPaint* paint,
                                          SrcRectConstraint constraint) {
  AutoOp op(this, "DrawBitmapRect", paint);
  op.addParam("bitmap", AsValue(bitmap));
  if (src)
    op.addParam("src", AsValue(*src));
  op.addParam("dst", AsValue(dst));

  INHERITED::onDrawBitmapRect(bitmap, src, dst, op.paint(), constraint);
}

void BenchmarkingCanvas::onDrawImage(const SkImage* image,
                                     SkScalar left,
                                     SkScalar top,
                                     const SkPaint* paint) {
  DCHECK(image);
  AutoOp op(this, "DrawImage", paint);
  op.addParam("image", AsValue(*image));
  op.addParam("left", AsValue(left));
  op.addParam("top", AsValue(top));

  INHERITED::onDrawImage(image, left, top, op.paint());
}

void BenchmarkingCanvas::onDrawImageRect(const SkImage* image, const SkRect* src,
                                         const SkRect& dst, const SkPaint* paint,
                                         SrcRectConstraint constraint) {
  DCHECK(image);
  AutoOp op(this, "DrawImageRect", paint);
  op.addParam("image", AsValue(*image));
  if (src)
    op.addParam("src", AsValue(*src));
  op.addParam("dst", AsValue(dst));

  INHERITED::onDrawImageRect(image, src, dst, op.paint(), constraint);
}

void BenchmarkingCanvas::onDrawBitmapNine(const SkBitmap& bitmap,
                                          const SkIRect& center,
                                          const SkRect& dst,
                                          const SkPaint* paint) {
  AutoOp op(this, "DrawBitmapNine", paint);
  op.addParam("bitmap", AsValue(bitmap));
  op.addParam("center", AsValue(SkRect::Make(center)));
  op.addParam("dst", AsValue(dst));

  INHERITED::onDrawBitmapNine(bitmap, center, dst, op.paint());
}

void BenchmarkingCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                                        const SkPaint& paint) {
  DCHECK(blob);
  AutoOp op(this, "DrawTextBlob", &paint);
  op.addParam("blob", AsValue(*blob));
  op.addParam("x", AsValue(x));
  op.addParam("y", AsValue(y));

  INHERITED::onDrawTextBlob(blob, x, y, *op.paint());
}

} // namespace skia
