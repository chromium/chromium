// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/skia_benchmarking_extension.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/base64.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/render_thread_impl.h"
#include "gin/arguments.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "skia/ext/benchmarking_canvas.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace content {

namespace {

class Picture {
 public:
  gfx::Rect layer_rect;
  sk_sp<SkPicture> picture;
};

std::unique_ptr<base::Value> ParsePictureArg(v8::Isolate* isolate,
                                             v8::Local<v8::Value> arg) {
  return content::V8ValueConverter::Create()->FromV8Value(
      arg, isolate->GetCurrentContext());
}

std::unique_ptr<Picture> CreatePictureFromEncodedString(
    const std::string& encoded) {
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  sk_sp<SkPicture> skpicture =
      SkPicture::MakeFromData(decoded.data(), decoded.size());
  if (!skpicture)
    return nullptr;

  std::unique_ptr<Picture> picture(new Picture);
  picture->layer_rect = gfx::SkIRectToRect(skpicture->cullRect().roundOut());
  picture->picture = std::move(skpicture);
  return picture;
}

std::unique_ptr<Picture> ParsePictureStr(v8::Isolate* isolate,
                                         v8::Local<v8::Value> arg) {
  std::unique_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return nullptr;
  // Decode the picture from base64.
  const std::string* encoded = picture_value->GetIfString();
  return encoded ? CreatePictureFromEncodedString(*encoded) : nullptr;
}

std::unique_ptr<Picture> ParsePictureHash(v8::Isolate* isolate,
                                          v8::Local<v8::Value> arg) {
  std::unique_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value || !picture_value->is_dict())
    return nullptr;
  // Decode the picture from base64.
  std::string* encoded = picture_value->GetDict().FindString("skp64");
  if (!encoded)
    return nullptr;
  return CreatePictureFromEncodedString(std::move(*encoded));
}

class PicturePlaybackController : public SkPicture::AbortCallback {
 public:
  PicturePlaybackController(const skia::BenchmarkingCanvas& canvas,
                            size_t count)
      : canvas_(canvas), playback_count_(count) {}

  bool abort() override { return canvas_->CommandCount() > playback_count_; }

 private:
  const raw_ref<const skia::BenchmarkingCanvas> canvas_;
  size_t playback_count_;
};

}  // namespace

gin::WrapperInfo SkiaBenchmarking::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void SkiaBenchmarking::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<SkiaBenchmarking> controller =
      gin::CreateHandle(isolate, new SkiaBenchmarking());
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);
  chrome
      ->Set(context, gin::StringToV8(isolate, "skiaBenchmarking"),
            controller.ToV8())
      .Check();
}

// static
void SkiaBenchmarking::Initialize() {
  DCHECK(RenderThreadImpl::current());
  // FIXME: remove this after Skia updates SkGraphics::Init() to be
  //        thread-safe and idempotent.
  static bool skia_initialized = false;
  if (!skia_initialized) {
    LOG(WARNING) << "Enabling unsafe Skia benchmarking extension.";
    SkGraphics::Init();
    skia_initialized = true;
  }
}

SkiaBenchmarking::SkiaBenchmarking() {
  Initialize();
}

SkiaBenchmarking::~SkiaBenchmarking() {}

gin::ObjectTemplateBuilder SkiaBenchmarking::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SkiaBenchmarking>::GetObjectTemplateBuilder(isolate)
      .SetMethod("rasterize", &SkiaBenchmarking::Rasterize)
      .SetMethod("getOps", &SkiaBenchmarking::GetOps)
      .SetMethod("getOpTimings", &SkiaBenchmarking::GetOpTimings)
      .SetMethod("getInfo", &SkiaBenchmarking::GetInfo);
}

void SkiaBenchmarking::Rasterize(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  std::unique_ptr<Picture> picture = ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  double scale = 1.0;
  gfx::Rect clip_rect(picture->layer_rect);
  int stop_index = -1;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> params;
    args->GetNext(&params);
    std::unique_ptr<base::Value> params_value =
        content::V8ValueConverter::Create()->FromV8Value(params, context);

    if (params_value && params_value->is_dict()) {
      const base::Value::Dict& params_dict = params_value->GetDict();
      scale = params_dict.FindDouble("scale").value_or(scale);
      if (std::optional<int> stop = params_dict.FindInt("stop")) {
        stop_index = *stop;
      }

      if (const base::Value* clip_value = params_dict.Find("clip"))
        cc::MathUtil::FromValue(clip_value, &clip_rect);
    }
  }

  clip_rect.Intersect(picture->layer_rect);
  gfx::Rect snapped_clip = gfx::ScaleToEnclosingRect(clip_rect, scale);

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(snapped_clip.width(), snapped_clip.height()))
    return;
  bitmap.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.translate(SkIntToScalar(-clip_rect.x()),
                   SkIntToScalar(-clip_rect.y()));
  canvas.clipRect(gfx::RectToSkRect(snapped_clip));
  canvas.scale(scale, scale);
  canvas.translate(picture->layer_rect.x(), picture->layer_rect.y());

  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);
  size_t playback_count =
      (stop_index < 0) ? std::numeric_limits<size_t>::max() : stop_index;
  PicturePlaybackController controller(benchmarking_canvas, playback_count);
  picture->picture->playback(&benchmarking_canvas, &controller);

  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::Create(bitmap.computeByteSize(), 1);
  uint32_t* packed_pixels = reinterpret_cast<uint32_t*>(bitmap.getPixels());
  uint8_t* buffer_pixels = reinterpret_cast<uint8_t*>(buffer.Data());
  // Swizzle from native Skia format to RGBA as we copy out.
  for (size_t i = 0; i < bitmap.computeByteSize(); i += 4) {
    uint32_t c = packed_pixels[i >> 2];
    buffer_pixels[i] = SkGetPackedR32(c);
    buffer_pixels[i + 1] = SkGetPackedG32(c);
    buffer_pixels[i + 2] = SkGetPackedB32(c);
    buffer_pixels[i + 3] = SkGetPackedA32(c);
  }

  args->Return(gin::DataObjectBuilder(isolate)
                   .Set("width", snapped_clip.width())
                   .Set("height", snapped_clip.height())
                   .Set("data", blink::WebArrayBufferConverter::ToV8Value(
                                    &buffer, isolate))
                   .Build());
}

void SkiaBenchmarking::GetOps(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  std::unique_ptr<Picture> picture = ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  SkCanvas canvas(picture->layer_rect.width(), picture->layer_rect.height(),
                  &props);
  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);
  picture->picture->playback(&benchmarking_canvas);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  args->Return(content::V8ValueConverter::Create()->ToV8Value(
      benchmarking_canvas.Commands(), context));
}

void SkiaBenchmarking::GetOpTimings(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  std::unique_ptr<Picture> picture = ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  gfx::Rect bounds = picture->layer_rect;

  // Measure the total time by drawing straight into a bitmap-backed canvas.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(bounds.width(), bounds.height());
  SkCanvas bitmap_canvas(bitmap, SkSurfaceProps{});
  bitmap_canvas.clear(SK_ColorTRANSPARENT);
  base::TimeTicks t0 = base::TimeTicks::Now();
  picture->picture->playback(&bitmap_canvas);
  base::TimeDelta total_time = base::TimeTicks::Now() - t0;

  // Gather per-op timing info by drawing into a BenchmarkingCanvas.
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SK_ColorTRANSPARENT);
  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);
  picture->picture->playback(&benchmarking_canvas);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> op_times =
      v8::Array::New(isolate, benchmarking_canvas.CommandCount());
  for (size_t i = 0; i < benchmarking_canvas.CommandCount(); ++i) {
    op_times
        ->CreateDataProperty(
            context, i,
            v8::Number::New(isolate, benchmarking_canvas.GetTime(i)))
        .Check();
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "total_time",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            v8::Number::New(isolate, total_time.InMillisecondsF()))
      .Check();
  result
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "cmd_times",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            op_times)
      .Check();

  args->Return(result);
}

void SkiaBenchmarking::GetInfo(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  std::unique_ptr<Picture> picture = ParsePictureStr(isolate, picture_handle);
  if (!picture.get())
    return;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "width",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            v8::Number::New(isolate, picture->layer_rect.width()))
      .Check();
  result
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "height",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            v8::Number::New(isolate, picture->layer_rect.height()))
      .Check();

  args->Return(result);
}

} // namespace content
