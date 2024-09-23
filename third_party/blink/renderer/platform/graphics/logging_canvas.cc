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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"

#include <unicode/unistr.h>

#include "base/logging.h"
#include "build/build_config.h"
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
#include "ui/gfx/geometry/size.h"

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

String PointModeName(SkCanvas::PointMode mode) {
  switch (mode) {
    case SkCanvas::kPoints_PointMode:
      return "Points";
    case SkCanvas::kLines_PointMode:
      return "Lines";
    case SkCanvas::kPolygon_PointMode:
      return "Polygon";
    default:
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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

String FillTypeName(SkPathFillType type) {
  switch (type) {
    case SkPathFillType::kWinding:
      return "Winding";
    case SkPathFillType::kEvenOdd:
      return "EvenOdd";
    case SkPathFillType::kInverseWinding:
      return "InverseWinding";
    case SkPathFillType::kInverseEvenOdd:
      return "InverseEvenOdd";
    default:
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return VerbParams("?", 0, 0);
  };
}

std::unique_ptr<JSONObject> ObjectForSkPath(const SkPath& path) {
  auto path_item = std::make_unique<JSONObject>();
  path_item->SetString("fillType", FillTypeName(path.getFillType()));
  path_item->SetBoolean("convex", path.isConvex());
  path_item->SetBoolean("isRect", path.isRect(nullptr));
  SkPath::RawIter iter(path);
  SkPoint points[4];
  auto path_points_array = std::make_unique<JSONArray>();
  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    VerbParams verb_params = SegmentParams(verb);
    auto path_point_item = std::make_unique<JSONObject>();
    path_point_item->SetString("verb", verb_params.name);
    DCHECK_LE(verb_params.point_count + verb_params.point_offset,
              std::size(points));
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

std::unique_ptr<JSONObject> ObjectForSkImage(const SkImage* image) {
  auto image_item = std::make_unique<JSONObject>();
  image_item->SetInteger("width", image->width());
  image_item->SetInteger("height", image->height());
  image_item->SetBoolean("opaque", image->isOpaque());
  image_item->SetInteger("uniqueID", image->uniqueID());
  return image_item;
}

std::unique_ptr<JSONArray> ArrayForSkScalars(size_t count,
                                             const SkScalar array[]) {
  auto points_array_item = std::make_unique<JSONArray>();
  for (size_t i = 0; i < count; ++i)
    points_array_item->PushDouble(array[i]);
  return points_array_item;
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

String StrokeCapName(SkPaint::Cap cap) {
  switch (cap) {
    case SkPaint::kButt_Cap:
      return "Butt";
    case SkPaint::kRound_Cap:
      return "Round";
    case SkPaint::kSquare_Cap:
      return "Square";
    default:
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return "?";
  };
}

String StyleName(SkPaint::Style style) {
  switch (style) {
    case SkPaint::kFill_Style:
      return "Fill";
    case SkPaint::kStroke_Style:
      return "Stroke";
    default:
      NOTREACHED_IN_MIGRATION();
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
  paint_item->SetString("strokeCap", StrokeCapName(paint.getStrokeCap()));
  paint_item->SetString("strokeJoin", StrokeJoinName(paint.getStrokeJoin()));
  paint_item->SetString("styleName", StyleName(paint.getStyle()));
  const auto bm = paint.asBlendMode();
  if (bm != SkBlendMode::kSrcOver) {
    paint_item->SetString("blendMode",
                          bm ? SkBlendMode_Name(bm.value()) : "custom");
  }
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

String FilterModeName(SkFilterMode fm) {
  switch (fm) {
    case SkFilterMode::kNearest:
      return "kNearest";
    case SkFilterMode::kLinear:
      return "kLinear";
  }
  return "not reachable";
}

String MipmapModeName(SkMipmapMode mm) {
  switch (mm) {
    case SkMipmapMode::kNone:
      return "kNone";
    case SkMipmapMode::kNearest:
      return "kNearest";
    case SkMipmapMode::kLinear:
      return "kLinear";
  }
  return "not reachable";
}

std::unique_ptr<JSONObject> ObjectForSkSamplingOptions(
    const SkSamplingOptions& sampling) {
  auto sampling_item = std::make_unique<JSONObject>();
  if (sampling.useCubic) {
    sampling_item->SetDouble("B", sampling.cubic.B);
    sampling_item->SetDouble("C", sampling.cubic.C);
  } else {
    sampling_item->SetString("filter", FilterModeName(sampling.filter));
    sampling_item->SetString("mipmap", MipmapModeName(sampling.mipmap));
  }
  return sampling_item;
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
  SkCanvas::onDrawPaint(paint);
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
  SkCanvas::onDrawPoints(mode, count, pts, paint);
}

void LoggingCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawRect");
  params->SetObject("rect", ObjectForSkRect(rect));
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawRect(rect, paint);
}

void LoggingCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawOval");
  params->SetObject("oval", ObjectForSkRect(oval));
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawOval(oval, paint);
}

void LoggingCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawRRect");
  params->SetObject("rrect", ObjectForSkRRect(rrect));
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawRRect(rrect, paint);
}

void LoggingCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawPath");
  params->SetObject("path", ObjectForSkPath(path));
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawPath(path, paint);
}

void LoggingCanvas::onDrawImage2(const SkImage* image,
                                 SkScalar left,
                                 SkScalar top,
                                 const SkSamplingOptions& sampling,
                                 const SkPaint* paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawImage");
  params->SetDouble("left", left);
  params->SetDouble("top", top);
  params->SetObject("sampling", ObjectForSkSamplingOptions(sampling));
  params->SetObject("image", ObjectForSkImage(image));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  SkCanvas::onDrawImage2(image, left, top, sampling, paint);
}

void LoggingCanvas::onDrawImageRect2(const SkImage* image,
                                     const SkRect& src,
                                     const SkRect& dst,
                                     const SkSamplingOptions& sampling,
                                     const SkPaint* paint,
                                     SrcRectConstraint constraint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawImageRect");
  params->SetObject("image", ObjectForSkImage(image));
  params->SetObject("src", ObjectForSkRect(src));
  params->SetObject("dst", ObjectForSkRect(dst));
  params->SetObject("sampling", ObjectForSkSamplingOptions(sampling));
  if (paint)
    params->SetObject("paint", ObjectForSkPaint(*paint));
  SkCanvas::onDrawImageRect2(image, src, dst, sampling, paint, constraint);
}

void LoggingCanvas::onDrawVerticesObject(const SkVertices* vertices,
                                         SkBlendMode bmode,
                                         const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawVertices");
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawVerticesObject(vertices, bmode, paint);
}

void LoggingCanvas::onDrawDRRect(const SkRRect& outer,
                                 const SkRRect& inner,
                                 const SkPaint& paint) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("drawDRRect");
  params->SetObject("outer", ObjectForSkRRect(outer));
  params->SetObject("inner", ObjectForSkRRect(inner));
  params->SetObject("paint", ObjectForSkPaint(paint));
  SkCanvas::onDrawDRRect(outer, inner, paint);
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
  SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

void LoggingCanvas::onClipRect(const SkRect& rect,
                               SkClipOp op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRect");
  params->SetObject("rect", ObjectForSkRect(rect));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  SkCanvas::onClipRect(rect, op, style);
}

void LoggingCanvas::onClipRRect(const SkRRect& rrect,
                                SkClipOp op,
                                ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRRect");
  params->SetObject("rrect", ObjectForSkRRect(rrect));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  SkCanvas::onClipRRect(rrect, op, style);
}

void LoggingCanvas::onClipPath(const SkPath& path,
                               SkClipOp op,
                               ClipEdgeStyle style) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipPath");
  params->SetObject("path", ObjectForSkPath(path));
  params->SetString("SkRegion::Op", ClipOpName(op));
  params->SetBoolean("softClipEdgeStyle", kSoft_ClipEdgeStyle == style);
  SkCanvas::onClipPath(path, op, style);
}

void LoggingCanvas::onClipRegion(const SkRegion& region, SkClipOp op) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("clipRegion");
  params->SetString("op", ClipOpName(op));
  SkCanvas::onClipRegion(region, op);
}

void LoggingCanvas::onDrawPicture(const SkPicture* picture,
                                  const SkMatrix* matrix,
                                  const SkPaint* paint) {
  UnrollDrawPicture(picture, matrix, paint, nullptr);
}

void LoggingCanvas::didSetM44(const SkM44& matrix) {
  SkScalar m[16];
  matrix.getColMajor(m);
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("setMatrix");
  params->SetArray("matrix44", ArrayForSkScalars(16, m));
}

void LoggingCanvas::didConcat44(const SkM44& matrix) {
  SkScalar m[16];
  matrix.getColMajor(m);
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("concat44");
  params->SetArray("matrix44", ArrayForSkScalars(16, m));
}

void LoggingCanvas::didScale(SkScalar x, SkScalar y) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("scale");
  params->SetDouble("scaleX", x);
  params->SetDouble("scaleY", y);
}

void LoggingCanvas::didTranslate(SkScalar x, SkScalar y) {
  AutoLogger logger(this);
  JSONObject* params = logger.LogItemWithParams("translate");
  params->SetDouble("dx", x);
  params->SetDouble("dy", y);
}

void LoggingCanvas::willSave() {
  AutoLogger logger(this);
  logger.LogItem("save");
  SkCanvas::willSave();
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
  return SkCanvas::getSaveLayerStrategy(rec);
}

void LoggingCanvas::willRestore() {
  AutoLogger logger(this);
  logger.LogItem("restore");
  SkCanvas::willRestore();
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
