// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#if defined(UNSAFE_BUFFERS_BUILD)
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/queue.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/accessibility.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/content_restriction.h"
#include "pdf/document_layout.h"
#include "pdf/loader/result_codes.h"
#include "pdf/loader/url_loader.h"
#include "pdf/metrics_handler.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/paint_manager.h"
#include "pdf/paint_ready_rect.h"
#include "pdf/parsed_params.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_init.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_client.h"
#include "pdf/post_message_receiver.h"
#include "pdf/ui/document_properties.h"
#include "pdf/ui/file_name.h"
#include "pdf/ui/thumbnail.h"
#include "printing/metafile_skia.h"
#include "printing/units.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "pdf/pdf_ink_module.h"
#include "third_party/skia/include/core/SkCanvas.h"
#endif

namespace chrome_pdf {

namespace {

// The minimum zoom level allowed.
constexpr double kMinZoom = 0.01;

// A delay to wait between each accessibility page to keep the system
// responsive.
constexpr base::TimeDelta kAccessibilityPageDelay = base::Milliseconds(100);

constexpr base::TimeDelta kFindResultCooldown = base::Milliseconds(100);

constexpr std::string_view kChromeExtensionHost =
    "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/";

// Print Preview base URL.
constexpr std::string_view kChromePrintHost = "chrome://print/";

// Untrusted Print Preview base URL.
constexpr std::string_view kChromeUntrustedPrintHost =
    "chrome-untrusted://print/";

// Same value as `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX`.
constexpr int kCompletePDFIndex = -1;

// A different negative value to differentiate itself from `kCompletePDFIndex`.
constexpr int kInvalidPDFIndex = -2;

// Enumeration of pinch states.
// This should match PinchPhase enum in
// chrome/browser/resources/pdf/viewport.ts.
enum class PinchPhase {
  kNone = 0,
  kStart = 1,
  kUpdateZoomOut = 2,
  kUpdateZoomIn = 3,
  kEnd = 4,
};

// Initialization performed per renderer process. Initialization may be
// triggered from multiple plugin instances, but should only execute once.
//
// TODO(crbug.com/40147027): We may be able to simplify this once we've figured
// out exactly which processes need to initialize and shutdown PDFium.
class PerProcessInitializer final {
 public:
  ~PerProcessInitializer() {
    // On some configs, thread checker is trivially destructible, which makes
    // `PerProcessInitializer` trivially destructible as well. This is a problem
    // because `base::NoDestructor` only allows non-trivially destructible
    // types. Force `PerProcessInitializer` to be non-trivially destructible by
    // declaring a non-default destructor.
  }

  static PerProcessInitializer& GetInstance() {
    static base::NoDestructor<PerProcessInitializer> instance;
    return *instance;
  }

  void Acquire(bool use_skia) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    DCHECK_GE(init_count_, 0);
    if (init_count_++ > 0)
      return;

    DCHECK(!IsSDKInitializedViaPlugin());
    InitializeSDK(/*enable_v8=*/true, use_skia, FontMappingMode::kBlink);
    SetIsSDKInitializedViaPlugin(true);
  }

  void Release() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    DCHECK_GT(init_count_, 0);
    if (--init_count_ > 0)
      return;

    DCHECK(IsSDKInitializedViaPlugin());
    ShutdownSDK();
    SetIsSDKInitializedViaPlugin(false);
  }

 private:
  int init_count_ GUARDED_BY_CONTEXT(thread_checker_) = 0;

  // TODO(crbug.com/40147080): Assuming PDFium is thread-hostile for now, and
  // must use one thread exclusively.
  THREAD_CHECKER(thread_checker_);
};

base::Value::Dict DictFromRect(const gfx::Rect& rect) {
  base::Value::Dict dict;
  dict.Set("x", rect.x());
  dict.Set("y", rect.y());
  dict.Set("width", rect.width());
  dict.Set("height", rect.height());
  return dict;
}

bool IsPrintPreviewUrl(std::string_view url) {
  return base::StartsWith(url, kChromeUntrustedPrintHost);
}

int ExtractPrintPreviewPageIndex(std::string_view src_url) {
  // Sample `src_url` format: chrome-untrusted://print/id/page_index/print.pdf
  // The page_index is zero-based, but can be negative with special meanings.
  std::vector<std::string_view> url_substr =
      base::SplitStringPiece(src_url.substr(kChromeUntrustedPrintHost.size()),
                             "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return kInvalidPDFIndex;

  if (url_substr[2] != "print.pdf")
    return kInvalidPDFIndex;

  int page_index = 0;
  if (!base::StringToInt(url_substr[1], &page_index))
    return kInvalidPDFIndex;
  return page_index;
}

bool IsPreviewingPDF(int print_preview_page_count) {
  return print_preview_page_count == 0;
}

// Prepares messages from the plugin that reply to messages from the embedder.
// If the "type" value of `message` is "foo", then the `reply_type` must be
// "fooReply". The `message` from the embedder must have a "messageId" value
// that will be copied to the reply message.
base::Value::Dict PrepareReplyMessage(std::string_view reply_type,
                                      const base::Value::Dict& message) {
  DCHECK_EQ(reply_type, *message.FindString("type") + "Reply");

  base::Value::Dict reply;
  reply.Set("type", reply_type);
  reply.Set("messageId", *message.FindString("messageId"));
  return reply;
}

bool IsSaveDataSizeValid(size_t size) {
  return size > 0 && size <= PdfViewWebPlugin::kMaximumSavedFileSize;
}

#if BUILDFLAG(ENABLE_PDF_INK2)
std::unique_ptr<PdfInkModule> MaybeCreatePdfInkModule(
    PdfInkModuleClient& client) {
  if (!base::FeatureList::IsEnabled(features::kPdfInk2)) {
    return nullptr;
  }
  return std::make_unique<PdfInkModule>(client);
}
#endif

}  // namespace

std::unique_ptr<PDFiumEngine> PdfViewWebPlugin::Client::CreateEngine(
    PDFiumEngineClient* client,
    PDFiumFormFiller::ScriptOption script_option) {
  return std::make_unique<PDFiumEngine>(client, script_option);
}

std::unique_ptr<PdfAccessibilityDataHandler>
PdfViewWebPlugin::Client::CreateAccessibilityDataHandler(
    PdfAccessibilityActionHandler* action_handler,
    PdfAccessibilityImageFetcher* image_fetcher,
    blink::WebPluginContainer* plugin_container,
    bool print_preview) {
  return nullptr;
}

PdfViewWebPlugin::PdfViewWebPlugin(
    std::unique_ptr<Client> client,
    mojo::AssociatedRemote<pdf::mojom::PdfHost> pdf_host,
    blink::WebPluginParams params)
    : client_(std::move(client)),
      pdf_host_(std::move(pdf_host)),
#if BUILDFLAG(ENABLE_PDF_INK2)
      ink_module_(MaybeCreatePdfInkModule(*this)),
#endif
      initial_params_(std::move(params)) {
  DCHECK(pdf_host_);
  pdf_host_->SetListener(listener_receiver_.BindNewPipeAndPassRemote());
}

PdfViewWebPlugin::~PdfViewWebPlugin() = default;

bool PdfViewWebPlugin::Initialize(blink::WebPluginContainer* container) {
  DCHECK(container);
  client_->SetPluginContainer(container);
  DCHECK_EQ(container->Plugin(), this);

  return InitializeCommon();
}

bool PdfViewWebPlugin::InitializeForTesting() {
  return InitializeCommon();
}

bool PdfViewWebPlugin::InitializeCommon() {
  // Allow the plugin to handle touch events.
  client_->RequestTouchEventType(
      blink::WebPluginContainer::kTouchEventRequestTypeRaw);

  // Allow the plugin to handle find requests.
  client_->UsePluginAsFindHandler();

  std::optional<ParsedParams> params = ParseWebPluginParams(initial_params_);

  // The contents of `initial_params_` are no longer needed.
  initial_params_ = {};

  if (!params.has_value())
    return false;

  // Sets crash keys like `ppapi::proxy::PDFResource::SetCrashData()`. Note that
  // we don't set the active URL from the top-level URL, as unlike within a
  // plugin process, the active URL changes frequently within a renderer process
  // (see crbug.com/1266050 for details).
  //
  // TODO(crbug.com/40801869): If multiple PDF plugin instances share the same
  // renderer process, the crash key will be overwritten by the newest value.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url, params->original_url);

  PerProcessInitializer::GetInstance().Acquire(params->use_skia);
  initialized_ = true;

  // Check if the PDF is being loaded in the PDF chrome extension. We only allow
  // the plugin to be loaded in the extension and print preview to avoid
  // exposing sensitive APIs directly to external websites.
  //
  // This is enforced before creating the plugin (see
  // `pdf::CreateInternalPlugin()`), so we just `CHECK` for defense-in-depth.
  const std::string& embedder_origin = client_->GetEmbedderOriginString();
  is_print_preview_ = (embedder_origin == kChromePrintHost);
  CHECK(IsPrintPreview() || embedder_origin == kChromeExtensionHost);

  full_frame_ = params->full_frame;
  background_color_ = params->background_color;

  engine_ = client_->CreateEngine(this, params->script_option);
  DCHECK(engine_);

  SendSetSmoothScrolling();

  pdf_accessibility_data_handler_ = client_->CreateAccessibilityDataHandler(
      this, this, client_->PluginContainer(), IsPrintPreview());

  // Skip the remaining initialization when in Print Preview mode. Loading will
  // continue after the plugin receives a "resetPrintPreviewMode" message.
  if (IsPrintPreview())
    return true;

  last_progress_sent_ = 0;
  LoadUrl(params->src_url, base::BindOnce(&PdfViewWebPlugin::DidOpen,
                                          weak_factory_.GetWeakPtr()));
  url_ = params->original_url;

  // Not all edits go through the PDF plugin's form filler. The plugin instance
  // can be restarted by exiting annotation mode on ChromeOS, which can set the
  // document to an edited state.
  edit_mode_ = params->has_edits;
#if !BUILDFLAG(ENABLE_INK)
  DCHECK(!edit_mode_);
#endif  // !BUILDFLAG(ENABLE_INK)

  metrics_handler_ = std::make_unique<MetricsHandler>();
  return true;
}

void PdfViewWebPlugin::SendSetSmoothScrolling() {
  base::Value::Dict message;
  message.Set("type", "setSmoothScrolling");
  message.Set("smoothScrolling",
              blink::Platform::Current()->IsScrollAnimatorEnabled());
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::DidOpen(std::unique_ptr<UrlLoader> loader,
                               int32_t result) {
  if (result == kSuccess) {
    if (!engine_->HandleDocumentLoad(std::move(loader), url_)) {
      document_load_state_ = DocumentLoadState::kLoading;
      DocumentLoadFailed();
    }
  } else if (result != kErrorAborted) {
    DocumentLoadFailed();
  }
}

void PdfViewWebPlugin::Destroy() {
  if (initialized_) {
    // Explicitly destroy the PDFiumEngine during destruction as it may call
    // back into this object.
    preview_engine_.reset();
    engine_.reset();
    PerProcessInitializer::GetInstance().Release();
  }

  client_->SetPluginContainer(nullptr);

  delete this;
}

blink::WebPluginContainer* PdfViewWebPlugin::Container() const {
  return client_->PluginContainer();
}

v8::Local<v8::Object> PdfViewWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  if (scriptable_receiver_.IsEmpty()) {
    // TODO(crbug.com/40147080): Messages should not be handled on the renderer
    // main thread.
    scriptable_receiver_.Reset(
        isolate, PostMessageReceiver::Create(
                     isolate, client_->GetWeakPtr(), weak_factory_.GetWeakPtr(),
                     base::SequencedTaskRunner::GetCurrentDefault()));
  }

  return scriptable_receiver_.Get(isolate);
}

bool PdfViewWebPlugin::SupportsKeyboardFocus() const {
  return !IsPrintPreview();
}

void PdfViewWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void PdfViewWebPlugin::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  // Clip the intersection of the paint rect and the plugin rect, so that
  // painting outside the plugin or the paint rect area can be avoided.
  // Note: `rect` is in CSS pixels. We need to use `css_plugin_rect_`
  // to calculate the intersection.
  SkRect invalidate_rect =
      gfx::RectToSkRect(gfx::IntersectRects(css_plugin_rect_, rect));
  cc::PaintCanvasAutoRestore auto_restore(canvas, /*save=*/true);
  canvas->clipRect(invalidate_rect);

  // Paint with the plugin's background color if the snapshot is not ready.
  if (snapshot_.GetSkImageInfo().isEmpty()) {
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setColor(GetBackgroundColor());
    canvas->drawRect(invalidate_rect, flags);
    return;
  }

  // Layer translate is independent of scaling, so apply first.
  if (!total_translate_.IsZero())
    canvas->translate(total_translate_.x(), total_translate_.y());

  // Position layer at plugin origin before layer scaling.
  if (!plugin_rect_.origin().IsOrigin())
    canvas->translate(plugin_rect_.x(), plugin_rect_.y());

  if (snapshot_scale_ != 1.0f)
    canvas->scale(snapshot_scale_, snapshot_scale_);

  canvas->drawImage(snapshot_, 0, 0);

#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_) {
    SkBitmap sk_bitmap;
    sk_bitmap.allocPixels(
        SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
    SkCanvas sk_canvas(sk_bitmap);
    sk_canvas.clear(SK_ColorTRANSPARENT);
    ink_module_->Draw(sk_canvas);

    sk_sp<SkImage> snapshot = sk_bitmap.asImage();
    CHECK(snapshot);
    cc::PaintImage cc_snapshot =
        cc::PaintImageBuilder::WithDefault()
            .set_image(std::move(snapshot), cc::PaintImage::GetNextContentId())
            .set_id(cc::PaintImage::GetNextId())
            .set_no_cache(true)
            .TakePaintImage();
    canvas->drawImage(cc_snapshot, 0, 0);
  }
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
}

void PdfViewWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                      const gfx::Rect& clip_rect,
                                      const gfx::Rect& unobscured_rect,
                                      bool is_visible) {
  // An empty `window_rect` can be received here in the following cases:
  // - If the embedded plugin size is 0.
  // - If the embedded plugin size is not 0, it can come from re-layouts during
  //   the plugin initialization.
  // For either case, there is no need to create a graphic device to display
  // a PDF in an empty window. Since an empty `window_rect` can cause failure
  // to create the graphic device, avoid all updates on the geometries and the
  // device scales used by the plugin, the PaintManager and the PDFiumEngine
  // unless a non-empty `window_rect` is received.
  if (window_rect.IsEmpty())
    return;

  OnViewportChanged(window_rect, client_->DeviceScaleFactor());

  gfx::PointF scroll_position = client_->GetScrollPosition();
  // Convert back to CSS pixels.
  scroll_position.Scale(1.0f / device_scale_);
  UpdateScroll(scroll_position);
}

void PdfViewWebPlugin::UpdateScroll(const gfx::PointF& scroll_position) {
  if (stop_scrolling_)
    return;

  float max_x = std::max(document_size_.width() * static_cast<float>(zoom_) -
                             plugin_dip_size_.width(),
                         0.0f);
  float max_y = std::max(document_size_.height() * static_cast<float>(zoom_) -
                             plugin_dip_size_.height(),
                         0.0f);

  gfx::PointF scaled_scroll_position(
      std::clamp(scroll_position.x(), 0.0f, max_x),
      std::clamp(scroll_position.y(), 0.0f, max_y));
  scaled_scroll_position.Scale(device_scale_);

  engine_->ScrolledToXPosition(scaled_scroll_position.x());
  engine_->ScrolledToYPosition(scaled_scroll_position.y());
}

void PdfViewWebPlugin::UpdateFocus(bool focused,
                                   blink::mojom::FocusType focus_type) {
  if (has_focus_ != focused) {
    engine_->UpdateFocus(focused);
    client_->UpdateTextInputState();

    // Make sure `this` is still alive after the UpdateSelectionBounds() call.
    auto weak_this = weak_factory_.GetWeakPtr();
    client_->UpdateSelectionBounds();
    if (!weak_this) {
      return;
    }
  }
  has_focus_ = focused;

  if (!has_focus_ || !SupportsKeyboardFocus())
    return;

  if (focus_type != blink::mojom::FocusType::kBackward &&
      focus_type != blink::mojom::FocusType::kForward) {
    return;
  }

  const int modifiers = focus_type == blink::mojom::FocusType::kForward
                            ? blink::WebInputEvent::kNoModifiers
                            : blink::WebInputEvent::kShiftKey;

  blink::WebKeyboardEvent simulated_event(blink::WebInputEvent::Type::kKeyDown,
                                          modifiers, base::TimeTicks());
  simulated_event.windows_key_code = ui::KeyboardCode::VKEY_TAB;
  HandleWebInputEvent(simulated_event);
}

void PdfViewWebPlugin::UpdateVisibility(bool visibility) {}

blink::WebInputEventResult PdfViewWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  const blink::WebInputEventResult result =
      HandleWebInputEvent(event.Event())
          ? blink::WebInputEventResult::kHandledApplication
          : blink::WebInputEventResult::kNotHandled;

  *cursor = cursor_;

  return result;
}

void PdfViewWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {}

void PdfViewWebPlugin::DidReceiveData(base::span<const char> data) {}

void PdfViewWebPlugin::DidFinishLoading() {}

void PdfViewWebPlugin::DidFailLoading(const blink::WebURLError& error) {}

bool PdfViewWebPlugin::SupportsPaginatedPrint() {
  return true;
}

bool PdfViewWebPlugin::GetPrintPresetOptionsFromDocument(
    blink::WebPrintPresetOptions* print_preset_options) {
  print_preset_options->is_scaling_disabled = !engine_->GetPrintScaling();
  print_preset_options->copies = engine_->GetCopiesToPrint();
  print_preset_options->duplex_mode = engine_->GetDuplexMode();
  print_preset_options->uniform_page_size = engine_->GetUniformPageSizePoints();
  return true;
}

int PdfViewWebPlugin::PrintBegin(const blink::WebPrintParams& print_params) {
  // The returned value is always equal to the number of pages in the PDF
  // document irrespective of the printable area.
  int32_t ret = engine_->GetNumberOfPages();
  if (!ret)
    return 0;

  if (!engine_->HasPermission(DocumentPermission::kPrintLowQuality))
    return 0;

  print_params_ = print_params;
  if (!engine_->HasPermission(DocumentPermission::kPrintHighQuality))
    print_params_->rasterize_pdf = true;

  engine_->PrintBegin();
  return ret;
}

void PdfViewWebPlugin::PrintPage(int page_index, cc::PaintCanvas* canvas) {
  // The entire document goes into one metafile. However, it is impossible to
  // know if a call to `PrintPage()` is the last call. Thus, `PrintPage()` just
  // stores the pages to print and the metafile. Eventually, the printed output
  // is generated in `PrintEnd()` and copied over to the metafile.

  // Every `canvas` passed to this method should have a valid `metafile`.
  printing::MetafileSkia* metafile = canvas->GetPrintingMetafile();
  DCHECK(metafile);

  // `pages_to_print_` should be empty iff `printing_metafile_` is not set.
  DCHECK_EQ(pages_to_print_.empty(), !printing_metafile_);

  // The metafile should be the same across all calls for a given print job.
  DCHECK(!printing_metafile_ || (printing_metafile_ == metafile));

  if (!printing_metafile_)
    printing_metafile_ = metafile;

  pages_to_print_.push_back(page_index);
}

void PdfViewWebPlugin::PrintEnd() {
  if (pages_to_print_.empty())
    return;

  print_pages_called_ = true;
  printing_metafile_->InitFromData(
      engine_->PrintPages(pages_to_print_, print_params_.value()));

  if (print_pages_called_)
    client_->RecordComputedAction("PDF.PrintPage");
  print_pages_called_ = false;
  print_params_.reset();
  engine_->PrintEnd();

  printing_metafile_ = nullptr;
  pages_to_print_.clear();
}

bool PdfViewWebPlugin::HasSelection() const {
  return !selected_text_.IsEmpty();
}

blink::WebString PdfViewWebPlugin::SelectionAsText() const {
  return selected_text_;
}

blink::WebString PdfViewWebPlugin::SelectionAsMarkup() const {
  return selected_text_;
}

bool PdfViewWebPlugin::CanEditText() const {
  return engine_->CanEditText();
}

bool PdfViewWebPlugin::HasEditableText() const {
  return engine_->HasEditableText();
}

bool PdfViewWebPlugin::CanUndo() const {
  return engine_->CanUndo();
}

bool PdfViewWebPlugin::CanRedo() const {
  return engine_->CanRedo();
}

bool PdfViewWebPlugin::CanCopy() const {
  return engine_->HasPermission(DocumentPermission::kCopy);
}

bool PdfViewWebPlugin::ExecuteEditCommand(const blink::WebString& name,
                                          const blink::WebString& value) {
  if (name == "SelectAll") {
    return SelectAll();
  }

  if (name == "Cut") {
    SendExecutedEditCommand("Cut");
    return Cut();
  }

  if (name == "Copy") {
    // Deliberately do nothing other than call SendExecutedEditCommand(). The
    // caller is expected to separately call CanCopy() and SelectionAsText().
    SendExecutedEditCommand("Copy");
    return false;
  }

  if (name == "Paste" || name == "PasteAndMatchStyle") {
    SendExecutedEditCommand("Paste");
    return Paste(value);
  }

  if (name == "Undo") {
    return Undo();
  }

  if (name == "Redo") {
    return Redo();
  }

  return false;
}

blink::WebURL PdfViewWebPlugin::LinkAtPosition(
    const gfx::Point& /*position*/) const {
  return GURL(link_under_cursor_);
}

bool PdfViewWebPlugin::StartFind(const blink::WebString& search_text,
                                 bool case_sensitive,
                                 int identifier) {
  if (find_identifier_ == -1) {
    // Only go through this code path when `find_identifier_` is -1. i.e. The
    // first time the user performs find-in-page, or after a StopFind() call.
    // Since StartFind() gets called every time the user changes `search_text`,
    // if this conditional did not exist, then SendStartedFindInPage() would
    // get called too many times compared to the "Find" action in
    // tools/metrics/actions/actions.xml.
    SendStartedFindInPage();
  }

  ResetRecentlySentFindUpdate();
  find_identifier_ = identifier;
  engine_->StartFind(search_text.Utf16(), case_sensitive);
  return true;
}

void PdfViewWebPlugin::SelectFindResult(bool forward, int identifier) {
  find_identifier_ = identifier;
  engine_->SelectFindResult(forward);
}

void PdfViewWebPlugin::StopFind() {
  find_identifier_ = -1;
  engine_->StopFind();
  tickmarks_.clear();
  client_->ReportFindInPageTickmarks(tickmarks_);
}

bool PdfViewWebPlugin::CanRotateView() {
  return !IsPrintPreview();
}

void PdfViewWebPlugin::RotateView(blink::WebPlugin::RotationType type) {
  DCHECK(CanRotateView());

  switch (type) {
    case blink::WebPlugin::RotationType::k90Clockwise:
      engine_->RotateClockwise();
      return;
    case blink::WebPlugin::RotationType::k90Counterclockwise:
      engine_->RotateCounterclockwise();
      return;
  }
  NOTREACHED();
}

bool PdfViewWebPlugin::ShouldDispatchImeEventsToPlugin() {
  return true;
}

blink::WebTextInputType PdfViewWebPlugin::GetPluginTextInputType() {
  return text_input_type_;
}

gfx::Rect PdfViewWebPlugin::GetPluginCaretBounds() {
  return caret_rect_;
}

void PdfViewWebPlugin::ImeSetCompositionForPlugin(
    const blink::WebString& text,
    const std::vector<ui::ImeTextSpan>& /*ime_text_spans*/,
    const gfx::Range& /*replacement_range*/,
    int /*selection_start*/,
    int /*selection_end*/) {
  composition_text_ = text;
}

void PdfViewWebPlugin::ImeCommitTextForPlugin(
    const blink::WebString& text,
    const std::vector<ui::ImeTextSpan>& /*ime_text_spans*/,
    const gfx::Range& /*replacement_range*/,
    int /*relative_cursor_pos*/) {
  HandleImeCommit(text);
}

void PdfViewWebPlugin::ImeFinishComposingTextForPlugin(
    bool /*keep_selection*/) {
  HandleImeCommit(composition_text_);
}

void PdfViewWebPlugin::ProposeDocumentLayout(const DocumentLayout& layout) {
  base::Value::Dict message;
  message.Set("type", "documentDimensions");
  message.Set("width", layout.size().width());
  message.Set("height", layout.size().height());
  message.Set("layoutOptions", layout.options().ToValue());
  base::Value::List page_dimensions;
  for (size_t i = 0; i < layout.page_count(); ++i)
    page_dimensions.Append(DictFromRect(layout.page_rect(i)));
  message.Set("pageDimensions", std::move(page_dimensions));
  client_->PostMessage(std::move(message));

  // Reload the accessibility tree on layout changes because the relative page
  // bounds are no longer valid.
  if (layout.dirty() && accessibility_state_ == AccessibilityState::kLoaded)
    LoadAccessibility();
}

void PdfViewWebPlugin::Invalidate(const gfx::Rect& rect) {
  if (in_paint_) {
    deferred_invalidates_.push_back(rect);
    return;
  }

  gfx::Rect offset_rect = rect + available_area_.OffsetFromOrigin();
  paint_manager_.InvalidateRect(offset_rect);
}

void PdfViewWebPlugin::DidScroll(const gfx::Vector2d& offset) {
  if (!image_data_.drawsNothing())
    paint_manager_.ScrollRect(available_area_, offset);
}

void PdfViewWebPlugin::ScrollToX(int x_screen_coords) {
  const float x_scroll_pos = x_screen_coords / device_scale_;

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("x", static_cast<double>(x_scroll_pos));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::ScrollToY(int y_screen_coords) {
  const float y_scroll_pos = y_screen_coords / device_scale_;

  base::Value::Dict message;
  message.Set("type", "setScrollPosition");
  message.Set("y", static_cast<double>(y_scroll_pos));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::ScrollBy(const gfx::Vector2d& delta) {
  const float x_delta = delta.x() / device_scale_;
  const float y_delta = delta.y() / device_scale_;

  base::Value::Dict message;
  message.Set("type", "scrollBy");
  message.Set("x", static_cast<double>(x_delta));
  message.Set("y", static_cast<double>(y_delta));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::ScrollToPage(int page) {
  if (!engine_ || engine_->GetNumberOfPages() == 0)
    return;

  base::Value::Dict message;
  message.Set("type", "goToPage");
  message.Set("page", page);
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::NavigateTo(const std::string& url,
                                  WindowOpenDisposition disposition) {
  base::Value::Dict message;
  message.Set("type", "navigate");
  message.Set("url", url);
  message.Set("disposition", static_cast<int>(disposition));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::NavigateToDestination(int page,
                                             const float* x,
                                             const float* y,
                                             const float* zoom) {
  base::Value::Dict message;
  message.Set("type", "navigateToDestination");
  message.Set("page", page);
  if (x)
    message.Set("x", static_cast<double>(*x));
  if (y)
    message.Set("y", static_cast<double>(*y));
  if (zoom)
    message.Set("zoom", static_cast<double>(*zoom));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::UpdateCursor(ui::mojom::CursorType new_cursor_type) {
#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_ && ink_module_->enabled()) {
    // Block normal mouse cursor updates, so the cursor set by PdfInkModule
    // while it is enabled does not get overwritten.
    return;
  }
#endif

  cursor_ = new_cursor_type;
}

void PdfViewWebPlugin::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  tickmarks_ = tickmarks;
}

void PdfViewWebPlugin::NotifyNumberOfFindResultsChanged(int total,
                                                        bool final_result) {
  // We don't want to spam the renderer with too many updates to the number of
  // find results. Don't send an update if we sent one too recently. If it's the
  // final update, we always send it though.
  if (recently_sent_find_update_ && !final_result)
    return;

  // After stopping search and setting `find_identifier_` to -1 there still may
  // be a NotifyNumberOfFindResultsChanged notification pending from engine.
  // Just ignore them.
  if (find_identifier_ != -1) {
    client_->ReportFindInPageMatchCount(find_identifier_, total, final_result);
  }

  client_->ReportFindInPageTickmarks(tickmarks_);

  if (final_result)
    return;

  recently_sent_find_update_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::ResetRecentlySentFindUpdate,
                     weak_factory_.GetWeakPtr()),
      kFindResultCooldown);
}

void PdfViewWebPlugin::NotifySelectedFindResultChanged(int current_find_index,
                                                       bool final_result) {
  if (find_identifier_ == -1 || !client_->PluginContainer())
    return;

  DCHECK_GE(current_find_index, -1);
  client_->ReportFindInPageSelection(find_identifier_, current_find_index + 1,
                                     final_result);
}

void PdfViewWebPlugin::NotifyTouchSelectionOccurred() {
  base::Value::Dict message;
  message.Set("type", "touchSelectionOccurred");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::CaretChanged(const gfx::Rect& caret_rect) {
  caret_rect_ = caret_rect + available_area_.OffsetFromOrigin();
}

void PdfViewWebPlugin::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(password_callback_.is_null());
  password_callback_ = std::move(callback);

  base::Value::Dict message;
  message.Set("type", "getPassword");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::Beep() {
  base::Value::Dict message;
  message.Set("type", "beep");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::Alert(const std::string& message) {
  client_->Alert(blink::WebString::FromUTF8(message));
}

bool PdfViewWebPlugin::Confirm(const std::string& message) {
  return client_->Confirm(blink::WebString::FromUTF8(message));
}

std::string PdfViewWebPlugin::Prompt(const std::string& question,
                                     const std::string& default_answer) {
  return client_
      ->Prompt(blink::WebString::FromUTF8(question),
               blink::WebString::FromUTF8(default_answer))
      .Utf8();
}

std::string PdfViewWebPlugin::GetURL() {
  return url_;
}

void PdfViewWebPlugin::LoadUrl(std::string_view url, LoadUrlCallback callback) {
  UrlRequest request;
  request.url = std::string(url);
  request.method = "GET";
  request.ignore_redirects = true;

  auto loader = std::make_unique<UrlLoader>(weak_factory_.GetWeakPtr());
  UrlLoader* raw_loader = loader.get();
  raw_loader->Open(request,
                   base::BindOnce(std::move(callback), std::move(loader)));
}

void PdfViewWebPlugin::Email(const std::string& to,
                             const std::string& cc,
                             const std::string& bcc,
                             const std::string& subject,
                             const std::string& body) {
  base::Value::Dict message;
  message.Set("type", "email");
  message.Set("to", base::EscapeUrlEncodedData(to, false));
  message.Set("cc", base::EscapeUrlEncodedData(cc, false));
  message.Set("bcc", base::EscapeUrlEncodedData(bcc, false));
  message.Set("subject", base::EscapeUrlEncodedData(subject, false));
  message.Set("body", base::EscapeUrlEncodedData(body, false));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::Print() {
  if (!engine_)
    return;

  const bool can_print =
      engine_->HasPermission(DocumentPermission::kPrintLowQuality) ||
      engine_->HasPermission(DocumentPermission::kPrintHighQuality);
  if (!can_print)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PdfViewWebPlugin::OnInvokePrintDialog,
                                weak_factory_.GetWeakPtr()));
}

void PdfViewWebPlugin::SubmitForm(const std::string& url,
                                  const void* data,
                                  int length) {
  // `url` might be a relative URL. Resolve it against the document's URL.
  // TODO(crbug.com/40224475): Probably redundant with `Client::CompleteURL()`.
  GURL resolved_url = GURL(url_).Resolve(url);
  if (!resolved_url.is_valid())
    return;

  UrlRequest request;
  request.url = resolved_url.spec();
  request.method = "POST";
  request.body.assign(static_cast<const char*>(data), length);

  form_loader_ = std::make_unique<UrlLoader>(weak_factory_.GetWeakPtr());
  form_loader_->Open(request, base::BindOnce(&PdfViewWebPlugin::DidFormOpen,
                                             weak_factory_.GetWeakPtr()));
}

void PdfViewWebPlugin::DidFormOpen(int32_t result) {
  // TODO(crbug.com/41317525): Process response.
  LOG_IF(ERROR, result != kSuccess) << "DidFormOpen failed: " << result;
  form_loader_.reset();
}

void PdfViewWebPlugin::DidStartLoading() {
  if (did_call_start_loading_)
    return;

  client_->DidStartLoading();
  did_call_start_loading_ = true;
}

void PdfViewWebPlugin::DidStopLoading() {
  if (!did_call_start_loading_)
    return;

  client_->DidStopLoading();
  did_call_start_loading_ = false;
}

int PdfViewWebPlugin::GetContentRestrictions() const {
  int content_restrictions = kContentRestrictionCut | kContentRestrictionPaste;
  if (!engine_->HasPermission(DocumentPermission::kCopy))
    content_restrictions |= kContentRestrictionCopy;

  if (!engine_->HasPermission(DocumentPermission::kPrintLowQuality) &&
      !engine_->HasPermission(DocumentPermission::kPrintHighQuality)) {
    content_restrictions |= kContentRestrictionPrint;
  }

  return content_restrictions;
}

std::unique_ptr<UrlLoader> PdfViewWebPlugin::CreateUrlLoader() {
  if (full_frame_) {
    DidStartLoading();

    // Disable save and print until the document is fully loaded, since they
    // would generate an incomplete document. This needs to be done each time
    // DidStartLoading() is called because that resets the content restrictions.
    pdf_host_->UpdateContentRestrictions(kContentRestrictionSave |
                                         kContentRestrictionPrint);
  }

  return std::make_unique<UrlLoader>(weak_factory_.GetWeakPtr());
}

v8::Isolate* PdfViewWebPlugin::GetIsolate() {
  return client_->GetIsolate();
}

std::vector<PDFiumEngineClient::SearchStringResult>
PdfViewWebPlugin::SearchString(const char16_t* string,
                               const char16_t* term,
                               bool case_sensitive) {
  base::i18n::RepeatingStringSearch searcher(
      /*find_this=*/term, /*in_this=*/string, case_sensitive);
  std::vector<SearchStringResult> results;
  int match_index;
  int match_length;
  while (searcher.NextMatchResult(match_index, match_length))
    results.push_back({.start_index = match_index, .length = match_length});
  return results;
}

void PdfViewWebPlugin::DocumentLoadComplete() {
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state_);
  document_load_state_ = DocumentLoadState::kComplete;

  client_->RecordComputedAction("PDF.LoadSuccess");

  // Clear the focus state for on-screen keyboards.
  FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kNoFocus);

  if (IsPrintPreview()) {
    // Scroll location is retained across document loads in Print Preview, so
    // there's no need to override the scroll position by scrolling again.
    if (IsPreviewingPDF(print_preview_page_count_)) {
      SendPrintPreviewLoadedNotification();
    } else {
      DCHECK_EQ(0, print_preview_loaded_page_count_);
      print_preview_loaded_page_count_ = 1;
      engine_->AppendBlankPages(print_preview_page_count_);
      LoadNextPreviewPage();
    }

    OnGeometryChanged(0, 0);
    if (!document_size_.IsEmpty())
      paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
  }

  RecordDocumentMetrics();

  if (base::FeatureList::IsEnabled(chrome_pdf::features::kPdfPortfolio)) {
    SendAttachments();
  }
  SendBookmarks();
  SendMetadata();

  if (accessibility_state_ == AccessibilityState::kPending)
    LoadAccessibility();

  // To avoid delaying page load for searchify, start searchify after document
  // load is completed.
  client_->SetOcrDisconnectedCallback(engine_->GetOcrDisconnectHandler());
  engine_->StartSearchify(
      base::BindRepeating(&Client::PerformOcr, client_->GetWeakPtr()));

  if (!full_frame_)
    return;

  DidStopLoading();
  pdf_host_->UpdateContentRestrictions(GetContentRestrictions());
}

void PdfViewWebPlugin::DocumentLoadFailed() {
  DCHECK_EQ(DocumentLoadState::kLoading, document_load_state_);
  document_load_state_ = DocumentLoadState::kFailed;

  client_->RecordComputedAction("PDF.LoadFailure");

  // Send a progress value of -1 to indicate a failure.
  SendLoadingProgress(-1);

  DidStopLoading();

  paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
}

void PdfViewWebPlugin::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  DCHECK(!feature.empty());
  std::string metric = base::StrCat({"PDF_Unsupported_", feature});
  if (unsupported_features_reported_.insert(metric).second)
    client_->RecordComputedAction(metric);

  if (!full_frame_ || notified_browser_about_unsupported_feature_)
    return;

  notified_browser_about_unsupported_feature_ = true;
  pdf_host_->HasUnsupportedFeature();
}

void PdfViewWebPlugin::DocumentLoadProgress(uint32_t available,
                                            uint32_t doc_size) {
  double progress = 0.0;
  if (doc_size > 0) {
    progress = 100.0 * static_cast<double>(available) / doc_size;
  } else {
    // Use heuristics when the document size is unknown.
    // Progress logarithmically from 0 to 100M.
    static const double kFactor = std::log(100'000'000.0) / 100.0;
    if (available > 0)
      progress =
          std::min(std::log(static_cast<double>(available)) / kFactor, 100.0);
  }

  // DocumentLoadComplete() will send the 100% load progress.
  if (progress >= 100)
    return;

  // Avoid sending too many progress messages over PostMessage.
  if (progress <= last_progress_sent_ + 1)
    return;

  SendLoadingProgress(progress);
}

void PdfViewWebPlugin::FormFieldFocusChange(
    PDFiumEngineClient::FocusFieldType type) {
  base::Value::Dict message;
  message.Set("type", "formFocusChange");
  std::string field_type;
  // LINT.IfChange(FocusFieldTypes)
  switch (type) {
    case PDFiumEngineClient::FocusFieldType::kNoFocus:
      field_type = "none";
      break;
    case PDFiumEngineClient::FocusFieldType::kNonText:
      field_type = "non-text";
      break;
    case PDFiumEngineClient::FocusFieldType::kText:
      field_type = "text";
      break;
  }
  // LINT.ThenChange(//chrome/browser/resources/pdf/constants.ts:FocusFieldTypes)
  message.Set("focused", field_type);
  client_->PostMessage(std::move(message));

  text_input_type_ = type == PDFiumEngineClient::FocusFieldType::kText
                         ? blink::WebTextInputType::kWebTextInputTypeText
                         : blink::WebTextInputType::kWebTextInputTypeNone;
  client_->UpdateTextInputState();
}

bool PdfViewWebPlugin::IsPrintPreview() const {
  return is_print_preview_;
}

SkColor PdfViewWebPlugin::GetBackgroundColor() const {
  return background_color_;
}

void PdfViewWebPlugin::SelectionChanged(const gfx::Rect& left,
                                        const gfx::Rect& right) {
  gfx::PointF left_point(left.x() + available_area_.x(), left.y());
  gfx::PointF right_point(right.x() + available_area_.x(), right.y());

  const float inverse_scale = 1.0f / device_scale_;
  left_point.Scale(inverse_scale);
  right_point.Scale(inverse_scale);

  pdf_host_->SelectionChanged(left_point, left.height() * inverse_scale,
                              right_point, right.height() * inverse_scale);

  if (accessibility_state_ == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
}

void PdfViewWebPlugin::EnteredEditMode() {
  edit_mode_ = true;
  pdf_host_->SetPluginCanSave(true);

  base::Value::Dict message;
  message.Set("type", "setIsEditing");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::DocumentFocusChanged(bool document_has_focus) {
  base::Value::Dict message;
  message.Set("type", "documentFocusChanged");
  message.Set("hasFocus", document_has_focus);
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SetSelectedText(const std::string& selected_text) {
  selected_text_ = blink::WebString::FromUTF8(selected_text);
  client_->TextSelectionChanged(selected_text_, /*offset=*/0,
                                gfx::Range(0, selected_text_.length()));
}

void PdfViewWebPlugin::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  link_under_cursor_ = link_under_cursor;
}

bool PdfViewWebPlugin::IsValidLink(const std::string& url) {
  return base::Value(url).is_string();
}

#if BUILDFLAG(ENABLE_PDF_INK2)
bool PdfViewWebPlugin::IsInAnnotationMode() const {
  return ink_module_ && ink_module_->enabled();
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

void PdfViewWebPlugin::SetCaretPosition(const gfx::PointF& position) {
  engine_->SetCaretPosition(FrameToPdfCoordinates(position));
}

void PdfViewWebPlugin::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  engine_->MoveRangeSelectionExtent(FrameToPdfCoordinates(extent));
}

void PdfViewWebPlugin::SetSelectionBounds(const gfx::PointF& base,
                                          const gfx::PointF& extent) {
  engine_->SetSelectionBounds(FrameToPdfCoordinates(base),
                              FrameToPdfCoordinates(extent));
}

void PdfViewWebPlugin::GetPdfBytes(uint32_t size_limit,
                                   GetPdfBytesCallback callback) {
  if (engine_->GetLoadedByteSize() > size_limit) {
    std::move(callback).Run(GetPdfBytesStatus::kSizeLimitExceeded, {});
    return;
  }

  std::move(callback).Run(GetPdfBytesStatus::kSuccess, engine_->GetSaveData());
}

bool PdfViewWebPlugin::IsValid() const {
  return client_->HasFrame();
}

blink::WebURL PdfViewWebPlugin::CompleteURL(
    const blink::WebString& partial_url) const {
  DCHECK(IsValid());
  return client_->CompleteURL(partial_url);
}

net::SiteForCookies PdfViewWebPlugin::SiteForCookies() const {
  DCHECK(IsValid());
  return client_->SiteForCookies();
}

void PdfViewWebPlugin::SetReferrerForRequest(
    blink::WebURLRequest& request,
    const blink::WebURL& referrer_url) {
  client_->SetReferrerForRequest(request, referrer_url);
}

std::unique_ptr<blink::WebAssociatedURLLoader>
PdfViewWebPlugin::CreateAssociatedURLLoader(
    const blink::WebAssociatedURLLoaderOptions& options) {
  return client_->CreateAssociatedURLLoader(options);
}

void PdfViewWebPlugin::OnMessage(const base::Value::Dict& message) {
#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_ && ink_module_->OnMessage(message)) {
    return;
  }
#endif

  using MessageHandler = void (PdfViewWebPlugin::*)(const base::Value::Dict&);

  static constexpr auto kMessageHandlers =
      base::MakeFixedFlatMap<std::string_view, MessageHandler>({
          {"displayAnnotations",
           &PdfViewWebPlugin::HandleDisplayAnnotationsMessage},
          {"getNamedDestination",
           &PdfViewWebPlugin::HandleGetNamedDestinationMessage},
          {"getPageBoundingBox",
           &PdfViewWebPlugin::HandleGetPageBoundingBoxMessage},
          {"getPasswordComplete",
           &PdfViewWebPlugin::HandleGetPasswordCompleteMessage},
          {"getSelectedText", &PdfViewWebPlugin::HandleGetSelectedTextMessage},
          {"getThumbnail", &PdfViewWebPlugin::HandleGetThumbnailMessage},
          {"print", &PdfViewWebPlugin::HandlePrintMessage},
          {"loadPreviewPage", &PdfViewWebPlugin::HandleLoadPreviewPageMessage},
          {"resetPrintPreviewMode",
           &PdfViewWebPlugin::HandleResetPrintPreviewModeMessage},
          {"rotateClockwise", &PdfViewWebPlugin::HandleRotateClockwiseMessage},
          {"rotateCounterclockwise",
           &PdfViewWebPlugin::HandleRotateCounterclockwiseMessage},
          {"save", &PdfViewWebPlugin::HandleSaveMessage},
          {"saveAttachment", &PdfViewWebPlugin::HandleSaveAttachmentMessage},
          {"selectAll", &PdfViewWebPlugin::HandleSelectAllMessage},
          {"setBackgroundColor",
           &PdfViewWebPlugin::HandleSetBackgroundColorMessage},
          {"setPresentationMode",
           &PdfViewWebPlugin::HandleSetPresentationModeMessage},
          {"setTwoUpView", &PdfViewWebPlugin::HandleSetTwoUpViewMessage},
          {"stopScrolling", &PdfViewWebPlugin::HandleStopScrollingMessage},
          {"viewport", &PdfViewWebPlugin::HandleViewportMessage},
      });

  MessageHandler handler = kMessageHandlers.at(*message.FindString("type"));
  (this->*handler)(message);
}

void PdfViewWebPlugin::HandleDisplayAnnotationsMessage(
    const base::Value::Dict& message) {
  engine_->DisplayAnnotations(message.FindBool("display").value());
}

void PdfViewWebPlugin::HandleGetNamedDestinationMessage(
    const base::Value::Dict& message) {
  std::optional<PDFiumEngine::NamedDestination> named_destination =
      engine_->GetNamedDestination(*message.FindString("namedDestination"));

  const int page_number = named_destination.has_value()
                              ? base::checked_cast<int>(named_destination->page)
                              : -1;

  base::Value::Dict reply =
      PrepareReplyMessage("getNamedDestinationReply", message);
  reply.Set("pageNumber", page_number);

  if (named_destination.has_value() && !named_destination->view.empty()) {
    std::ostringstream view_stream;
    view_stream << named_destination->view;
    if (named_destination->xyz_params.empty()) {
      for (unsigned long i = 0; i < named_destination->num_params; ++i)
        view_stream << "," << named_destination->params[i];
    } else {
      view_stream << "," << named_destination->xyz_params;
    }

    reply.Set("namedDestinationView", view_stream.str());
  }

  client_->PostMessage(std::move(reply));
}

void PdfViewWebPlugin::HandleGetPageBoundingBoxMessage(
    const base::Value::Dict& message) {
  const int page_index = message.FindInt("page").value();
  base::Value::Dict reply =
      PrepareReplyMessage("getPageBoundingBoxReply", message);

  PDFiumPage* page = engine_->GetPage(page_index);
  CHECK(page);
  gfx::RectF bounding_box = page->GetBoundingBox();
  gfx::Rect page_bounds = page->rect();

  // Flip the origin from bottom-left to top-left.
  bounding_box.set_y(static_cast<float>(page_bounds.height()) -
                     bounding_box.bottom());
  reply.Set("x", bounding_box.x());
  reply.Set("y", bounding_box.y());
  reply.Set("width", bounding_box.width());
  reply.Set("height", bounding_box.height());

  client_->PostMessage(std::move(reply));
}

void PdfViewWebPlugin::HandleGetPasswordCompleteMessage(
    const base::Value::Dict& message) {
  DCHECK(password_callback_);
  std::move(password_callback_).Run(*message.FindString("password"));
}

void PdfViewWebPlugin::HandleGetSelectedTextMessage(
    const base::Value::Dict& message) {
  // Always return unix newlines to JavaScript.
  std::string selected_text;
  base::RemoveChars(engine_->GetSelectedText(), "\r", &selected_text);

  base::Value::Dict reply =
      PrepareReplyMessage("getSelectedTextReply", message);
  reply.Set("selectedText", selected_text);
  client_->PostMessage(std::move(reply));
}

void PdfViewWebPlugin::HandleGetThumbnailMessage(
    const base::Value::Dict& message) {
  const int page_index = message.FindInt("pageIndex").value();
  base::Value::Dict reply = PrepareReplyMessage("getThumbnailReply", message);

  engine_->RequestThumbnail(
      page_index, device_scale_,
      base::BindOnce(&PdfViewWebPlugin::SendThumbnail,
                     weak_factory_.GetWeakPtr(), std::move(reply), page_index));
}

void PdfViewWebPlugin::HandlePrintMessage(
    const base::Value::Dict& /*message*/) {
  Print();
}

void PdfViewWebPlugin::HandleRotateClockwiseMessage(
    const base::Value::Dict& /*message*/) {
  engine_->RotateClockwise();
}

void PdfViewWebPlugin::HandleRotateCounterclockwiseMessage(
    const base::Value::Dict& /*message*/) {
  engine_->RotateCounterclockwise();
}

void PdfViewWebPlugin::HandleSaveAttachmentMessage(
    const base::Value::Dict& message) {
  const int index = message.FindInt("attachmentIndex").value();

  const std::vector<DocumentAttachmentInfo>& list =
      engine_->GetDocumentAttachmentInfoList();
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<size_t>(index), list.size());
  DCHECK(list[index].is_readable);
  DCHECK(IsSaveDataSizeValid(list[index].size_bytes));

  std::vector<uint8_t> data = engine_->GetAttachmentData(index);
  base::Value data_to_save(
      IsSaveDataSizeValid(data.size()) ? data : std::vector<uint8_t>());

  base::Value::Dict reply = PrepareReplyMessage("saveAttachmentReply", message);
  reply.Set("dataToSave", std::move(data_to_save));
  client_->PostMessage(std::move(reply));
}

void PdfViewWebPlugin::HandleSaveMessage(const base::Value::Dict& message) {
  const std::string& token = *message.FindString("token");
  int request_type = message.FindInt("saveRequestType").value();
  DCHECK_GE(request_type, static_cast<int>(SaveRequestType::kAnnotation));
  DCHECK_LE(request_type, static_cast<int>(SaveRequestType::kEdited));

  switch (static_cast<SaveRequestType>(request_type)) {
    case SaveRequestType::kAnnotation:
#if BUILDFLAG(ENABLE_INK)
      // In annotation mode, assume the user will make edits and prefer saving
      // using the plugin data.
      pdf_host_->SetPluginCanSave(true);
      SaveToBuffer(token);
      return;
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_INK)
    case SaveRequestType::kOriginal:
      pdf_host_->SetPluginCanSave(false);
      SaveToFile(token);
      pdf_host_->SetPluginCanSave(edit_mode_);
      return;
    case SaveRequestType::kEdited:
      SaveToBuffer(token);
      return;
  }
  NOTREACHED();
}

void PdfViewWebPlugin::HandleSelectAllMessage(
    const base::Value::Dict& /*message*/) {
  engine_->SelectAll();
}

void PdfViewWebPlugin::HandleSetBackgroundColorMessage(
    const base::Value::Dict& message) {
  background_color_ =
      base::checked_cast<SkColor>(message.FindDouble("color").value());
}

void PdfViewWebPlugin::HandleSetPresentationModeMessage(
    const base::Value::Dict& message) {
  engine_->SetReadOnly(message.FindBool("enablePresentationMode").value());
}

void PdfViewWebPlugin::HandleSetTwoUpViewMessage(
    const base::Value::Dict& message) {
  engine_->SetDocumentLayout(message.FindBool("enableTwoUpView").value()
                                 ? DocumentLayout::PageSpread::kTwoUpOdd
                                 : DocumentLayout::PageSpread::kOneUp);
}

void PdfViewWebPlugin::HandleStopScrollingMessage(
    const base::Value::Dict& /*message*/) {
  stop_scrolling_ = true;
}

void PdfViewWebPlugin::HandleViewportMessage(const base::Value::Dict& message) {
  const base::Value::Dict* layout_options_value =
      message.FindDict("layoutOptions");
  if (layout_options_value) {
    DocumentLayout::Options layout_options;
    layout_options.FromValue(*layout_options_value);

    ui_direction_ = layout_options.direction();

    // TODO(crbug.com/40652841): Eliminate need to get document size from here.
    document_size_ = engine_->ApplyDocumentLayout(layout_options);

    OnGeometryChanged(zoom_, device_scale_);
    if (!document_size_.IsEmpty())
      paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));

    // Send 100% loading progress only after initial layout negotiated.
    if (last_progress_sent_ < 100 &&
        document_load_state_ == DocumentLoadState::kComplete) {
      SendLoadingProgress(/*percentage=*/100);
    }
  }

  gfx::Vector2dF scroll_offset(*message.FindDouble("xOffset"),
                               *message.FindDouble("yOffset"));
  double new_zoom = *message.FindDouble("zoom");
  const PinchPhase pinch_phase =
      static_cast<PinchPhase>(*message.FindInt("pinchPhase"));

  received_viewport_message_ = true;
  stop_scrolling_ = false;
  const double zoom_ratio = new_zoom / zoom_;

  if (pinch_phase == PinchPhase::kStart) {
    scroll_offset_at_last_raster_ = scroll_offset;
    last_bitmap_smaller_ = false;
    needs_reraster_ = false;
    return;
  }

  // When zooming in, we set a layer transform to avoid unneeded rerasters.
  // Also, if we're zooming out and the last time we rerastered was when
  // we were even further zoomed out (i.e. we pinch zoomed in and are now
  // pinch zooming back out in the same gesture), we update the layer
  // transform instead of rerastering.
  if (pinch_phase == PinchPhase::kUpdateZoomIn ||
      (pinch_phase == PinchPhase::kUpdateZoomOut && zoom_ratio > 1.0)) {
    // Get the coordinates of the center of the pinch gesture.
    const double pinch_x = *message.FindDouble("pinchX");
    const double pinch_y = *message.FindDouble("pinchY");
    gfx::Point pinch_center(pinch_x, pinch_y);

    // Get the pinch vector which represents the panning caused by the change in
    // pinch center between the start and the end of the gesture.
    const double pinch_vector_x = *message.FindDouble("pinchVectorX");
    const double pinch_vector_y = *message.FindDouble("pinchVectorY");
    gfx::Vector2d pinch_vector =
        gfx::Vector2d(pinch_vector_x * zoom_ratio, pinch_vector_y * zoom_ratio);

    gfx::Vector2d scroll_delta;
    // If the rendered document doesn't fill the display area we will
    // use `paint_offset` to anchor the paint vertically into the same place.
    // We use the scroll bars instead of the pinch vector to get the actual
    // position on screen of the paint.
    gfx::Vector2d paint_offset;

    if (plugin_rect_.width() > GetDocumentPixelWidth() * zoom_ratio) {
      // We want to keep the paint in the middle but it must stay in the same
      // position relative to the scroll bars.
      paint_offset = gfx::Vector2d(0, (1 - zoom_ratio) * pinch_center.y());
      scroll_delta = gfx::Vector2d(
          0,
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));

      pinch_vector = gfx::Vector2d();
      last_bitmap_smaller_ = true;
    } else if (last_bitmap_smaller_) {
      // When the document width covers the display area's width, we will anchor
      // the scroll bars disregarding where the actual pinch certer is.
      pinch_center = gfx::Point((plugin_rect_.width() / device_scale_) / 2,
                                (plugin_rect_.height() / device_scale_) / 2);
      const double zoom_when_doc_covers_plugin_width =
          zoom_ * plugin_rect_.width() / GetDocumentPixelWidth();
      paint_offset = gfx::Vector2d(
          (1 - new_zoom / zoom_when_doc_covers_plugin_width) * pinch_center.x(),
          (1 - zoom_ratio) * pinch_center.y());
      pinch_vector = gfx::Vector2d();
      scroll_delta = gfx::Vector2d(
          (scroll_offset.x() - scroll_offset_at_last_raster_.x() * zoom_ratio),
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));
    }

    paint_manager_.SetTransform(zoom_ratio, pinch_center,
                                pinch_vector + paint_offset + scroll_delta,
                                true);
    needs_reraster_ = false;
    return;
  }

  if (pinch_phase == PinchPhase::kUpdateZoomOut ||
      pinch_phase == PinchPhase::kEnd) {
    // We reraster on pinch zoom out in order to solve the invalid regions
    // that appear after zooming out.
    // On pinch end the scale is again 1.f and we request a reraster
    // in the new position.
    paint_manager_.ClearTransform();
    last_bitmap_smaller_ = false;
    needs_reraster_ = true;

    // If we're rerastering due to zooming out, we need to update the scroll
    // offset for the last raster, in case the user continues the gesture by
    // zooming in.
    scroll_offset_at_last_raster_ = scroll_offset;
  }

  // Bound the input parameters.
  new_zoom = std::max(kMinZoom, new_zoom);
  DCHECK(message.FindBool("userInitiated").has_value());

  double old_zoom = zoom_;
  zoom_ = new_zoom;

  OnGeometryChanged(old_zoom, device_scale_);
  if (!document_size_.IsEmpty())
    paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));

  UpdateScroll(GetScrollPositionFromOffset(scroll_offset));
}

void PdfViewWebPlugin::SaveToBuffer(const std::string& token) {
  engine_->KillFormFocus();

  base::Value::Dict message;
  message.Set("type", "saveData");
  message.Set("token", token);
  message.Set("fileName", GetFileNameForSaveFromUrl(url_));

  // Expose `edit_mode_` state for integration testing.
  message.Set("editModeForTesting", edit_mode_);

  base::Value data_to_save;
  if (edit_mode_) {
    base::Value::BlobStorage data = engine_->GetSaveData();
    if (IsSaveDataSizeValid(data.size()))
      data_to_save = base::Value(std::move(data));
  } else {
#if BUILDFLAG(ENABLE_INK)
    uint32_t length = engine_->GetLoadedByteSize();
    if (IsSaveDataSizeValid(length)) {
      base::Value::BlobStorage data(length);
      if (engine_->ReadLoadedBytes(length, data.data()))
        data_to_save = base::Value(std::move(data));
    }
#else
    NOTREACHED();
#endif  // BUILDFLAG(ENABLE_INK)
  }

  message.Set("dataToSave", std::move(data_to_save));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SaveToFile(const std::string& token) {
  engine_->KillFormFocus();

  base::Value::Dict message;
  message.Set("type", "consumeSaveToken");
  message.Set("token", token);
  client_->PostMessage(std::move(message));

  pdf_host_->SaveUrlAs(GURL(url_), network::mojom::ReferrerPolicy::kDefault);
}

void PdfViewWebPlugin::InvalidatePluginContainer() {
  client_->Invalidate();
}

void PdfViewWebPlugin::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                               std::vector<PaintReadyRect>& ready,
                               std::vector<gfx::Rect>& pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  DoPaint(paint_rects, ready, pending);
}

gfx::PointF PdfViewWebPlugin::GetScrollPositionFromOffset(
    const gfx::Vector2dF& scroll_offset) const {
  gfx::PointF scroll_origin;

  // TODO(crbug.com/40726602): Right-to-left scrolling currently is not
  // compatible with the PDF viewer's sticky "scroller" element.
  if (ui_direction_ == base::i18n::RIGHT_TO_LEFT && IsPrintPreview()) {
    scroll_origin.set_x(
        std::max(document_size_.width() * static_cast<float>(zoom_) -
                     plugin_dip_size_.width(),
                 0.0f));
  }

  return scroll_origin + scroll_offset;
}

void PdfViewWebPlugin::DoPaint(const std::vector<gfx::Rect>& paint_rects,
                               std::vector<PaintReadyRect>& ready,
                               std::vector<gfx::Rect>& pending) {
  if (image_data_.drawsNothing()) {
    DCHECK(plugin_rect_.IsEmpty());
    return;
  }

  PrepareForFirstPaint(ready);

  if (!received_viewport_message_ || !needs_reraster_)
    return;

  engine_->PrePaint();

  std::vector<gfx::Rect> ready_rects;
  for (const gfx::Rect& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    gfx::Rect rect =
        gfx::IntersectRects(paint_rect, gfx::Rect(plugin_rect_.size()));
    if (rect.IsEmpty())
      continue;

    // Paint the rendering of the PDF document.
    gfx::Rect pdf_rect = gfx::IntersectRects(rect, available_area_);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(-available_area_.x(), 0);

      std::vector<gfx::Rect> pdf_ready;
      std::vector<gfx::Rect> pdf_pending;
      engine_->Paint(pdf_rect, image_data_, pdf_ready, pdf_pending);
      for (gfx::Rect& ready_rect : pdf_ready) {
        ready_rect.Offset(available_area_.OffsetFromOrigin());
        ready_rects.push_back(ready_rect);
      }
      for (gfx::Rect& pending_rect : pdf_pending) {
        pending_rect.Offset(available_area_.OffsetFromOrigin());
        pending.push_back(pending_rect);
      }
    }

    // Ensure the region above the first page (if any) is filled;
    const int32_t first_page_ypos = 0 == engine_->GetNumberOfPages()
                                        ? 0
                                        : engine_->GetPageScreenRect(0).y();
    if (rect.y() < first_page_ypos) {
      gfx::Rect region = gfx::IntersectRects(
          rect, gfx::Rect(gfx::Size(plugin_rect_.width(), first_page_ypos)));
      image_data_.erase(GetBackgroundColor(), gfx::RectToSkIRect(region));
      ready_rects.push_back(region);
    }

    // Ensure the background parts are filled.
    for (const BackgroundPart& background_part : background_parts_) {
      gfx::Rect intersection =
          gfx::IntersectRects(background_part.location, rect);
      if (!intersection.IsEmpty()) {
        image_data_.erase(background_part.color,
                          gfx::RectToSkIRect(intersection));
        ready_rects.push_back(intersection);
      }
    }
  }

  engine_->PostPaint();

  // TODO(crbug.com/40203030): Write pixels directly to the `SkSurface` in
  // `PaintManager`, rather than using an intermediate `SkBitmap` and `SkImage`.
  sk_sp<SkImage> painted_image = image_data_.asImage();
  for (const gfx::Rect& ready_rect : ready_rects)
    ready.emplace_back(ready_rect, painted_image);

  InvalidateAfterPaintDone();
}

void PdfViewWebPlugin::PrepareForFirstPaint(
    std::vector<PaintReadyRect>& ready) {
  if (!first_paint_)
    return;

  // Fill the image data buffer with the background color.
  first_paint_ = false;
  image_data_.eraseColor(background_color_);
  ready.emplace_back(gfx::SkIRectToRect(image_data_.bounds()),
                     image_data_.asImage(), /*flush_now=*/true);
}

void PdfViewWebPlugin::OnGeometryChanged(double old_zoom,
                                         float old_device_scale) {
  RecalculateAreas(old_zoom, old_device_scale);

  if (accessibility_state_ == AccessibilityState::kLoaded) {
    PrepareAndSetAccessibilityViewportInfo();
  }

#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_) {
    ink_module_->OnGeometryChanged();
  }
#endif
}

void PdfViewWebPlugin::RecalculateAreas(double old_zoom,
                                        float old_device_scale) {
  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    engine_->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = gfx::Rect(plugin_rect_.size());
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    // Center the document horizontally inside the plugin rectangle.
    available_area_.Offset((plugin_rect_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }

  // The distance between top of the plugin and the bottom of the document in
  // pixels.
  int bottom_of_document = GetDocumentPixelHeight();
  if (bottom_of_document < plugin_rect_.height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();

  engine_->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine_->PluginSizeUpdated(available_area_.size());
}

void PdfViewWebPlugin::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = std::abs(plugin_rect_.width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_rect_.height());

  // Note: we assume the display of the PDF document is always centered
  // horizontally, but not necessarily centered vertically.
  // Add the left rectangle.
  BackgroundPart part = {gfx::Rect(left_width, bottom), GetBackgroundColor()};
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the right rectangle.
  part.location = gfx::Rect(right_start, 0, right_width, bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);

  // Add the bottom rectangle.
  part.location = gfx::Rect(0, bottom, plugin_rect_.width(),
                            plugin_rect_.height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

int PdfViewWebPlugin::GetDocumentPixelWidth() const {
  return static_cast<int>(
      std::ceil(document_size_.width() * zoom_ * device_scale_));
}

int PdfViewWebPlugin::GetDocumentPixelHeight() const {
  return static_cast<int>(
      std::ceil(document_size_.height() * zoom_ * device_scale_));
}

void PdfViewWebPlugin::InvalidateAfterPaintDone() {
  if (deferred_invalidates_.empty())
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PdfViewWebPlugin::ClearDeferredInvalidates,
                                weak_factory_.GetWeakPtr()));
}

void PdfViewWebPlugin::ClearDeferredInvalidates() {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

void PdfViewWebPlugin::UpdateSnapshot(sk_sp<SkImage> snapshot) {
  // Every time something changes (e.g. scale or scroll position),
  // `UpdateSnapshot()` is called, so the snapshot is effectively used only
  // once. Make it "no-cache" so that the old snapshots are not cached
  // downstream.
  //
  // Otherwise, for instance when scrolling, all the previous snapshots end up
  // accumulating in the (for the GPU path) GpuImageDecodeCache, and then in the
  // service transfer cache. The size of the service transfer cache is bounded,
  // so on desktop this "only" causes a 256MiB memory spike, but it's completely
  // wasted memory nonetheless.
  snapshot_ =
      cc::PaintImageBuilder::WithDefault()
          .set_image(std::move(snapshot), cc::PaintImage::GetNextContentId())
          .set_id(cc::PaintImage::GetNextId())
          .set_no_cache(true)
          .TakePaintImage();

  if (!plugin_rect_.IsEmpty())
    InvalidatePluginContainer();
}

void PdfViewWebPlugin::UpdateScaledValues() {
  total_translate_ = snapshot_translate_;

  if (viewport_to_dip_scale_ != 1.0f)
    total_translate_.Scale(1.0f / viewport_to_dip_scale_);
}

void PdfViewWebPlugin::UpdateScale(float scale) {
  CHECK_GT(scale, 0.0f);
  viewport_to_dip_scale_ = scale;
  UpdateScaledValues();
}

void PdfViewWebPlugin::UpdateLayerTransform(float scale,
                                            const gfx::Vector2dF& translate) {
  snapshot_translate_ = translate;
  snapshot_scale_ = scale;
  UpdateScaledValues();
}

void PdfViewWebPlugin::EnableAccessibility() {
  if (accessibility_state_ == AccessibilityState::kLoaded)
    return;

  LoadOrReloadAccessibility();
}

SkBitmap PdfViewWebPlugin::GetImageForOcr(int32_t page_index,
                                          int32_t page_object_index) {
  return engine_->GetImageForOcr(page_index, page_object_index);
}

#if BUILDFLAG(ENABLE_PDF_INK2)
PageOrientation PdfViewWebPlugin::GetOrientation() const {
  return engine_->GetCurrentOrientation();
}

gfx::Rect PdfViewWebPlugin::GetPageContentsRect(int index) {
  if (index < 0 || index >= engine_->GetNumberOfPages()) {
    return gfx::Rect();
  }

  return engine_->GetPageContentsRect(index);
}

gfx::Vector2dF PdfViewWebPlugin::GetViewportOriginOffset() {
  return available_area_.OffsetFromOrigin();
}

float PdfViewWebPlugin::GetZoom() const {
  return zoom_;
}

bool PdfViewWebPlugin::IsPageVisible(int page_index) {
  return engine_->IsPageVisible(page_index);
}

#if BUILDFLAG(ENABLE_PDF_INK2)
void PdfViewWebPlugin::OnAnnotationModeToggled(bool enable) {
  engine_->SetFormHighlight(/*enable_form=*/!enable);
  if (enable) {
    engine_->ClearTextSelection();
  }
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

void PdfViewWebPlugin::StrokeFinished() {
  base::Value::Dict message;
  message.Set("type", "finishInkStroke");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::UpdateInkCursorImage(SkBitmap bitmap) {
  gfx::Point hotspot(bitmap.width() / 2, bitmap.height() / 2);
  cursor_ = ui::Cursor::NewCustom(std::move(bitmap), std::move(hotspot));
}

void PdfViewWebPlugin::UpdateThumbnail(int page_index) {
  GenerateAndSendInkThumbnail(
      page_index, engine_->GetThumbnailSize(page_index, device_scale_));
}

int PdfViewWebPlugin::VisiblePageIndexFromPoint(const gfx::PointF& point) {
  for (int i = 0; i < engine_->GetNumberOfPages(); ++i) {
    if (!IsPageVisible(i)) {
      continue;
    }

    // Explicitly construct a gfx::RectF from gfx::Rect, so the Contains() call
    // below works with `point`, which has float values.
    gfx::RectF rect(engine_->GetPageContentsRect(i));
    if (!rect.Contains(point)) {
      continue;
    }
    return i;
  }
  return -1;
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

void PdfViewWebPlugin::HandleAccessibilityAction(
    const AccessibilityActionData& action_data) {
  engine_->HandleAccessibilityAction(action_data);
}

void PdfViewWebPlugin::LoadOrReloadAccessibility() {
  if (accessibility_state_ == AccessibilityState::kOff) {
    accessibility_state_ = AccessibilityState::kPending;
  }

  if (document_load_state_ == DocumentLoadState::kComplete) {
    LoadAccessibility();
  }
}

void PdfViewWebPlugin::OnViewportChanged(
    const gfx::Rect& new_plugin_rect_in_css_pixel,
    float new_device_scale) {
  DCHECK_GT(new_device_scale, 0.0f);

  css_plugin_rect_ = new_plugin_rect_in_css_pixel;

  if (new_device_scale == device_scale_ &&
      new_plugin_rect_in_css_pixel == plugin_rect_) {
    return;
  }

  const float old_device_scale = device_scale_;
  device_scale_ = new_device_scale;
  plugin_rect_ = new_plugin_rect_in_css_pixel;

  // TODO(crbug.com/40791703): We should try to avoid the downscaling in this
  // calculation, perhaps by migrating off `plugin_dip_size_`.
  plugin_dip_size_ = gfx::ScaleToEnclosingRect(new_plugin_rect_in_css_pixel,
                                               1.0f / new_device_scale)
                         .size();

  paint_manager_.SetSize(plugin_rect_.size(), device_scale_);

  // Initialize the image data buffer if the context size changes.
  const gfx::Size old_image_size = gfx::SkISizeToSize(image_data_.dimensions());
  const gfx::Size new_image_size =
      PaintManager::GetNewContextSize(old_image_size, plugin_rect_.size());
  if (new_image_size != old_image_size) {
    image_data_.allocPixels(
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(new_image_size)));
    first_paint_ = true;
  }

  // Skip updating the geometry if the new image data buffer is empty.
  if (image_data_.drawsNothing())
    return;

  OnGeometryChanged(zoom_, old_device_scale);
}

bool PdfViewWebPlugin::SelectAll() {
  engine_->SelectAll();
  return true;
}

bool PdfViewWebPlugin::Cut() {
  if (!HasSelection() || !CanEditText())
    return false;

  engine_->ReplaceSelection("");
  return true;
}

bool PdfViewWebPlugin::Paste(const blink::WebString& value) {
  if (!CanEditText())
    return false;

  engine_->ReplaceSelection(value.Utf8());
  return true;
}

bool PdfViewWebPlugin::Undo() {
  if (!CanUndo())
    return false;

  engine_->Undo();
  return true;
}

bool PdfViewWebPlugin::Redo() {
  if (!CanRedo())
    return false;

  engine_->Redo();
  return true;
}

bool PdfViewWebPlugin::HandleWebInputEvent(const blink::WebInputEvent& event) {
  // Ignore user input in read-only mode.
  if (engine_->IsReadOnly())
    return false;

  // `engine_` expects input events in device coordinates.
  float viewport_to_device_scale = viewport_to_dip_scale_ * device_scale_;
  std::unique_ptr<blink::WebInputEvent> transformed_event =
      ui::TranslateAndScaleWebInputEvent(
          event,
          gfx::Vector2dF(-available_area_.x() / viewport_to_device_scale, 0),
          viewport_to_device_scale);

  const blink::WebInputEvent& event_to_handle =
      transformed_event ? *transformed_event : event;

#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_ && ink_module_->HandleInputEvent(event_to_handle)) {
    return true;
  }

  if (IsInAnnotationMode()) {
    // When in annotation mode, only handle ink input events.
    return false;
  }
#endif

  if (engine_->HandleInputEvent(event_to_handle))
    return true;

  // Middle click is used for scrolling and is handled by the container page.
  if (blink::WebInputEvent::IsMouseEventType(event_to_handle.GetType()) &&
      static_cast<const blink::WebMouseEvent&>(event_to_handle).button ==
          blink::WebPointerProperties::Button::kMiddle) {
    return false;
  }

  // Return true for unhandled clicks so the plugin takes focus.
  return event_to_handle.GetType() == blink::WebInputEvent::Type::kMouseDown;
}

void PdfViewWebPlugin::HandleImeCommit(const blink::WebString& text) {
  if (text.IsEmpty())
    return;

  std::u16string text16 = text.Utf16();
  composition_text_.Reset();

  size_t i = 0;
  for (base::i18n::UTF16CharIterator iterator(text16); iterator.Advance();) {
    blink::WebKeyboardEvent char_event(blink::WebInputEvent::Type::kChar,
                                       blink::WebInputEvent::kNoModifiers,
                                       ui::EventTimeForNow());
    char_event.windows_key_code = text16[i];
    char_event.native_key_code = text16[i];

    for (const size_t char_start = i; i < iterator.array_pos(); ++i) {
      char_event.text[i - char_start] = text16[i];
      char_event.unmodified_text[i - char_start] = text16[i];
    }

    blink::WebCoalescedInputEvent input_event(char_event, ui::LatencyInfo());
    ui::Cursor dummy_cursor_info;
    HandleInputEvent(input_event, &dummy_cursor_info);
  }
}

void PdfViewWebPlugin::OnInvokePrintDialog() {
  client_->Print();
}

void PdfViewWebPlugin::ResetRecentlySentFindUpdate() {
  recently_sent_find_update_ = false;
}

void PdfViewWebPlugin::RecordDocumentMetrics() {
  if (!metrics_handler_)
    return;

  metrics_handler_->RecordDocumentMetrics(engine_->GetDocumentMetadata());
}

void PdfViewWebPlugin::SendAttachments() {
  const std::vector<DocumentAttachmentInfo>& attachment_infos =
      engine_->GetDocumentAttachmentInfoList();
  if (attachment_infos.empty())
    return;

  base::Value::List attachments;
  for (const DocumentAttachmentInfo& attachment_info : attachment_infos) {
    // Send `size` as -1 to indicate that the attachment is too large to be
    // downloaded.
    const int size = attachment_info.size_bytes <= kMaximumSavedFileSize
                         ? static_cast<int>(attachment_info.size_bytes)
                         : -1;

    base::Value::Dict attachment;
    attachment.Set("name", attachment_info.name);
    attachment.Set("size", size);
    attachment.Set("readable", attachment_info.is_readable);
    attachments.Append(std::move(attachment));
  }

  base::Value::Dict message;
  message.Set("type", "attachments");
  message.Set("attachmentsData", std::move(attachments));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendBookmarks() {
  base::Value::List bookmarks = engine_->GetBookmarks();
  if (bookmarks.empty())
    return;

  base::Value::Dict message;
  message.Set("type", "bookmarks");
  message.Set("bookmarksData", std::move(bookmarks));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendExecutedEditCommand(std::string_view edit_command) {
  base::Value::Dict message;
  message.Set("type", "executedEditCommand");
  message.Set("editCommand", edit_command);
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendStartedFindInPage() {
  base::Value::Dict message;
  message.Set("type", "startedFindInPage");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendMetadata() {
  base::Value::Dict metadata;
  const DocumentMetadata& document_metadata = engine_->GetDocumentMetadata();

  const std::string version = FormatPdfVersion(document_metadata.version);
  if (!version.empty())
    metadata.Set("version", version);

  metadata.Set("fileSize", ui::FormatBytes(document_metadata.size_bytes));

  metadata.Set("linearized", document_metadata.linearized);

  if (!document_metadata.title.empty())
    metadata.Set("title", document_metadata.title);

  if (!document_metadata.author.empty())
    metadata.Set("author", document_metadata.author);

  if (!document_metadata.subject.empty())
    metadata.Set("subject", document_metadata.subject);

  if (!document_metadata.keywords.empty())
    metadata.Set("keywords", document_metadata.keywords);

  if (!document_metadata.creator.empty())
    metadata.Set("creator", document_metadata.creator);

  if (!document_metadata.producer.empty())
    metadata.Set("producer", document_metadata.producer);

  if (!document_metadata.creation_date.is_null()) {
    metadata.Set("creationDate", base::TimeFormatShortDateAndTime(
                                     document_metadata.creation_date));
  }

  if (!document_metadata.mod_date.is_null()) {
    metadata.Set("modDate",
                 base::TimeFormatShortDateAndTime(document_metadata.mod_date));
  }

  metadata.Set("pageSize", FormatPageSize(engine_->GetUniformPageSizePoints()));

  metadata.Set("canSerializeDocument",
               IsSaveDataSizeValid(engine_->GetLoadedByteSize()));

  base::Value::Dict message;
  message.Set("type", "metadata");
  message.Set("metadataData", std::move(metadata));
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendLoadingProgress(double percentage) {
  DCHECK(percentage == -1 || (percentage >= 0 && percentage <= 100));
  last_progress_sent_ = percentage;

  base::Value::Dict message;
  message.Set("type", "loadProgress");
  message.Set("progress", percentage);
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::HandleResetPrintPreviewModeMessage(
    const base::Value::Dict& message) {
  const std::string& url = *message.FindString("url");
  bool is_grayscale = message.FindBool("grayscale").value();
  int print_preview_page_count = message.FindInt("pageCount").value();

  // For security reasons, crash if `url` is not for Print Preview.
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));

  DCHECK_GE(print_preview_page_count, 0);

  int page_index = ExtractPrintPreviewPageIndex(url);
  if (IsPreviewingPDF(print_preview_page_count)) {
    DCHECK_EQ(page_index, kCompletePDFIndex);
  } else {
    DCHECK_GE(page_index, 0);
  }

  print_preview_page_count_ = print_preview_page_count;
  print_preview_loaded_page_count_ = 0;
  url_ = url;
  preview_pages_info_ = base::queue<PreviewPageInfo>();
  preview_document_load_state_ = DocumentLoadState::kComplete;
  document_load_state_ = DocumentLoadState::kLoading;
  last_progress_sent_ = 0;
  LoadUrl(url_, base::BindOnce(&PdfViewWebPlugin::DidOpen,
                               weak_factory_.GetWeakPtr()));
  preview_engine_.reset();

  // TODO(crbug.com/40193305): Figure out a more consistent way to preserve
  // engine settings across a Print Preview reset.
  engine_ = client_->CreateEngine(
      this, PDFiumFormFiller::ScriptOption::kNoJavaScript);
  engine_->ZoomUpdated(zoom_ * device_scale_);
  engine_->PageOffsetUpdated(available_area_.OffsetFromOrigin());
  engine_->PluginSizeUpdated(available_area_.size());
  engine_->SetGrayscale(is_grayscale);

  paint_manager_.InvalidateRect(gfx::Rect(plugin_rect_.size()));
}

void PdfViewWebPlugin::HandleLoadPreviewPageMessage(
    const base::Value::Dict& message) {
  const std::string& url = *message.FindString("url");
  int dest_page_index = message.FindInt("index").value();

  // For security reasons, crash if `url` is not for Print Preview.
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));

  DCHECK_GE(dest_page_index, 0);
  DCHECK_LT(dest_page_index, print_preview_page_count_);

  // Print Preview JS will send the loadPreviewPage message for every page,
  // including the first page in the print preview, which has already been
  // loaded when handing the resetPrintPreviewMode message. Just ignore it.
  if (dest_page_index == 0)
    return;

  int src_page_index = ExtractPrintPreviewPageIndex(url);
  DCHECK_GE(src_page_index, 0);

  preview_pages_info_.push({.url = url, .dest_page_index = dest_page_index});
  LoadAvailablePreviewPage();
}

void PdfViewWebPlugin::LoadAvailablePreviewPage() {
  if (preview_pages_info_.empty() ||
      document_load_state_ != DocumentLoadState::kComplete ||
      preview_document_load_state_ == DocumentLoadState::kLoading) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kLoading;
  const std::string& url = preview_pages_info_.front().url;

  // Note that `last_progress_sent_` is not reset for preview page loads.
  LoadUrl(url, base::BindOnce(&PdfViewWebPlugin::DidOpenPreview,
                              weak_factory_.GetWeakPtr()));
}

void PdfViewWebPlugin::DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                                      int32_t result) {
  DCHECK_EQ(result, kSuccess);

  // `preview_engine_` holds a `raw_ptr` to `preview_client_`.
  // We need to explicitly destroy it before clobbering
  // `preview_client_` to dodge lifetime issues.
  preview_engine_.reset();

  preview_client_ = std::make_unique<PreviewModeClient>(this);
  preview_engine_ = client_->CreateEngine(
      preview_client_.get(), PDFiumFormFiller::ScriptOption::kNoJavaScript);
  preview_engine_->PluginSizeUpdated({});
  preview_engine_->HandleDocumentLoad(std::move(loader), url_);
}

void PdfViewWebPlugin::PreviewDocumentLoadComplete() {
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kComplete;

  int dest_page_index = preview_pages_info_.front().dest_page_index;
  preview_pages_info_.pop();
  engine_->AppendPage(preview_engine_.get(), dest_page_index);

  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

void PdfViewWebPlugin::PreviewDocumentLoadFailed() {
  client_->RecordComputedAction("PDF.PreviewDocumentLoadFailure");
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  // Even if a print preview page failed to load, keep going.
  preview_document_load_state_ = DocumentLoadState::kFailed;
  preview_pages_info_.pop();
  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

void PdfViewWebPlugin::LoadNextPreviewPage() {
  if (!preview_pages_info_.empty()) {
    DCHECK_LT(print_preview_loaded_page_count_, print_preview_page_count_);
    LoadAvailablePreviewPage();
    return;
  }

  if (print_preview_loaded_page_count_ == print_preview_page_count_)
    SendPrintPreviewLoadedNotification();
}

void PdfViewWebPlugin::SendPrintPreviewLoadedNotification() {
  base::Value::Dict message;
  message.Set("type", "printPreviewLoaded");
  client_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SendThumbnailForTesting(base::Value::Dict reply,
                                               int page_index,
                                               Thumbnail thumbnail) {
  SendThumbnail(std::move(reply), page_index, std::move(thumbnail));
}

void PdfViewWebPlugin::SendThumbnail(base::Value::Dict reply,
                                     int page_index,
                                     Thumbnail thumbnail) {
  DCHECK_EQ(*reply.FindString("type"), "getThumbnailReply");
  DCHECK(reply.FindString("messageId"));

  reply.Set("imageData", thumbnail.TakeData());
  reply.Set("width", thumbnail.image_size().width());
  reply.Set("height", thumbnail.image_size().height());
  client_->PostMessage(std::move(reply));

#if BUILDFLAG(ENABLE_PDF_INK2)
  if (ink_module_) {
    GenerateAndSendInkThumbnail(page_index, thumbnail.image_size());
  }
#endif
}

#if BUILDFLAG(ENABLE_PDF_INK2)
void PdfViewWebPlugin::GenerateAndSendInkThumbnail(int page_index,
                                                   const gfx::Size& size) {
  CHECK(!size.IsEmpty());
  CHECK(ink_module_);

  auto info = SkImageInfo::Make(size.width(), size.height(),
                                kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  const size_t alloc_size = info.computeMinByteSize();
  CHECK(!SkImageInfo::ByteSizeOverflowed(alloc_size));
  std::vector<uint8_t> image_data(alloc_size);

  SkBitmap sk_bitmap;
  sk_bitmap.installPixels(info, image_data.data(), info.minRowBytes());
  SkCanvas canvas(sk_bitmap);
  if (!ink_module_->DrawThumbnail(canvas, page_index)) {
    return;
  }

  base::Value::Dict message;
  message.Set("type", "updateInk2Thumbnail");
  message.Set("pageNumber", page_index + 1);
  message.Set("imageData", std::move(image_data));
  message.Set("width", size.width());
  message.Set("height", size.height());
  client_->PostMessage(std::move(message));
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

gfx::Point PdfViewWebPlugin::FrameToPdfCoordinates(
    const gfx::PointF& frame_coordinates) const {
  // TODO(crbug.com/40817151): Use methods on `blink::WebPluginContainer`.
  return gfx::ToFlooredPoint(
             gfx::ScalePoint(frame_coordinates, device_scale_)) -
         gfx::Vector2d(available_area_.x(), 0);
}

AccessibilityDocInfo PdfViewWebPlugin::GetAccessibilityDocInfo() const {
  AccessibilityDocInfo doc_info;
  doc_info.page_count = engine_->GetNumberOfPages();
  doc_info.text_accessible =
      engine_->HasPermission(DocumentPermission::kCopyAccessible);
  doc_info.text_copyable = engine_->HasPermission(DocumentPermission::kCopy);
  return doc_info;
}

void PdfViewWebPlugin::PrepareAndSetAccessibilityPageInfo(int32_t page_index) {
  // Ignore outdated or out of range calls.
  if (page_index != next_accessibility_page_index_ || page_index < 0 ||
      page_index >= engine_->GetNumberOfPages()) {
    return;
  }

  // Wait for the page to be loaded and searchified before getting accessibility
  // page info.
  // Ensure page is loaded so that it can schedule a searchify operation if
  // needed.
  engine_->GetPage(page_index)->GetPage();
  if (engine_->PageNeedsSearchify(page_index)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PdfViewWebPlugin::PrepareAndSetAccessibilityPageInfo,
                       weak_factory_.GetWeakPtr(), page_index),
        kAccessibilityPageDelay * 10);
    return;
  }

  ++next_accessibility_page_index_;

  AccessibilityPageInfo page_info;
  std::vector<AccessibilityTextRunInfo> text_runs;
  std::vector<AccessibilityCharInfo> chars;
  AccessibilityPageObjects page_objects;

  GetAccessibilityInfo(engine_.get(), page_index, page_info, text_runs, chars,
                       page_objects);

  pdf_accessibility_data_handler_->SetAccessibilityPageInfo(
      std::move(page_info), std::move(text_runs), std::move(chars),
      std::move(page_objects));

  // Schedule loading the next page if there's more.
  if (page_index + 1 < engine_->GetNumberOfPages()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PdfViewWebPlugin::PrepareAndSetAccessibilityPageInfo,
                       weak_factory_.GetWeakPtr(), page_index + 1),
        kAccessibilityPageDelay);
  }
}

void PdfViewWebPlugin::PrepareAndSetAccessibilityViewportInfo() {
  AccessibilityViewportInfo viewport_info;
  viewport_info.offset = gfx::ScaleToFlooredPoint(available_area_.origin(),
                                                  1 / (device_scale_ * zoom_));
  viewport_info.zoom = zoom_;
  viewport_info.scale = device_scale_;
  viewport_info.orientation =
      static_cast<int32_t>(engine_->GetCurrentOrientation());
  viewport_info.focus_info = {FocusObjectType::kNone, 0, 0};

  engine_->GetSelection(&viewport_info.selection_start_page_index,
                        &viewport_info.selection_start_char_index,
                        &viewport_info.selection_end_page_index,
                        &viewport_info.selection_end_char_index);

  pdf_accessibility_data_handler_->SetAccessibilityViewportInfo(
      std::move(viewport_info));
}

void PdfViewWebPlugin::LoadAccessibility() {
  accessibility_state_ = AccessibilityState::kLoaded;

  // A new document layout will trigger the creation of a new accessibility
  // tree, so `next_accessibility_page_index_` should be reset to ignore
  // outdated asynchronous calls of PrepareAndSetAccessibilityPageInfo().
  next_accessibility_page_index_ = 0;
  pdf_accessibility_data_handler_->SetAccessibilityDocInfo(
      GetAccessibilityDocInfo());

  // Record whether the PDF is tagged when opened by an accessibility user.
  if (metrics_handler_) {
    metrics_handler_->RecordAccessibilityIsDocTagged(engine_->IsPDFDocTagged());
  }

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine_->HasPermission(DocumentPermission::kCopy) ||
        engine_->HasPermission(DocumentPermission::kCopyAccessible))) {
    return;
  }

  PrepareAndSetAccessibilityViewportInfo();

  // Schedule loading the first page.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::PrepareAndSetAccessibilityPageInfo,
                     weak_factory_.GetWeakPtr(), /*page_index=*/0),
      kAccessibilityPageDelay);
}

}  // namespace chrome_pdf
