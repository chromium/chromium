// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu_benchmarking_extension.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/layer_tree_host.h"
#include "content/common/input/actions_parser.h"
#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/modules/canvas/canvas_test_utils.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_image_cache.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/docs/SkMultiPictureDocument.h"
#include "third_party/skia/include/docs/SkXPSDocument.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/geometry/size_f.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"

#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
// XpsObjectModel.h indirectly includes <wincrypt.h> which is
// incompatible with Chromium's OpenSSL. By including wincrypt_shim.h
// first, problems are avoided.
// clang-format off
#include "base/win/wincrypt_shim.h"
// clang-format on

#include <objbase.h>

#include <XpsObjectModel.h>
#include <wrl/client.h>
#endif

namespace blink {

// This class allows us to access the LayerTreeHost on WebFrameWidget. It is
// hidden from the public interface. It also extracts some commonly
// used objects from RenderFrameImpl.
class GpuBenchmarkingContext {
 public:
  explicit GpuBenchmarkingContext(content::RenderFrameImpl* frame)
      : web_frame_(frame->GetWebFrame()),
        web_view_(web_frame_->View()),
        frame_widget_(frame->GetLocalRootWebFrameWidget()),
        layer_tree_host_(frame_widget_->LayerTreeHost()) {}

  GpuBenchmarkingContext(const GpuBenchmarkingContext&) = delete;
  GpuBenchmarkingContext& operator=(const GpuBenchmarkingContext&) = delete;

  WebLocalFrame* web_frame() const {
    DCHECK(web_frame_ != nullptr);
    return web_frame_;
  }
  WebView* web_view() const {
    DCHECK(web_view_ != nullptr);
    return web_view_;
  }
  WebFrameWidget* frame_widget() const { return frame_widget_; }
  cc::LayerTreeHost* layer_tree_host() const {
    DCHECK(layer_tree_host_ != nullptr);
    return layer_tree_host_;
  }

 private:
  raw_ptr<WebLocalFrame> web_frame_;
  raw_ptr<WebView> web_view_;
  raw_ptr<WebFrameWidget> frame_widget_;
  raw_ptr<cc::LayerTreeHost> layer_tree_host_;
};

}  // namespace blink

using blink::GpuBenchmarkingContext;
using blink::WebImageCache;
using blink::WebLocalFrame;
using blink::WebView;

namespace content {

namespace {

int GestureSourceTypeAsInt(content::mojom::GestureSourceType type) {
  switch (type) {
    case content::mojom::GestureSourceType::kDefaultInput:
      return 0;
    case content::mojom::GestureSourceType::kTouchInput:
      return 1;
    case content::mojom::GestureSourceType::kMouseInput:
      return 2;
    case content::mojom::GestureSourceType::kPenInput:
      return 3;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

class SkPictureSerializer {
 public:
  explicit SkPictureSerializer(const base::FilePath& dirpath)
      : dirpath_(dirpath), layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    SkiaBenchmarking::Initialize();
  }

  // Recursively serializes the layer tree.
  // Each layer in the tree is serialized into a separate skp file
  // in the given directory.
  void Serialize(const cc::Layer* root_layer) {
    for (auto* layer : *root_layer->layer_tree_host()) {
      sk_sp<const SkPicture> picture = layer->GetPicture();
      if (!picture)
        continue;

      // Serialize picture to file.
      // TODO(alokp): Note that for this to work Chrome needs to be launched
      // with
      // --no-sandbox command-line flag. Get rid of this limitation.
      // CRBUG: 139640.
      std::string filename =
          "layer_" + base::NumberToString(layer_id_++) + ".skp";
      std::string filepath = dirpath_.AppendASCII(filename).MaybeAsASCII();
      DCHECK(!filepath.empty());
      SkFILEWStream file(filepath.c_str());
      DCHECK(file.isValid());

      SkSerialProcs procs{
          .fImageProc = [](SkImage* img, void*) -> sk_sp<SkData> {
            // Note: if the picture contains texture-backed (gpu) images, they
            // will fail to be read-back and therefore fail to be encoded unless
            // we can thread the correct GrDirectContext through to here.
            return SkPngEncoder::Encode(nullptr, img, SkPngEncoder::Options{});
          }};
      auto data = picture->serialize(&procs);
      file.write(data->data(), data->size());
      file.fsync();
    }
  }

 private:
  base::FilePath dirpath_;
  int layer_id_;
};

template <typename T>
bool GetArg(gin::Arguments* args, T* value) {
  if (!args->GetNext(value)) {
    args->ThrowError();
    return false;
  }
  return true;
}

template <>
bool GetArg(gin::Arguments* args, int* value) {
  float number;
  bool ret = GetArg(args, &number);
  *value = number;
  return ret;
}

template <typename T>
bool GetOptionalArg(gin::Arguments* args, T* value) {
  if (args->PeekNext().IsEmpty())
    return true;
  if (args->PeekNext()->IsUndefined()) {
    args->Skip();
    return true;
  }
  return GetArg(args, value);
}

class CallbackAndContext : public base::RefCounted<CallbackAndContext> {
 public:
  CallbackAndContext(v8::Isolate* isolate,
                     v8::Local<v8::Function> callback,
                     v8::Local<v8::Context> context)
      : isolate_(isolate) {
    callback_.Reset(isolate_, callback);
    context_.Reset(isolate_, context);
  }

  CallbackAndContext(const CallbackAndContext&) = delete;
  CallbackAndContext& operator=(const CallbackAndContext&) = delete;

  v8::Isolate* isolate() { return isolate_; }

  v8::Local<v8::Function> GetCallback() {
    return v8::Local<v8::Function>::New(isolate_, callback_);
  }

  v8::Local<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  friend class base::RefCounted<CallbackAndContext>;

  virtual ~CallbackAndContext() {
    callback_.Reset();
    context_.Reset();
  }

  raw_ptr<v8::Isolate> isolate_;
  v8::Persistent<v8::Function> callback_;
  v8::Persistent<v8::Context> context_;
};

void RunCallbackHelper(CallbackAndContext* callback_and_context,
                       std::optional<base::Value> value) {
  v8::Isolate* isolate = callback_and_context->isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = callback_and_context->GetContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback = callback_and_context->GetCallback();
  WebLocalFrame* frame = WebLocalFrame::FrameForContext(context);

  if (frame && !callback.IsEmpty()) {
    if (value.has_value()) {
      v8::Local<v8::Value> v8_value =
          V8ValueConverter::Create()->ToV8Value(*value, context);
      v8::Local<v8::Value> argv[] = {v8_value};
      frame->CallFunctionEvenIfScriptDisabled(
          callback, v8::Object::New(isolate), /*argc=*/1, argv);
    } else {
      frame->CallFunctionEvenIfScriptDisabled(
          callback, v8::Object::New(isolate), 0, nullptr);
    }
  }
}

void OnMicroBenchmarkCompleted(CallbackAndContext* callback_and_context,
                               base::Value::Dict result) {
  RunCallbackHelper(callback_and_context,
                    std::optional<base::Value>(std::move(result)));
}

#if BUILDFLAG(IS_MAC)
void OnSwapCompletedWithCoreAnimationErrorCode(
    CallbackAndContext* callback_and_context,
    gfx::CALayerResult error_code) {
  RunCallbackHelper(callback_and_context,
                    std::optional<base::Value>(base::Value(error_code)));
}
#endif

void OnSyntheticGestureCompleted(CallbackAndContext* callback_and_context) {
  RunCallbackHelper(callback_and_context, /*value=*/{});
}

bool ThrowIfPointOutOfBounds(GpuBenchmarkingContext* context,
                             gin::Arguments* args,
                             const gfx::Point& point,
                             const std::string& message) {
  gfx::Rect rect = context->frame_widget()->ViewRect();
  rect -= rect.OffsetFromOrigin();

  // If the bounds are not available here, as is the case with an OOPIF,
  // for now, we will forgo the renderer-side bounds check.
  if (rect.IsEmpty())
    return false;

  if (!rect.Contains(point)) {
    args->ThrowTypeError(message);
    return true;
  }

  return false;
}

std::optional<gfx::Vector2dF> ToVector(const std::string& direction,
                                       float distance) {
  if (direction == "down") {
    return gfx::Vector2dF(0, distance);
  } else if (direction == "up") {
    return gfx::Vector2dF(0, -distance);
  } else if (direction == "right") {
    return gfx::Vector2dF(distance, 0);
  } else if (direction == "left") {
    return gfx::Vector2dF(-distance, 0);
  } else if (direction == "upleft") {
    return gfx::Vector2dF(-distance, -distance);
  } else if (direction == "upright") {
    return gfx::Vector2dF(distance, -distance);
  } else if (direction == "downleft") {
    return gfx::Vector2dF(-distance, distance);
  } else if (direction == "downright") {
    return gfx::Vector2dF(distance, distance);
  }
  return std::nullopt;
}

int ToKeyModifiers(std::string_view key) {
  if (key == "Alt")
    return blink::WebInputEvent::kAltKey;
  if (key == "Control")
    return blink::WebInputEvent::kControlKey;
  if (key == "Meta")
    return blink::WebInputEvent::kMetaKey;
  if (key == "Shift")
    return blink::WebInputEvent::kShiftKey;
  if (key == "CapsLock")
    return blink::WebInputEvent::kCapsLockOn;
  if (key == "NumLock")
    return blink::WebInputEvent::kNumLockOn;
  if (key == "AltGraph")
    return blink::WebInputEvent::kAltGrKey;
  NOTREACHED_IN_MIGRATION() << "invalid key modifier";
  return 0;
}

int ToButtonModifiers(std::string_view button) {
  if (button == "Left")
    return blink::WebMouseEvent::kLeftButtonDown;
  if (button == "Middle")
    return blink::WebMouseEvent::kMiddleButtonDown;
  if (button == "Right")
    return blink::WebMouseEvent::kRightButtonDown;
  if (button == "Back")
    return blink::WebMouseEvent::kBackButtonDown;
  if (button == "Forward")
    return blink::WebMouseEvent::kForwardButtonDown;
  NOTREACHED_IN_MIGRATION() << "invalid button modifier";
  return 0;
}

// BeginSmoothScroll takes pixels_to_scroll_x and pixels_to_scroll_y, positive
// pixels_to_scroll_y means scroll down, positive pixels_to_scroll_x means
// scroll right.
bool BeginSmoothScroll(GpuBenchmarkingContext* context,
                       gin::Arguments* args,
                       const mojo::Remote<mojom::InputInjector>& injector,
                       const gfx::Vector2dF& pixels_to_scroll,
                       v8::Local<v8::Function> callback,
                       int gesture_source_type,
                       float speed_in_pixels_s,
                       bool prevent_fling,
                       float start_x,
                       float start_y,
                       const gfx::Vector2dF& fling_velocity,
                       bool precise_scrolling_deltas,
                       bool scroll_by_page,
                       bool cursor_visible,
                       bool scroll_by_percentage,
                       int modifiers,
                       float vsync_offset_ms,
                       int input_event_pattern) {
  DCHECK(!(precise_scrolling_deltas && scroll_by_page));
  DCHECK(!(precise_scrolling_deltas && scroll_by_percentage));
  DCHECK(!(scroll_by_page && scroll_by_percentage));
  if (ThrowIfPointOutOfBounds(context, args, gfx::Point(start_x, start_y),
                              "Start point not in bounds")) {
    return false;
  }

  if (gesture_source_type ==
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kMouseInput)) {
    // Ensure the mouse is visible and move to start position, in case it will
    // trigger any hover or mousemove effects.
    context->web_view()->SetIsActive(true);
    blink::WebMouseEvent mouseMove(blink::WebInputEvent::Type::kMouseMove,
                                   modifiers, ui::EventTimeForNow());
    mouseMove.SetPositionInWidget(start_x, start_y);
    CHECK(context->web_view()->MainFrameWidget());
    context->web_view()->MainFrameWidget()->HandleInputEvent(
        blink::WebCoalescedInputEvent(mouseMove, ui::LatencyInfo()));
    context->web_view()->MainFrameWidget()->SetCursorVisibilityState(
        cursor_visible);
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context->web_frame()->MainWorldScriptContext());

  SyntheticSmoothScrollGestureParams gesture_params;

  if (gesture_source_type < 0 ||
      gesture_source_type >
          GestureSourceTypeAsInt(
              content::mojom::GestureSourceType::kGestureSourceTypeMax)) {
    return false;
  }
  gesture_params.gesture_source_type =
      static_cast<content::mojom::GestureSourceType>(gesture_source_type);

  gesture_params.speed_in_pixels_s = speed_in_pixels_s;
  gesture_params.vsync_offset_ms = vsync_offset_ms;
  gesture_params.input_event_pattern =
      static_cast<content::mojom::InputEventPattern>(input_event_pattern);
  gesture_params.prevent_fling = prevent_fling;

  if (scroll_by_page)
    gesture_params.granularity = ui::ScrollGranularity::kScrollByPage;
  else if (precise_scrolling_deltas)
    gesture_params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;
  else if (scroll_by_percentage)
    gesture_params.granularity = ui::ScrollGranularity::kScrollByPercentage;
  else
    gesture_params.granularity = ui::ScrollGranularity::kScrollByPixel;

  gesture_params.anchor.SetPoint(start_x, start_y);

  DCHECK(gesture_source_type !=
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kTouchInput) ||
         fling_velocity.IsZero());
  // Positive pixels_to_scroll_y means scroll down, positive pixels_to_scroll_x
  // means scroll right, but SyntheticSmoothScrollGestureParams requests
  // Positive X/Y to scroll left/up, which is opposite. Positive
  // fling_velocity_x and fling_velocity_y means scroll left and up, which is
  // the same direction with SyntheticSmoothScrollGestureParams.
  gesture_params.fling_velocity_x = fling_velocity.x();
  gesture_params.fling_velocity_y = fling_velocity.y();
  gesture_params.distances.push_back(-pixels_to_scroll);

  gesture_params.modifiers = modifiers;

  injector->QueueSyntheticSmoothScroll(
      gesture_params, base::BindOnce(&OnSyntheticGestureCompleted,
                                     base::RetainedRef(callback_and_context)));

  return true;
}

bool BeginSmoothDrag(GpuBenchmarkingContext* context,
                     gin::Arguments* args,
                     const mojo::Remote<mojom::InputInjector>& injector,
                     float start_x,
                     float start_y,
                     float end_x,
                     float end_y,
                     v8::Local<v8::Function> callback,
                     int gesture_source_type,
                     float speed_in_pixels_s,
                     float vsync_offset_ms,
                     int input_event_pattern) {
  if (ThrowIfPointOutOfBounds(context, args, gfx::Point(start_x, start_y),
                              "Start point not in bounds")) {
    return false;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context->web_frame()->MainWorldScriptContext());

  SyntheticSmoothDragGestureParams gesture_params;

  gesture_params.start_point.SetPoint(start_x, start_y);
  gfx::PointF end_point(end_x, end_y);
  gfx::Vector2dF distance = end_point - gesture_params.start_point;
  gesture_params.distances.push_back(distance);
  gesture_params.speed_in_pixels_s = speed_in_pixels_s;
  gesture_params.vsync_offset_ms = vsync_offset_ms;
  gesture_params.input_event_pattern =
      static_cast<content::mojom::InputEventPattern>(input_event_pattern);
  gesture_params.gesture_source_type =
      static_cast<content::mojom::GestureSourceType>(gesture_source_type);

  injector->QueueSyntheticSmoothDrag(
      gesture_params, base::BindOnce(&OnSyntheticGestureCompleted,
                                     base::RetainedRef(callback_and_context)));

  return true;
}

static void PrintDocument(blink::WebLocalFrame* frame, SkDocument* doc) {
  const float kPageWidth = 612.0f;   // 8.5 inch
  const float kPageHeight = 792.0f;  // 11 inch
  const float kMarginTop = 29.0f;    // 0.40 inch
  const float kMarginLeft = 29.0f;   // 0.40 inch
  const int kContentWidth = 555;     // 7.71 inch
  const int kContentHeight = 735;    // 10.21 inch
  blink::WebPrintParams params(gfx::SizeF(kContentWidth, kContentHeight));
  params.printer_dpi = 300;
  uint32_t page_count = frame->PrintBegin(params, blink::WebNode());
  for (uint32_t i = 0; i < page_count; ++i) {
    SkCanvas* sk_canvas = doc->beginPage(kPageWidth, kPageHeight);
    cc::SkiaPaintCanvas canvas(sk_canvas);
    cc::PaintCanvasAutoRestore auto_restore(&canvas, true);
    canvas.translate(kMarginLeft, kMarginTop);
    frame->PrintPage(i, &canvas);
  }
  frame->PrintEnd();
}

static void PrintDocumentTofile(v8::Isolate* isolate,
                                const std::string& filename,
                                sk_sp<SkDocument> (*make_doc)(SkWStream*),
                                RenderFrameImpl* render_frame) {
  GpuBenchmarkingContext context(render_frame);

  base::FilePath path = base::FilePath::FromUTF8Unsafe(filename);
  if (!base::PathIsWritable(path.DirName())) {
    std::string msg("Path is not writable: ");
    msg.append(path.DirName().MaybeAsASCII());
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, msg.c_str(),
                                v8::NewStringType::kNormal, msg.length())
            .ToLocalChecked()));
    return;
  }
  SkFILEWStream wStream(path.MaybeAsASCII().c_str());
  sk_sp<SkDocument> doc = make_doc(&wStream);
  if (doc) {
    context.web_frame()->View()->GetSettings()->SetShouldPrintBackgrounds(true);
    PrintDocument(context.web_frame(), doc.get());
    doc->close();
  }
}

void OnSwapCompletedHelper(CallbackAndContext* callback_and_context,
                           const viz::FrameTimingDetails&) {
  RunCallbackHelper(callback_and_context, /*value=*/{});
}

// This function is only used for correctness testing of this experimental
// feature; no need for it in release builds.
// Also note:  You must execute Chrome with `--no-sandbox` and
// `--enable-gpu-benchmarking` for this to work.
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
static sk_sp<SkDocument> MakeXPSDocument(SkWStream* s) {
  // I am not sure why this hasn't been initialized yet.
  std::ignore = CoInitializeEx(
      nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  // In non-sandboxed mode, we will need to create and hold on to the
  // factory before entering the sandbox.
  Microsoft::WRL::ComPtr<IXpsOMObjectFactory> factory;
  HRESULT hr = ::CoCreateInstance(CLSID_XpsOMObjectFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) {
    LOG(ERROR) << "CoCreateInstance(CLSID_XpsOMObjectFactory, ...) failed:"
               << logging::SystemErrorCodeToString(hr);
  }
  return SkXPS::MakeDocument(s, factory.Get());
}
#endif
}  // namespace

gin::WrapperInfo GpuBenchmarking::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void GpuBenchmarking::Install(base::WeakPtr<RenderFrameImpl> frame) {
  v8::Isolate* isolate =
      frame->GetWebFrame()->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GpuBenchmarking> controller =
      gin::CreateHandle(isolate, new GpuBenchmarking(frame));
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate, context);
  chrome
      ->Set(context, gin::StringToV8(isolate, "gpuBenchmarking"),
            controller.ToV8())
      .Check();
}

GpuBenchmarking::GpuBenchmarking(base::WeakPtr<RenderFrameImpl> frame)
    : render_frame_(std::move(frame)) {}

GpuBenchmarking::~GpuBenchmarking() = default;

void GpuBenchmarking::EnsureRemoteInterface() {
  if (!input_injector_) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        input_injector_.BindNewPipeAndPassReceiver(
            render_frame_->GetTaskRunner(blink::TaskType::kInternalDefault)));
  }
}

gin::ObjectTemplateBuilder GpuBenchmarking::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GpuBenchmarking>::GetObjectTemplateBuilder(isolate)
      .SetMethod("setNeedsDisplayOnAllLayers",
                 &GpuBenchmarking::SetNeedsDisplayOnAllLayers)
      .SetMethod("setRasterizeOnlyVisibleContent",
                 &GpuBenchmarking::SetRasterizeOnlyVisibleContent)
      .SetMethod("printToSkPicture", &GpuBenchmarking::PrintToSkPicture)
      .SetMethod("printPagesToSkPictures",
                 &GpuBenchmarking::PrintPagesToSkPictures)
      .SetMethod("printPagesToXPS", &GpuBenchmarking::PrintPagesToXPS)
      .SetValue("DEFAULT_INPUT",
                GestureSourceTypeAsInt(
                    content::mojom::GestureSourceType::kDefaultInput))
      .SetValue("TOUCH_INPUT",
                GestureSourceTypeAsInt(
                    content::mojom::GestureSourceType::kTouchInput))
      .SetValue("MOUSE_INPUT",
                GestureSourceTypeAsInt(
                    content::mojom::GestureSourceType::kMouseInput))
      .SetValue("TOUCHPAD_INPUT",
                GestureSourceTypeAsInt(
                    content::mojom::GestureSourceType::kTouchpadInput))
      .SetValue("PEN_INPUT", GestureSourceTypeAsInt(
                                 content::mojom::GestureSourceType::kPenInput))
      .SetValue(
          "DEFAULT_INPUT_PATTERN",
          static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern))
      .SetValue(
          "ONE_PER_VSYNC_INPUT_PATTERN",
          static_cast<int>(content::mojom::InputEventPattern::kOnePerVsync))
      .SetValue(
          "TWO_PER_VSYNC_INPUT_PATTERN",
          static_cast<int>(content::mojom::InputEventPattern::kTwoPerVsync))
      .SetValue(
          "EVERY_OTHER_VSYNC_INPUT_PATTERN",
          static_cast<int>(content::mojom::InputEventPattern::kEveryOtherVsync))
      .SetMethod("gestureSourceTypeSupported",
                 &GpuBenchmarking::GestureSourceTypeSupported)
      .SetMethod("smoothScrollBy", &GpuBenchmarking::SmoothScrollBy)
      .SetMethod("smoothScrollByXY", &GpuBenchmarking::SmoothScrollByXY)
      .SetMethod("smoothDrag", &GpuBenchmarking::SmoothDrag)
      .SetMethod("swipe", &GpuBenchmarking::Swipe)
      .SetMethod("scrollBounce", &GpuBenchmarking::ScrollBounce)
      .SetMethod("pinchBy", &GpuBenchmarking::PinchBy)
      .SetMethod("pageScaleFactor", &GpuBenchmarking::PageScaleFactor)
      .SetMethod("setPageScaleFactor", &GpuBenchmarking::SetPageScaleFactor)
      .SetMethod("setBrowserControlsShown",
                 &GpuBenchmarking::SetBrowserControlsShown)
      .SetMethod("tap", &GpuBenchmarking::Tap)
      .SetMethod("pointerActionSequence",
                 &GpuBenchmarking::PointerActionSequence)
      .SetMethod("visualViewportX", &GpuBenchmarking::VisualViewportX)
      .SetMethod("visualViewportY", &GpuBenchmarking::VisualViewportY)
      .SetMethod("visualViewportHeight", &GpuBenchmarking::VisualViewportHeight)
      .SetMethod("visualViewportWidth", &GpuBenchmarking::VisualViewportWidth)
      .SetMethod("clearImageCache", &GpuBenchmarking::ClearImageCache)
      .SetMethod("runMicroBenchmark", &GpuBenchmarking::RunMicroBenchmark)
      .SetMethod("sendMessageToMicroBenchmark",
                 &GpuBenchmarking::SendMessageToMicroBenchmark)
      .SetMethod("hasGpuChannel", &GpuBenchmarking::HasGpuChannel)
      .SetMethod("hasGpuProcess", &GpuBenchmarking::HasGpuProcess)
      .SetMethod("crashGpuProcess", &GpuBenchmarking::CrashGpuProcess)
      .SetMethod("terminateGpuProcessNormally",
                 &GpuBenchmarking::TerminateGpuProcessNormally)
      .SetMethod("getGpuDriverBugWorkarounds",
                 &GpuBenchmarking::GetGpuDriverBugWorkarounds)
      .SetMethod("startProfiling", &GpuBenchmarking::StartProfiling)
      .SetMethod("stopProfiling", &GpuBenchmarking::StopProfiling)
      .SetMethod("freeze", &GpuBenchmarking::Freeze)
#if BUILDFLAG(IS_MAC)
      .SetMethod("addCoreAnimationStatusEventListener",
                 &GpuBenchmarking::AddCoreAnimationStatusEventListener)
#endif
      .SetMethod("addSwapCompletionEventListener",
                 &GpuBenchmarking::AddSwapCompletionEventListener)
      .SetMethod("isAcceleratedCanvasImageSource",
                 &GpuBenchmarking::IsAcceleratedCanvasImageSource);
}

void GpuBenchmarking::SetNeedsDisplayOnAllLayers() {
  GpuBenchmarkingContext context(render_frame_.get());
  context.layer_tree_host()->SetNeedsDisplayOnAllLayers();
}

void GpuBenchmarking::SetRasterizeOnlyVisibleContent() {
  GpuBenchmarkingContext context(render_frame_.get());
  cc::LayerTreeDebugState current = context.layer_tree_host()->GetDebugState();
  current.rasterize_only_visible_content = true;
  context.layer_tree_host()->SetDebugState(current);
}

namespace {
sk_sp<SkDocument> make_multipicturedocument(SkWStream* stream) {
  return SkMultiPictureDocument::Make(stream);
}
}  // namespace
void GpuBenchmarking::PrintPagesToSkPictures(v8::Isolate* isolate,
                                             const std::string& filename) {
  PrintDocumentTofile(isolate, filename, &make_multipicturedocument,
                      render_frame_.get());
}

void GpuBenchmarking::PrintPagesToXPS(v8::Isolate* isolate,
                                      const std::string& filename) {
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
  PrintDocumentTofile(isolate, filename, &MakeXPSDocument, render_frame_.get());
#else
  std::string msg("PrintPagesToXPS is unsupported.");
  isolate->ThrowException(v8::Exception::Error(
      v8::String::NewFromUtf8(isolate, msg.c_str(), v8::NewStringType::kNormal,
                              msg.length())
          .ToLocalChecked()));
#endif
}

void GpuBenchmarking::PrintToSkPicture(v8::Isolate* isolate,
                                       const std::string& dirname) {
  GpuBenchmarkingContext context(render_frame_.get());

  const cc::Layer* root_layer = context.layer_tree_host()->root_layer();
  if (!root_layer)
    return;

  base::FilePath dirpath = base::FilePath::FromUTF8Unsafe(dirname);
  if (!base::CreateDirectory(dirpath) || !base::PathIsWritable(dirpath)) {
    std::string msg("Path is not writable: ");
    msg.append(dirpath.MaybeAsASCII());
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, msg.c_str(),
                                v8::NewStringType::kNormal, msg.length())
            .ToLocalChecked()));
    return;
  }

  SkPictureSerializer serializer(dirpath);
  serializer.Serialize(root_layer);
}

bool GpuBenchmarking::GestureSourceTypeSupported(int gesture_source_type) {
  if (gesture_source_type < 0 ||
      gesture_source_type >
          GestureSourceTypeAsInt(
              content::mojom::GestureSourceType::kGestureSourceTypeMax)) {
    return false;
  }

  return SyntheticGestureParams::IsGestureSourceTypeSupported(
      static_cast<content::mojom::GestureSourceType>(gesture_source_type));
}

// TODO(lanwei): this is will be removed after this is replaced by
// SmoothScrollByXY in telemetry/internal/actions/scroll.js.
bool GpuBenchmarking::SmoothScrollBy(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());
  gfx::Rect rect = context.frame_widget()->ViewRect();

  float pixels_to_scroll = 0;
  v8::Local<v8::Function> callback;
  float start_x = rect.width() / 2;
  float start_y = rect.height() / 2;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kDefaultInput);
  std::string direction = "down";
  float speed_in_pixels_s = 800;
  bool precise_scrolling_deltas = true;
  bool scroll_by_page = false;
  bool cursor_visible = true;
  bool scroll_by_percentage = false;
  std::string keys_value;
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetOptionalArg(args, &pixels_to_scroll) ||
      !GetOptionalArg(args, &callback) || !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &speed_in_pixels_s) ||
      !GetOptionalArg(args, &precise_scrolling_deltas) ||
      !GetOptionalArg(args, &scroll_by_page) ||
      !GetOptionalArg(args, &cursor_visible) ||
      !GetOptionalArg(args, &scroll_by_percentage) ||
      !GetOptionalArg(args, &keys_value) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  // For all touch inputs, always scroll by precise deltas.
  DCHECK(gesture_source_type !=
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kTouchInput) ||
         precise_scrolling_deltas);
  // Scroll by page only for mouse inputs.
  DCHECK(!scroll_by_page ||
         gesture_source_type ==
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kMouseInput));
  // Scroll by percentage only for mouse inputs.
  DCHECK(!scroll_by_percentage ||
         gesture_source_type ==
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kMouseInput));
  // Scroll by percentage does not require speed in pixels
  DCHECK(!scroll_by_percentage || (speed_in_pixels_s == 800));

  std::optional<gfx::Vector2dF> pixels_to_scrol_vector =
      ToVector(direction, pixels_to_scroll);
  if (!pixels_to_scrol_vector.has_value())
    return false;
  gfx::Vector2dF fling_velocity(0, 0);
  int modifiers = 0;
  std::vector<std::string_view> key_list = base::SplitStringPiece(
      keys_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string_view key : key_list) {
    int key_modifier = ToKeyModifiers(key);
    if (key_modifier == 0) {
      return false;
    }
    modifiers |= key_modifier;
  }

  EnsureRemoteInterface();
  return BeginSmoothScroll(
      &context, args, input_injector_, pixels_to_scrol_vector.value(), callback,
      gesture_source_type, speed_in_pixels_s, true /* prevent_fling */, start_x,
      start_y, fling_velocity, precise_scrolling_deltas, scroll_by_page,
      cursor_visible, scroll_by_percentage, modifiers, vsync_offset_ms,
      input_event_pattern);
}

// SmoothScrollByXY does not take direction as one of the arguments, and
// instead we pass two scroll delta values for both x and y directions, when
// pixels_to_scroll_y is positive, it will scroll down, otherwise scroll up.
// When pixels_to_scroll_x is positive, it will scroll right, otherwise
// scroll left.
bool GpuBenchmarking::SmoothScrollByXY(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());
  gfx::Rect rect = context.frame_widget()->ViewRect();

  float pixels_to_scroll_x = 0;
  float pixels_to_scroll_y = 0;
  v8::Local<v8::Function> callback;
  float start_x = rect.width() / 2;
  float start_y = rect.height() / 2;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kDefaultInput);
  float speed_in_pixels_s = 800;
  bool precise_scrolling_deltas = true;
  bool scroll_by_page = false;
  bool cursor_visible = true;
  bool scroll_by_percentage = false;
  // It should be one or multiple values in the |Modifiers| in the function
  // ToKeyModifiers, multiple values are expressed as a string
  // separated by comma.
  std::string keys_value;
  // It should be one or multiple values in the |Buttons| in the function
  // ToButtonModifiers, multiple values are expressed as a string separated by
  // comma.
  std::string buttons_value;
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetOptionalArg(args, &pixels_to_scroll_x) ||
      !GetOptionalArg(args, &pixels_to_scroll_y) ||
      !GetOptionalArg(args, &callback) || !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &speed_in_pixels_s) ||
      !GetOptionalArg(args, &precise_scrolling_deltas) ||
      !GetOptionalArg(args, &scroll_by_page) ||
      !GetOptionalArg(args, &cursor_visible) ||
      !GetOptionalArg(args, &scroll_by_percentage) ||
      !GetOptionalArg(args, &keys_value) ||
      !GetOptionalArg(args, &buttons_value) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  // For all touch inputs, always scroll by precise deltas.
  DCHECK(gesture_source_type !=
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kTouchInput) ||
         precise_scrolling_deltas);
  // Scroll by page only for mouse inputs.
  DCHECK(!scroll_by_page ||
         gesture_source_type ==
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kMouseInput));
  // Scroll by percentage only for mouse inputs.
  DCHECK(!scroll_by_percentage ||
         gesture_source_type ==
             GestureSourceTypeAsInt(
                 content::mojom::GestureSourceType::kMouseInput));

  gfx::Vector2dF distances(pixels_to_scroll_x, pixels_to_scroll_y);
  gfx::Vector2dF fling_velocity(0, 0);
  int modifiers = 0;
  std::vector<std::string_view> key_list = base::SplitStringPiece(
      keys_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string_view key : key_list) {
    int key_modifier = ToKeyModifiers(key);
    if (key_modifier == 0) {
      return false;
    }
    modifiers |= key_modifier;
  }

  std::vector<std::string_view> button_list = base::SplitStringPiece(
      buttons_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string_view button : button_list) {
    int button_modifier = ToButtonModifiers(button);
    if (button_modifier == 0) {
      return false;
    }
    modifiers |= button_modifier;
  }

  EnsureRemoteInterface();
  return BeginSmoothScroll(
      &context, args, input_injector_, distances, callback, gesture_source_type,
      speed_in_pixels_s, true /* prevent_fling */, start_x, start_y,
      fling_velocity, precise_scrolling_deltas, scroll_by_page, cursor_visible,
      scroll_by_percentage, modifiers, vsync_offset_ms, input_event_pattern);
}

bool GpuBenchmarking::SmoothDrag(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());
  float start_x;
  float start_y;
  float end_x;
  float end_y;
  v8::Local<v8::Function> callback;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kDefaultInput);
  float speed_in_pixels_s = 800;
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetArg(args, &start_x) || !GetArg(args, &start_y) ||
      !GetArg(args, &end_x) || !GetArg(args, &end_y) ||
      !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &speed_in_pixels_s) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  EnsureRemoteInterface();
  return BeginSmoothDrag(&context, args, input_injector_, start_x, start_y,
                         end_x, end_y, callback, gesture_source_type,
                         speed_in_pixels_s, vsync_offset_ms,
                         input_event_pattern);
}

// TODO(lanwei): Swipe takes pixels_to_scroll and direction. When the
// pixels_to_scroll is positive and direction is up, it means the finger moves
// up, but the page scrolls down, which is opposite to SmoothScrollBy. We
// should change this to match with SmoothScrollBy or SmoothScrollByXY.
bool GpuBenchmarking::Swipe(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());
  gfx::Rect rect = context.frame_widget()->ViewRect();

  std::string direction = "up";
  float pixels_to_scroll = 0;
  v8::Local<v8::Function> callback;
  float start_x = rect.width() / 2;
  float start_y = rect.height() / 2;
  float speed_in_pixels_s = 800;
  float fling_velocity = 0;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kTouchInput);
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &pixels_to_scroll) ||
      !GetOptionalArg(args, &callback) || !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &speed_in_pixels_s) ||
      !GetOptionalArg(args, &fling_velocity) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  // For touchpad swipe, we should be given a fling velocity, but it is not
  // needed for touchscreen swipe, because we will calculate the velocity in
  // our code.
  if (gesture_source_type ==
          GestureSourceTypeAsInt(
              content::mojom::GestureSourceType::kTouchpadInput) &&
      fling_velocity == 0) {
    fling_velocity = 1000;
  }

  std::optional<gfx::Vector2dF> pixels_to_scrol_vector =
      ToVector(direction, pixels_to_scroll);
  std::optional<gfx::Vector2dF> fling_velocity_vector =
      ToVector(direction, fling_velocity);
  if (!pixels_to_scrol_vector.has_value() ||
      !fling_velocity_vector.has_value()) {
    return false;
  }

  EnsureRemoteInterface();
  return BeginSmoothScroll(
      &context, args, input_injector_, -pixels_to_scrol_vector.value(),
      callback, gesture_source_type, speed_in_pixels_s,
      false /* prevent_fling */, start_x, start_y,
      fling_velocity_vector.value(), true /* precise_scrolling_deltas */,
      false /* scroll_by_page */, true /* cursor_visible */,
      false /* scroll_by_percentage */, 0 /* modifiers */, vsync_offset_ms,
      input_event_pattern);
}

bool GpuBenchmarking::ScrollBounce(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());
  gfx::Rect content_rect = context.frame_widget()->ViewRect();

  std::string direction = "down";
  float distance_length = 0;
  float overscroll_length = 0;
  int repeat_count = 1;
  v8::Local<v8::Function> callback;
  float start_x = content_rect.width() / 2;
  float start_y = content_rect.height() / 2;
  float speed_in_pixels_s = 800;
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetOptionalArg(args, &direction) ||
      !GetOptionalArg(args, &distance_length) ||
      !GetOptionalArg(args, &overscroll_length) ||
      !GetOptionalArg(args, &repeat_count) ||
      !GetOptionalArg(args, &callback) || !GetOptionalArg(args, &start_x) ||
      !GetOptionalArg(args, &start_y) ||
      !GetOptionalArg(args, &speed_in_pixels_s) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  if (ThrowIfPointOutOfBounds(&context, args, gfx::Point(start_x, start_y),
                              "Start point not in bounds")) {
    return false;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context.web_frame()->MainWorldScriptContext());

  SyntheticSmoothScrollGestureParams gesture_params;

  gesture_params.speed_in_pixels_s = speed_in_pixels_s;
  gesture_params.vsync_offset_ms = vsync_offset_ms;
  gesture_params.input_event_pattern =
      static_cast<content::mojom::InputEventPattern>(input_event_pattern);

  gesture_params.anchor.SetPoint(start_x, start_y);

  gfx::Vector2dF distance;
  gfx::Vector2dF overscroll;
  if (direction == "down") {
    distance.set_y(-distance_length);
    overscroll.set_y(overscroll_length);
  } else if (direction == "up") {
    distance.set_y(distance_length);
    overscroll.set_y(-overscroll_length);
  } else if (direction == "right") {
    distance.set_x(-distance_length);
    overscroll.set_x(overscroll_length);
  } else if (direction == "left") {
    distance.set_x(distance_length);
    overscroll.set_x(-overscroll_length);
  } else {
    return false;
  }

  for (int i = 0; i < repeat_count; i++) {
    gesture_params.distances.push_back(distance);
    gesture_params.distances.push_back(-distance + overscroll);
  }
  EnsureRemoteInterface();
  input_injector_->QueueSyntheticSmoothScroll(
      gesture_params, base::BindOnce(&OnSyntheticGestureCompleted,
                                     base::RetainedRef(callback_and_context)));

  return true;
}

bool GpuBenchmarking::PinchBy(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());

  float scale_factor;
  float anchor_x;
  float anchor_y;
  v8::Local<v8::Function> callback;
  float relative_pointer_speed_in_pixels_s = 800;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kDefaultInput);
  float vsync_offset_ms = 0.0f;
  int input_event_pattern =
      static_cast<int>(content::mojom::InputEventPattern::kDefaultPattern);

  if (!GetArg(args, &scale_factor) || !GetArg(args, &anchor_x) ||
      !GetArg(args, &anchor_y) || !GetOptionalArg(args, &callback) ||
      !GetOptionalArg(args, &relative_pointer_speed_in_pixels_s) ||
      !GetOptionalArg(args, &gesture_source_type) ||
      !GetOptionalArg(args, &vsync_offset_ms) ||
      !GetOptionalArg(args, &input_event_pattern)) {
    return false;
  }

  if (ThrowIfPointOutOfBounds(&context, args, gfx::Point(anchor_x, anchor_y),
                              "Anchor point not in bounds")) {
    return false;
  }

  SyntheticPinchGestureParams gesture_params;

  gesture_params.scale_factor = scale_factor;
  gesture_params.anchor.SetPoint(anchor_x, anchor_y);
  gesture_params.relative_pointer_speed_in_pixels_s =
      relative_pointer_speed_in_pixels_s;

  if (gesture_source_type < 0 ||
      gesture_source_type >
          GestureSourceTypeAsInt(
              content::mojom::GestureSourceType::kGestureSourceTypeMax)) {
    args->ThrowTypeError("Unknown gesture source type");
    return false;
  }

  gesture_params.gesture_source_type =
      static_cast<content::mojom::GestureSourceType>(gesture_source_type);
  gesture_params.vsync_offset_ms = vsync_offset_ms;
  gesture_params.input_event_pattern =
      static_cast<content::mojom::InputEventPattern>(input_event_pattern);

  switch (gesture_params.gesture_source_type) {
    case content::mojom::GestureSourceType::kDefaultInput:
    case content::mojom::GestureSourceType::kTouchInput:
    case content::mojom::GestureSourceType::kMouseInput:
      break;
    case content::mojom::GestureSourceType::kPenInput:
      args->ThrowTypeError(
          "Gesture is not implemented for the given source type");
      return false;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context.web_frame()->MainWorldScriptContext());
  EnsureRemoteInterface();
  input_injector_->QueueSyntheticPinch(
      gesture_params, base::BindOnce(&OnSyntheticGestureCompleted,
                                     base::RetainedRef(callback_and_context)));

  return true;
}

float GpuBenchmarking::PageScaleFactor() {
  GpuBenchmarkingContext context(render_frame_.get());
  return context.web_view()->PageScaleFactor();
}

void GpuBenchmarking::SetPageScaleFactor(float scale) {
  GpuBenchmarkingContext context(render_frame_.get());
  context.web_view()->SetPageScaleFactor(scale);
}

void GpuBenchmarking::SetBrowserControlsShown(bool show) {
  GpuBenchmarkingContext context(render_frame_.get());
  context.layer_tree_host()->UpdateBrowserControlsState(
      cc::BrowserControlsState::kBoth,
      show ? cc::BrowserControlsState::kShown
           : cc::BrowserControlsState::kHidden,
      false, std::nullopt);
}

float GpuBenchmarking::VisualViewportY() {
  GpuBenchmarkingContext context(render_frame_.get());
  float y = context.web_view()->VisualViewportOffset().y();
  gfx::RectF rect_in_dips =
      context.frame_widget()->BlinkSpaceToDIPs(gfx::RectF(0, y, 0, 0));
  return rect_in_dips.y();
}

float GpuBenchmarking::VisualViewportX() {
  GpuBenchmarkingContext context(render_frame_.get());
  float x = context.web_view()->VisualViewportOffset().x();
  gfx::RectF rect_in_dips =
      context.frame_widget()->BlinkSpaceToDIPs(gfx::RectF(x, 0, 0, 0));
  return rect_in_dips.x();
}

float GpuBenchmarking::VisualViewportHeight() {
  GpuBenchmarkingContext context(render_frame_.get());
  float height = context.web_view()->VisualViewportSize().height();
  gfx::RectF rect_in_dips =
      context.frame_widget()->BlinkSpaceToDIPs(gfx::RectF(0, 0, 0, height));
  return rect_in_dips.height();
}

float GpuBenchmarking::VisualViewportWidth() {
  GpuBenchmarkingContext context(render_frame_.get());
  float width = context.web_view()->VisualViewportSize().width();
  gfx::RectF rect_in_dips =
      context.frame_widget()->BlinkSpaceToDIPs(gfx::RectF(0, 0, width, 0));
  return rect_in_dips.width();
}

bool GpuBenchmarking::Tap(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());

  float position_x;
  float position_y;
  v8::Local<v8::Function> callback;
  int duration_ms = 50;
  int gesture_source_type =
      GestureSourceTypeAsInt(content::mojom::GestureSourceType::kDefaultInput);

  if (!GetArg(args, &position_x) || !GetArg(args, &position_y) ||
      !GetOptionalArg(args, &callback) || !GetOptionalArg(args, &duration_ms) ||
      !GetOptionalArg(args, &gesture_source_type)) {
    return false;
  }

  if (ThrowIfPointOutOfBounds(&context, args,
                              gfx::Point(position_x, position_y),
                              "Start point not in bounds")) {
    return false;
  }

  SyntheticTapGestureParams gesture_params;

  gesture_params.position.SetPoint(position_x, position_y);
  gesture_params.duration_ms = duration_ms;

  if (gesture_source_type < 0 ||
      gesture_source_type >
          GestureSourceTypeAsInt(
              content::mojom::GestureSourceType::kGestureSourceTypeMax)) {
    return false;
  }
  gesture_params.gesture_source_type =
      static_cast<content::mojom::GestureSourceType>(gesture_source_type);

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context.web_frame()->MainWorldScriptContext());
  EnsureRemoteInterface();
  input_injector_->QueueSyntheticTap(
      gesture_params, base::BindOnce(&OnSyntheticGestureCompleted,
                                     base::RetainedRef(callback_and_context)));

  return true;
}

bool GpuBenchmarking::PointerActionSequence(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());

  v8::Local<v8::Function> callback;

  v8::Local<v8::Object> obj;
  if (!args->GetNext(&obj)) {
    args->ThrowError();
    return false;
  }

  v8::Local<v8::Context> v8_context =
      context.web_frame()->MainWorldScriptContext();
  std::unique_ptr<base::Value> value =
      V8ValueConverter::Create()->FromV8Value(obj, v8_context);
  if (!value.get()) {
    args->ThrowError();
    return false;
  }

  // Get all the pointer actions from the user input and wrap them into a
  // SyntheticPointerActionListParams object.
  ActionsParser actions_parser(
      base::Value::FromUniquePtrValue(std::move(value)));
  if (!actions_parser.Parse()) {
    args->ThrowTypeError(actions_parser.error_message());
    return false;
  }

  if (!GetOptionalArg(args, &callback)) {
    args->ThrowError();
    return false;
  }

  // At the end, we will send a 'FINISH' action and need a callback.
  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context.web_frame()->MainWorldScriptContext());
  EnsureRemoteInterface();
  if (actions_parser.parsed_gesture_type() ==
      SyntheticGestureParams::SMOOTH_SCROLL_GESTURE) {
    input_injector_->QueueSyntheticSmoothScroll(
        actions_parser.smooth_scroll_params(),
        base::BindOnce(&OnSyntheticGestureCompleted,
                       base::RetainedRef(callback_and_context)));
  } else {
    CHECK_EQ(actions_parser.parsed_gesture_type(),
             SyntheticGestureParams::POINTER_ACTION_LIST);
    input_injector_->QueueSyntheticPointerAction(
        actions_parser.pointer_action_params(),
        base::BindOnce(&OnSyntheticGestureCompleted,
                       base::RetainedRef(callback_and_context)));
  }
  return true;
}

void GpuBenchmarking::ClearImageCache() {
  WebImageCache::Clear();
}

int GpuBenchmarking::RunMicroBenchmark(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());

  std::string name;
  v8::Local<v8::Function> callback;
  v8::Local<v8::Object> arguments;

  if (!GetArg(args, &name) || !GetArg(args, &callback) ||
      !GetOptionalArg(args, &arguments)) {
    return 0;
  }

  scoped_refptr<CallbackAndContext> callback_and_context =
      new CallbackAndContext(args->isolate(), callback,
                             context.web_frame()->MainWorldScriptContext());

  v8::Local<v8::Context> v8_context = callback_and_context->GetContext();
  std::unique_ptr<base::Value> value =
      V8ValueConverter::Create()->FromV8Value(arguments, v8_context);
  DCHECK(value);
  if (!value->is_dict()) {
    return 0;
  }

  return context.layer_tree_host()->ScheduleMicroBenchmark(
      name, std::move(*value).TakeDict(),
      base::BindOnce(&OnMicroBenchmarkCompleted,
                     base::RetainedRef(callback_and_context)));
}

bool GpuBenchmarking::SendMessageToMicroBenchmark(
    int id,
    v8::Local<v8::Object> message) {
  GpuBenchmarkingContext context(render_frame_.get());

  v8::Local<v8::Context> v8_context =
      context.web_frame()->MainWorldScriptContext();
  std::unique_ptr<base::Value> value =
      V8ValueConverter::Create()->FromV8Value(message, v8_context);
  DCHECK(value);
  if (!value->is_dict()) {
    return false;
  }

  return context.layer_tree_host()->SendMessageToMicroBenchmark(
      id, std::move(*value).TakeDict());
}

bool GpuBenchmarking::HasGpuChannel() {
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  return !!gpu_channel;
}

bool GpuBenchmarking::HasGpuProcess() {
  bool has_gpu_process = false;
  if (!RenderThreadImpl::current()->GetRendererHost()->HasGpuProcess(
          &has_gpu_process)) {
    return false;
  }
  return has_gpu_process;
}

void GpuBenchmarking::CrashGpuProcess() {
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel)
    return;
  gpu_channel->CrashGpuProcessForTesting();
}

// Terminates the GPU process with an exit code of 0.
void GpuBenchmarking::TerminateGpuProcessNormally() {
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel)
    return;
  gpu_channel->TerminateGpuProcessForTesting();
}

void GpuBenchmarking::GetGpuDriverBugWorkarounds(gin::Arguments* args) {
  std::vector<std::string> gpu_driver_bug_workarounds;
  gpu::GpuChannelHost* gpu_channel =
      RenderThreadImpl::current()->GetGpuChannel();
  if (!gpu_channel)
    return;
  const gpu::GpuFeatureInfo& gpu_feature_info = gpu_channel->gpu_feature_info();
  const std::vector<int32_t>& workarounds =
      gpu_feature_info.enabled_gpu_driver_bug_workarounds;
  for (int32_t workaround : workarounds) {
    gpu_driver_bug_workarounds.push_back(
        gpu::GpuDriverBugWorkaroundTypeToString(
            static_cast<gpu::GpuDriverBugWorkaroundType>(workaround)));
  }

  // This code must be kept in sync with compositor_util's
  // GetDriverBugWorkaroundsImpl.
  for (auto ext :
       base::SplitString(gpu_feature_info.disabled_extensions, " ",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    gpu_driver_bug_workarounds.push_back("disabled_extension_" + ext);
  }
  for (auto ext :
       base::SplitString(gpu_feature_info.disabled_webgl_extensions, " ",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    gpu_driver_bug_workarounds.push_back("disabled_webgl_extension_" + ext);
  }

  v8::Local<v8::Value> result;
  if (gin::TryConvertToV8(args->isolate(), gpu_driver_bug_workarounds, &result))
    args->Return(result);
}

void GpuBenchmarking::StartProfiling(gin::Arguments* args) {
  if (base::debug::BeingProfiled())
    return;
  std::string file_name;
  if (!GetOptionalArg(args, &file_name))
    return;
  if (!file_name.length())
    file_name = "profile.pb";
  base::debug::StartProfiling(file_name);
  base::debug::RestartProfilingAfterFork();
}

void GpuBenchmarking::StopProfiling() {
  if (base::debug::BeingProfiled())
    base::debug::StopProfiling();
}

void GpuBenchmarking::Freeze() {
  GpuBenchmarkingContext context(render_frame_.get());
  // TODO(fmeawad): Instead of forcing a visibility change, only allow
  // freezing a page if it was already hidden.
  context.web_view()->SetVisibilityState(
      blink::mojom::PageVisibilityState::kHidden,
      /*is_initial_state=*/false);
  context.web_view()->SetPageFrozen(true);
}

bool GpuBenchmarking::AddSwapCompletionEventListener(gin::Arguments* args) {
  v8::Local<v8::Function> callback;
  if (!GetArg(args, &callback))
    return false;
  GpuBenchmarkingContext context(render_frame_.get());

  auto callback_and_context = base::MakeRefCounted<CallbackAndContext>(
      args->isolate(), callback, context.web_frame()->MainWorldScriptContext());
  context.frame_widget()->NotifyPresentationTime(base::BindOnce(
      &OnSwapCompletedHelper, base::RetainedRef(callback_and_context)));
  // Request a begin frame explicitly, as the test-api expects a 'swap' to
  // happen for the above queued swap promise even if there is no actual update.
  context.layer_tree_host()->SetNeedsAnimateIfNotInsideMainFrame();
  return true;
}

#if BUILDFLAG(IS_MAC)
int GpuBenchmarking::AddCoreAnimationStatusEventListener(gin::Arguments* args) {
  v8::Local<v8::Function> callback;
  if (!GetArg(args, &callback))
    return false;
  GpuBenchmarkingContext context(render_frame_.get());

  auto callback_and_context = base::MakeRefCounted<CallbackAndContext>(
      args->isolate(), callback, context.web_frame()->MainWorldScriptContext());
  context.frame_widget()->NotifyCoreAnimationErrorCode(
      base::BindOnce(&OnSwapCompletedWithCoreAnimationErrorCode,
                     base::RetainedRef(callback_and_context)));
  // Request a begin frame explicitly, as the test-api expects a 'swap' to
  // happen for the above queued swap promise even if there is no actual update.
  context.layer_tree_host()->SetNeedsAnimateIfNotInsideMainFrame();

  return true;
}
#endif

bool GpuBenchmarking::IsAcceleratedCanvasImageSource(gin::Arguments* args) {
  GpuBenchmarkingContext context(render_frame_.get());

  v8::Local<v8::Value> value;
  if (!args->GetNext(&value)) {
    args->ThrowError();
    return false;
  }
  return blink::IsAcceleratedCanvasImageSource(args->isolate(), value);
}

}  // namespace content
