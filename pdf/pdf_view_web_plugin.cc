// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/i18n/char_iterator.h"
#include "base/i18n/string_search.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/accessibility_structs.h"
#include "pdf/metrics_handler.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/parsed_params.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdf_init.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/post_message_receiver.h"
#include "pdf/post_message_sender.h"
#include "pdf/ppapi_migration/result_codes.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/ui/document_properties.h"
#include "printing/metafile_skia.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/display/screen_info.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

namespace {

constexpr base::TimeDelta kFindResultCooldown = base::Milliseconds(100);

// Initialization performed per renderer process. Initialization may be
// triggered from multiple plugin instances, but should only execute once.
//
// TODO(crbug.com/1123621): We may be able to simplify this once we've figured
// out exactly which processes need to initialize and shutdown PDFium.
class PerProcessInitializer final {
 public:
  static PerProcessInitializer& GetInstance() {
    static base::NoDestructor<PerProcessInitializer,
                              base::AllowForTriviallyDestructibleType>
        instance;
    return *instance;
  }

  void Acquire() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    DCHECK_GE(init_count_, 0);
    if (init_count_++ > 0)
      return;

    DCHECK(!IsSDKInitializedViaPlugin());
    InitializeSDK(/*enable_v8=*/true, FontMappingMode::kBlink);
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

  // TODO(crbug.com/1123731): Assuming PDFium is thread-hostile for now, and
  // must use one thread exclusively.
  THREAD_CHECKER(thread_checker_);
};

class BlinkContainerWrapper final : public PdfViewWebPlugin::ContainerWrapper {
 public:
  BlinkContainerWrapper(blink::WebPluginContainer* container,
                        V8ValueConverter* v8_value_converter)
      : container_(container),
        post_message_sender_(container_, v8_value_converter) {
    DCHECK(container_);
    DCHECK(v8_value_converter);
  }
  BlinkContainerWrapper(const BlinkContainerWrapper&) = delete;
  BlinkContainerWrapper& operator=(const BlinkContainerWrapper&) = delete;
  ~BlinkContainerWrapper() override = default;

  void Invalidate() override { container_->Invalidate(); }

  void RequestTouchEventType(
      blink::WebPluginContainer::TouchEventRequestType request_type) override {
    container_->RequestTouchEventType(request_type);
  }

  void ReportFindInPageMatchCount(int identifier,
                                  int total,
                                  bool final_update) override {
    container_->ReportFindInPageMatchCount(identifier, total, final_update);
  }

  void ReportFindInPageSelection(int identifier, int index) override {
    container_->ReportFindInPageSelection(identifier, index);
  }

  void ReportFindInPageTickmarks(
      const std::vector<gfx::Rect>& tickmarks) override {
    blink::WebLocalFrame* frame = GetFrame();
    if (frame) {
      frame->SetTickmarks(blink::WebElement(),
                          blink::WebVector<gfx::Rect>(tickmarks));
    }
  }

  float DeviceScaleFactor() override {
    // Do not reply on the device scale returned by
    // `container_->DeviceScaleFactor()`, since it doesn't always reflect the
    // real screen's device scale. Instead, get the real device scale from the
    // top-level `blink::WebLocalFrame`'s screen info.
    blink::WebWidget* widget = GetFrame()->LocalRoot()->FrameWidget();
    return widget->GetOriginalScreenInfo().device_scale_factor;
  }

  gfx::PointF GetScrollPosition() override {
    // Note that `blink::WebLocalFrame::GetScrollOffset()` actually returns a
    // scroll position (a point relative to the top-left corner).
    return GetFrame()->GetScrollOffset();
  }

  void PostMessage(base::Value::Dict message) override {
    post_message_sender_.Post(std::move(message));
  }

  void UsePluginAsFindHandler() override {
    container_->UsePluginAsFindHandler();
  }

  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override {
    GetFrame()->SetReferrerForRequest(request, referrer_url);
  }

  void Alert(const blink::WebString& message) override {
    blink::WebLocalFrame* frame = GetFrame();
    if (frame)
      frame->Alert(message);
  }

  bool Confirm(const blink::WebString& message) override {
    blink::WebLocalFrame* frame = GetFrame();
    return frame && frame->Confirm(message);
  }

  blink::WebString Prompt(const blink::WebString& message,
                          const blink::WebString& default_value) override {
    blink::WebLocalFrame* frame = GetFrame();
    return frame ? frame->Prompt(message, default_value) : blink::WebString();
  }

  void TextSelectionChanged(const blink::WebString& selection_text,
                            uint32_t offset,
                            const gfx::Range& range) override {
    // Focus the plugin's containing frame before changing the text selection.
    // TODO(crbug.com/1234559): Would it make more sense not to change the text
    // selection at all in this case? Maybe we only have this problem because we
    // support a "selectAll" message.
    blink::WebLocalFrame* frame = GetFrame();
    frame->View()->SetFocusedFrame(frame);

    frame->TextSelectionChanged(selection_text, offset, range);
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override {
    return GetFrame()->CreateAssociatedURLLoader(options);
  }

  void UpdateTextInputState() override {
    // `widget` is null in Print Preview.
    auto* widget = GetFrame()->FrameWidget();
    if (widget)
      widget->UpdateTextInputState();
  }

  void UpdateSelectionBounds() override {
    // `widget` is null in Print Preview.
    auto* widget = GetFrame()->FrameWidget();
    if (widget)
      widget->UpdateSelectionBounds();
  }

  std::string GetEmbedderOriginString() override {
    auto* frame = GetFrame();
    if (!frame)
      return {};

    auto* parent_frame = frame->Parent();
    if (!parent_frame)
      return {};

    return GURL(parent_frame->GetSecurityOrigin().ToString().Utf8()).spec();
  }

  blink::WebLocalFrame* GetFrame() override {
    return container_->GetDocument().GetFrame();
  }

  blink::WebLocalFrameClient* GetWebLocalFrameClient() override {
    return GetFrame()->Client();
  }

  blink::WebPluginContainer* Container() override { return container_; }

 private:
  const raw_ptr<blink::WebPluginContainer> container_;
  PostMessageSender post_message_sender_;
};

}  // namespace

std::unique_ptr<PdfAccessibilityDataHandler>
PdfViewWebPlugin::Client::CreateAccessibilityDataHandler(
    PdfAccessibilityActionHandler* action_handler) {
  return nullptr;
}

PdfViewWebPlugin::PdfViewWebPlugin(
    std::unique_ptr<Client> client,
    mojo::AssociatedRemote<pdf::mojom::PdfService> pdf_service_remote,
    const blink::WebPluginParams& params)
    : client_(std::move(client)),
      pdf_service_remote_(std::move(pdf_service_remote)),
      initial_params_(params),
      pdf_accessibility_data_handler_(
          client_->CreateAccessibilityDataHandler(this)) {
  auto* service = GetPdfService();
  if (service)
    service->SetListener(listener_receiver_.BindNewPipeAndPassRemote());
}

PdfViewWebPlugin::~PdfViewWebPlugin() = default;

bool PdfViewWebPlugin::Initialize(blink::WebPluginContainer* container) {
  DCHECK_EQ(container->Plugin(), this);
  return InitializeCommon(
      std::make_unique<BlinkContainerWrapper>(container, client_.get()),
      /*engine_override=*/nullptr);
}

bool PdfViewWebPlugin::InitializeForTesting(
    std::unique_ptr<ContainerWrapper> container_wrapper,
    std::unique_ptr<PDFiumEngine> engine,
    std::unique_ptr<UrlLoader> loader) {
  test_loader_ = std::move(loader);
  return InitializeCommon(std::move(container_wrapper), std::move(engine));
}

bool PdfViewWebPlugin::InitializeCommon(
    std::unique_ptr<ContainerWrapper> container_wrapper,
    std::unique_ptr<PDFiumEngine> engine_override) {
  container_wrapper_ = std::move(container_wrapper);

  // Allow the plugin to handle touch events.
  container_wrapper_->RequestTouchEventType(
      blink::WebPluginContainer::kTouchEventRequestTypeRaw);

  // Allow the plugin to handle find requests.
  container_wrapper_->UsePluginAsFindHandler();

  absl::optional<ParsedParams> params = ParseWebPluginParams(initial_params_);

  // The contents of `initial_params_` are no longer needed.
  initial_params_ = {};

  if (!params.has_value())
    return false;

  // Sets crash keys like `ppapi::proxy::PDFResource::SetCrashData()`. Note that
  // we don't set the active URL from the top-level URL, as unlike within a
  // plugin process, the active URL changes frequently within a renderer process
  // (see crbug.com/1266050 for details).
  //
  // TODO(crbug.com/1266087): If multiple PDF plugin instances share the same
  // renderer process, the crash key will be overwritten by the newest value.
  static base::debug::CrashKeyString* subresource_url =
      base::debug::AllocateCrashKeyString("subresource_url",
                                          base::debug::CrashKeySize::Size256);
  base::debug::SetCrashKeyString(subresource_url, params->original_url);

  PerProcessInitializer::GetInstance().Acquire();
  InitializeBase(
      engine_override
          ? std::move(engine_override)
          : std::make_unique<PDFiumEngine>(this, params->script_option),
      /*embedder_origin=*/container_wrapper_->GetEmbedderOriginString(),
      /*src_url=*/params->src_url,
      /*original_url=*/params->original_url,
      /*full_frame=*/params->full_frame,
      /*background_color=*/params->background_color,
      /*has_edits=*/params->has_edits);

  SendSetSmoothScrolling();

  if (!IsPrintPreview())
    metrics_handler_ = std::make_unique<MetricsHandler>();

  return true;
}

void PdfViewWebPlugin::SendSetSmoothScrolling() {
  base::Value::Dict message;
  message.Set("type", "setSmoothScrolling");
  message.Set("smoothScrolling",
              blink::Platform::Current()->IsScrollAnimatorEnabled());
  SendMessage(std::move(message));
}

void PdfViewWebPlugin::Destroy() {
  if (container_wrapper_) {
    // Explicitly destroy the PDFEngine during destruction as it may call back
    // into this object.
    DestroyPreviewEngine();
    DestroyEngine();
    PerProcessInitializer::GetInstance().Release();
    container_wrapper_.reset();
  }

  delete this;
}

blink::WebPluginContainer* PdfViewWebPlugin::Container() const {
  return container_wrapper_ ? container_wrapper_->Container() : nullptr;
}

v8::Local<v8::Object> PdfViewWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  if (scriptable_receiver_.IsEmpty()) {
    // TODO(crbug.com/1123731): Messages should not be handled on the renderer
    // main thread.
    scriptable_receiver_.Reset(
        isolate, PostMessageReceiver::Create(
                     isolate, client_->GetWeakPtr(), weak_factory_.GetWeakPtr(),
                     base::SequencedTaskRunnerHandle::Get()));
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

  if (device_to_css_scale_ != 1.0f)
    canvas->scale(device_to_css_scale_, device_to_css_scale_);

  // Position layer at plugin origin before layer scaling.
  if (!plugin_rect().origin().IsOrigin())
    canvas->translate(plugin_rect().x(), plugin_rect().y());

  if (snapshot_scale_ != 1.0f)
    canvas->scale(snapshot_scale_, snapshot_scale_);

  canvas->drawImage(snapshot_, 0, 0);
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

  OnViewportChanged(window_rect, container_wrapper_->DeviceScaleFactor());

  gfx::PointF scroll_position = container_wrapper_->GetScrollPosition();
  // Convert back to CSS pixels.
  scroll_position.Scale(1.0f / device_scale());
  UpdateScroll(scroll_position);
}

void PdfViewWebPlugin::UpdateFocus(bool focused,
                                   blink::mojom::FocusType focus_type) {
  if (has_focus_ != focused) {
    engine()->UpdateFocus(focused);
    container_wrapper_->UpdateTextInputState();
    container_wrapper_->UpdateSelectionBounds();
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
  PdfViewPluginBase::HandleInputEvent(simulated_event);
}

void PdfViewWebPlugin::UpdateVisibility(bool visibility) {}

blink::WebInputEventResult PdfViewWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  // TODO(crbug.com/1302059): The input events received by the Pepper plugin
  // already have the viewport-to-DIP scale applied. The scaling done here
  // should be moved into `PdfViewPluginBase::HandleInputEvent()` once the
  // Pepper plugin is removed.
  std::unique_ptr<blink::WebInputEvent> scaled_event =
      ui::ScaleWebInputEvent(event.Event(), viewport_to_dip_scale_);

  const blink::WebInputEvent& event_to_handle =
      scaled_event ? *scaled_event : event.Event();

  const blink::WebInputEventResult result =
      PdfViewPluginBase::HandleInputEvent(event_to_handle)
          ? blink::WebInputEventResult::kHandledApplication
          : blink::WebInputEventResult::kNotHandled;

  *cursor = cursor_type();

  return result;
}

void PdfViewWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {}

void PdfViewWebPlugin::DidReceiveData(const char* data, size_t data_length) {}

void PdfViewWebPlugin::DidFinishLoading() {}

void PdfViewWebPlugin::DidFailLoading(const blink::WebURLError& error) {}

bool PdfViewWebPlugin::SupportsPaginatedPrint() {
  return true;
}

bool PdfViewWebPlugin::GetPrintPresetOptionsFromDocument(
    blink::WebPrintPresetOptions* print_preset_options) {
  *print_preset_options = GetPrintPresetOptions();
  return true;
}

int PdfViewWebPlugin::PrintBegin(const blink::WebPrintParams& print_params) {
  return PdfViewPluginBase::PrintBegin(print_params);
}

void PdfViewWebPlugin::PrintPage(int page_number, cc::PaintCanvas* canvas) {
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

  pages_to_print_.push_back(page_number);
}

void PdfViewWebPlugin::PrintEnd() {
  if (pages_to_print_.empty())
    return;

  printing_metafile_->InitFromData(PrintPages(pages_to_print_));

  PdfViewPluginBase::PrintEnd();
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
  return engine()->CanEditText();
}

bool PdfViewWebPlugin::HasEditableText() const {
  return engine()->HasEditableText();
}

bool PdfViewWebPlugin::CanUndo() const {
  return engine()->CanUndo();
}

bool PdfViewWebPlugin::CanRedo() const {
  return engine()->CanRedo();
}

bool PdfViewWebPlugin::ExecuteEditCommand(const blink::WebString& name,
                                          const blink::WebString& value) {
  if (name == "SelectAll")
    return SelectAll();

  if (name == "Cut")
    return Cut();

  if (name == "Paste" || name == "PasteAndMatchStyle")
    return Paste(value);

  if (name == "Undo")
    return Undo();

  if (name == "Redo")
    return Redo();

  return false;
}

blink::WebURL PdfViewWebPlugin::LinkAtPosition(
    const gfx::Point& /*position*/) const {
  return GURL(link_under_cursor());
}

bool PdfViewWebPlugin::StartFind(const blink::WebString& search_text,
                                 bool case_sensitive,
                                 int identifier) {
  find_identifier_ = identifier;
  engine()->StartFind(search_text.Utf8(), case_sensitive);
  return true;
}

void PdfViewWebPlugin::SelectFindResult(bool forward, int identifier) {
  find_identifier_ = identifier;
  engine()->SelectFindResult(forward);
}

void PdfViewWebPlugin::StopFind() {
  find_identifier_ = -1;
  engine()->StopFind();
  tickmarks_.clear();
  container_wrapper_->ReportFindInPageTickmarks(tickmarks_);
}

bool PdfViewWebPlugin::CanRotateView() {
  return !IsPrintPreview();
}

void PdfViewWebPlugin::RotateView(blink::WebPlugin::RotationType type) {
  DCHECK(CanRotateView());

  switch (type) {
    case blink::WebPlugin::RotationType::k90Clockwise:
      engine()->RotateClockwise();
      break;
    case blink::WebPlugin::RotationType::k90Counterclockwise:
      engine()->RotateCounterclockwise();
      break;
  }
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

void PdfViewWebPlugin::UpdateCursor(ui::mojom::CursorType new_cursor_type) {
  set_cursor_type(new_cursor_type);
}

void PdfViewWebPlugin::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  float inverse_scale = 1.0f / device_scale();
  tickmarks_.clear();
  tickmarks_.reserve(tickmarks.size());
  std::transform(tickmarks.begin(), tickmarks.end(),
                 std::back_inserter(tickmarks_),
                 [inverse_scale](const gfx::Rect& t) -> gfx::Rect {
                   return gfx::ScaleToEnclosingRect(t, inverse_scale);
                 });
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
    container_wrapper_->ReportFindInPageMatchCount(find_identifier_, total,
                                                   final_result);
  }

  container_wrapper_->ReportFindInPageTickmarks(tickmarks_);

  if (final_result)
    return;

  recently_sent_find_update_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::ResetRecentlySentFindUpdate,
                     weak_factory_.GetWeakPtr()),
      kFindResultCooldown);
}

void PdfViewWebPlugin::NotifySelectedFindResultChanged(int current_find_index) {
  if (find_identifier_ == -1 || !container_wrapper_)
    return;

  DCHECK_GE(current_find_index, -1);
  container_wrapper_->ReportFindInPageSelection(find_identifier_,
                                                current_find_index + 1);
}

void PdfViewWebPlugin::CaretChanged(const gfx::Rect& caret_rect) {
  caret_rect_ = gfx::ScaleToEnclosingRect(
      caret_rect + available_area().OffsetFromOrigin(), device_to_css_scale_);
}

void PdfViewWebPlugin::Alert(const std::string& message) {
  container_wrapper_->Alert(blink::WebString::FromUTF8(message));
}

bool PdfViewWebPlugin::Confirm(const std::string& message) {
  return container_wrapper_->Confirm(blink::WebString::FromUTF8(message));
}

std::string PdfViewWebPlugin::Prompt(const std::string& question,
                                     const std::string& default_answer) {
  return container_wrapper_
      ->Prompt(blink::WebString::FromUTF8(question),
               blink::WebString::FromUTF8(default_answer))
      .Utf8();
}

std::vector<PDFEngine::Client::SearchStringResult>
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

void PdfViewWebPlugin::SetSelectedText(const std::string& selected_text) {
  selected_text_ = blink::WebString::FromUTF8(selected_text);
  container_wrapper_->TextSelectionChanged(
      selected_text_, /*offset=*/0, gfx::Range(0, selected_text_.length()));
}

bool PdfViewWebPlugin::IsValidLink(const std::string& url) {
  return base::Value(url).is_string();
}

void PdfViewWebPlugin::SetCaretPosition(const gfx::PointF& position) {
  PdfViewPluginBase::SetCaretPosition(position);
}

void PdfViewWebPlugin::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  PdfViewPluginBase::MoveRangeSelectionExtent(extent);
}

void PdfViewWebPlugin::SetSelectionBounds(const gfx::PointF& base,
                                          const gfx::PointF& extent) {
  PdfViewPluginBase::SetSelectionBounds(base, extent);
}

bool PdfViewWebPlugin::IsValid() const {
  return container_wrapper_ && container_wrapper_->GetFrame();
}

blink::WebURL PdfViewWebPlugin::CompleteURL(
    const blink::WebString& partial_url) const {
  DCHECK(IsValid());
  return Container()->GetDocument().CompleteURL(partial_url);
}

net::SiteForCookies PdfViewWebPlugin::SiteForCookies() const {
  DCHECK(IsValid());
  return Container()->GetDocument().SiteForCookies();
}

void PdfViewWebPlugin::SetReferrerForRequest(
    blink::WebURLRequest& request,
    const blink::WebURL& referrer_url) {
  container_wrapper_->SetReferrerForRequest(request, referrer_url);
}

std::unique_ptr<blink::WebAssociatedURLLoader>
PdfViewWebPlugin::CreateAssociatedURLLoader(
    const blink::WebAssociatedURLLoaderOptions& options) {
  return container_wrapper_->CreateAssociatedURLLoader(options);
}

void PdfViewWebPlugin::OnMessage(const base::Value::Dict& message) {
  PdfViewPluginBase::HandleMessage(message);
}

void PdfViewWebPlugin::InvalidatePluginContainer() {
  container_wrapper_->Invalidate();
}

void PdfViewWebPlugin::UpdateSnapshot(sk_sp<SkImage> snapshot) {
  snapshot_ =
      cc::PaintImageBuilder::WithDefault()
          .set_image(std::move(snapshot), cc::PaintImage::GetNextContentId())
          .set_id(cc::PaintImage::GetNextId())
          .TakePaintImage();
  if (!plugin_rect().IsEmpty())
    InvalidatePluginContainer();
}

void PdfViewWebPlugin::UpdateScaledValues() {
  total_translate_ = snapshot_translate_;

  if (viewport_to_dip_scale_ != 1.0f)
    total_translate_.Scale(1.0f / viewport_to_dip_scale_);
}

void PdfViewWebPlugin::UpdateScale(float scale) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  viewport_to_dip_scale_ = scale;
  device_to_css_scale_ = 1.0f;
  UpdateScaledValues();
}

void PdfViewWebPlugin::UpdateLayerTransform(float scale,
                                            const gfx::Vector2dF& translate) {
  snapshot_translate_ = translate;
  snapshot_scale_ = scale;
  UpdateScaledValues();
}

void PdfViewWebPlugin::EnableAccessibility() {
  PdfViewPluginBase::EnableAccessibility();
}

void PdfViewWebPlugin::HandleAccessibilityAction(
    const AccessibilityActionData& action_data) {
  PdfViewPluginBase::HandleAccessibilityAction(action_data);
}

base::WeakPtr<PdfViewPluginBase> PdfViewWebPlugin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<UrlLoader> PdfViewWebPlugin::CreateUrlLoaderInternal() {
  if (test_loader_)
    return std::move(test_loader_);

  auto loader = std::make_unique<BlinkUrlLoader>(weak_factory_.GetWeakPtr());
  loader->GrantUniversalAccess();
  return loader;
}

void PdfViewWebPlugin::OnDocumentLoadComplete() {
  RecordDocumentMetrics();

  SendAttachments();
  SendBookmarks();
  SendMetadata();
}

void PdfViewWebPlugin::SendMessage(base::Value::Dict message) {
  container_wrapper_->PostMessage(std::move(message));
}

void PdfViewWebPlugin::SaveAs() {
  auto* service = GetPdfService();
  if (!service)
    return;

  service->SaveUrlAs(GURL(GetURL().c_str()),
                     network::mojom::ReferrerPolicy::kDefault);
}

void PdfViewWebPlugin::SetFormTextFieldInFocus(bool in_focus) {
  text_input_type_ = in_focus ? blink::WebTextInputType::kWebTextInputTypeText
                              : blink::WebTextInputType::kWebTextInputTypeNone;
  container_wrapper_->UpdateTextInputState();
}

void PdfViewWebPlugin::SetAccessibilityDocInfo(AccessibilityDocInfo doc_info) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::OnSetAccessibilityDocInfo,
                     weak_factory_.GetWeakPtr(), std::move(doc_info)));
}

void PdfViewWebPlugin::SetAccessibilityPageInfo(
    AccessibilityPageInfo page_info,
    std::vector<AccessibilityTextRunInfo> text_runs,
    std::vector<AccessibilityCharInfo> chars,
    AccessibilityPageObjects page_objects) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PdfViewWebPlugin::OnSetAccessibilityPageInfo,
                                weak_factory_.GetWeakPtr(),
                                std::move(page_info), std::move(text_runs),
                                std::move(chars), std::move(page_objects)));
}

void PdfViewWebPlugin::SetAccessibilityViewportInfo(
    AccessibilityViewportInfo viewport_info) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::OnSetAccessibilityViewportInfo,
                     weak_factory_.GetWeakPtr(), std::move(viewport_info)));
}

void PdfViewWebPlugin::SetContentRestrictions(int content_restrictions) {
  auto* service = GetPdfService();
  if (!service)
    return;

  service->UpdateContentRestrictions(content_restrictions);
}

void PdfViewWebPlugin::SetPluginCanSave(bool can_save) {
  auto* service = GetPdfService();
  if (!service)
    return;

  service->SetPluginCanSave(can_save);
}

void PdfViewWebPlugin::PluginDidStartLoading() {
  auto* client = container_wrapper_->GetWebLocalFrameClient();
  if (!client)
    return;

  client->DidStartLoading();
}

void PdfViewWebPlugin::PluginDidStopLoading() {
  auto* client = container_wrapper_->GetWebLocalFrameClient();
  if (!client)
    return;

  client->DidStopLoading();
}

void PdfViewWebPlugin::InvokePrintDialog() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PdfViewWebPlugin::OnInvokePrintDialog,
                                weak_factory_.GetWeakPtr()));
}

void PdfViewWebPlugin::NotifySelectionChanged(const gfx::PointF& left,
                                              int left_height,
                                              const gfx::PointF& right,
                                              int right_height) {
  auto* service = GetPdfService();
  if (!service)
    return;

  service->SelectionChanged(left, left_height, right, right_height);
}

void PdfViewWebPlugin::NotifyUnsupportedFeature() {
  DCHECK(full_frame());
  GetPdfService()->HasUnsupportedFeature();
}

void PdfViewWebPlugin::UserMetricsRecordAction(const std::string& action) {
  client_->RecordComputedAction(action);
}

gfx::Vector2d PdfViewWebPlugin::plugin_offset_in_frame() const {
  return gfx::Vector2d();
}

void PdfViewWebPlugin::OnViewportChanged(
    const gfx::Rect& plugin_rect_in_css_pixel,
    float new_device_scale) {
  css_plugin_rect_ = plugin_rect_in_css_pixel;

  // `plugin_rect_in_css_pixel` needs to be converted to device pixels before
  // getting passed into PdfViewPluginBase::UpdateGeometryOnPluginRectChanged().
  UpdateGeometryOnPluginRectChanged(plugin_rect_in_css_pixel, new_device_scale);
}

bool PdfViewWebPlugin::SelectAll() {
  if (!CanEditText())
    return false;

  engine()->SelectAll();
  return true;
}

bool PdfViewWebPlugin::Cut() {
  if (!HasSelection() || !CanEditText())
    return false;

  engine()->ReplaceSelection("");
  return true;
}

bool PdfViewWebPlugin::Paste(const blink::WebString& value) {
  if (!CanEditText())
    return false;

  engine()->ReplaceSelection(value.Utf8());
  return true;
}

bool PdfViewWebPlugin::Undo() {
  if (!CanUndo())
    return false;

  engine()->Undo();
  return true;
}

bool PdfViewWebPlugin::Redo() {
  if (!CanRedo())
    return false;

  engine()->Redo();
  return true;
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
  client_->Print(Container()->GetElement());
}

void PdfViewWebPlugin::OnSetAccessibilityDocInfo(
    AccessibilityDocInfo doc_info) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityDocInfo(doc_info);
  // `this` may be deleted. Don't do anything else.
}

void PdfViewWebPlugin::OnSetAccessibilityPageInfo(
    AccessibilityPageInfo page_info,
    std::vector<AccessibilityTextRunInfo> text_runs,
    std::vector<AccessibilityCharInfo> chars,
    AccessibilityPageObjects page_objects) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityPageInfo(
      page_info, text_runs, chars, page_objects);
  // `this` may be deleted. Don't do anything else.
}

void PdfViewWebPlugin::OnSetAccessibilityViewportInfo(
    AccessibilityViewportInfo viewport_info) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityViewportInfo(viewport_info);
  // `this` may be deleted. Don't do anything else.
}

pdf::mojom::PdfService* PdfViewWebPlugin::GetPdfService() {
  return pdf_service_remote_.is_bound() ? pdf_service_remote_.get() : nullptr;
}

void PdfViewWebPlugin::ResetRecentlySentFindUpdate() {
  recently_sent_find_update_ = false;
}

void PdfViewWebPlugin::RecordDocumentMetrics() {
  if (!metrics_handler_)
    return;

  metrics_handler_->RecordDocumentMetrics(engine()->GetDocumentMetadata());
  metrics_handler_->RecordAttachmentTypes(
      engine()->GetDocumentAttachmentInfoList());
}

void PdfViewWebPlugin::SendAttachments() {
  const std::vector<DocumentAttachmentInfo>& attachment_infos =
      engine()->GetDocumentAttachmentInfoList();
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
  SendMessage(std::move(message));
}

void PdfViewWebPlugin::SendBookmarks() {
  base::Value::List bookmarks = engine()->GetBookmarks();
  if (bookmarks.empty())
    return;

  base::Value::Dict message;
  message.Set("type", "bookmarks");
  message.Set("bookmarksData", std::move(bookmarks));
  SendMessage(std::move(message));
}

void PdfViewWebPlugin::SendMetadata() {
  base::Value::Dict metadata;
  const DocumentMetadata& document_metadata = engine()->GetDocumentMetadata();

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

  metadata.Set("pageSize",
               FormatPageSize(engine()->GetUniformPageSizePoints()));

  metadata.Set("canSerializeDocument",
               IsSaveDataSizeValid(engine()->GetLoadedByteSize()));

  base::Value::Dict message;
  message.Set("type", "metadata");
  message.Set("metadataData", std::move(metadata));
  SendMessage(std::move(message));
}

}  // namespace chrome_pdf
