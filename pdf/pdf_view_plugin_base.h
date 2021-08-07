// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_PLUGIN_BASE_H_
#define PDF_PDF_VIEW_PLUGIN_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "pdf/accessibility_structs.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/preview_mode_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class Value;
}  // namespace base

namespace blink {
class WebInputEvent;
struct WebPrintPresetOptions;
}  // namespace blink

namespace chrome_pdf {

class Image;
class PDFiumEngine;
class Thumbnail;
class UrlLoader;
struct AccessibilityActionData;
struct AccessibilityCharInfo;
struct AccessibilityDocInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;
struct AccessibilityViewportInfo;

// Common base to share code between the two plugin implementations,
// `OutOfProcessInstance` (Pepper) and `PdfViewWebPlugin` (Blink).
class PdfViewPluginBase : public PDFEngine::Client,
                          public PaintManager::Client,
                          public PreviewModeClient::Client {
 public:
  using PDFEngine::Client::ScheduleTaskOnMainThread;

  // Do not save files with over 100 MB. This cap should be kept in sync with
  // and is also enforced in chrome/browser/resources/pdf/pdf_viewer.js.
  static constexpr size_t kMaximumSavedFileSize = 100 * 1000 * 1000;

  enum class AccessibilityState {
    kOff = 0,  // Off.
    kPending,  // Enabled but waiting for doc to load.
    kLoaded,   // Fully loaded.
  };

  enum class DocumentLoadState {
    kLoading = 0,
    kComplete,
    kFailed,
  };

  // Must match `SaveRequestType` in chrome/browser/resources/pdf/constants.js.
  enum class SaveRequestType {
    kAnnotation = 0,
    kOriginal = 1,
    kEdited = 2,
  };

  PdfViewPluginBase(const PdfViewPluginBase& other) = delete;
  PdfViewPluginBase& operator=(const PdfViewPluginBase& other) = delete;

  // PDFEngine::Client:
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
  void NotifyTouchSelectionOccurred() override;
  void GetDocumentPassword(
      base::OnceCallback<void(const std::string&)> callback) override;
  void Beep() override;
  std::string GetURL() override;
  void Email(const std::string& to,
             const std::string& cc,
             const std::string& bcc,
             const std::string& subject,
             const std::string& body) override;
  void Print() override;
  std::unique_ptr<UrlLoader> CreateUrlLoader() override;
  void DocumentLoadComplete() override;
  void DocumentLoadFailed() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() const override;
  SkColor GetBackgroundColor() override;
  void SetIsSelecting(bool is_selecting) override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void EnteredEditMode() override;
  void DocumentFocusChanged(bool document_has_focus) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;

  // PaintManager::Client:
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending) override;

  // PreviewModeClient::Client:
  void PreviewDocumentLoadComplete() override;
  void PreviewDocumentLoadFailed() override;

  // Enable accessibility for PDF plugin.
  void EnableAccessibility();

  // Handle invoked accessibility actions.
  void HandleAccessibilityAction(const AccessibilityActionData& action_data);

  // Gets the content restrictions based on the permissions which `engine_` has.
  int GetContentRestrictions() const;

  // Gets the accessibility doc info based on the information from `engine_`.
  AccessibilityDocInfo GetAccessibilityDocInfo() const;

  bool GetDidCallStartLoadingForTesting() const {
    return did_call_start_loading_;
  }

  bool UnsupportedFeatureIsReportedForTesting(const std::string& feature) const;

  bool GetNotifiedBrowserAboutUnsupportedFeatureForTesting() const {
    return notified_browser_about_unsupported_feature_;
  }

 protected:
  struct BackgroundPart {
    gfx::Rect location;
    uint32_t color;
  };

  PdfViewPluginBase();
  ~PdfViewPluginBase() override;

  // Initializes the main `PDFiumEngine` with `engine`. Any existing engine will
  // be replaced. `engine` must not be nullptr.
  void InitializeEngine(std::unique_ptr<PDFiumEngine> engine);

  // Destroys the main `PDFiumEngine`. Subclasses should call this method in
  // their destructor to ensure the engine is destroyed first.
  void DestroyEngine();

  // Destroys the `PDFiumEngine` used for Print Preview. Subclasses should call
  // this method in their destructor to ensure the engine is destroyed first.
  void DestroyPreviewEngine();

  const PDFiumEngine* engine() const { return engine_.get(); }
  PDFiumEngine* engine() { return engine_.get(); }

  void ValidateDocumentUrl(base::StringPiece document_url);

  // Starts loading `url`. If `is_print_preview` is `true`, load for print
  // preview instead of normal PDF viewing.
  void LoadUrl(const std::string& url, bool is_print_preview);

  // Gets a weak pointer with a lifetime matching the derived class.
  virtual base::WeakPtr<PdfViewPluginBase> GetWeakPtr() = 0;

  // Creates a URL loader and allows it to access all urls, i.e. not just the
  // frame's origin.
  virtual std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() = 0;

  // Handles `LoadUrl()` result.
  virtual void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result) = 0;

  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Handles `postMessage()` calls from the embedder.
  void HandleMessage(const base::Value& message);

  // Enqueues a "message" event carrying `message` to the embedder. Messages are
  // guaranteed to be received in the order that they are sent. This method is
  // non-blocking.
  virtual void SendMessage(base::Value message) = 0;

  // Invokes the "SaveAs" dialog.
  virtual void SaveAs() = 0;

  void SaveToBuffer(const std::string& token);

  // Consumes a token for saving the document.
  void ConsumeSaveToken(const std::string& token);

  // Sends the loading progress, where `percentage` represents the progress, or
  // -1 for loading error.
  void SendLoadingProgress(double percentage);

  // Send a notification that the print preview has loaded.
  void SendPrintPreviewLoadedNotification();

  // Initialize image buffer(s) according to the new context size.
  virtual void InitImageData(const gfx::Size& size) = 0;

  // Schedules invalidation tasks after painting finishes.
  void InvalidateAfterPaintDone();

  // Updates the available area and the background parts, notifies the PDF
  // engine, and updates the accessibility information.
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // Returns the plugin-specific image data buffer.
  virtual Image GetPluginImageData() const;

  // Updates the geometry of the plugin and its image data if the view's
  // size or scale has changed. `new_view_rect` must be in CSS pixels (without
  // device scale applied).
  void UpdateGeometryOnViewChanged(const gfx::Rect& new_view_rect,
                                   float new_device_scale);

  // A helper of OnGeometryChanged() which updates the available area and
  // the background parts, and notifies the PDF engine of geometry changes.
  void RecalculateAreas(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Repaints the plugin contents based on the current scroll position.
  void UpdateScroll();

  // Bound the given scroll position to the document.
  gfx::PointF BoundScrollPositionToDocument(const gfx::PointF& scroll_position);

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  // Sets the text input type for this plugin based on `in_focus`.
  virtual void SetFormFieldInFocus(bool in_focus) = 0;

  // Sets the accessibility information about the PDF document in the renderer.
  virtual void SetAccessibilityDocInfo(
      const AccessibilityDocInfo& doc_info) = 0;

  // Sets the accessibility information about the given `page_index` in the
  // renderer.
  void PrepareAndSetAccessibilityPageInfo(int32_t page_index);

  // Sets the accessibility information about the page in the renderer.
  virtual void SetAccessibilityPageInfo(
      AccessibilityPageInfo page_info,
      std::vector<AccessibilityTextRunInfo> text_runs,
      std::vector<AccessibilityCharInfo> chars,
      AccessibilityPageObjects page_objects) = 0;

  // Prepares the accessibility information about the current viewport. Calls
  // SetAccessibilityViewportInfo() internally to set this information in the
  // renderer. This is done once when accessibility is first loaded and again
  // when the geometry changes.
  void PrepareAndSetAccessibilityViewportInfo();

  // Sets the accessibility information about the current viewport in the
  // renderer.
  virtual void SetAccessibilityViewportInfo(
      const AccessibilityViewportInfo& viewport_info) = 0;

  // Returns the print preset options for the document.
  blink::WebPrintPresetOptions GetPrintPresetOptions();

  // Begins a print session with the given `print_params`. A call to
  // `PrintPages()` can only be made after after a successful call to
  // `PrintBegin()`. Returns the number of pages required for the print output.
  // A returned value of 0 indicates failure.
  int PrintBegin(const blink::WebPrintParams& print_params);

  // Prints the pages specified by `page_numbers` using the parameters passed to
  // `PrintBegin()` Returns a vector of bytes containing the printed output. An
  // empty returned value indicates failure.
  std::vector<uint8_t> PrintPages(const std::vector<int>& page_numbers);

  // Ends the print session. Further calls to `PrintPages()` will fail.
  void PrintEnd();

  // Disables browser commands because of restrictions on how the data is to be
  // used (i.e. can't copy/print). `content_restrictions` should have its bits
  // set by `chrome_pdf::ContentRestriction` enum values.
  virtual void SetContentRestrictions(int content_restrictions) = 0;

  // Informs the embedder whether the plugin can handle save commands
  // internally.
  virtual void SetPluginCanSave(bool can_save) = 0;

  // Sends start/stop loading notifications to the plugin's render frame.
  virtual void PluginDidStartLoading() = 0;
  virtual void PluginDidStopLoading() = 0;

  // Requests the plugin's render frame to set up a print dialog for the
  // document.
  virtual void InvokePrintDialog() = 0;

  // Notifies the embedder about a new link under the cursor.
  // TODO(crbug.com/702993): This is only needed by `OutOfProcessInstance`.
  // Remove this method when that class ceases to exist.
  virtual void NotifyLinkUnderCursor() {}

  // Notifies the embedder of the top-left and bottom-right coordinates of the
  // current selection.
  virtual void NotifySelectionChanged(const gfx::PointF& left,
                                      int left_height,
                                      const gfx::PointF& right,
                                      int right_height) = 0;

  // Notifies the user about unsupported feature if the PDF Viewer occupies the
  // full frame.
  virtual void NotifyUnsupportedFeature() = 0;

  // Records user actions.
  virtual void UserMetricsRecordAction(const std::string& action) = 0;

  void set_url(const std::string& url) { url_ = url; }

  ui::mojom::CursorType cursor_type() const { return cursor_type_; }
  void set_cursor_type(ui::mojom::CursorType cursor_type) {
    cursor_type_ = cursor_type;
  }

  const std::string& link_under_cursor() const { return link_under_cursor_; }

  bool full_frame() const { return full_frame_; }
  void set_full_frame(bool full_frame) { full_frame_ = full_frame; }

  SkBitmap& mutable_image_data() { return image_data_; }

  const gfx::Rect& available_area() const { return available_area_; }

  void set_document_size(const gfx::Size& size) { document_size_ = size; }

  const gfx::Rect& plugin_rect() const { return plugin_rect_; }

  void SetBackgroundColor(SkColor background_color) {
    background_color_ = background_color;
  }

  // Sets the new zoom scale.
  void SetZoom(double scale);

  double zoom() const { return zoom_; }

  float device_scale() const { return device_scale_; }

  void set_scroll_position(const gfx::Point& scroll_position) {
    scroll_position_ = scroll_position;
  }

  bool stop_scrolling() const { return stop_scrolling_; }

  DocumentLoadState document_load_state() const { return document_load_state_; }
  void set_document_load_state(DocumentLoadState state) {
    document_load_state_ = state;
  }

  AccessibilityState accessibility_state() const {
    return accessibility_state_;
  }

  bool edit_mode() const { return edit_mode_; }
  void set_edit_mode(bool edit_mode) { edit_mode_ = edit_mode; }

  void set_is_print_preview(bool is_print_preview) {
    is_print_preview_ = is_print_preview;
  }

  static bool IsPrintPreviewUrl(base::StringPiece url);

  static constexpr bool IsSaveDataSizeValid(size_t size) {
    return size > 0 && size <= kMaximumSavedFileSize;
  }

 private:
  // Message handlers.
  void HandleDisplayAnnotationsMessage(const base::Value& message);
  void HandleGetNamedDestinationMessage(const base::Value& message);
  void HandleGetPasswordCompleteMessage(const base::Value& message);
  void HandleGetSelectedTextMessage(const base::Value& message);
  void HandleGetThumbnailMessage(const base::Value& message);
  void HandleLoadPreviewPageMessage(const base::Value& message);
  void HandlePrintMessage(const base::Value& /*message*/);
  void HandleResetPrintPreviewModeMessage(const base::Value& message);
  void HandleRotateClockwiseMessage(const base::Value& /*message*/);
  void HandleRotateCounterclockwiseMessage(const base::Value& /*message*/);
  void HandleSaveMessage(const base::Value& message);
  void HandleSaveAttachmentMessage(const base::Value& message);
  void HandleSelectAllMessage(const base::Value& /*message*/);
  void HandleSetBackgroundColorMessage(const base::Value& message);
  void HandleSetReadOnlyMessage(const base::Value& message);
  void HandleSetTwoUpViewMessage(const base::Value& message);
  void HandleStopScrollingMessage(const base::Value& /*message*/);
  void HandleUpdateScrollMessage(const base::Value& message);
  void HandleViewportMessage(const base::Value& message);

  // Sends start/stop loading notifications to the plugin's render frame
  // depending on `did_call_start_loading_`.
  void DidStartLoading();
  void DidStopLoading();

  // Saves the document to a file.
  void SaveToFile(const std::string& token);

  // Paints the given invalid area of the plugin to the given graphics device.
  // PaintManager::Client::OnPaint() should be its only caller.
  void DoPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>& ready,
               std::vector<gfx::Rect>& pending);

  // The preparation when painting on the image data buffer for the first
  // time.
  void PrepareForFirstPaint(std::vector<PaintReadyRect>& ready);

  // Callback to clear deferred invalidates after painting finishes.
  void ClearDeferredInvalidates(int32_t /*unused_but_required*/);

  // Sends the attachments data.
  void SendAttachments();

  // Sends the bookmarks data.
  void SendBookmarks();

  // Send document metadata data.
  void SendMetadata();

  // Sends the thumbnail image data.
  void SendThumbnail(base::Value reply, Thumbnail thumbnail);

  // Starts loading accessibility information.
  void LoadAccessibility();

  // Records metrics about the document metadata.
  void RecordDocumentMetrics();

  // Adds a sample to an enumerated histogram and filter out print preview
  // usage.
  template <typename T>
  void HistogramEnumeration(const char* name, T sample);

  // Adds a sample to a custom counts histogram and filter out print preview
  // usage.
  void HistogramCustomCounts(const char* name,
                             int32_t sample,
                             int32_t min,
                             int32_t max,
                             uint32_t bucket_count);

  // Handles `LoadUrl()` result for print preview.
  void DidOpenPreview(std::unique_ptr<UrlLoader> loader, int32_t result);

  // Performs tasks necessary when the document is loaded in print preview mode.
  void OnPrintPreviewLoaded();

  // Reduces the document to 1 page and appends `print_preview_page_count_` - 1
  // blank pages to the document for print preview.
  void AppendBlankPrintPreviewPages();

  // Process the preview page data information. `src_url` specifies the preview
  // page data location. The `src_url` is in the format:
  // chrome://print/id/page_number/print.pdf
  // `dest_page_index` specifies the blank page index that needs to be replaced
  // with the new page data.
  void ProcessPreviewPageInfo(const std::string& src_url, int dest_page_index);
  // Load the next available preview page into the blank page.
  void LoadAvailablePreviewPage();

  // Called after a preview page has loaded or failed to load.
  void LoadNextPreviewPage();

  std::unique_ptr<PDFiumEngine> engine_;
  PaintManager paint_manager_{this};

  // The URL of the PDF document.
  std::string url_;

  // The current cursor type.
  ui::mojom::CursorType cursor_type_ = ui::mojom::CursorType::kPointer;

  // The URL currently under the cursor.
  std::string link_under_cursor_;

  // True if the plugin occupies the entire frame (not embedded).
  bool full_frame_ = false;

  // Image data buffer for painting.
  SkBitmap image_data_;

  std::vector<BackgroundPart> background_parts_;

  // Deferred invalidates while `in_paint_` is true.
  std::vector<gfx::Rect> deferred_invalidates_;

  // Remaining area, in pixels, to render the pdf in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;

  // The size of the entire document in pixels (i.e. if each page is 800 pixels
  // high and there are 10 pages, the height will be 8000).
  gfx::Size document_size_;

  // Size, in DIPs, of plugin rectangle.
  gfx::Size plugin_dip_size_;

  // The plugin rectangle in device pixels.
  gfx::Rect plugin_rect_;

  // The background color of the PDF viewer.
  SkColor background_color_ = SK_ColorTRANSPARENT;

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

  // The scroll position in CSS pixels.
  gfx::Point scroll_position_;

  // The scroll position for the last raster, before any transformations are
  // applied.
  gfx::PointF scroll_position_at_last_raster_;

  // If this is true, then don't scroll the plugin in response to the messages
  // from DidChangeView() or HandleUpdateScrollMessage(). This will be true when
  // the extension page is in the process of zooming the plugin so that
  // flickering doesn't occur while zooming.
  bool stop_scrolling_ = false;

  // Whether the plugin has received a viewport changed message. Nothing should
  // be painted until this is received.
  bool received_viewport_message_ = false;

  // The callback for receiving the password from the page.
  base::OnceCallback<void(const std::string&)> password_callback_;

  // The last document load progress value sent to the web page.
  double last_progress_sent_ = 0.0;

  // The current state of document load.
  DocumentLoadState document_load_state_ = DocumentLoadState::kLoading;

  // If true, the render frame has been notified that we're starting a network
  // request so that it can start the throbber. It will be notified again once
  // the document finishes loading.
  bool did_call_start_loading_ = false;

  // The current state of accessibility.
  AccessibilityState accessibility_state_ = AccessibilityState::kOff;

  // The next accessibility page index, used to track interprocess calls when
  // reconstructing the tree for new document layouts.
  int32_t next_accessibility_page_index_ = 0;

  // Keeps track of which unsupported features have been reported to avoid
  // spamming the metrics if a feature shows up many times per document.
  base::flat_set<std::string> unsupported_features_reported_;

  // Indicates whether the browser has been notified about an unsupported
  // feature once, which helps prevent the infobar from going up more than once.
  bool notified_browser_about_unsupported_feature_ = false;

  // Whether the document is in edit mode.
  bool edit_mode_ = false;

  // Assigned a value only between `PrintBegin()` and `PrintEnd()` calls.
  absl::optional<blink::WebPrintParams> print_params_;

  // For identifying actual print operations to avoid double logging of UMA.
  bool print_pages_called_;

  // The PreviewModeClient used for print preview. Will be passed to
  // `preview_engine_`.
  std::unique_ptr<PreviewModeClient> preview_client_;

  // This engine is used to render the individual preview page data. This is
  // used only in print preview mode. This will use `PreviewModeClient`
  // interface which has very limited access to the pp::Instance.
  std::unique_ptr<PDFiumEngine> preview_engine_;

  DocumentLoadState preview_document_load_state_ = DocumentLoadState::kComplete;

  // True if the plugin is loaded in print preview, otherwise false.
  bool is_print_preview_ = false;

  // Number of pages in print preview mode for non-PDF source, 0 if print
  // previewing a PDF, and -1 if not in print preview mode.
  int print_preview_page_count_ = -1;

  // Number of pages loaded in print preview mode for non-PDF source. Always
  // less than or equal to `print_preview_page_count_`.
  int print_preview_loaded_page_count_ = -1;

  // Used to manage loaded print preview page information. A `PreviewPageInfo`
  // consists of data source URL string and the page index in the destination
  // document.
  // The URL string embeds a page number that can be found with
  // ExtractPrintPreviewPageIndex(). This page number is always greater than 0.
  // The page index is always in the range of [0, print_preview_page_count_).
  using PreviewPageInfo = std::pair<std::string, int>;
  base::queue<PreviewPageInfo> preview_pages_info_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
