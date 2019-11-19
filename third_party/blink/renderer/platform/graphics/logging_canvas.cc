/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"

#include <unicode/unistr.h>

#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/skia/image_pixel_locker.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

namespace {

struct VerbParams {
  STACK_ALLOCATED();

 public:
  String name;
  unsigned point_count;
  unsigned point_offset;

  VerbParams(const String& name, unsigned point_count, unsigned point_offset)
      : name(name), point_count(point_count), point_offset(point_offset) {}
};

std::unique_ptr<JSONObject> ObjectForSkRect(const SkRect& rect) {
  auto rect_item = std::make_unique<JSONObject>();
  rect_item->SetDouble("left", rect.left());
  rect_item->SetDouble("top", rect.top());
  rect_item->SetDouble("right", rect.right());
  rect_item->SetDouble("bottom", rect.bottom());
  return rect_item;
}

std::unique_ptr<JSONObject> ObjectForSkIRect(const SkIRect& rect) {
  auto rect_item = std::make_unique<JSONObject>();
  rect_item->SetDouble("left", rect.left());
  rect_item->SetDouble("top", rect.top());
  rect_item->SetDouble("right", rect.right());
  rect_item->SetDouble("bottom", rect.bottom());
  return rect_item;
}

String PointModeName(SkCanvas::PointMode mode) {
  switch (mode) {
    case SkCanvas::kPoints_PointMode:
      return "Points";
    case SkCanvas::kLines_PointMode:
      return "Lines";
    case SkCanvas::kPolygon_PointMode:
      return "Polygon";
    default:
      NOTREACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> ObjectForSkPoint(const SkPoint& point) {
  auto point_item = std::make_unique<JSONObject>();
  point_item->SetDouble("x", point.x());
  point_item->SetDouble("y", point.y());
  return point_item;
}

std::unique_ptr<JSONArray> ArrayForSkPoints(size_t count,
                                            const SkPoint points[]) {
  auto points_array_item = std::make_unique<JSONArray>();
  for (size_t i = 0; i < count; ++i)
    points_array_item->PushObject(ObjectForSkPoint(points[i]));
  return points_array_item;
}

std::unique_ptr<JSONObject> ObjectForRadius(const SkRRect& rrect,
                                            SkRRect::Corner corner) {
  auto radius_item = std::make_unique<JSONObject>();
  SkVector radius = rrect.radii(corner);
  radius_item->SetDouble("xRadius", radius.x());
  radius_item->SetDouble("yRadius", radius.y());
  return radius_item;
}

String RrectTypeName(SkRRect::Type type) {
  switch (type) {
    case SkRRect::kEmpty_Type:
      return "Empty";
    case SkRRect::kRect_Type:
      return "Rect";
    case SkRRect::kOval_Type:
      return "Oval";
    case SkRRect::kSimple_Type:
      return "Simple";
    case SkRRect::kNinePatch_Type:
      return "Nine-patch";
    case SkRRect::kComplex_Type:
      return "Complex";
    default:
      NOTREACHED();
      return "?";
  };
}

String RadiusName(SkRRect::Corner corner) {
  switch (corner) {
    case SkRRect::kUpperLeft_Corner:
      return "upperLeftRadius";
    case SkRRect::kUpperRight_Corner:
      return "upperRightRadius";
    case SkRRect::kLowerRight_Corner:
      return "lowerRightRadius";
    case SkRRect::kLowerLeft_Corner:
      return "lowerLeftRadius";
    default:
      NOTREACHED();
      return "?";
  }
}

std::unique_ptr<JSONObject> ObjectForSkRRect(const SkRRect& rrect) {
  auto rrect_item = std::make_unique<JSONObject>();
  rrect_item->SetString("type", RrectTypeName(rrect.type()));
  rrect_item->SetDouble("left", rrect.rect().left());
  rrect_item->SetDouble("top", rrect.rect().top());
  rrect_item->SetDouble("right", rrect.rect().right());
  rrect_item->SetDouble("bottom", rrect.rect().bottom());
  for (int i = 0; i < 4; ++i)
    rrect_item->SetObject(RadiusName((SkRRect::Corner)i),
                          ObjectForRadius(rrect, (SkRRect::Corner)i));
  return rrect_item;
}

String FillTypeName(SkPath::FillType type) {
  switch (type) {
    case SkPath::kWinding_FillType:
      return "Winding";
    case SkPath::kEvenOdd_FillType:
      return "EvenOdd";
    case SkPath::kInverseWinding_FillType:
      return "InverseWinding";
    case SkPath::kInverseEvenOdd_FillType:
      return "InverseEvenOdd";
    default:
      NOTREACHED();
      return "?";
  };
}

String ConvexityName(SkPath::Convexity convexity) {
  switch (convexity) {
    case SkPath::kUnknown_Convexity:
      return "Unknown";
    case SkPath::kConvex_Convexity:
      return "Convex";
    case SkPath::kConcave_Convexity:
      return "Concave";
    default:
      NOTREACHED();
      return "?";
  };
}

VerbParams SegmentParams(SkPath::Verb verb) {
  switch (verb) {
    case SkPath::kMove_Verb:
      return VerbParams("Move", 1, 0);
    case SkPath::kLine_Verb:
      return VerbParams("Line", 1, 1);
    case SkPath::kQuad_Verb:
      return VerbParams("Quad", 2, 1);
    case SkPath::kConic_Verb:
      return VerbParams("Conic", 2, 1);
    case SkPath::kCubic_Verb:
      return VerbParams("Cubic", 3, 1);
    case SkPath::kClose_Verb:
      return VerbParams("Close", 0, 0);
    case SkPath::kDone_Verb:
      return VerbParams("Done", 0, 0);
    default:
      NOTREACHED();
      return VerbParams("?", 0, 0);
  };
}

std::unique_ptr<JSONObject> ObjectForSkPath(const SkPath& path) {
  auto path_item = std::make_unique<JSONObject>();
  path_item->SetString("fillType", FillTypeName(path.getFillType()));
  path_item->SetString("convexity", ConvexityName(path.getConvexity()));
  path_item->SetBoolean("isRect", path.isRect(nullptr));
  SkPath::Iter iter(path, false);
  SkPoint points[4];
  auto path_points_array = std::make_unique<JSONArray>();
  for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb;
       verb = iter.next(points, false)) {
    VerbParams verb_params = SegmentParams(verb);
    auto path_point_item = std::make_unique<JSONObject>();
    path_point_item->SetString("verb", verb_params.name);
    DCHECK_LE(verb_params.point_count + verb_params.point_offset,
              base::size(points));
    path_point_item->SetArray(
        "points", ArrayForSkPoints(verb_params.point_count,
                                   points + verb_params.point_offset));
    if (SkPath::kConic_Verb == verb)
      path_point_item->SetDouble("conicWeight", iter.conicWeight());
    path_points_array->PushObject(std::move(path_point_item));
  }
  path_item->SetArray("pathPoints", std::move(path_points_array));
  path_item->SetObject("bounds", ObjectForSkRect(path.getBounds()));
  return path_item;
}

String ColorTypeName(SkColorType color_type) {
  switch (color_type) {
    case kUnknown_SkColorType:
      return "None";
    case kAlpha_8_SkColorType:
      return "A8";
    case kRGB_565_SkColorType:
      return "RGB565";
    case kARGB_4444_SkColorType:
      return "ARGB4444";
    case kN32_SkColorType:
      return "ARGB8888";
    default:
      NOTREACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> ObjectForBitmapData(const SkBitmap& bitmap) {
  Vector<unsigned char> output;

  SkPixmap src;
  bool peekResult = bitmap.peekPixels(&src);
  DCHECK(peekResult);

  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  if (!ImageEncoder::Encode(&output, src, options)) {
    return nullptr;
  }

  auto data_item = std::make_unique<JSONObject>();
  data_item->SetString("base64", Base64Encode(output));
  data_item->SetString("mimeType", "image/png");
  return data_item;
}

std::unique_ptr<JSONObject> ObjectForSkBitmap(const SkBitmap& bitmap) {
  auto bitmap_item = std::make_unique<JSONObject>();
  bitmap_item->SetInteger("width", bitmap.width());
  bitmap_item->SetInteger("height", bitmap.height());
  bitmap_item->SetString("config", ColorTypeName(bitmap.colorType()));
  bitmap_item->SetBoolean("opaque", bitmap.isOpaque());
  bitmap_item->SetBoolean("immutable", bitmap.isImmutable());
  bitmap_item->SetBoolean("volatile", bitmap.isVolatile());
  bitmap_item->SetInteger("genID", bitmap.getGenerationID());
  bitmap_item->SetObject("data", ObjectForBitmapData(bitmap));
  return bitmap_item;
}

std::unique_ptr<JSONObject> ObjectForSkImage(const SkImage* image) {
  auto image_item = std::make_unique<JSONObject>();
  image_item->SetInteger("width", image->width());
  image_item->SetInteger("height", image->height());
  image_item->SetBoolean("opaque", image->isOpaque());
  image_item->SetInteger("uniqueID", image->uniqueID());
  return image_item;
}

std::unique_ptr<JSONArray> ArrayForSkMatrix(const SkMatrix& matrix) {
  auto matrix_array = std::make_unique<JSONArray>();
  for (int i = 0; i < 9; ++i)
    matrix_array->PushDouble(matrix[i]);
  return matrix_array;
}

std::unique_ptr<JSONObject> ObjectForSkShader(const SkShader& shader) {
  return std::make_unique<JSONObject>();
}

String StringForSkColor(SkColor color) {
  // #AARRGGBB.
  return String::Format("#%08X", color);
}

void AppendFlagToString(StringBuilder* flags_string,
                        bool is_set,
                        const StringView& name) {
  if (!is_set)
    return;
  if (flags_string->length())
    flags_string->Append("|");
  flags_string->Append(name);
}

String StringForSkPaintFlags(const SkPaint& paint) {
  if (!paint.isAntiAlias() && !paint.isDither())
    return "none";
  StringBuilder flags_string;
  AppendFlagToString(&flags_string, paint.isAntiAlias(), "AntiAlias");
  AppendFlagToString(&flags_string, paint.isDither(), "Dither");
  return flags_string.ToString();
}

String FilterQualityName(SkFilterQuality filter_quality) {
  switch (filter_quality) {
    case kNone_SkFilterQuality:
      return "None";
    case kLow_SkFilterQuality:
      return "Low";
    case kMedium_SkFilterQuality:
      return "Medium";
    case kHigh_SkFilterQuality:
      return "High";
    default:
      NOTREACHED();
      return "?";
  };
}

String StrokeCapName(SkPaint::Cap cap) {
  switch (cap) {
    case SkPaint::kButt_Cap:
      return "Butt";
    case SkPaint::kRound_Cap:
      return "Round";
    case SkPaint::kSquare_Cap:
      return "Square";
    default:
      NOTREACHED();
      return "?";
  };
}

String StrokeJoinName(SkPaint::Join join) {
  switch (join) {
    case SkPaint::kMiter_Join:
      return "Miter";
    case SkPaint::kRound_Join:
      return "Round";
    case SkPaint::kBevel_Join:
      return "Bevel";
    default:
      NOTREACHED();
      return "?";
  };
}

String StyleName(SkPaint::Style style) {
  switch (style) {
    case SkPaint::kFill_Style:
      return "Fill";
    case SkPaint::kStroke_Style:
      return "Stroke";
    case SkPaint::kStrokeAndFill_Style:
      return "StrokeAndFill";
    default:
      NOTREACHED();
      return "?";
  };
}

std::unique_ptr<JSONObject> ObjectForSkPaint(const SkPaint& paint) {
  auto paint_item = std::make_unique<JSONObject>();
  if (SkShader* shader = paint.getShader())
    paint_item->SetObject("shader", ObjectForSkShader(*shader));
  paint_item->SetString("color", StringForSkColor(paint.getColor()));
  paint_item->SetDouble("strokeWidth", paint.getStrokeWidth());
  paint_item->SetDouble("strokeMiter", paint.getStrokeMiter());
  paint_item->SetString("flags", StringForSkPaintFlags(paint));
  paint_item->SetString("filterLevel",
                        FilterQualityName(paint.getFilterQuality()));
  paint_item->SetString("strokeCap", StrokeCapName(paint.getStrokeCap()));
  paint_item->SetString("strokeJoin", StrokeJoinName(paint.getStrokeJoin()));
  paint_item->SetString("styleName", StyleName(paint.getStyle()));
  if (paint.getBlendMode() != SkBlendMode::kSrcOver)
    paint_item->SetString("blendMode", SkBlendMode_Name(paint.getBlendMode()));
  if (paint.getImageFilter())
    paint_item->SetString("imageFilter", "SkImageFilter");
  return paint_item;
}

String ClipOpName(SkClipOp op) {
  switch (op) {
    case SkClipOp::kDifference:
      return "kDifference_Op";
    case SkClipOp::kIntersect:
      return "kIntersect_Op";
    default:
      return "Unknown type";
  };
}

}  // namespace

class AutoLogger
    : InterceptingCanvasBase::CanvasInterceptorBase<LoggingCanvas> {
 public:
  explicit AutoLogger(LoggingCanvas* canvas)
      : InterceptingCanvasBase::CanvasInterceptorBase<LoggingCanvas>(canvas) {}

  JSONObject* LogItem(const String& name);
  JSONObject* LogItemWithParams(const String& name);
  ~AutoLogger() {
    if (TopLevelCall())
      Canvas()->log_->PushObject(std::move(log_item_));
  }

 private:
  std::unique_ptr<JSONObject> log_item_;
};

JSONObject* AutoLogger::LogItem(const String& name) {
  auto item = std::make_unique<JSONObject>();
  item->SetString("method", name);
  log_item_ = std::move(item);
  return log_item_.get();
}

JSONObject* AutoLogger::LogItemWithParams(const String& name) {
  JSONObject* item = LogItem(name);
  auto params = std::make_unique<JSONObject>();
  item->SetObject("params", std::move(params));
  return item->GetJSONObject("params");
}

LoggingCanvas::LoggingCanvas()
    : InterceptingCanvasBase(999999, 999999),
      log_(std::make_unique<JSONArray>()) {}

void LoggingCanvas::onDrawPaint(const SkPaint& paint) {
  AutoLogger logger(this);
  logger.LogItemWithParams("drawPaint")
      ->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawPaint(paint);
}

void LoggingCanvas::onDrawPoints(PointMode mode,
                                 size_t count,
                                 const SkPoint pts[],
                                 const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawPoints");
  params->SetString("pointMode", PointModeName(mode));
  params->SetArray("points", ArrayForSkPoints(count, pts));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawPoints(mode, count, pts, paint);
}

void LoggingCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawRect");
  params->SetObject("rect", ObjectForSkRect(rect));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawRect(rect, paint);
}

void LoggingCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawOval");
  params->SetObject("oval", ObjectForSkRect(oval));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawOval(oval, paint);
}

void LoggingCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawRRect");
  params->SetObject("rrect", ObjectForSkRRect(rrect));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawRRect(rrect, paint);
}

void LoggingCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawPath");
  params->SetObject("path", ObjectForSkPath(path));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawPath(path, paint);
}

void LoggingCanvas::onDrawBitmap(const SkBitmap& bitmap,
                                 SkScalar left,
                                 SkScalar top,
                                 const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawBitmap");
  params->SetDouble("left", left);
  params->SetDouble("top", top);
  params->SetObject("bitmap", ObjectForSkBitmap(bitmap));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  this->SkCanvas::onDrawBitmap(bitmap, left, top, paint);
}

void LoggingCanvas::onDrawBitmapRect(const SkBitmap& bitmap,
                                     const SkRect* src,
                                     const SkRect& dst,
                                     const SkPaint* paint,
                                     SrcRectConstraint constraint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawBitmapRectToRect");
  params->SetObject("bitmap", ObjectForSkBitmap(bitmap));
  if (src)
    params->SetObject("src", ObjectForSkRect(*src));
  params->SetObject("dst", ObjectForSkRect(dst));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  params->SetInteger("flags", constraint);
  this->SkCanvas::onDrawBitmapRect(bitmap, src, dst, paint, constraint);
}

void LoggingCanvas::onDrawBitmapNine(const SkBitmap& bitmap,
                                     const SkIRect& center,
                                     const SkRect& dst,
                                     const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawBitmapNine");
  params->SetObject("bitmap", ObjectForSkBitmap(bitmap));
  params->SetObject("center", ObjectForSkIRect(center));
  params->SetObject("dst", ObjectForSkRect(dst));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  this->SkCanvas::onDrawBitmapNine(bitmap, center, dst, paint);
}

void LoggingCanvas::onDrawImage(const SkImage* image,
                                SkScalar left,
                                SkScalar top,
                                const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawImage");
  params->SetDouble("left", left);
  params->SetDouble("top", top);
  params->SetObject("image", ObjectForSkImage(image));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  this->SkCanvas::onDrawImage(image, left, top, paint);
}

void LoggingCanvas::onDrawImageRect(const SkImage* image,
                                    const SkRect* src,
                                    const SkRect& dst,
                                    const SkPaint* paint,
                                    SrcRectConstraint constraint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawImageRect");
  params->SetObject("image", ObjectForSkImage(image));
  if (src)
    params->SetObject("src", ObjectForSkRect(*src));
  params->SetObject("dst", ObjectForSkRect(dst));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  this->SkCanvas::onDrawImageRect(image, src, dst, paint, constraint);
}

void LoggingCanvas::onDrawVerticesObject(const SkVertices* vertices,
                                         SkBlendMode bmode,
                                         const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawVertices");
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawVerticesObject(vertices, bmode, paint);
}

void LoggingCanvas::onDrawDRRect(const SkRRect& outer,
                                 const SkRRect& inner,
                                 const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawDRRect");
  params->SetObject("outer", ObjectForSkRRect(outer));
  params->SetObject("inner", ObjectForSkRRect(inner));
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawDRRect(outer, inner, paint);
}

void LoggingCanvas::onDrawTextBlob(const SkTextBlob* blob,
                                   SkScalar x,
                                   SkScalar y,
                                   const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawTextBlob");
  params->SetDouble("x", x);
  params->SetDouble("y", y);
  params->SetObject("paint", ObjectForSkPaint(paint));
  this->SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

void LoggingCanvas::onClipRect(const SkRect& rect,
                               SkClipOp op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRect");
  params->SetObject("rect", ObjectForSkRect(rect));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipRect(rect, op, style);
}

void LoggingCanvas::onClipRRect(const SkRRect& rrect,
                                SkClipOp op,
                                ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRRect");
  params->SetObject("rrect", ObjectForSkRRect(rrect));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipRRect(rrect, op, style);
}

void LoggingCanvas::onClipPath(const SkPath& path,
                               SkClipOp op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipPath");
  params->SetObject("path", ObjectForSkPath(path));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  this->SkCanvas::onClipPath(path, op, style);
}

void LoggingCanvas::onClipRegion(const SkRegion& region, SkClipOp op) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRegion");
  params->SetString("op", ClipOpName(op));
  this->SkCanvas::onClipRegion(region, op);
}

void LoggingCanvas::onDrawPicture(const SkPicture* picture,
                                  const SkMatrix* matrix,
                                  const SkPaint* paint) {
  this->UnrollDrawPicture(picture, matrix, paint, nullptr);
}

void LoggingCanvas::didSetMatrix(const SkMatrix& matrix) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("setMatrix");
  params->SetArray("matrix", ArrayForSkMatrix(matrix));
  this->SkCanvas::didSetMatrix(matrix);
}

void LoggingCanvas::didConcat(const SkMatrix& matrix) {
  AutoLogger logger(this);
  JSONObject* params;

  switch (matrix.getType()) {
    case SkMatrix::kTranslate_Mask:
      params = logger.LogItemWithParams("translate");
      params->SetDouble("dx", matrix.getTranslateX());
      params->SetDouble("dy", matrix.getTranslateY());
      break;

    case SkMatrix::kScale_Mask:
      params = logger.LogItemWithParams("scale");
      params->SetDouble("scaleX", matrix.getScaleX());
      params->SetDouble("scaleY", matrix.getScaleY());
      break;

    default:
      params = logger.LogItemWithParams("concat");
      params->SetArray("matrix", ArrayForSkMatrix(matrix));
  }
  this->SkCanvas::didConcat(matrix);
}

void LoggingCanvas::willSave() {
  AutoLogger logger(this);
  logger.LogItem("save");
  this->SkCanvas::willSave();
}

SkCanvas::SaveLayerStrategy LoggingCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("saveLayer");
  if (rec.fBounds)
    params->SetObject("bounds", ObjectForSkRect(*rec.fBounds));
  if (rec.fPaint)
    params->SetObject("paint", ObjectForSkPaint(*rec.fPaint));
  params->SetInteger("saveFlags", static_cast<int>(rec.fSaveLayerFlags));
  return this->SkCanvas::getSaveLayerStrategy(rec);
}

void LoggingCanvas::willRestore() {
  AutoLogger logger(this);
  logger.LogItem("restore");
  this->SkCanvas::willRestore();
}

std::unique_ptr<JSONArray> LoggingCanvas::Log() {
  return JSONArray::From(log_->Clone());
}

std::unique_ptr<JSONArray> RecordAsJSON(const PaintRecord& record) {
  LoggingCanvas canvas;
  record.Playback(&canvas);
  return canvas.Log();
}

String RecordAsDebugString(const PaintRecord& record) {
  return RecordAsJSON(record)->ToPrettyJSONString();
}

void ShowPaintRecord(const PaintRecord& record) {
  DLOG(INFO) << RecordAsDebugString(record).Utf8();
}

std::unique_ptr<JSONArray> SkPictureAsJSON(const SkPicture& picture) {
  LoggingCanvas canvas;
  picture.playback(&canvas);
  return canvas.Log();
}

String SkPictureAsDebugString(const SkPicture& picture) {
  return SkPictureAsJSON(picture)->ToPrettyJSONString();
}

void ShowSkPicture(const SkPicture& picture) {
  DLOG(INFO) << SkPictureAsDebugString(picture).Utf8();
}

}  // namespace blink
