// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "pdf/document_loader.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_range.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_progressive.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

class PDFiumDocument;
class ShadowMatrix;

class PDFiumEngine : public PDFEngine,
                     public DocumentLoader::Client,
                     public IFSDK_PAUSE {
 public:
  PDFiumEngine(PDFEngine::Client* client, bool enable_javascript);
  ~PDFiumEngine() override;

  using CreateDocumentLoaderFunction =
      std::unique_ptr<DocumentLoader> (*)(DocumentLoader::Client* client);
  static void SetCreateDocumentLoaderFunctionForTesting(
      CreateDocumentLoaderFunction function);

  // PDFEngine implementation.
  bool New(const char* url, const char* headers) override;
  void PageOffsetUpdated(const pp::Point& page_offset) override;
  void PluginSizeUpdated(const pp::Size& size) override;
  void ScrolledToXPosition(int position) override;
  void ScrolledToYPosition(int position) override;
  void PrePaint() override;
  void Paint(const pp::Rect& rect,
             pp::ImageData* image_data,
             std::vector<pp::Rect>* ready,
             std::vector<pp::Rect>* pending) override;
  void PostPaint() override;
  bool HandleDocumentLoad(const pp::URLLoader& loader) override;
  bool HandleEvent(const pp::InputEvent& event) override;
  uint32_t QuerySupportedPrintOutputFormats() override;
  void PrintBegin() override;
  pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings) override;
  void PrintEnd() override;
  void StartFind(const std::string& text, bool case_sensitive) override;
  bool SelectFindResult(bool forward) override;
  void StopFind() override;
  void ZoomUpdated(double new_zoom_level) override;
  void RotateClockwise() override;
  void RotateCounterclockwise() override;
  std::string GetSelectedText() override;
  bool CanEditText() override;
  bool HasEditableText() override;
  void ReplaceSelection(const std::string& text) override;
  bool CanUndo() override;
  bool CanRedo() override;
  void Undo() override;
  void Redo() override;
  std::string GetLinkAtPosition(const pp::Point& point) override;
  bool HasPermission(DocumentPermission permission) const override;
  void SelectAll() override;
  int GetNumberOfPages() override;
  pp::VarArray GetBookmarks() override;
  base::Optional<PDFEngine::NamedDestination> GetNamedDestination(
      const std::string& destination) override;
  gfx::PointF TransformPagePoint(int page_index,
                                 const gfx::PointF& page_xy) override;
  int GetMostVisiblePage() override;
  pp::Rect GetPageRect(int index) override;
  pp::Rect GetPageBoundsRect(int index) override;
  pp::Rect GetPageContentsRect(int index) override;
  pp::Rect GetPageScreenRect(int page_index) const override;
  int GetVerticalScrollbarYPosition() override;
  void SetGrayscale(bool grayscale) override;
  int GetCharCount(int page_index) override;
  pp::FloatRect GetCharBounds(int page_index, int char_index) override;
  uint32_t GetCharUnicode(int page_index, int char_index) override;
  void GetTextRunInfo(int page_index,
                      int start_char_index,
                      uint32_t* out_len,
                      double* out_font_size,
                      pp::FloatRect* out_bounds) override;
  bool GetPrintScaling() override;
  int GetCopiesToPrint() override;
  int GetDuplexType() override;
  bool GetPageSizeAndUniformity(pp::Size* size) override;
  void AppendBlankPages(int num_pages) override;
  void AppendPage(PDFEngine* engine, int index) override;
  std::string GetMetadata(const std::string& key) override;
  std::vector<uint8_t> GetSaveData() override;
  void SetCaretPosition(const pp::Point& position) override;
  void MoveRangeSelectionExtent(const pp::Point& extent) override;
  void SetSelectionBounds(const pp::Point& base,
                          const pp::Point& extent) override;
  void GetSelection(uint32_t* selection_start_page_index,
                    uint32_t* selection_start_char_index,
                    uint32_t* selection_end_page_index,
                    uint32_t* selection_end_char_index) override;

  // DocumentLoader::Client implementation.
  pp::Instance* GetPluginInstance() override;
  std::unique_ptr<URLLoaderWrapper> CreateURLLoader() override;
  void OnPendingRequestComplete() override;
  void OnNewDataReceived() override;
  void OnDocumentComplete() override;
  void OnDocumentCanceled() override;
  void CancelBrowserDownload() override;
  void KillFormFocus() override;

#if defined(PDF_ENABLE_XFA)
  void UpdatePageCount();
#endif  // defined(PDF_ENABLE_XFA)

  void UnsupportedFeature(const std::string& feature);
  void FontSubstituted();

  FPDF_AVAIL fpdf_availability() const;
  FPDF_DOCUMENT doc() const;
  FPDF_FORMHANDLE form() const;

 private:
  // This helper class is used to detect the difference in selection between
  // construction and destruction.  At destruction, it invalidates all the
  // parts that are newly selected, along with all the parts that used to be
  // selected but are not anymore.
  class SelectionChangeInvalidator {
   public:
    explicit SelectionChangeInvalidator(PDFiumEngine* engine);
    ~SelectionChangeInvalidator();

   private:
    // Returns all the currently visible selection rectangles, in screen
    // coordinates.
    std::vector<pp::Rect> GetVisibleSelections() const;

    // Invalidates |selection|, but with |selection| slightly expanded to
    // compensate for any rounding errors.
    void Invalidate(const pp::Rect& selection);

    PDFiumEngine* const engine_;
    // The origin at the time this object was constructed.
    const pp::Point previous_origin_;
    // Screen rectangles that were selected on construction.
    std::vector<pp::Rect> old_selections_;
  };

  // Used to store mouse down state to handle it in other mouse event handlers.
  class MouseDownState {
   public:
    MouseDownState(const PDFiumPage::Area& area,
                   const PDFiumPage::LinkTarget& target);
    ~MouseDownState();

    void Set(const PDFiumPage::Area& area,
             const PDFiumPage::LinkTarget& target);
    void Reset();
    bool Matches(const PDFiumPage::Area& area,
                 const PDFiumPage::LinkTarget& target) const;

   private:
    PDFiumPage::Area area_;
    PDFiumPage::LinkTarget target_;

    DISALLOW_COPY_AND_ASSIGN(MouseDownState);
  };

  friend class PDFiumFormFiller;
  friend class SelectionChangeInvalidator;

  // We finished getting the pdf file, so load it. This will complete
  // asynchronously (due to password fetching) and may be run multiple times.
  void LoadDocument();

  // Try loading the document. Returns true if the document is successfully
  // loaded or is already loaded otherwise it will return false. If there is a
  // password, then |password| is non-empty. If the document could not be loaded
  // and needs a password, |needs_password| will be set to true.
  bool TryLoadingDoc(const std::string& password, bool* needs_password);

  // Asks the user for the document password and then continue loading the
  // document.
  void GetPasswordAndLoad();

  // Called when the password has been retrieved.
  void OnGetPasswordComplete(int32_t result, const pp::Var& password);

  // Continues loading the document when the password has been retrieved, or if
  // there is no password. If there is no password, then |password| is empty.
  void ContinueLoadingDocument(const std::string& password);

  // Finishes loading the document. Recalculate the document size if there were
  // pages that were not previously available.
  // Also notifies the client that the document has been loaded.
  // This should only be called after |doc_| has been loaded and the document is
  // fully downloaded.
  // If this has been run once, it will not notify the client again.
  void FinishLoadingDocument();

  // Loads information about the pages in the document and calculate the
  // document size.
  void LoadPageInfo(bool reload);

  void LoadBody();

  void LoadPages();

  void LoadForm();

  // Calculates which pages should be displayed right now.
  void CalculateVisiblePages();

  // Returns true iff the given page index is visible.  CalculateVisiblePages
  // must have been called first.
  bool IsPageVisible(int index) const;

  // Internal interface that caches the page index requested by PDFium to get
  // scrolled to. The cache is to be be used during the interval the PDF
  // plugin has not finished handling the scroll request.
  void ScrollToPage(int page);

  // Checks if a page is now available, and if so marks it as such and returns
  // true.  Otherwise, it will return false and will add the index to the given
  // array if it's not already there.
  bool CheckPageAvailable(int index, std::vector<int>* pending);

  // Helper function to get a given page's size in pixels.  This is not part of
  // PDFiumPage because we might not have that structure when we need this.
  pp::Size GetPageSize(int index);

  void GetAllScreenRectsUnion(std::vector<PDFiumRange>* rect_range,
                              const pp::Point& offset_point,
                              std::vector<pp::Rect>* rect_vector);

  void UpdateTickMarks();

  // Called to continue searching so we don't block the main thread.
  void ContinueFind(int32_t result);

  // Inserts a find result into |find_results_|, which is sorted.
  void AddFindResult(const PDFiumRange& result);

  // Search a page using PDFium's methods.  Doesn't work with unicode.  This
  // function is just kept arount in case PDFium code is fixed.
  void SearchUsingPDFium(const base::string16& term,
                         bool case_sensitive,
                         bool first_search,
                         int character_to_start_searching_from,
                         int current_page);

  // Search a page ourself using ICU.
  void SearchUsingICU(const base::string16& term,
                      bool case_sensitive,
                      bool first_search,
                      int character_to_start_searching_from,
                      int current_page);

  // Input event handlers.
  bool OnMouseDown(const pp::MouseInputEvent& event);
  bool OnMouseUp(const pp::MouseInputEvent& event);
  bool OnMouseMove(const pp::MouseInputEvent& event);
  void OnMouseEnter(const pp::MouseInputEvent& event);
  bool OnKeyDown(const pp::KeyboardInputEvent& event);
  bool OnKeyUp(const pp::KeyboardInputEvent& event);
  bool OnChar(const pp::KeyboardInputEvent& event);

  // Decide what cursor should be displayed.
  PP_CursorType_Dev DetermineCursorType(PDFiumPage::Area area,
                                        int form_type) const;

  bool ExtendSelection(int page_index, int char_index);

  pp::Buffer_Dev PrintPagesAsRasterPdf(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings);

  pp::Buffer_Dev PrintPagesAsPdf(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings);

  pp::Buffer_Dev ConvertPdfToBufferDev(const std::vector<uint8_t>& pdf_data);

  // Checks if |page| has selected text in a form element. If so, sets that as
  // the plugin's text selection.
  void SetFormSelectedText(FPDF_FORMHANDLE form_handle, FPDF_PAGE page);

  // Given |point|, returns which page and character location it's closest to,
  // as well as extra information about objects at that point.
  PDFiumPage::Area GetCharIndex(const pp::Point& point,
                                int* page_index,
                                int* char_index,
                                int* form_type,
                                PDFiumPage::LinkTarget* target);

  void OnSingleClick(int page_index, int char_index);
  void OnMultipleClick(int click_count, int page_index, int char_index);
  bool OnLeftMouseDown(const pp::MouseInputEvent& event);
  bool OnMiddleMouseDown(const pp::MouseInputEvent& event);
  bool OnRightMouseDown(const pp::MouseInputEvent& event);

  // Starts a progressive paint operation given a rectangle in screen
  // coordinates. Returns the index in progressive_rects_.
  int StartPaint(int page_index, const pp::Rect& dirty);

  // Continues a paint operation that was started earlier.  Returns true if the
  // paint is done, or false if it needs to be continued.
  bool ContinuePaint(int progressive_index, pp::ImageData* image_data);

  // Called once PDFium is finished rendering a page so that we draw our
  // borders, highlighting etc.
  void FinishPaint(int progressive_index, pp::ImageData* image_data);

  // Stops any paints that are in progress.
  void CancelPaints();

  // Invalidates all pages. Use this when some global parameter, such as page
  // orientation, has changed.
  void InvalidateAllPages();

  // If the page is narrower than the document size, paint the extra space
  // with the page background.
  void FillPageSides(int progressive_index);

  void PaintPageShadow(int progressive_index, pp::ImageData* image_data);

  // Highlight visible find results and selections.
  void DrawSelections(int progressive_index, pp::ImageData* image_data);

  // Paints an page that hasn't finished downloading.
  void PaintUnavailablePage(int page_index,
                            const pp::Rect& dirty,
                            pp::ImageData* image_data);

  // Given a page index, returns the corresponding index in progressive_rects_,
  // or -1 if it doesn't exist.
  int GetProgressiveIndex(int page_index) const;

  // Creates a FPDF_BITMAP from a rectangle in screen coordinates.
  ScopedFPDFBitmap CreateBitmap(const pp::Rect& rect,
                                pp::ImageData* image_data) const;

  // Given a rectangle in screen coordinates, returns the coordinates in the
  // units that PDFium rendering functions expect.
  void GetPDFiumRect(int page_index,
                     const pp::Rect& rect,
                     int* start_x,
                     int* start_y,
                     int* size_x,
                     int* size_y) const;

  // Returns the rendering flags to pass to PDFium.
  int GetRenderingFlags() const;

  // Returns the currently visible rectangle in document coordinates.
  pp::Rect GetVisibleRect() const;

  // Given a rectangle in document coordinates, returns the rectange into screen
  // coordinates (i.e. 0,0 is top left corner of plugin area).  If it's not
  // visible, an empty rectangle is returned.
  pp::Rect GetScreenRect(const pp::Rect& rect) const;

  // Given an image |buffer| with |stride|, highlights |rect|.
  // |highlighted_rects| contains the already highlighted rectangles and will be
  // updated to include |rect| if |rect| has not already been highlighted.
  void Highlight(void* buffer,
                 int stride,
                 const pp::Rect& rect,
                 std::vector<pp::Rect>* highlighted_rects);

  // Helper function to convert a device to page coordinates.  If the page is
  // not yet loaded, |page_x| and |page_y| will be set to 0.
  void DeviceToPage(int page_index,
                    const pp::Point& device_point,
                    double* page_x,
                    double* page_y);

  // Helper function to get the index of a given FPDF_PAGE.  Returns -1 if not
  // found.
  int GetVisiblePageIndex(FPDF_PAGE page);

  // Helper function to change the current page, running page open/close
  // triggers as necessary.
  void SetCurrentPage(int index);

  void DrawPageShadow(const pp::Rect& page_rect,
                      const pp::Rect& shadow_rect,
                      const pp::Rect& clip_rect,
                      pp::ImageData* image_data);

  void GetRegion(const pp::Point& location,
                 pp::ImageData* image_data,
                 void** region,
                 int* stride) const;

  // Called when the selection changes.
  void OnSelectionTextChanged();
  void OnSelectionPositionChanged();

  // Common code shared by RotateClockwise() and RotateCounterclockwise().
  void RotateInternal();

  // Sets text selection status of document. This does not include text
  // within form text fields.
  void SetSelecting(bool selecting);

  // Sets whether or not focus is in form text field or form combobox text
  // field.
  void SetInFormTextArea(bool in_form_text_area);

  // Sets whether or not left mouse button is currently being held down.
  void SetMouseLeftButtonDown(bool is_mouse_left_button_down);

  // Given coordinates on |page| has a form of |form_type| which is known to be
  // a form text area, check if it is an editable form text area.
  bool IsPointInEditableFormTextArea(FPDF_PAGE page,
                                     double page_x,
                                     double page_y,
                                     int form_type);

  bool PageIndexInBounds(int index) const;

  // Gets the height of the top toolbar in screen coordinates. This is
  // independent of whether it is hidden or not at the moment.
  float GetToolbarHeightInScreenCoords();

  void ScheduleTouchTimer(const pp::TouchInputEvent& event);
  void KillTouchTimer();
  void HandleLongPress(const pp::TouchInputEvent& event);

  // Returns a VarDictionary (representing a bookmark), which in turn contains
  // child VarDictionaries (representing the child bookmarks).
  // If nullptr is passed in as the bookmark then we traverse from the "root".
  // Note that the "root" bookmark contains no useful information.
  pp::VarDictionary TraverseBookmarks(FPDF_BOOKMARK bookmark,
                                      unsigned int depth);

  // Set if the document has any local edits.
  void SetEditMode(bool edit_mode);

  // IFSDK_PAUSE callbacks
  static FPDF_BOOL Pause_NeedToPauseNow(IFSDK_PAUSE* param);

  PDFEngine::Client* const client_;
  pp::Size document_size_;  // Size of document in pixels.

  // The scroll position in screen coordinates.
  pp::Point position_;
  // The offset of the page into the viewport.
  pp::Point page_offset_;
  // The plugin size in screen coordinates.
  pp::Size plugin_size_;
  double current_zoom_ = 1.0;
  unsigned int current_rotation_ = 0;

  std::unique_ptr<DocumentLoader> doc_loader_;  // Main document's loader.
  std::string url_;
  std::string headers_;
  pp::CompletionCallbackFactory<PDFiumEngine> find_factory_;
  pp::CompletionCallbackFactory<PDFiumEngine> password_factory_;

  // Set to true if the user is being prompted for their password. Will be set
  // to false after the user finishes getting their password.
  bool getting_password_ = false;
  int password_tries_remaining_ = 0;

  // Needs to be above pages_, as destroying a page may call some methods of
  // form filler.
  PDFiumFormFiller form_filler_;

  std::unique_ptr<PDFiumDocument> document_;

  // The page(s) of the document.
  std::vector<std::unique_ptr<PDFiumPage>> pages_;

  // The indexes of the pages currently visible.
  std::vector<int> visible_pages_;

  // The indexes of the pages pending download.
  std::vector<int> pending_pages_;

  // During handling of input events we don't want to unload any pages in
  // callbacks to us from PDFium, since the current page can change while PDFium
  // code still has a pointer to it.
  bool defer_page_unload_ = false;
  std::vector<int> deferred_page_unloads_;

  // Used for text selection, but does not include text within form text areas.
  // There could be more than one range if selection spans more than one page.
  std::vector<PDFiumRange> selection_;

  // True if we're in the middle of text selection.
  bool selecting_ = false;

  MouseDownState mouse_down_state_;

  // Text selection within form text fields and form combobox text fields.
  std::string selected_form_text_;

  // True if focus is in form text field or form combobox text field.
  bool in_form_text_area_ = false;

  // True if the form text area currently in focus is not read only, and is a
  // form text field or user-editable form combobox text field.
  bool editable_form_text_area_ = false;

  // True if left mouse button is currently being held down.
  bool mouse_left_button_down_ = false;

  // True if middle mouse button is currently being held down.
  bool mouse_middle_button_down_ = false;

  // Last known position while performing middle mouse button pan.
  pp::Point mouse_middle_button_last_position_;

  // The current text used for searching.
  std::string current_find_text_;
  // The results found.
  std::vector<PDFiumRange> find_results_;
  // Whether a search is in progress.
  bool search_in_progress_ = false;
  // Which page to search next.
  int next_page_to_search_ = -1;
  // Where to stop searching.
  int last_page_to_search_ = -1;
  int last_character_index_to_search_ = -1;  // -1 if search until end of page.
  // Which result the user has currently selected. (0-based)
  base::Optional<size_t> current_find_index_;
  // Where to resume searching. (0-based)
  base::Optional<size_t> resume_find_index_;

  // Permissions bitfield.
  unsigned long permissions_ = 0;

  // Permissions security handler revision number. -1 for unknown.
  int permissions_handler_revision_ = -1;

  pp::Size default_page_size_;

  // Timer for touch long press detection.
  base::OneShotTimer touch_timer_;

  // Holds the zero-based page index of the last page that the mouse clicked on.
  int last_page_mouse_down_ = -1;

  // Holds the zero-based page index of the most visible page; refreshed by
  // calling CalculateVisiblePages()
  int most_visible_page_ = -1;

  // Holds the page index requested by PDFium while the scroll operation
  // is being handled (asynchronously).
  base::Optional<int> in_flight_visible_page_;

  // Set to true after FORM_DoDocumentJSAction/FORM_DoDocumentOpenAction have
  // been called. Only after that can we call FORM_DoPageAAction.
  bool called_do_document_action_ = false;

  // Records parts of form fields that need to be highlighted at next paint, in
  // screen coordinates.
  std::vector<pp::Rect> form_highlights_;

  // Whether to render in grayscale or in color.
  bool render_grayscale_ = false;

  // Whether to render PDF annotations.
  bool render_annots_ = true;

  // The link currently under the cursor.
  std::string link_under_cursor_;

  // Pending progressive paints.
  class ProgressivePaint {
   public:
    ProgressivePaint(int page_index, const pp::Rect& rect);
    ProgressivePaint(ProgressivePaint&& that);
    ~ProgressivePaint();

    ProgressivePaint& operator=(ProgressivePaint&& that);

    int page_index() const { return page_index_; }
    const pp::Rect& rect() const { return rect_; }
    FPDF_BITMAP bitmap() const { return bitmap_.get(); }
    bool painted() const { return painted_; }

    void set_painted(bool enable) { painted_ = enable; }
    void SetBitmapAndImageData(ScopedFPDFBitmap bitmap,
                               pp::ImageData image_data);

   private:
    int page_index_;
    pp::Rect rect_;             // In screen coordinates.
    pp::ImageData image_data_;  // Maintains reference while |bitmap_| exists.
    ScopedFPDFBitmap bitmap_;   // Must come after |image_data_|.
    // Temporary used to figure out if in a series of Paint() calls whether this
    // pending paint was updated or not.
    bool painted_ = false;

    DISALLOW_COPY_AND_ASSIGN(ProgressivePaint);
  };
  std::vector<ProgressivePaint> progressive_paints_;

  // Keeps track of when we started the last progressive paint, so that in our
  // callback we can determine if we need to pause.
  base::Time last_progressive_start_time_;

  // The timeout to use for the current progressive paint.
  base::TimeDelta progressive_paint_timeout_;

  // Shadow matrix for generating the page shadow bitmap.
  std::unique_ptr<ShadowMatrix> page_shadow_;

  // While true, the document try to be opened and parsed after download each
  // part. Else the document will be opened and parsed only on finish of
  // downloading.
  bool process_when_pending_request_complete_ = true;

  enum class RangeSelectionDirection { Left, Right };
  RangeSelectionDirection range_selection_direction_ =
      RangeSelectionDirection::Right;

  pp::Point range_selection_base_;

  bool edit_mode_ = false;

  PDFiumPrint print_;

  DISALLOW_COPY_AND_ASSIGN(PDFiumEngine);
};

// Create a local variable of this when calling PDFium functions which can call
// our global callback when a substitute font is mapped.
class ScopedSubstFont {
 public:
  explicit ScopedSubstFont(PDFiumEngine* engine);
  ~ScopedSubstFont();

 private:
  PDFiumEngine* const old_engine_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSubstFont);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_H_
