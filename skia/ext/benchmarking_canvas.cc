// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/benchmarking_canvas.h"

#include <array>
#include <memory>
#include <sstream>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
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

base::Value AsValue(bool b) {
  return base::Value(b);
}

base::Value AsValue(SkScalar scalar) {
  return base::Value(scalar);
}

base::Value AsValue(const SkSize& size) {
  base::Value::Dict val;
  val.Set("width", AsValue(size.width()));
  val.Set("height", AsValue(size.height()));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkPoint& point) {
  base::Value::Dict val;
  val.Set("x", AsValue(point.x()));
  val.Set("y", AsValue(point.y()));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkRect& rect) {
  base::Value::Dict val;
  val.Set("left", AsValue(rect.fLeft));
  val.Set("top", AsValue(rect.fTop));
  val.Set("right", AsValue(rect.fRight));
  val.Set("bottom", AsValue(rect.fBottom));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkRRect& rrect) {
  base::Value::Dict radii_val;
  radii_val.Set("upper-left", AsValue(rrect.radii(SkRRect::kUpperLeft_Corner)));
  radii_val.Set("upper-right",
                AsValue(rrect.radii(SkRRect::kUpperRight_Corner)));
  radii_val.Set("lower-right",
                AsValue(rrect.radii(SkRRect::kLowerRight_Corner)));
  radii_val.Set("lower-left", AsValue(rrect.radii(SkRRect::kLowerLeft_Corner)));

  base::Value::Dict val;
  val.Set("rect", AsValue(rrect.rect()));
  val.Set("radii", std::move(radii_val));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkMatrix& matrix) {
  base::Value::List val;
  for (int i = 0; i < 9; ++i)
    val.Append(AsValue(matrix[i]));

  return base::Value(std::move(val));
}

base::Value AsValue(SkColor color) {
  base::Value::Dict val;
  val.Set("a", int{SkColorGetA(color)});
  val.Set("r", int{SkColorGetR(color)});
  val.Set("g", int{SkColorGetG(color)});
  val.Set("b", int{SkColorGetB(color)});

  return base::Value(std::move(val));
}

base::Value AsValue(SkBlendMode mode) {
  return base::Value(SkBlendMode_Name(mode));
}

base::Value AsValue(SkCanvas::PointMode mode) {
  static const char* gModeStrings[] = { "Points", "Lines", "Polygon" };
  DCHECK_LT(static_cast<size_t>(mode), std::size(gModeStrings));

  return base::Value(gModeStrings[mode]);
}

base::Value AsValue(const SkColorFilter& filter) {
  base::Value::Dict val;

  if (filter.isAlphaUnchanged()) {
    FlagsBuilder builder('|');
    builder.addFlag(true, "kAlphaUnchanged_Flag");

    val.Set("flags", builder.str());
  }

  SkScalar color_matrix[20];
  if (filter.asAColorMatrix(color_matrix)) {
    base::Value::List color_matrix_val;
    for (unsigned i = 0; i < 20; ++i) {
      color_matrix_val.Append(AsValue(color_matrix[i]));
    }

    val.Set("color_matrix", std::move(color_matrix_val));
  }

  return base::Value(std::move(val));
}

base::Value AsValue(const SkImageFilter& filter) {
  base::Value::Dict val;
  val.Set("inputs", filter.countInputs());

  SkColorFilter* color_filter;
  if (filter.asColorFilter(&color_filter)) {
    val.Set("color_filter", AsValue(*color_filter));
    SkSafeUnref(color_filter); // ref'd in asColorFilter
  }

  return base::Value(std::move(val));
}

base::Value AsValue(const SkPaint& paint) {
  base::Value::Dict val;
  SkPaint default_paint;

  if (paint.getColor() != default_paint.getColor())
    val.Set("Color", AsValue(paint.getColor()));

  if (paint.getStyle() != default_paint.getStyle()) {
    static const char* gStyleStrings[] = { "Fill", "Stroke", "StrokeFill" };
    DCHECK_LT(static_cast<size_t>(paint.getStyle()),
              std::size(gStyleStrings));
    val.Set("Style", gStyleStrings[paint.getStyle()]);
  }

  if (paint.asBlendMode() != default_paint.asBlendMode()) {
    val.Set("Xfermode", AsValue(paint.getBlendMode_or(SkBlendMode::kSrcOver)));
  }

  if (paint.isAntiAlias() || paint.isDither()) {
    FlagsBuilder builder('|');
    builder.addFlag(paint.isAntiAlias(), "AntiAlias");
    builder.addFlag(paint.isDither(), "Dither");

    val.Set("Flags", builder.str());
  }

  if (paint.getColorFilter())
    val.Set("ColorFilter", AsValue(*paint.getColorFilter()));

  if (paint.getImageFilter())
    val.Set("ImageFilter", AsValue(*paint.getImageFilter()));

  return base::Value(std::move(val));
}

base::Value SaveLayerFlagsAsValue(SkCanvas::SaveLayerFlags flags) {
  return base::Value(int{flags});
}

base::Value AsValue(SkClipOp op) {
  static const char* gOpStrings[] = { "Difference",
                                      "Intersect",
                                      "Union",
                                      "XOR",
                                      "ReverseDifference",
                                      "Replace"
                                    };
  size_t index = static_cast<size_t>(op);
  DCHECK_LT(index, std::size(gOpStrings));
  return base::Value(gOpStrings[index]);
}

base::Value AsValue(const SkRegion& region) {
  base::Value::Dict val;
  val.Set("bounds", AsValue(SkRect::Make(region.getBounds())));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkImage& image) {
  base::Value::Dict val;
  val.Set("size", AsValue(SkSize::Make(image.width(), image.height())));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkTextBlob& blob) {
  base::Value::Dict val;
  val.Set("bounds", AsValue(blob.bounds()));

  return base::Value(std::move(val));
}

base::Value AsValue(const SkPath& path) {
  base::Value::Dict val;

  static const char* gFillStrings[] =
      { "winding", "even-odd", "inverse-winding", "inverse-even-odd" };
  size_t index = static_cast<size_t>(path.getFillType());
  DCHECK_LT(index, std::size(gFillStrings));
  val.Set("fill-type", gFillStrings[index]);
  val.Set("convex", path.isConvex());
  val.Set("is-rect", path.isRect(nullptr));
  val.Set("bounds", AsValue(path.getBounds()));

  static const char* gVerbStrings[] =
      { "move", "line", "quad", "conic", "cubic", "close", "done" };
  static const int gPtsPerVerb[] = { 1, 1, 2, 2, 3, 0, 0 };
  static const int gPtOffsetPerVerb[] = { 0, 1, 1, 1, 1, 0, 0 };
  static_assert(
      std::size(gVerbStrings) == static_cast<size_t>(SkPath::kDone_Verb + 1),
      "gVerbStrings size mismatch");
  static_assert(
      std::size(gVerbStrings) == std::size(gPtsPerVerb),
      "gPtsPerVerb size mismatch");
  static_assert(
      std::size(gVerbStrings) == std::size(gPtOffsetPerVerb),
      "gPtOffsetPerVerb size mismatch");

  base::Value::List verbs_val;
  SkPath::RawIter iter(const_cast<SkPath&>(path));
  SkPoint points[4];

  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    DCHECK_LT(static_cast<size_t>(verb), std::size(gVerbStrings));

    base::Value::Dict verb_val;
    base::Value::List pts_val;

    for (int i = 0; i < gPtsPerVerb[verb]; ++i)
      pts_val.Append(AsValue(points[i + gPtOffsetPerVerb[verb]]));

    verb_val.Set(gVerbStrings[verb], std::move(pts_val));

    if (SkPath::kConic_Verb == verb)
      verb_val.Set("weight", AsValue(iter.conicWeight()));

    verbs_val.Append(std::move(verb_val));
  }
  val.Set("verbs", std::move(verbs_val));

  return base::Value(std::move(val));
}

template <typename T>
base::Value AsListValue(const T array[], size_t count) {
  base::Value::List val;

  for (size_t i = 0; i < count; ++i)
    val.Append(AsValue(array[i]));

  return base::Value(std::move(val));
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
     : canvas_(canvas) {
   DCHECK(canvas);
   DCHECK(op_name);

   op_record_.Set("cmd_string", op_name);
   base::Value* op_params = op_record_.Set("info", base::Value::List());
   DCHECK(op_params);
   DCHECK(op_params->is_list());
   op_params_ = &op_params->GetList();

   if (paint) {
     this->addParam("paint", AsValue(*paint));
     filtered_paint_ = *paint;
   }

   start_ticks_ = base::TimeTicks::Now();
 }

  ~AutoOp() {
    base::TimeDelta ticks = base::TimeTicks::Now() - start_ticks_;
    op_record_.Set("cmd_time", ticks.InMillisecondsF());

    canvas_->op_records_.Append(std::move(op_record_));
  }

  void addParam(const char name[], base::Value value) {
    base::Value::Dict param;
    param.Set(name, std::move(value));

    op_params_->Append(std::move(param));
  }

  const SkPaint* paint() const { return &filtered_paint_; }

private:
 raw_ptr<BenchmarkingCanvas> canvas_;
 base::Value::Dict op_record_;
 raw_ptr<base::Value::List> op_params_;
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
  return op_records_.size();
}

const base::Value::List& BenchmarkingCanvas::Commands() const {
  return op_records_;
}

double BenchmarkingCanvas::GetTime(size_t index) {
  const base::Value& op = op_records_[index];
  if (!op.is_dict())
    return 0;
  return op.GetDict().FindDouble("cmd_time").value_or(0);
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

void BenchmarkingCanvas::didConcat44(const SkM44& m) {
  SkScalar values[16];
  m.getColMajor(values);
  AutoOp op(this, "Concat");
  op.addParam("matrix", AsListValue(values, 16));

  INHERITED::didConcat44(m);
}

void BenchmarkingCanvas::didScale(SkScalar x, SkScalar y) {
  AutoOp op(this, "Scale");
  op.addParam("scale-x", AsValue(x));
  op.addParam("scale-y", AsValue(y));

  INHERITED::didScale(x, y);
}

void BenchmarkingCanvas::didTranslate(SkScalar x, SkScalar y) {
  AutoOp op(this, "Translate");
  op.addParam("translate-x", AsValue(x));
  op.addParam("translate-y", AsValue(y));

  INHERITED::didTranslate(x, y);
}

void BenchmarkingCanvas::didSetM44(const SkM44& m) {
  SkScalar values[16];
  m.getColMajor(values);
  AutoOp op(this, "SetMatrix");
  op.addParam("matrix", AsListValue(values, 16));

  INHERITED::didSetM44(m);
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

void BenchmarkingCanvas::onDrawImage2(const SkImage* image,
                                      SkScalar left,
                                      SkScalar top,
                                      const SkSamplingOptions& sampling,
                                      const SkPaint* paint) {
  DCHECK(image);
  AutoOp op(this, "DrawImage", paint);
  op.addParam("image", AsValue(*image));
  op.addParam("left", AsValue(left));
  op.addParam("top", AsValue(top));

  INHERITED::onDrawImage2(image, left, top, sampling, op.paint());
}

void BenchmarkingCanvas::onDrawImageRect2(const SkImage* image,
                                          const SkRect& src,
                                          const SkRect& dst,
                                          const SkSamplingOptions& sampling,
                                          const SkPaint* paint,
                                          SrcRectConstraint constraint) {
  DCHECK(image);
  AutoOp op(this, "DrawImageRect", paint);
  op.addParam("image", AsValue(*image));
  op.addParam("src", AsValue(src));
  op.addParam("dst", AsValue(dst));

  INHERITED::onDrawImageRect2(image, src, dst, sampling, op.paint(),
                              constraint);
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
