// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_OUT_OF_PROCESS_INSTANCE_H_
#define PDF_OUT_OF_PROCESS_INSTANCE_H_

#include <stdint.h>
#include <string.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_view_plugin_base.h"
#include "pdf/preview_mode_client.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/find_private.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Rect;
class Size;
class Vector2d;
}  // namespace gfx

namespace pp {
class Size;
class TextInput_Dev;
}  // namespace pp

namespace chrome_pdf {

class Graphics;
class PaintReadyRect;
class PDFiumEngine;
class Thumbnail;
class UrlLoader;

class OutOfProcessInstance : public PdfViewPluginBase,
                             public pp::Instance,
                             public pp::Find_Private,
                             public pp::Printing_Dev,
                             public PaintManager::Client,
                             public PreviewModeClient::Client {
 public:
  explicit OutOfProcessInstance(PP_Instance instance);
  OutOfProcessInstance(const OutOfProcessInstance&) = delete;
  OutOfProcessInstance& operator=(const OutOfProcessInstance&) = delete;
  ~OutOfProcessInstance() override;

  // pp::Instance:
  bool Init(uint32_t argc, const char* argn[], const char* argv[]) override;
  void HandleMessage(const pp::Var& message) override;
  bool HandleInputEvent(const pp::InputEvent& event) override;
  void DidChangeView(const pp::View& view) override;
  void DidChangeFocus(bool has_focus) override;

  // pp::Find_Private:
  bool StartFind(const std::string& text, bool case_sensitive) override;
  void SelectFindResult(bool forward) override;
  void StopFind() override;

  // pp::PaintManager::Client:
  std::unique_ptr<Graphics> CreatePaintGraphics(const gfx::Size& size) override;
  bool BindPaintGraphics(Graphics& graphics) override;
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>* ready,
               std::vector<gfx::Rect>* pending) override;

  // pp::Printing_Dev:
  uint32_t QuerySupportedPrintOutputFormats() override;
  int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) override;
  pp::Resource PrintPages(const PP_PrintPageNumberRange_Dev* page_ranges,
                          uint32_t page_range_count) override;
  void PrintEnd() override;
  bool IsPrintScalingDisabled() override;

  // pp::Private:
  pp::Var GetLinkAtPosition(const pp::Point& point);
  void GetPrintPresetOptionsFromDocument(PP_PdfPrintPresetOptions_Dev* options);
  void EnableAccessibility();
  void SetCaretPosition(const pp::FloatPoint& position);
  void MoveRangeSelectionExtent(const pp::FloatPoint& extent);
  void SetSelectionBounds(const pp::FloatPoint& base,
                          const pp::FloatPoint& extent);
  bool CanEditText();
  bool HasEditableText();
  void ReplaceSelection(const std::string& text);
  bool CanUndo();
  bool CanRedo();
  void Undo();
  void Redo();
  void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data);
  int32_t PdfPrintBegin(const PP_PrintSettings_Dev* print_settings,
                        const PP_PdfPrintSettings_Dev* pdf_print_settings);

  void FlushCallback(int32_t result);

  // PdfViewPluginBase:
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  void Invalidate(const gfx::Rect& rect) override;
  void DidScroll(const gfx::Vector2d& offset) override;
  void ScrollToX(int x_in_screen_coords) override;
  void ScrollToY(int y_in_screen_coords, bool compensate_for_toolbar) override;
  void ScrollBy(const gfx::Vector2d& scroll_delta) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override;
  void NavigateToDestination(int page,
                             const float* x,
                             const float* y,
                             const float* zoom) override;
  void UpdateCursor(PP_CursorType_Dev cursor) override;
  void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
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
  std::vector<SearchStringResult> SearchString(const base::char16* string,
                                               const base::char16* term,
                                               bool case_sensitive) override;
  void DocumentLoadComplete(
      const PDFEngine::DocumentFeatures& document_features) override;
  void DocumentLoadFailed() override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() override;
  uint32_t GetBackgroundColor() override;
  void IsSelectingChanged(bool is_selecting) override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void EnteredEditMode() override;
  float GetToolbarHeightInScreenCoords() override;
  void DocumentFocusChanged(bool document_has_focus) override;

  // PreviewModeClient::Client:
  void PreviewDocumentLoadComplete() override;
  void PreviewDocumentLoadFailed() override;

  // Helper functions for implementing PPP_PDF.
  void RotateClockwise();
  void RotateCounterclockwise();

  // Creates a file name for saving a PDF file, given the source URL. Exposed
  // for testing.
  static std::string GetFileNameFromUrl(const std::string& url);

 protected:
  // PdfViewPluginBase:
  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override;
  std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() override;
  void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result) override;
  void DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                      int32_t result) override;

 private:
  // Message handlers.
  void HandleBackgroundColorChangedMessage(const pp::VarDictionary& dict);
  void HandleDisplayAnnotations(const pp::VarDictionary& dict);
  void HandleGetNamedDestinationMessage(const pp::VarDictionary& dict);
  void HandleGetPasswordCompleteMessage(const pp::VarDictionary& dict);
  void HandleGetSelectedTextMessage(const pp::VarDictionary& dict);
  void HandleGetThumbnailMessage(const pp::VarDictionary& dict);
  void HandleLoadPreviewPageMessage(const pp::VarDictionary& dict);
  void HandleResetPrintPreviewModeMessage(const pp::VarDictionary& dict);
  void HandleSaveAttachmentMessage(const pp::VarDictionary& dict);
  void HandleSaveMessage(const pp::VarDictionary& dict);
  void HandleSetTwoUpViewMessage(const pp::VarDictionary& dict);
  void HandleUpdateScrollMessage(const pp::VarDictionary& dict);
  void HandleViewportMessage(const pp::VarDictionary& dict);

  // Repaints plugin contents based on the current scroll position.
  void UpdateScroll();

  void ResetRecentlySentFindUpdate(int32_t);

  // Called whenever the plugin geometry changes to update the location of the
  // background parts, and notifies the pdf engine.
  void OnGeometryChanged(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Returns a VarArray of Attachments. Each Attachment is a VarDictionary
  // which contains the following key/values:
  // - "name" - a string Var.
  // - "size" - an int Var (-1 indicates the attachment file is too big to be
  // downloaded).
  // - "readable" a bool Var.
  pp::VarArray GetDocumentAttachments();

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  // Draws a rectangle with the specified dimensions and color in our buffer.
  void FillRect(const pp::Rect& rect, uint32_t color);

  bool CanSaveEdits() const;
  void SaveToFile(const std::string& token);
  void SaveToBuffer(const std::string& token);
  void ConsumeSaveToken(const std::string& token);

  void FormDidOpen(int32_t result);

  void UserMetricsRecordAction(const std::string& action);

  // Start loading accessibility information.
  void LoadAccessibility();

  // Send accessibility information about the given page index.
  void SendNextAccessibilityPage(int32_t page_index);

  // Send the accessibility information about the current viewport. This is
  // done once when accessibility is first loaded and again when the geometry
  // changes.
  void SendAccessibilityViewportInfo();

  enum DocumentLoadState {
    LOAD_STATE_LOADING,
    LOAD_STATE_COMPLETE,
    LOAD_STATE_FAILED,
  };

  // Must match SaveRequestType in chrome/browser/resources/pdf/constants.js.
  enum class SaveRequestType {
    kAnnotation = 0,
    kOriginal = 1,
    kEdited = 2,
  };

  // Set new zoom scale.
  void SetZoom(double scale);

  // Reduces the document to 1 page and appends |print_preview_page_count_| - 1
  // blank pages to the document for print preview.
  void AppendBlankPrintPreviewPages();

  // Process the preview page data information. |src_url| specifies the preview
  // page data location. The |src_url| is in the format:
  // chrome://print/id/page_number/print.pdf
  // |dest_page_index| specifies the blank page index that needs to be replaced
  // with the new page data.
  void ProcessPreviewPageInfo(const std::string& src_url, int dest_page_index);
  // Load the next available preview page into the blank page.
  void LoadAvailablePreviewPage();

  // Called after a preview page has loaded or failed to load.
  void LoadNextPreviewPage();

  // Send a notification that the print preview has loaded.
  void SendPrintPreviewLoadedNotification();

  // Send document metadata. (e.g. PDF title, attachments and bookmarks.)
  void SendDocumentMetadata();

  // Send the loading progress, where |percentage| represents the progress, or
  // -1 for loading error.
  void SendLoadingProgress(double percentage);

  // Sends the thumbnail image data.
  void SendThumbnail(const std::string& message_id, Thumbnail thumbnail);

  // Bound the given scroll offset to the document.
  pp::FloatPoint BoundScrollOffsetToDocument(
      const pp::FloatPoint& scroll_offset);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PdfHasAttachment {
    kNo = 0,
    kYes = 1,
    kMaxValue = kYes,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PdfIsTagged {
    kNo = 0,
    kYes = 1,
    kMaxValue = kYes,
  };

  // Add a sample to an enumerated histogram and filter out print preview usage.
  template <typename T>
  void HistogramEnumeration(const char* name, T sample);

  // Add a sample to an enumerated legacy histogram and filter out print preview
  // usage.
  template <typename T>
  void HistogramEnumeration(const char* name, T sample, T enum_size);

  // Add a sample to a custom counts histogram and filter out print preview
  // usage.
  void HistogramCustomCounts(const char* name,
                             int32_t sample,
                             int32_t min,
                             int32_t max,
                             uint32_t bucket_count);

  // Callback to print without re-entrancy issues.
  void OnPrint(int32_t /*unused_but_required*/);

  // Callback to do invalidation after painting finishes.
  void InvalidateAfterPaintDone(int32_t /*unused_but_required*/);

  // Helper for HandleInputEvent(). Returns whether engine() handled the event
  // or not.
  bool SendInputEventToEngine(const pp::InputEvent& event);

  pp::ImageData image_data_;
  SkBitmap skia_image_data_;  // Must be kept in sync with |image_data_|.

  // The current cursor.
  PP_CursorType_Dev cursor_ = PP_CURSORTYPE_POINTER;

  // Size, in pixels, of plugin rectangle.
  pp::Size plugin_size_;
  // Size, in DIPs, of plugin rectangle.
  pp::Size plugin_dip_size_;
  // Remaining area, in pixels, to render the pdf in after accounting for
  // horizontal centering.
  pp::Rect available_area_;
  // Size of entire document in pixels (i.e. if each page is 800 pixels high and
  // there are 10 pages, the height will be 8000).
  pp::Size document_size_;
  // Positional offset, in CSS pixels, of the plugin rectangle.
  pp::Point plugin_offset_;
  // The scroll offset in CSS pixels.
  pp::Point scroll_offset_;

  // Enumeration of pinch states.
  // This should match PinchPhase enum in
  // chrome/browser/resources/pdf/viewport.js
  enum PinchPhase {
    PINCH_NONE = 0,
    PINCH_START = 1,
    PINCH_UPDATE_ZOOM_OUT = 2,
    PINCH_UPDATE_ZOOM_IN = 3,
    PINCH_END = 4
  };

  // Current zoom factor.
  double zoom_ = 1.0;
  // True if we request a new bitmap rendering.
  bool needs_reraster_ = true;
  // The scroll position for the last raster, before any transformations are
  // applied.
  pp::FloatPoint scroll_offset_at_last_raster_;
  // True if last bitmap was smaller than screen.
  bool last_bitmap_smaller_ = false;
  // Current device scale factor. Multiply by |device_scale_| to convert from
  // viewport to screen coordinates. Divide by |device_scale_| to convert from
  // screen to viewport coordinates.
  float device_scale_ = 1.0f;
  // True if the plugin is full-page.
  bool full_ = false;

  PaintManager paint_manager_;

  // True if we haven't painted the plugin viewport yet.
  bool first_paint_ = true;
  // Whether OnPaint() is in progress or not.
  bool in_paint_ = false;
  // Deferred invalidates while |in_paint_| is true.
  std::vector<gfx::Rect> deferred_invalidates_;

  struct BackgroundPart {
    pp::Rect location;
    uint32_t color;
  };
  std::vector<BackgroundPart> background_parts_;

  struct PrintSettings {
    PrintSettings() { Clear(); }

    void Clear();

    // This is set to true when PdfPrintBegin() is called and false when
    // PrintEnd() is called.
    bool is_printing;

    // To know whether this was an actual print operation, so we don't double
    // count UMA logging.
    bool print_pages_called;

    // Generic print settings.
    PP_PrintSettings_Dev pepper_print_settings;

    // PDF-specific print settings.
    PP_PdfPrintSettings_Dev pdf_print_settings;
  };

  PrintSettings print_settings_;

  // The PreviewModeClient used for print preview. Will be passed to
  // |preview_engine_|.
  std::unique_ptr<PreviewModeClient> preview_client_;

  // This engine is used to render the individual preview page data. This is
  // used only in print preview mode. This will use |PreviewModeClient|
  // interface which has very limited access to the pp::Instance.
  std::unique_ptr<PDFiumEngine> preview_engine_;

  std::string url_;

  // Used for submitting forms.
  std::unique_ptr<UrlLoader> form_loader_;

  // The callback for receiving the password from the page.
  base::OnceCallback<void(const std::string&)> password_callback_;

  DocumentLoadState document_load_state_ = LOAD_STATE_LOADING;
  DocumentLoadState preview_document_load_state_ = LOAD_STATE_COMPLETE;

  // Used so that we only tell the browser once about an unsupported feature, to
  // avoid the infobar going up more than once.
  bool told_browser_about_unsupported_feature_ = false;

  // Keeps track of which unsupported features we reported, so we avoid spamming
  // the stats if a feature shows up many times per document.
  std::set<std::string> unsupported_features_reported_;

  // True if the plugin is loaded in print preview, otherwise false.
  bool is_print_preview_ = false;

  // Number of pages in print preview mode for non-PDF source, 0 if print
  // previewing a PDF, and -1 if not in print preview mode.
  int print_preview_page_count_ = -1;

  // Number of pages loaded in print preview mode for non-PDF source. Always
  // less than or equal to |print_preview_page_count_|.
  int print_preview_loaded_page_count_ = -1;

  // Used to manage loaded print preview page information. A |PreviewPageInfo|
  // consists of data source URL string and the page index in the destination
  // document.
  // The URL string embeds a page number that can be found with
  // ExtractPrintPreviewPageIndex(). This page number is always greater than 0.
  // The page index is always in the range of [0, print_preview_page_count_).
  using PreviewPageInfo = std::pair<std::string, int>;
  base::queue<PreviewPageInfo> preview_pages_info_;

  // Used to signal the browser about focus changes to trigger the OSK.
  // TODO(abodenha@chromium.org) Implement full IME support in the plugin.
  // http://crbug.com/132565
  std::unique_ptr<pp::TextInput_Dev> text_input_;

  // The last document load progress value sent to the web page.
  double last_progress_sent_ = 0.0;

  // Whether an update to the number of find results found was sent less than
  // |kFindResultCooldownMs| milliseconds ago.
  bool recently_sent_find_update_ = false;

  // The tickmarks.
  std::vector<pp::Rect> tickmarks_;

  // Whether the plugin has received a viewport changed message. Nothing should
  // be painted until this is received.
  bool received_viewport_message_ = false;

  // If true, this means we told the RenderView that we're starting a network
  // request so that it can start the throbber. We will tell it again once the
  // document finishes loading.
  bool did_call_start_loading_ = false;

  // If this is true, then don't scroll the plugin in response to DidChangeView
  // messages. This will be true when the extension page is in the process of
  // zooming the plugin so that flickering doesn't occur while zooming.
  bool stop_scrolling_ = false;

  // The background color of the PDF viewer.
  uint32_t background_color_ = 0;

  // The blank space above the first page of the document reserved for the
  // toolbar.
  int top_toolbar_height_in_viewport_coords_ = 0;

  bool edit_mode_ = false;

  // The current state of accessibility: either off, enabled but waiting
  // for the document to load, or fully loaded.
  enum AccessibilityState {
    ACCESSIBILITY_STATE_OFF,
    ACCESSIBILITY_STATE_PENDING,  // Enabled but waiting for doc to load.
    ACCESSIBILITY_STATE_LOADED
  } accessibility_state_ = ACCESSIBILITY_STATE_OFF;

  base::WeakPtrFactory<OutOfProcessInstance> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_OUT_OF_PROCESS_INSTANCE_H_
