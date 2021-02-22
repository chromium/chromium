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
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "pdf/pdf_view_plugin_base.h"
#include "pdf/preview_mode_client.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/find_private.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Size;
class Vector2d;
}  // namespace gfx

namespace pp {
class Size;
class TextInput_Dev;
class VarArray;
class VarDictionary;
}  // namespace pp

namespace chrome_pdf {

class Graphics;
class PDFiumEngine;
class Thumbnail;
class UrlLoader;

class OutOfProcessInstance : public PdfViewPluginBase,
                             public pp::Instance,
                             public pp::Find_Private,
                             public pp::Printing_Dev,
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
  int32_t PdfPrintBegin(const PP_PrintSettings_Dev* print_settings,
                        const PP_PdfPrintSettings_Dev* pdf_print_settings);

  void FlushCallback(int32_t result);

  // PdfViewPluginBase:
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  void DidScroll(const gfx::Vector2d& offset) override;
  void ScrollToX(int x_in_screen_coords) override;
  void ScrollToY(int y_in_screen_coords) override;
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
  void DocumentLoadComplete() override;
  void DocumentLoadFailed() override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() override;
  void IsSelectingChanged(bool is_selecting) override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void EnteredEditMode() override;
  void DocumentFocusChanged(bool document_has_focus) override;
  void SetSelectedText(const std::string& selected_text) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;
  bool IsValidLink(const std::string& url) override;
  std::unique_ptr<Graphics> CreatePaintGraphics(const gfx::Size& size) override;
  bool BindPaintGraphics(Graphics& graphics) override;
  void ScheduleTaskOnMainThread(
      base::TimeDelta delay,
      ResultCallback callback,
      int32_t result,
      const base::Location& from_here = base::Location::Current()) override;

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
  void SendMessage(base::Value message) override;
  void InitImageData(const gfx::Size& size) override;
  Image GetPluginImageData() const override;
  void SetAccessibilityDocInfo(const AccessibilityDocInfo& doc_info) override;
  void SetAccessibilityPageInfo(AccessibilityPageInfo page_info,
                                std::vector<AccessibilityTextRunInfo> text_runs,
                                std::vector<AccessibilityCharInfo> chars,
                                AccessibilityPageObjects page_objects) override;
  void SetAccessibilityViewportInfo(
      const AccessibilityViewportInfo& viewport_info) override;

 private:
  // Message handlers.
  void HandleGetPasswordCompleteMessage(const pp::VarDictionary& dict);
  void HandleGetThumbnailMessage(const pp::VarDictionary& dict);
  void HandleLoadPreviewPageMessage(const pp::VarDictionary& dict);
  void HandleResetPrintPreviewModeMessage(const pp::VarDictionary& dict);
  void HandleSaveAttachmentMessage(const pp::VarDictionary& dict);
  void HandleSaveMessage(const pp::VarDictionary& dict);
  void HandleUpdateScrollMessage(const pp::VarDictionary& dict);

  // Repaints plugin contents based on the current scroll position.
  void UpdateScroll();

  void ResetRecentlySentFindUpdate(int32_t);

  // Returns a VarArray of Attachments. Each Attachment is a VarDictionary
  // which contains the following key/values:
  // - "name" - a string Var.
  // - "size" - an int Var (-1 indicates the attachment file is too big to be
  // downloaded).
  // - "readable" a bool Var.
  pp::VarArray GetDocumentAttachments();

  bool CanSaveEdits() const;
  void SaveToFile(const std::string& token);
  void SaveToBuffer(const std::string& token);
  void ConsumeSaveToken(const std::string& token);

  void FormDidOpen(int32_t result);

  void RecordDocumentMetrics();
  void UserMetricsRecordAction(const std::string& action);

  // Must match SaveRequestType in chrome/browser/resources/pdf/constants.js.
  enum class SaveRequestType {
    kAnnotation = 0,
    kOriginal = 1,
    kEdited = 2,
  };

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

  // Send attachments.
  void SendAttachments();

  // Send bookmarks.
  void SendBookmarks();

  // Send document metadata.
  void SendMetadata();

  // Send the loading progress, where |percentage| represents the progress, or
  // -1 for loading error.
  void SendLoadingProgress(double percentage);

  // Sends the thumbnail image data.
  void SendThumbnail(const std::string& message_id, Thumbnail thumbnail);

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

  // Add a sample to a custom counts histogram and filter out print preview
  // usage.
  void HistogramCustomCounts(const char* name,
                             int32_t sample,
                             int32_t min,
                             int32_t max,
                             uint32_t bucket_count);

  // Callback to print without re-entrancy issues.
  void OnPrint(int32_t /*unused_but_required*/);

  // Helper for HandleInputEvent(). Returns whether engine() handled the event
  // or not.
  bool SendInputEventToEngine(const pp::InputEvent& event);

  // The Pepper image data that is in sync with image_data().
  pp::ImageData pepper_image_data_;

  // The current cursor.
  PP_CursorType_Dev cursor_ = PP_CURSORTYPE_POINTER;

  // The scroll position in CSS pixels.
  gfx::Point scroll_position_;

  // True if the plugin is full-page.
  bool full_ = false;

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

  DocumentLoadState preview_document_load_state_ = DocumentLoadState::kComplete;

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

  // If true, this means we told the RenderView that we're starting a network
  // request so that it can start the throbber. We will tell it again once the
  // document finishes loading.
  bool did_call_start_loading_ = false;

  bool edit_mode_ = false;

  base::WeakPtrFactory<OutOfProcessInstance> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_OUT_OF_PROCESS_INSTANCE_H_
