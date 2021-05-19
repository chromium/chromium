// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
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
#include "pdf/pdf_engine.h"
#include "pdf/pdf_init.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/post_message_receiver.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "ppapi/c/pp_errors.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
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
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/skia_util.h"
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
    static base::NoDestructor<PerProcessInitializer> instance;
    return *instance;
  }

  void Acquire() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    DCHECK_GE(init_count_, 0);
    if (init_count_++ > 0)
      return;

    DCHECK(!IsSDKInitializedViaPlugin());
    // TODO(crbug.com/1111024): Support JavaScript.
    InitializeSDK(/*enable_v8=*/false);
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

  float DeviceScaleFactor() const override {
    return container_->DeviceScaleFactor();
  }

  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override {
    GetFrame()->SetReferrerForRequest(request, referrer_url);
  }

  void TextSelectionChanged(const blink::WebString& selection_text,
                            uint32_t offset,
                            const gfx::Range& range) override {
    GetFrame()->TextSelectionChanged(selection_text, offset, range);
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override {
    return GetFrame()->CreateAssociatedURLLoader(options);
  }

  void UpdateTextInputState() override {
    return GetFrame()->FrameWidget()->UpdateTextInputState();
  }

  blink::WebLocalFrame* GetFrame() override {
    return container_->GetDocument().GetFrame();
  }

  blink::WebPluginContainer* Container() override { return container_; }

 private:
  blink::WebPluginContainer* const container_;
};

}  // namespace

PdfViewWebPlugin::PdfViewWebPlugin(const blink::WebPluginParams& params)
    : initial_params_(params) {}

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

  std::string stream_url;
  for (size_t i = 0; i < initial_params_.attribute_names.size(); ++i) {
    if (initial_params_.attribute_names[i] == "stream-url") {
      stream_url = initial_params_.attribute_values[i].Utf8();
    } else if (initial_params_.attribute_names[i] == "full-frame") {
      set_full_frame(true);
    } else if (initial_params_.attribute_names[i] == "background-color") {
      SkColor background_color;
      if (!base::StringToUint(initial_params_.attribute_values[i].Utf8(),
                              &background_color)) {
        return false;
      }
      SetBackgroundColor(background_color);
    }
  }

  // Contents of `initial_params_` no longer needed.
  initial_params_ = {};

  PerProcessInitializer::GetInstance().Acquire();
  InitializeEngine(PDFiumFormFiller::ScriptOption::kNoJavaScript);
  LoadUrl(stream_url, /*is_print_preview=*/false);
  post_message_sender_.set_container(Container());
  return true;
}

void PdfViewWebPlugin::Destroy() {
  if (container_wrapper_) {
    // Explicitly destroy the PDFEngine during destruction as it may call back
    // into this object.
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

void PdfViewWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void PdfViewWebPlugin::Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) {
  const float inverse_scale = 1.0f / device_scale();

  // Clip the intersection of the paint rect and the plugin rect, so that
  // painting outside the plugin or the paint rect area can be avoided.
  // Note: `invalidate_rect` and `rect` are in CSS pixels. The plugin rect (with
  // the device scale applied) must be converted to CSS pixels as well before
  // calculating `invalidate_rect`.
  SkRect invalidate_rect = gfx::RectToSkRect(gfx::IntersectRects(
      gfx::ScaleToEnclosingRectSafe(plugin_rect(), inverse_scale), rect));
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
  OnViewportChanged(window_rect, container_wrapper_->DeviceScaleFactor());
}

void PdfViewWebPlugin::UpdateFocus(bool focused,
                                   blink::mojom::FocusType focus_type) {}

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

blink::WebTextInputType PdfViewWebPlugin::GetPluginTextInputType() {
  return text_input_type_;
}

void PdfViewWebPlugin::UpdateCursor(ui::mojom::CursorType new_cursor_type) {
  set_cursor_type(new_cursor_type);
}

void PdfViewWebPlugin::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {}

void PdfViewWebPlugin::NotifyNumberOfFindResultsChanged(int total,
                                                        bool final_result) {}

void PdfViewWebPlugin::NotifySelectedFindResultChanged(int current_find_index) {
}

void PdfViewWebPlugin::Alert(const std::string& message) {}

bool PdfViewWebPlugin::Confirm(const std::string& message) {
  return false;
}

std::string PdfViewWebPlugin::Prompt(const std::string& question,
                                     const std::string& default_answer) {
  return "";
}

void PdfViewWebPlugin::Print() {}

void PdfViewWebPlugin::SubmitForm(const std::string& url,
                                  const void* data,
                                  int length) {}

std::vector<PDFEngine::Client::SearchStringResult>
PdfViewWebPlugin::SearchString(const char16_t* string,
                               const char16_t* term,
                               bool case_sensitive) {
  return {};
}

pp::Instance* PdfViewWebPlugin::GetPluginInstance() {
  return nullptr;
}

void PdfViewWebPlugin::DocumentHasUnsupportedFeature(
    const std::string& feature) {}

bool PdfViewWebPlugin::IsPrintPreview() {
  return false;
}

void PdfViewWebPlugin::SelectionChanged(const gfx::Rect& left,
                                        const gfx::Rect& right) {}

void PdfViewWebPlugin::EnteredEditMode() {}

void PdfViewWebPlugin::SetSelectedText(const std::string& selected_text) {
  selected_text_ = blink::WebString::FromUTF8(selected_text);
  container_wrapper_->TextSelectionChanged(
      selected_text_, /*offset=*/0, gfx::Range(0, selected_text_.length()));
}

void PdfViewWebPlugin::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  NOTIMPLEMENTED();
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
  if (result == PP_OK) {
    if (!engine()->HandleDocumentLoad(std::move(loader)))
      DocumentLoadFailed();
  } else {
    NOTIMPLEMENTED();
  }
}

void PdfViewWebPlugin::DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                                      int32_t result) {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::SendMessage(base::Value message) {
  post_message_sender_.Post(std::move(message));
}

void PdfViewWebPlugin::InitImageData(const gfx::Size& size) {
  mutable_image_data() = CreateN32PremulSkBitmap(gfx::SizeToSkISize(size));
}

void PdfViewWebPlugin::SetFormFieldInFocus(bool in_focus) {
  text_input_type_ = in_focus ? blink::WebTextInputType::kWebTextInputTypeText
                              : blink::WebTextInputType::kWebTextInputTypeNone;
  container_wrapper_->UpdateTextInputState();
}

// TODO(https://crbug.com/1144444): Add a Pepper-free implementation to set
// accessibility document information.
void PdfViewWebPlugin::SetAccessibilityDocInfo(
    const AccessibilityDocInfo& doc_info) {
  NOTIMPLEMENTED();
}

// TODO(https://crbug.com/1144444): Add a Pepper-free implementation to set
// accessibility page information.
void PdfViewWebPlugin::SetAccessibilityPageInfo(
    AccessibilityPageInfo page_info,
    std::vector<AccessibilityTextRunInfo> text_runs,
    std::vector<AccessibilityCharInfo> chars,
    AccessibilityPageObjects page_objects) {
  NOTIMPLEMENTED();
}

// TODO(https://crbug.com/1144444): Add a Pepper-free implementation to set
// accessibility viewport information.
void PdfViewWebPlugin::SetAccessibilityViewportInfo(
    const AccessibilityViewportInfo& viewport_info) {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::SetContentRestrictions(int content_restrictions) {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::DidStartLoading() {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::DidStopLoading() {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::OnPrintPreviewLoaded() {
  NOTIMPLEMENTED();
}

void PdfViewWebPlugin::UserMetricsRecordAction(const std::string& action) {
  base::RecordAction(base::UserMetricsAction(action.c_str()));
}

void PdfViewWebPlugin::OnViewportChanged(const gfx::Rect& view_rect,
                                         float new_device_scale) {
  UpdateGeometryOnViewChanged(view_rect, new_device_scale);

  // TODO(http://crbug.com/1099020): Update scroll position for painting the
  // print preview plugin.
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

}  // namespace chrome_pdf
