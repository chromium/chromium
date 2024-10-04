// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_WEB_PLUGIN_H_
#define PDF_PDF_VIEW_WEB_PLUGIN_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/paint/paint_image.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/loader/url_loader.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_accessibility_action_handler.h"
#include "pdf/pdf_accessibility_image_fetcher.h"
#include "pdf/pdfium/pdfium_engine_client.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/post_message_receiver.h"
#include "pdf/preview_mode_client.h"
#include "pdf/v8_value_converter.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "pdf/pdf_ink_module_client.h"
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#endif

namespace blink {
class WebAssociatedURLLoader;
class WebInputEvent;
class WebURL;
class WebURLRequest;
struct WebAssociatedURLLoaderOptions;
struct WebPrintPresetOptions;
}  // namespace blink

namespace gfx {
class PointF;
class Range;
}  // namespace gfx

namespace net {
class SiteForCookies;
}  // namespace net

namespace printing {
class MetafileSkia;
}  // namespace printing

namespace chrome_pdf {

class MetricsHandler;
class PDFiumEngine;
class PdfAccessibilityDataHandler;
class Thumbnail;

#if BUILDFLAG(ENABLE_PDF_INK2)
class PdfInkModule;
#endif

class PdfViewWebPlugin final : public PDFiumEngineClient,
                               public blink::WebPlugin,
                               public pdf::mojom::PdfListener,
                               public UrlLoader::Client,
                               public PostMessageReceiver::Client,
                               public PaintManager::Client,
                               public PdfAccessibilityActionHandler,
                               public PdfAccessibilityImageFetcher,
#if BUILDFLAG(ENABLE_PDF_INK2)
                               public PdfInkModuleClient,
#endif
                               public PreviewModeClient::Client {
 public:
  // Do not save files larger than 100 MB. This cap should be kept in sync with
  // and is also enforced in chrome/browser/resources/pdf/pdf_viewer.ts.
  static constexpr size_t kMaximumSavedFileSize = 100 * 1000 * 1000;

  enum class DocumentLoadState {
    kLoading = 0,
    kComplete,
    kFailed,
  };

  // Must match `SaveRequestType` in chrome/browser/resources/pdf/constants.ts.
  enum class SaveRequestType {
    kAnnotation = 0,
    kOriginal = 1,
    kEdited = 2,
  };

  // Provides services from the plugin's container.
  class Client : public V8ValueConverter {
   public:
    virtual ~Client() = default;

    virtual base::WeakPtr<Client> GetWeakPtr() = 0;

    // Creates a new `PDFiumEngine`.
    virtual std::unique_ptr<PDFiumEngine> CreateEngine(
        PDFiumEngineClient* client,
        PDFiumFormFiller::ScriptOption script_option);

    // Passes the plugin container to the client. This is first called in
    // `Initialize()`, and cleared to null in `Destroy()`. The container may
    // also be null for testing.
    virtual void SetPluginContainer(blink::WebPluginContainer* container) = 0;

    // Returns the plugin container set by `SetPluginContainer()`.
    virtual blink::WebPluginContainer* PluginContainer() = 0;

    // Returns the current V8 isolate, if any.
    virtual v8::Isolate* GetIsolate() = 0;

    // Returns the document's site for cookies.
    virtual net::SiteForCookies SiteForCookies() const = 0;

    // Resolves `partial_url` relative to the document's base URL.
    virtual blink::WebURL CompleteURL(
        const blink::WebString& partial_url) const = 0;

    // Enqueues a "message" event carrying `message` to the embedder.
    // Messages are guaranteed to be received in the order that they are sent.
    // This method is non-blocking.
    virtual void PostMessage(base::Value::Dict message) {}

    // Invalidates the entire web plugin container and schedules a paint of the
    // page in it.
    virtual void Invalidate() = 0;

    // Notifies the container about which touch events the plugin accepts.
    virtual void RequestTouchEventType(
        blink::WebPluginContainer::TouchEventRequestType request_type) = 0;

    // Notify the web plugin container about the total matches of a find
    // request.
    virtual void ReportFindInPageMatchCount(int identifier,
                                            int total,
                                            bool final_update) = 0;

    // Notify the web plugin container about the selected find result in plugin.
    virtual void ReportFindInPageSelection(int identifier,
                                           int index,
                                           bool final_update) = 0;

    // Notify the web plugin container about find result tickmarks.
    virtual void ReportFindInPageTickmarks(
        const std::vector<gfx::Rect>& tickmarks) = 0;

    // Returns the device scale factor.
    virtual float DeviceScaleFactor() = 0;

    // Gets the scroll position.
    virtual gfx::PointF GetScrollPosition() = 0;

    // Tells the embedder to allow the plugin to handle find requests.
    virtual void UsePluginAsFindHandler() = 0;

    // Calls underlying WebLocalFrame::SetReferrerForRequest().
    virtual void SetReferrerForRequest(blink::WebURLRequest& request,
                                       const blink::WebURL& referrer_url) = 0;

    // Calls underlying WebLocalFrame::Alert().
    virtual void Alert(const blink::WebString& message) = 0;

    // Calls underlying WebLocalFrame::Confirm().
    virtual bool Confirm(const blink::WebString& message) = 0;

    // Calls underlying WebLocalFrame::Prompt().
    virtual blink::WebString Prompt(const blink::WebString& message,
                                    const blink::WebString& default_value) = 0;

    // Calls underlying WebLocalFrame::TextSelectionChanged().
    virtual void TextSelectionChanged(const blink::WebString& selection_text,
                                      uint32_t offset,
                                      const gfx::Range& range) = 0;

    // Calls underlying WebLocalFrame::CreateAssociatedURLLoader().
    virtual std::unique_ptr<blink::WebAssociatedURLLoader>
    CreateAssociatedURLLoader(
        const blink::WebAssociatedURLLoaderOptions& options) = 0;

    // Notifies the frame widget about the text input type change.
    virtual void UpdateTextInputState() = 0;

    // Notifies the frame widget about the selection bound change.
    virtual void UpdateSelectionBounds() = 0;

    // Gets the embedder's origin as a serialized string.
    virtual std::string GetEmbedderOriginString() = 0;

    // Returns whether the plugin container's frame exists.
    virtual bool HasFrame() const = 0;

    // Notifies the frame's client that the plugin started loading.
    virtual void DidStartLoading() = 0;

    // Notifies the frame's client that the plugin stopped loading.
    virtual void DidStopLoading() = 0;

    // Prints the plugin element.
    virtual void Print() {}

    // Sends over a string to be recorded by user metrics as a computed action.
    // When you use this, you need to also update the rules for extracting known
    // actions in tools/metrics/actions/extract_actions.py.
    virtual void RecordComputedAction(const std::string& action) {}

    // Creates an implementation of `PdfAccessibilityDataHandler` catered to the
    // client.
    virtual std::unique_ptr<PdfAccessibilityDataHandler>
    CreateAccessibilityDataHandler(
        PdfAccessibilityActionHandler* action_handler,
        PdfAccessibilityImageFetcher* image_fetcher,
        blink::WebPluginContainer* plugin_container,
        bool print_preview);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    // Performs OCR on `image` and sends the recognized text to `callback`.
    // In case OCR service gets disconnected before or during running this
    // request, `callback` will not be called.
    virtual void PerformOcr(
        const SkBitmap& image,
        base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)>
            callback) = 0;

    // Sets a `callback` that is notified if OCR service gets disconnected.
    virtual void SetOcrDisconnectedCallback(
        base::RepeatingClosure callback) = 0;
#endif
  };

  PdfViewWebPlugin(std::unique_ptr<Client> client,
                   mojo::AssociatedRemote<pdf::mojom::PdfHost> pdf_host,
                   blink::WebPluginParams params);
  PdfViewWebPlugin(const PdfViewWebPlugin& other) = delete;
  PdfViewWebPlugin& operator=(const PdfViewWebPlugin& other) = delete;

  // blink::WebPlugin:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  bool SupportsKeyboardFocus() const override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visibility) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;
  bool SupportsPaginatedPrint() override;
  bool GetPrintPresetOptionsFromDocument(
      blink::WebPrintPresetOptions* print_preset_options) override;
  int PrintBegin(const blink::WebPrintParams& print_params) override;
  void PrintPage(int page_index, cc::PaintCanvas* canvas) override;
  void PrintEnd() override;
  bool HasSelection() const override;
  blink::WebString SelectionAsText() const override;
  blink::WebString SelectionAsMarkup() const override;
  bool CanEditText() const override;
  bool HasEditableText() const override;
  bool CanUndo() const override;
  bool CanRedo() const override;
  bool CanCopy() const override;
  bool ExecuteEditCommand(const blink::WebString& name,
                          const blink::WebString& value) override;
  blink::WebURL LinkAtPosition(const gfx::Point& /*position*/) const override;
  bool StartFind(const blink::WebString& search_text,
                 bool case_sensitive,
                 int identifier) override;
  void SelectFindResult(bool forward, int identifier) override;
  void StopFind() override;
  bool CanRotateView() override;
  void RotateView(blink::WebPlugin::RotationType type) override;

  bool ShouldDispatchImeEventsToPlugin() override;
  blink::WebTextInputType GetPluginTextInputType() override;
  gfx::Rect GetPluginCaretBounds() override;
  void ImeSetCompositionForPlugin(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end) override;
  void ImeCommitTextForPlugin(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int relative_cursor_pos) override;
  void ImeFinishComposingTextForPlugin(bool keep_selection) override;

  // PDFiumEngineClient:
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  void Invalidate(const gfx::Rect& rect) override;
  void DidScroll(const gfx::Vector2d& offset) override;
  void ScrollToX(int x_screen_coords) override;
  void ScrollToY(int y_screen_coords) override;
  void ScrollBy(const gfx::Vector2d& delta) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override;
  void NavigateToDestination(int page,
                             const float* x,
                             const float* y,
                             const float* zoom) override;
  void UpdateCursor(ui::mojom::CursorType new_cursor_type) override;
  void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index,
                                       bool final_result) override;
  void NotifyTouchSelectionOccurred() override;
  void GetDocumentPassword(
      base::OnceCallback<void(const std::string&)> callback) override;
  void Beep() override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::string GetURL() override;
  void Email(const std::string& to,
             const std::string& cc,
             const std::string& bcc,
             const std::string& subject,
             const std::string& body) override;
  void Print() override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  std::unique_ptr<UrlLoader> CreateUrlLoader() override;
  v8::Isolate* GetIsolate() override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  void DocumentLoadComplete() override;
  void DocumentLoadFailed() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormFieldFocusChange(PDFiumEngineClient::FocusFieldType type) override;
  bool IsPrintPreview() const override;
  SkColor GetBackgroundColor() const override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void CaretChanged(const gfx::Rect& caret_rect) override;
  void EnteredEditMode() override;
  void DocumentFocusChanged(bool document_has_focus) override;
  void SetSelectedText(const std::string& selected_text) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;
  bool IsValidLink(const std::string& url) override;
#if BUILDFLAG(ENABLE_PDF_INK2)
  bool IsInAnnotationMode() const override;
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

  // pdf::mojom::PdfListener:
  void SetCaretPosition(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SetSelectionBounds(const gfx::PointF& base,
                          const gfx::PointF& extent) override;
  void GetPdfBytes(uint32_t size_limit, GetPdfBytesCallback callback) override;

  // UrlLoader::Client:
  bool IsValid() const override;
  blink::WebURL CompleteURL(const blink::WebString& partial_url) const override;
  net::SiteForCookies SiteForCookies() const override;
  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override;
  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override;

  // PostMessageReceiver::Client:
  void OnMessage(const base::Value::Dict& message) override;

  // PaintManager::Client:
  void InvalidatePluginContainer() override;
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending) override;
  void UpdateSnapshot(sk_sp<SkImage> snapshot) override;
  void UpdateScale(float scale) override;
  void UpdateLayerTransform(float scale,
                            const gfx::Vector2dF& translate) override;

  // PdfAccessibilityActionHandler:
  void EnableAccessibility() override;
  void HandleAccessibilityAction(
      const AccessibilityActionData& action_data) override;
  void LoadOrReloadAccessibility() override;

  // PdfAccessibilityImageFetcher:
  SkBitmap GetImageForOcr(int32_t page_index,
                          int32_t page_object_index) override;

#if BUILDFLAG(ENABLE_PDF_INK2)
  // PdfInkModuleClient:
  PageOrientation GetOrientation() const override;
  gfx::Rect GetPageContentsRect(int index) override;
  gfx::Vector2dF GetViewportOriginOffset() override;
  float GetZoom() const override;
  bool IsPageVisible(int page_index) override;
  void OnAnnotationModeToggled(bool enable) override;
  void StrokeFinished() override;
  void UpdateInkCursorImage(SkBitmap bitmap) override;
  void UpdateThumbnail(int page_index) override;
  int VisiblePageIndexFromPoint(const gfx::PointF& point) override;
#endif

  // PreviewModeClient::Client:
  void PreviewDocumentLoadComplete() override;
  void PreviewDocumentLoadFailed() override;

  // Initializes the plugin for testing, bypassing certain consistency checks.
  bool InitializeForTesting();

  const gfx::Rect& GetPluginRectForTesting() const { return plugin_rect_; }

  float GetDeviceScaleForTesting() const { return device_scale_; }

  void SendThumbnailForTesting(base::Value::Dict reply,
                               int page_index,
                               Thumbnail thumbnail);

  DocumentLoadState document_load_state_for_testing() const {
    return document_load_state_;
  }

  int GetContentRestrictionsForTesting() const {
    return GetContentRestrictions();
  }

  AccessibilityDocInfo GetAccessibilityDocInfoForTesting() const {
    return GetAccessibilityDocInfo();
  }

  int32_t next_accessibility_page_index_for_testing() const {
    return next_accessibility_page_index_;
  }

  void set_next_accessibility_page_index_for_testing(int32_t index) {
    next_accessibility_page_index_ = index;
  }

 private:
  // Callback that runs after `LoadUrl()`. The `loader` is the loader used to
  // load the URL, and `result` is the result code for the load.
  using LoadUrlCallback =
      base::OnceCallback<void(std::unique_ptr<UrlLoader> loader,
                              int32_t result)>;

  enum class AccessibilityState {
    kOff = 0,  // Off.
    kPending,  // Enabled but waiting for doc to load.
    kLoaded,   // Fully loaded.
  };

  struct BackgroundPart {
    gfx::Rect location;
    uint32_t color;
  };

  // Metadata about an available preview page.
  struct PreviewPageInfo {
    // Data source URL.
    std::string url;

    // Page index in destination document.
    int dest_page_index = -1;
  };

  // Call `Destroy()` instead.
  ~PdfViewWebPlugin() override;

  bool InitializeCommon();

  // Sends whether to do smooth scrolling.
  void SendSetSmoothScrolling();

  // Handles `LoadUrl()` result for the main document.
  void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result);

  // Updates the scroll position, which is in CSS pixels relative to the
  // top-left corner.
  void UpdateScroll(const gfx::PointF& scroll_position);

  // Loads `url`, invoking `callback` on receiving the initial response.
  void LoadUrl(std::string_view url, LoadUrlCallback callback);

  // Handles `Open()` result for `form_loader_`.
  void DidFormOpen(int32_t result);

  // Sends start/stop loading notifications to the plugin's render frame.
  void DidStartLoading();
  void DidStopLoading();

  // Gets the content restrictions based on the permissions which `engine_` has.
  int GetContentRestrictions() const;

  // Message handlers.
  void HandleDisplayAnnotationsMessage(const base::Value::Dict& message);
  void HandleGetNamedDestinationMessage(const base::Value::Dict& message);
  void HandleGetPageBoundingBoxMessage(const base::Value::Dict& message);
  void HandleGetPasswordCompleteMessage(const base::Value::Dict& message);
  void HandleGetSelectedTextMessage(const base::Value::Dict& message);
  void HandleGetThumbnailMessage(const base::Value::Dict& message);
  void HandlePrintMessage(const base::Value::Dict& /*message*/);
  void HandleRotateClockwiseMessage(const base::Value::Dict& /*message*/);
  void HandleRotateCounterclockwiseMessage(
      const base::Value::Dict& /*message*/);
  void HandleSaveAttachmentMessage(const base::Value::Dict& message);
  void HandleSaveMessage(const base::Value::Dict& message);
  void HandleSelectAllMessage(const base::Value::Dict& /*message*/);
  void HandleSetBackgroundColorMessage(const base::Value::Dict& message);
  void HandleSetPresentationModeMessage(const base::Value::Dict& message);
  void HandleSetTwoUpViewMessage(const base::Value::Dict& message);
  void HandleStopScrollingMessage(const base::Value::Dict& message);
  void HandleViewportMessage(const base::Value::Dict& message);

  void SaveToBuffer(const std::string& token);
  void SaveToFile(const std::string& token);

  // Converts a scroll offset (which is relative to a UI direction-dependent
  // scroll origin) to a scroll position (which is always relative to the
  // top-left corner).
  gfx::PointF GetScrollPositionFromOffset(
      const gfx::Vector2dF& scroll_offset) const;

  // Paints the given invalid area of the plugin to the given graphics device.
  // PaintManager::Client::OnPaint() should be its only caller.
  void DoPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending);

  // The preparation when painting on the image data buffer for the first
  // time.
  void PrepareForFirstPaint(std::vector<PaintReadyRect>& ready);

  // Updates the available area and the background parts, notifies the PDF
  // engine, and updates the accessibility information.
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // A helper of OnGeometryChanged() which updates the available area and
  // the background parts, and notifies the PDF engine of geometry changes.
  void RecalculateAreas(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  // Schedules invalidation tasks after painting finishes.
  void InvalidateAfterPaintDone();

  // Callback to clear deferred invalidates after painting finishes.
  void ClearDeferredInvalidates();

  // Recalculates values that depend on scale factors.
  void UpdateScaledValues();

  void OnViewportChanged(const gfx::Rect& new_plugin_rect_in_css_pixel,
                         float new_device_scale);

  // Text editing methods.
  bool SelectAll();
  bool Cut();
  bool Paste(const blink::WebString& value);
  bool Undo();
  bool Redo();

  bool HandleWebInputEvent(const blink::WebInputEvent& event);

  // Helper method for converting IME text to input events.
  // TODO(crbug.com/40199248): Consider handling composition events.
  void HandleImeCommit(const blink::WebString& text);

  // Callback to print without re-entrancy issues. The callback prevents the
  // invocation of printing in the middle of an event handler, which is risky;
  // see crbug.com/66334.
  // TODO(crbug.com/40185029): Re-evaluate the need for a callback when parts of
  // the plugin are moved off the main thread.
  void OnInvokePrintDialog();

  void ResetRecentlySentFindUpdate();

  // Records metrics about the document metadata.
  void RecordDocumentMetrics();

  // Sends the attachments data to the frontend.
  void SendAttachments();

  // Sends the bookmarks data to the frontend.
  void SendBookmarks();

  // Notifies the frontend that `edit_command` got executed.
  void SendExecutedEditCommand(std::string_view edit_command);

  // Notifies the frontend that find-in-page started.
  void SendStartedFindInPage();

  // Sends document metadata data to the frontend.
  void SendMetadata();

  // Sends the loading progress, where `percentage` represents the progress, or
  // -1 for loading error.
  void SendLoadingProgress(double percentage);

  // Handles message for resetting Print Preview.
  void HandleResetPrintPreviewModeMessage(const base::Value::Dict& message);

  // Handles message for loading a preview page.
  void HandleLoadPreviewPageMessage(const base::Value::Dict& message);

  // Starts loading the next available preview page into a blank page.
  void LoadAvailablePreviewPage();

  // Handles `LoadUrl()` result for a preview page.
  void DidOpenPreview(std::unique_ptr<UrlLoader> loader, int32_t result);

  // Continues loading the next preview page.
  void LoadNextPreviewPage();

  // Sends a notification that the print preview has loaded.
  void SendPrintPreviewLoadedNotification();

  // Sends the thumbnail image data.
  void SendThumbnail(base::Value::Dict reply,
                     int page_index,
                     Thumbnail thumbnail);

#if BUILDFLAG(ENABLE_PDF_INK2)
  void GenerateAndSendInkThumbnail(int page_index, const gfx::Size& size);
#endif

  // Converts `frame_coordinates` to PDF coordinates.
  gfx::Point FrameToPdfCoordinates(const gfx::PointF& frame_coordinates) const;

  // Gets the accessibility doc info based on the information from `engine_`.
  AccessibilityDocInfo GetAccessibilityDocInfo() const;

  // Sets the accessibility information about the given `page_index` in the
  // renderer.
  void PrepareAndSetAccessibilityPageInfo(int32_t page_index);

  // Prepares the accessibility information about the current viewport. This is
  // done once when accessibility is first loaded and again when the geometry
  // changes.
  void PrepareAndSetAccessibilityViewportInfo();

  // Starts loading accessibility information.
  void LoadAccessibility();

  bool initialized_ = false;

  blink::WebString selected_text_;

  std::unique_ptr<Client> const client_;

  // Used to access the services provided by the browser.
  mojo::AssociatedRemote<pdf::mojom::PdfHost> const pdf_host_;

  mojo::Receiver<pdf::mojom::PdfListener> listener_receiver_{this};

#if BUILDFLAG(ENABLE_PDF_INK2)
  // Null if `features::kPdfInk2` is not enabled.
  std::unique_ptr<PdfInkModule> const ink_module_;
#endif

  std::unique_ptr<PDFiumEngine> engine_;

  // The URL of the PDF document.
  std::string url_;

  // The callback for receiving the password from the page.
  base::OnceCallback<void(const std::string&)> password_callback_;

  // The current cursor type.
  ui::Cursor cursor_ = ui::mojom::CursorType::kPointer;

  blink::WebTextInputType text_input_type_ =
      blink::WebTextInputType::kWebTextInputTypeNone;

  gfx::Rect caret_rect_;

  blink::WebString composition_text_;

  // Whether the plugin element currently has focus.
  bool has_focus_ = false;

  blink::WebPluginParams initial_params_;

  v8::Persistent<v8::Object> scriptable_receiver_;

  PaintManager paint_manager_{this};

  // Image data buffer for painting.
  SkBitmap image_data_;

  // The current image snapshot.
  cc::PaintImage snapshot_;

  // Translate from snapshot to device pixels.
  gfx::Vector2dF snapshot_translate_;

  // Scale from snapshot to device pixels.
  float snapshot_scale_ = 1.0f;

  // The viewport coordinates to DIP (device-independent pixel) ratio.
  float viewport_to_dip_scale_ = 1.0f;

  // Combined translate from snapshot to device to CSS pixels.
  gfx::Vector2dF total_translate_;

  // The plugin rect in CSS pixels.
  gfx::Rect css_plugin_rect_;

  // True if the plugin occupies the entire frame (not embedded).
  bool full_frame_ = false;

  // The background color of the PDF viewer.
  SkColor background_color_ = SK_ColorTRANSPARENT;

  // Size, in DIPs, of plugin rectangle.
  gfx::Size plugin_dip_size_;

  // The plugin rectangle in device pixels.
  gfx::Rect plugin_rect_;

  // Remaining area, in pixels, to render the PDF in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;

  // Current zoom factor.
  double zoom_ = 1.0;

  // Current device scale factor. Multiply by `device_scale_` to convert from
  // viewport to screen coordinates. Divide by `device_scale_` to convert from
  // screen to viewport coordinates.
  float device_scale_ = 1.0f;

  // True if we haven't painted the plugin viewport yet.
  bool first_paint_ = true;

  // Whether OnPaint() is in progress or not.
  bool in_paint_ = false;

  // True if last bitmap was smaller than the screen.
  bool last_bitmap_smaller_ = false;

  // True if we request a new bitmap rendering.
  bool needs_reraster_ = true;

  // The size of the entire document in pixels (i.e. if each page is 800 pixels
  // high and there are 10 pages, the height will be 8000).
  gfx::Size document_size_;

  std::vector<BackgroundPart> background_parts_;

  // Deferred invalidates while `in_paint_` is true.
  std::vector<gfx::Rect> deferred_invalidates_;

  // The UI direction.
  base::i18n::TextDirection ui_direction_ = base::i18n::UNKNOWN_DIRECTION;

  // The scroll offset for the last raster in CSS pixels, before any
  // transformations are applied.
  gfx::Vector2dF scroll_offset_at_last_raster_;

  // If this is true, then don't scroll the plugin in response to calls to
  // `UpdateScroll()`. This will be true when the extension page is in the
  // process of zooming the plugin so that flickering doesn't occur while
  // zooming.
  bool stop_scrolling_ = false;

  // Whether the plugin has received a viewport changed message. Nothing should
  // be painted until this is received.
  bool received_viewport_message_ = false;

  // If true, the render frame has been notified that we're starting a network
  // request so that it can start the throbber. It will be notified again once
  // the document finishes loading.
  bool did_call_start_loading_ = false;

  // The last document load progress value sent to the web page.
  double last_progress_sent_ = 0.0;

  // The current state of document load.
  DocumentLoadState document_load_state_ = DocumentLoadState::kLoading;

  // The current state of accessibility.
  AccessibilityState accessibility_state_ = AccessibilityState::kOff;

  // The next accessibility page index, used to track interprocess calls when
  // reconstructing the tree for new document layouts.
  int32_t next_accessibility_page_index_ = 0;

  // Used for submitting forms.
  std::unique_ptr<UrlLoader> form_loader_;

  // Handler for accessibility data updates.
  std::unique_ptr<PdfAccessibilityDataHandler> pdf_accessibility_data_handler_;

  // The URL currently under the cursor.
  std::string link_under_cursor_;

  // The ID of the current find operation, or -1 if no current operation is
  // present.
  int find_identifier_ = -1;

  // Whether an update to the number of find results found was sent less than
  // `kFindResultCooldown` TimeDelta ago.
  bool recently_sent_find_update_ = false;

  // Stores the tickmarks to be shown for the current find results.
  std::vector<gfx::Rect> tickmarks_;

  // Whether the document is in edit mode.
  bool edit_mode_ = false;

  // Only instantiated when not print previewing.
  std::unique_ptr<MetricsHandler> metrics_handler_;

  // Keeps track of which unsupported features have been reported to avoid
  // spamming the metrics if a feature shows up many times per document.
  base::flat_set<std::string> unsupported_features_reported_;

  // Indicates whether the browser has been notified about an unsupported
  // feature once, which helps prevent the infobar from going up more than once.
  bool notified_browser_about_unsupported_feature_ = false;

  // The metafile in which to save the printed output. Assigned a value only
  // between `PrintBegin()` and `PrintEnd()` calls.
  raw_ptr<printing::MetafileSkia> printing_metafile_ = nullptr;

  // The indices of pages to print.
  std::vector<int> pages_to_print_;

  // Assigned a value only between `PrintBegin()` and `PrintEnd()` calls.
  std::optional<blink::WebPrintParams> print_params_;

  // For identifying actual print operations to avoid double logging of UMA.
  bool print_pages_called_;

  // Whether the plugin is loaded in Print Preview.
  bool is_print_preview_ = false;

  // Number of pages in Print Preview (non-PDF). 0 if previewing a PDF, and -1
  // if not in Print Preview.
  int print_preview_page_count_ = -1;

  // Number of pages loaded in Print Preview (non-PDF). Always less than or
  // equal to `print_preview_page_count_`.
  int print_preview_loaded_page_count_ = -1;

  // The PreviewModeClient used for print preview. Will be passed to
  // `preview_engine_`.
  std::unique_ptr<PreviewModeClient> preview_client_;

  // Engine used to render individual preview pages. This will use the
  // `PreviewModeClient` interface.
  std::unique_ptr<PDFiumEngine> preview_engine_;

  // Document load state for the Print Preview engine.
  DocumentLoadState preview_document_load_state_ = DocumentLoadState::kComplete;

  // Queue of available preview pages to load next.
  base::queue<PreviewPageInfo> preview_pages_info_;

  base::WeakPtrFactory<PdfViewWebPlugin> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_WEB_PLUGIN_H_
