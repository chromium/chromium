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
#include "base/i18n/string_search.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/accessibility_structs.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/parsed_params.h"
#include "pdf/pdf_accessibility_data_handler.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdf_init.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/post_message_receiver.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/result_codes.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "printing/metafile_skia.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/screen_info.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

namespace {

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
  explicit BlinkContainerWrapper(blink::WebPluginContainer* container)
      : container_(container) {
    DCHECK(container_);
  }
  BlinkContainerWrapper(const BlinkContainerWrapper&) = delete;
  BlinkContainerWrapper& operator=(const BlinkContainerWrapper&) = delete;
  ~BlinkContainerWrapper() override = default;

  void Invalidate() override { container_->Invalidate(); }

  void ReportFindInPageMatchCount(int identifier,
                                  int total,
                                  bool final_update) override {
    container_->ReportFindInPageMatchCount(identifier, total, final_update);
  }

  void ReportFindInPageSelection(int identifier, int index) override {
    container_->ReportFindInPageSelection(identifier, index);
  }

  float DeviceScaleFactor() override {
    // Do not reply on the device scale returned by
    // `container_->DeviceScaleFactor()`, since it doesn't always reflect the
    // real screen's device scale. Instead, get the real device scale from the
    // top-level `blink::WebLocalFrame`'s screen info.
    blink::WebWidget* widget = GetFrame()->LocalRoot()->FrameWidget();
    return widget->GetOriginalScreenInfo().device_scale_factor;
  }

  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override {
    GetFrame()->SetReferrerForRequest(request, referrer_url);
  }

  void Alert(const blink::WebString& message) override {
    GetFrame()->Alert(message);
  }

  bool Confirm(const blink::WebString& message) override {
    return GetFrame()->Confirm(message);
  }

  blink::WebString Prompt(const blink::WebString& message,
                          const blink::WebString& default_value) override {
    return GetFrame()->Prompt(message, default_value);
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

  blink::WebLocalFrame* GetFrame() override {
    return container_->GetDocument().GetFrame();
  }

  blink::WebLocalFrameClient* GetWebLocalFrameClient() override {
    return GetFrame()->Client();
  }

  blink::WebPluginContainer* Container() override { return container_; }

 private:
  blink::WebPluginContainer* const container_;
};

}  // namespace

std::unique_ptr<PdfAccessibilityDataHandler>
PdfViewWebPlugin::Client::CreateAccessibilityDataHandler(
    PdfAccessibilityActionHandler* action_handler) {
  return nullptr;
}

bool PdfViewWebPlugin::Client::IsUseZoomForDSFEnabled() const {
  return false;
}

PdfViewWebPlugin::PdfViewWebPlugin(
    std::unique_ptr<Client> client,
    mojo::AssociatedRemote<pdf::mojom::PdfService> pdf_service_remote,
    const blink::WebPluginParams& params)
    : client_(std::move(client)),
      pdf_service_remote_(std::move(pdf_service_remote)),
      initial_params_(params),
      pdf_accessibility_data_handler_(
          client_->CreateAccessibilityDataHandler(this)) {}

PdfViewWebPlugin::~PdfViewWebPlugin() = default;

bool PdfViewWebPlugin::Initialize(blink::WebPluginContainer* container) {
  DCHECK_EQ(container->Plugin(), this);
  return InitializeCommon(std::make_unique<BlinkContainerWrapper>(container));
}

bool PdfViewWebPlugin::InitializeForTesting(
    std::unique_ptr<ContainerWrapper> container_wrapper) {
  return InitializeCommon(std::move(container_wrapper));
}

// Modeled on `OutOfProcessInstance::Init()`.
bool PdfViewWebPlugin::InitializeCommon(
    std::unique_ptr<ContainerWrapper> container_wrapper) {
  container_wrapper_ = std::move(container_wrapper);

  // Check if the PDF is being loaded in the PDF chrome extension. We only allow
  // the plugin to be loaded in the extension and print preview to avoid
  // exposing sensitive APIs directly to external websites.
  std::string document_url;
  auto* container = Container();
  if (container) {
    GURL maybe_url(container->GetDocument().Url());
    if (maybe_url.is_valid())
      document_url = maybe_url.possibly_invalid_spec();
  }

  base::StringPiece document_url_piece(document_url);
  set_is_print_preview(IsPrintPreviewUrl(document_url_piece));
  // TODO(crbug.com/1123621): Consider calling ValidateDocumentUrl() or
  // something like it once the process model has been finalized.

  // Allow the plugin to handle find requests.
  if (container)
    container->UsePluginAsFindHandler();

  absl::optional<ParsedParams> params = ParseWebPluginParams(initial_params_);

  // The contents of `initial_params_` are no longer needed.
  initial_params_ = {};

  if (!params.has_value())
    return false;

  set_full_frame(params->full_frame);
  if (params->background_color.has_value())
    SetBackgroundColor(params->background_color.value());

  PerProcessInitializer::GetInstance().Acquire();

  InitializeEngine(std::make_unique<PDFiumEngine>(this, params->script_option));
  LoadUrl(params->src_url, /*is_print_preview=*/false);
  set_url(params->original_url);
  post_message_sender_.set_container(Container());
  return true;
}

void PdfViewWebPlugin::Destroy() {
  if (container_wrapper_) {
    // Explicitly destroy the PDFEngine during destruction as it may call back
    // into this object.
    DestroyPreviewEngine();
    DestroyEngine();
    PerProcessInitializer::GetInstance().Release();
    container_wrapper_.reset();
    post_message_sender_.set_container(nullptr);
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
        isolate,
        PostMessageReceiver::Create(isolate, weak_factory_.GetWeakPtr(),
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
  // The scale level used to convert DIPs to CSS pixels.
  float inverse_scale = 1.0f / (device_scale() * viewport_to_dip_scale_);

  // `rect` is in CSS pixels, and the plugin rect is in DIPs. The plugin rect
  // needs to be converted into CSS pixels before calculating the rect area to
  // be invalidated.
  gfx::Rect plugin_rect_in_css_pixels =
      gfx::ScaleToEnclosingRectSafe(plugin_rect(), inverse_scale);

  // Clip the intersection of the paint rect and the plugin rect, so that
  // painting outside the plugin or the paint rect area can be avoided.
  SkRect invalidate_rect =
      gfx::RectToSkRect(gfx::IntersectRects(plugin_rect_in_css_pixels, rect));
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

  if (inverse_scale != 1.0f)
    canvas->scale(inverse_scale, inverse_scale);

  canvas->drawImage(snapshot_, plugin_rect().x(), plugin_rect().y());
}

void PdfViewWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                      const gfx::Rect& clip_rect,
                                      const gfx::Rect& unobscured_rect,
                                      bool is_visible) {
  float device_scale = container_wrapper_->DeviceScaleFactor();
  viewport_to_dip_scale_ =
      client_->IsUseZoomForDSFEnabled() ? 1.0f / device_scale : 1.0f;

  // Note that `window_rect` is in viewport coordinates. It needs to be
  // converted to DIPs before getting passed into
  // PdfViewPluginBase::UpdateGeometryOnViewChanged().
  OnViewportChanged(
      gfx::ScaleToEnclosingRectSafe(window_rect, viewport_to_dip_scale_),
      device_scale);
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
  // TODO(crbug.com/702993): The input events received by the Pepper plugin
  // already have the device scale applied. The scaling done here should be
  // moved into `PdfViewPluginBase::HandleInputEvent()` once the Pepper plugin
  // is removed.
  std::unique_ptr<blink::WebInputEvent> scaled_event =
      ui::ScaleWebInputEvent(event.Event(), device_scale());

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
  // TODO(crbug.com/1199999): Clear tickmarks on scroller when find is
  // dismissed.
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

blink::WebTextInputType PdfViewWebPlugin::GetPluginTextInputType() {
  return text_input_type_;
}

void PdfViewWebPlugin::UpdateCursor(ui::mojom::CursorType new_cursor_type) {
  set_cursor_type(new_cursor_type);
}

void PdfViewWebPlugin::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {}

void PdfViewWebPlugin::NotifyNumberOfFindResultsChanged(int total,
                                                        bool final_result) {
  // After stopping search and setting `find_identifier_` to -1 there still may
  // be a NotifyNumberOfFindResultsChanged notification pending from engine.
  // Just ignore them.
  if (find_identifier_ == -1 || !container_wrapper_)
    return;

  container_wrapper_->ReportFindInPageMatchCount(find_identifier_, total,
                                                 final_result);
  // TODO(crbug.com/1199999): Set tickmarks on scroller.
}

void PdfViewWebPlugin::NotifySelectedFindResultChanged(int current_find_index) {
  if (find_identifier_ == -1 || !container_wrapper_)
    return;

  DCHECK_GE(current_find_index, -1);
  container_wrapper_->ReportFindInPageSelection(find_identifier_,
                                                current_find_index + 1);
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

void PdfViewWebPlugin::SubmitForm(const std::string& url,
                                  const void* data,
                                  int length) {}

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

std::unique_ptr<Graphics> PdfViewWebPlugin::CreatePaintGraphics(
    const gfx::Size& size) {
  // `this` must be valid when creating new graphics. `this` is guaranteed to
  // outlive `graphics`; the implemented client interface owns the paint manager
  // in which the graphics device exists.
  auto graphics = SkiaGraphics::Create(this, size);
  DCHECK(graphics);
  return graphics;
}

bool PdfViewWebPlugin::BindPaintGraphics(Graphics& graphics) {
  InvalidatePluginContainer();
  return false;
}

void PdfViewWebPlugin::ScheduleTaskOnMainThread(const base::Location& from_here,
                                                ResultCallback callback,
                                                int32_t result,
                                                base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      from_here, base::BindOnce(std::move(callback), result), delay);
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

void PdfViewWebPlugin::OnMessage(const base::Value& message) {
  PdfViewPluginBase::HandleMessage(message);
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

void PdfViewWebPlugin::HandleAccessibilityAction(
    const AccessibilityActionData& action_data) {
  PdfViewPluginBase::HandleAccessibilityAction(action_data);
}

base::WeakPtr<PdfViewPluginBase> PdfViewWebPlugin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<UrlLoader> PdfViewWebPlugin::CreateUrlLoaderInternal() {
  auto loader = std::make_unique<BlinkUrlLoader>(weak_factory_.GetWeakPtr());
  loader->GrantUniversalAccess();
  return loader;
}

// Modeled on `OutOfProcessInstance::DidOpen()`.
void PdfViewWebPlugin::DidOpen(std::unique_ptr<UrlLoader> loader,
                               int32_t result) {
  if (result == Result::kSuccess) {
    if (!engine()->HandleDocumentLoad(std::move(loader), GetURL()))
      DocumentLoadFailed();
  } else {
    NOTIMPLEMENTED();
  }
}

void PdfViewWebPlugin::SendMessage(base::Value message) {
  post_message_sender_.Post(std::move(message));
}

void PdfViewWebPlugin::SaveAs() {
  auto* service = GetPdfService();
  if (!service)
    return;

  service->SaveUrlAs(GURL(GetURL().c_str()),
                     network::mojom::ReferrerPolicy::kDefault);
}

void PdfViewWebPlugin::InitImageData(const gfx::Size& size) {
  mutable_image_data() = CreateN32PremulSkBitmap(gfx::SizeToSkISize(size));
}

void PdfViewWebPlugin::SetFormFieldInFocus(bool in_focus) {
  text_input_type_ = in_focus ? blink::WebTextInputType::kWebTextInputTypeText
                              : blink::WebTextInputType::kWebTextInputTypeNone;
  container_wrapper_->UpdateTextInputState();
}

void PdfViewWebPlugin::SetAccessibilityDocInfo(
    const AccessibilityDocInfo& doc_info) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityDocInfo(doc_info);
}

void PdfViewWebPlugin::SetAccessibilityPageInfo(
    AccessibilityPageInfo page_info,
    std::vector<AccessibilityTextRunInfo> text_runs,
    std::vector<AccessibilityCharInfo> chars,
    AccessibilityPageObjects page_objects) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityPageInfo(
      page_info, text_runs, chars, page_objects);
}

void PdfViewWebPlugin::SetAccessibilityViewportInfo(
    const AccessibilityViewportInfo& viewport_info) {
  if (!pdf_accessibility_data_handler_)
    return;
  pdf_accessibility_data_handler_->SetAccessibilityViewportInfo(viewport_info);
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
  ScheduleTaskOnMainThread(
      FROM_HERE,
      base::BindOnce(&PdfViewWebPlugin::OnInvokePrintDialog,
                     weak_factory_.GetWeakPtr()),
      /*result=*/0, base::TimeDelta());
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

void PdfViewWebPlugin::OnViewportChanged(const gfx::Rect& view_rect,
                                         float new_device_scale) {
  UpdateGeometryOnViewChanged(view_rect, new_device_scale);

  if (IsPrintPreview() && !stop_scrolling()) {
    DCHECK_EQ(new_device_scale, device_scale());
    gfx::ScrollOffset scroll_offset =
        container_wrapper_->GetFrame()->GetScrollOffset();
    scroll_offset.Scale(device_scale());
    set_scroll_position(gfx::Point(scroll_offset.x(), scroll_offset.y()));
    UpdateScroll();
  }

  // Scrolling in the main PDF Viewer UI is already handled by
  // `HandleUpdateScrollMessage()`.
}

void PdfViewWebPlugin::InvalidatePluginContainer() {
  container_wrapper_->Invalidate();
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

void PdfViewWebPlugin::OnInvokePrintDialog(int32_t /*result*/) {
  client_->Print(Container()->GetElement());
}

pdf::mojom::PdfService* PdfViewWebPlugin::GetPdfService() {
  return pdf_service_remote_.is_bound() ? pdf_service_remote_.get() : nullptr;
}

}  // namespace chrome_pdf
