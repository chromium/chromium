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

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_loader.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_range.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/var_array.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_progressive.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

class KeyboardInputEvent;
class MouseInputEvent;
class PDFiumDocument;
class PDFiumPermissions;
class TouchInputEvent;

namespace draw_utils {
class ShadowMatrix;
struct PageInsetSizes;
}  // namespace draw_utils

class PDFiumEngine : public PDFEngine,
                     public DocumentLoader::Client,
                     public IFSDK_PAUSE {
 public:
  // State transition when tabbing forward:
  // None -> Document -> Page -> None (when focusable annotations on all pages
  // are done).
  // Exposed for testing.
  enum class FocusElementType { kNone, kDocument, kPage };

  // NOTE: |script_option| is ignored when PDF_ENABLE_V8 is not defined.
  PDFiumEngine(PDFEngine::Client* client,
               PDFiumFormFiller::ScriptOption script_option);
  PDFiumEngine(const PDFiumEngine&) = delete;
  PDFiumEngine& operator=(const PDFiumEngine&) = delete;
  ~PDFiumEngine() override;

  // Replaces the normal DocumentLoader for testing. Must be called before
  // HandleDocumentLoad().
  void SetDocumentLoaderForTesting(std::unique_ptr<DocumentLoader> loader);

  using SetSelectedTextFunction = void (*)(pp::Instance* instance,
                                           const std::string& selected_text);
  static void OverrideSetSelectedTextFunctionForTesting(
      SetSelectedTextFunction function);

  using SetLinkUnderCursorFunction =
      void (*)(pp::Instance* instance, const std::string& link_under_cursor);
  static void OverrideSetLinkUnderCursorFunctionForTesting(
      SetLinkUnderCursorFunction function);

  // PDFEngine:
  bool New(const char* url, const char* headers) override;
  void PageOffsetUpdated(const gfx::Vector2d& page_offset) override;
  void PluginSizeUpdated(const gfx::Size& size) override;
  void ScrolledToXPosition(int position) override;
  void ScrolledToYPosition(int position) override;
  void PrePaint() override;
  void Paint(const gfx::Rect& rect,
             SkBitmap& image_data,
             std::vector<gfx::Rect>& ready,
             std::vector<gfx::Rect>& pending) override;
  void PostPaint() override;
  bool HandleDocumentLoad(std::unique_ptr<UrlLoader> loader) override;
  bool HandleEvent(const InputEvent& event) override;
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
  void SetTwoUpView(bool enable) override;
  void DisplayAnnotations(bool display) override;
  gfx::Size ApplyDocumentLayout(
      const DocumentLayout::Options& options) override;
  std::string GetSelectedText() override;
  bool CanEditText() override;
  bool HasEditableText() override;
  void ReplaceSelection(const std::string& text) override;
  bool CanUndo() override;
  bool CanRedo() override;
  void Undo() override;
  void Redo() override;
  void HandleAccessibilityAction(
      const PP_PdfAccessibilityActionData& action_data) override;
  std::string GetLinkAtPosition(const gfx::Point& point) override;
  bool HasPermission(DocumentPermission permission) const override;
  void SelectAll() override;
  const std::vector<DocumentAttachmentInfo>& GetDocumentAttachmentInfoList()
      const override;
  std::vector<uint8_t> GetAttachmentData(size_t index) override;
  const DocumentMetadata& GetDocumentMetadata() const override;
  int GetNumberOfPages() const override;
  pp::VarArray GetBookmarks() override;
  base::Optional<PDFEngine::NamedDestination> GetNamedDestination(
      const std::string& destination) override;
  int GetMostVisiblePage() override;
  gfx::Rect GetPageBoundsRect(int index) override;
  gfx::Rect GetPageContentsRect(int index) override;
  gfx::Rect GetPageScreenRect(int page_index) const override;
  int GetVerticalScrollbarYPosition() override;
  void SetGrayscale(bool grayscale) override;
  int GetCharCount(int page_index) override;
  gfx::RectF GetCharBounds(int page_index, int char_index) override;
  uint32_t GetCharUnicode(int page_index, int char_index) override;
  base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo> GetTextRunInfo(
      int page_index,
      int start_char_index) override;
  std::vector<AccessibilityLinkInfo> GetLinkInfo(int page_index) override;
  std::vector<AccessibilityImageInfo> GetImageInfo(int page_index) override;
  std::vector<AccessibilityHighlightInfo> GetHighlightInfo(
      int page_index) override;
  std::vector<AccessibilityTextFieldInfo> GetTextFieldInfo(
      int page_index) override;
  bool GetPrintScaling() override;
  int GetCopiesToPrint() override;
  int GetDuplexType() override;
  bool GetPageSizeAndUniformity(gfx::Size* size) override;
  void AppendBlankPages(size_t num_pages) override;
  void AppendPage(PDFEngine* engine, int index) override;
  std::vector<uint8_t> GetSaveData() override;
  void SetCaretPosition(const gfx::Point& position) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void SetSelectionBounds(const gfx::Point& base,
                          const gfx::Point& extent) override;
  void GetSelection(uint32_t* selection_start_page_index,
                    uint32_t* selection_start_char_index,
                    uint32_t* selection_end_page_index,
                    uint32_t* selection_end_char_index) override;
  void KillFormFocus() override;
  void UpdateFocus(bool has_focus) override;
  PP_PrivateAccessibilityFocusInfo GetFocusInfo() override;
  uint32_t GetLoadedByteSize() override;
  bool ReadLoadedBytes(uint32_t length, void* buffer) override;
  void RequestThumbnail(int page_index,
                        float device_pixel_ratio,
                        SendThumbnailCallback send_callback) override;

  // DocumentLoader::Client:
  pp::Instance* GetPluginInstance() override;
  std::unique_ptr<URLLoaderWrapper> CreateURLLoader() override;
  void OnPendingRequestComplete() override;
  void OnNewDataReceived() override;
  void OnDocumentComplete() override;
  void OnDocumentCanceled() override;

#if defined(PDF_ENABLE_XFA)
  void UpdatePageCount();
#endif  // defined(PDF_ENABLE_XFA)

  void UnsupportedFeature(const std::string& feature);

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
    std::vector<gfx::Rect> GetVisibleSelections() const;

    // Invalidates |selection|, but with |selection| slightly expanded to
    // compensate for any rounding errors.
    void Invalidate(const gfx::Rect& selection);

    PDFiumEngine* const engine_;
    // The origin at the time this object was constructed.
    const gfx::Point previous_origin_;
    // Screen rectangles that were selected on construction.
    std::vector<gfx::Rect> old_selections_;
  };

  // Used to store mouse down state to handle it in other mouse event handlers.
  class MouseDownState {
   public:
    MouseDownState(const PDFiumPage::Area& area,
                   const PDFiumPage::LinkTarget& target);
    MouseDownState(const MouseDownState&) = delete;
    MouseDownState& operator=(const MouseDownState&) = delete;
    ~MouseDownState();

    void Set(const PDFiumPage::Area& area,
             const PDFiumPage::LinkTarget& target);
    void Reset();
    bool Matches(const PDFiumPage::Area& area,
                 const PDFiumPage::LinkTarget& target) const;

   private:
    PDFiumPage::Area area_;
    PDFiumPage::LinkTarget target_;
  };

  friend class FormFillerTest;
  friend class PDFiumEngineTabbingTest;
  friend class PDFiumFormFiller;
  friend class PDFiumTestBase;
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
  void OnGetPasswordComplete(const std::string& password);

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

  // Loads information about the pages in the document and performs layout.
  void LoadPageInfo();

  // Refreshes the document layout using the current pages and layout options.
  void RefreshCurrentDocumentLayout();

  // Proposes the next document layout using the current pages and
  // |desired_layout_options_|.
  void ProposeNextDocumentLayout();

  // Updates |layout| using the current page sizes.
  void UpdateDocumentLayout(DocumentLayout* layout);

  // Loads information about the pages in the document, calculating and
  // returning the individual page sizes.
  //
  // Note that the page rects of any new pages will be left uninitialized, so
  // layout must be performed immediately after calling this method.
  //
  // TODO(kmoon): LoadPageSizes() is a bit misnomer, but LoadPageInfo() is
  // taken right now...
  std::vector<gfx::Size> LoadPageSizes(
      const DocumentLayout::Options& layout_options);

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
  gfx::Size GetPageSize(int index);
  gfx::Size GetPageSizeForLayout(int index,
                                 const DocumentLayout::Options& layout_options);

  // Helper function for getting the inset sizes for the current layout. If
  // two-up view is enabled, the configuration of inset sizes depends on
  // the position of the page, specified by |page_index| and |num_of_pages|.
  draw_utils::PageInsetSizes GetInsetSizes(
      const DocumentLayout::Options& layout_options,
      size_t page_index,
      size_t num_of_pages) const;

  // If two-up view is disabled, enlarges |page_size| with inset sizes for
  // single-view. If two-up view is enabled, calls GetInsetSizes() with
  // |page_index| and |num_of_pages|, and uses the returned inset sizes to
  // enlarge |page_size|.
  void EnlargePage(const DocumentLayout::Options& layout_options,
                   size_t page_index,
                   size_t num_of_pages,
                   gfx::Size* page_size) const;

  // Similar to EnlargePage(), but insets a |rect|. Also multiplies the inset
  // sizes by |multiplier|, using the ceiling of the result.
  void InsetPage(const DocumentLayout::Options& layout_options,
                 size_t page_index,
                 size_t num_of_pages,
                 double multiplier,
                 gfx::Rect& rect) const;

  // If two-up view is enabled, returns the index of the page beside
  // |page_index| page. Returns base::nullopt if there is no adjacent page or
  // if two-up view is disabled.
  base::Optional<size_t> GetAdjacentPageIndexForTwoUpView(
      size_t page_index,
      size_t num_of_pages) const;

  std::vector<gfx::Rect> GetAllScreenRectsUnion(
      const std::vector<PDFiumRange>& rect_range,
      const gfx::Point& point) const;

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
  bool OnMouseDown(const MouseInputEvent& event);
  bool OnMouseUp(const MouseInputEvent& event);
  bool OnMouseMove(const MouseInputEvent& event);
  void OnMouseEnter(const MouseInputEvent& event);
  bool OnKeyDown(const KeyboardInputEvent& event);
  bool OnKeyUp(const KeyboardInputEvent& event);
  bool OnChar(const KeyboardInputEvent& event);

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
  PDFiumPage::Area GetCharIndex(const gfx::Point& point,
                                int* page_index,
                                int* char_index,
                                int* form_type,
                                PDFiumPage::LinkTarget* target);

  void OnSingleClick(int page_index, int char_index);
  void OnMultipleClick(int click_count, int page_index, int char_index);
  bool OnLeftMouseDown(const MouseInputEvent& event);
  bool OnMiddleMouseDown(const MouseInputEvent& event);
  bool OnRightMouseDown(const MouseInputEvent& event);

  // Starts a progressive paint operation given a rectangle in screen
  // coordinates. Returns the index in progressive_rects_.
  int StartPaint(int page_index, const gfx::Rect& dirty);

  // Continues a paint operation that was started earlier.  Returns true if the
  // paint is done, or false if it needs to be continued.
  bool ContinuePaint(int progressive_index, SkBitmap& image_data);

  // Called once PDFium is finished rendering a page so that we draw our
  // borders, highlighting etc.
  void FinishPaint(int progressive_index, SkBitmap& image_data);

  // Stops any paints that are in progress.
  void CancelPaints();

  // Invalidates all pages. Use this when some global parameter, such as page
  // orientation, has changed.
  void InvalidateAllPages();

  // If the page is narrower than the document size, paint the extra space
  // with the page background.
  void FillPageSides(int progressive_index);

  void PaintPageShadow(int progressive_index, SkBitmap& image_data);

  // Highlight visible find results and selections.
  void DrawSelections(int progressive_index, SkBitmap& image_data) const;

  // Paints an page that hasn't finished downloading.
  void PaintUnavailablePage(int page_index,
                            const gfx::Rect& dirty,
                            SkBitmap& image_data);

  // Given a page index, returns the corresponding index in progressive_rects_,
  // or -1 if it doesn't exist.
  int GetProgressiveIndex(int page_index) const;

  // Creates a FPDF_BITMAP from a rectangle in screen coordinates.
  ScopedFPDFBitmap CreateBitmap(const gfx::Rect& rect,
                                SkBitmap& image_data) const;

  // Given a rectangle in screen coordinates, returns the coordinates in the
  // units that PDFium rendering functions expect.
  void GetPDFiumRect(int page_index,
                     const gfx::Rect& rect,
                     int* start_x,
                     int* start_y,
                     int* size_x,
                     int* size_y) const;

  // Returns the rendering flags to pass to PDFium.
  int GetRenderingFlags() const;

  // Returns the currently visible rectangle in document coordinates.
  gfx::Rect GetVisibleRect() const;

  // Given |rect| in document coordinates, returns the rectangle in screen
  // coordinates. (i.e. 0,0 is top left corner of plugin area)
  gfx::Rect GetScreenRect(const gfx::Rect& rect) const;

  // Given an image |buffer| with |stride|, highlights |rect|.
  // |highlighted_rects| contains the already highlighted rectangles and will be
  // updated to include |rect| if |rect| has not already been highlighted.
  void Highlight(void* buffer,
                 int stride,
                 const gfx::Rect& rect,
                 int color_red,
                 int color_green,
                 int color_blue,
                 std::vector<gfx::Rect>& highlighted_rects) const;

  // Helper function to convert a device to page coordinates.  If the page is
  // not yet loaded, |page_x| and |page_y| will be set to 0.
  void DeviceToPage(int page_index,
                    const gfx::Point& device_point,
                    double* page_x,
                    double* page_y);

  // Helper function to get the index of a given FPDF_PAGE.  Returns -1 if not
  // found.
  int GetVisiblePageIndex(FPDF_PAGE page);

  // Helper function to change the current page, running page open/close
  // triggers as necessary.
  void SetCurrentPage(int index);

  void DrawPageShadow(const gfx::Rect& page_rect,
                      const gfx::Rect& shadow_rect,
                      const gfx::Rect& clip_rect,
                      SkBitmap& image_data);

  void GetRegion(const gfx::Point& location,
                 SkBitmap& image_data,
                 void*& region,
                 int& stride) const;

  // Called when the selection changes.
  void OnSelectionTextChanged();
  void OnSelectionPositionChanged();

  // Sets text selection status of document. This does not include text
  // within form text fields.
  void SetSelecting(bool selecting);

  // Sets whether or not focus is in form text field or form combobox text
  // field.
  void SetInFormTextArea(bool in_form_text_area);

  // Sets whether or not left mouse button is currently being held down.
  void SetMouseLeftButtonDown(bool is_mouse_left_button_down);

  // Given an annotation which is a form of |form_type| which is known to be a
  // form text area, check if it is an editable form text area.
  bool IsAnnotationAnEditableFormTextArea(FPDF_ANNOTATION annot,
                                          int form_type) const;

  bool PageIndexInBounds(int index) const;
  bool IsPageCharacterIndexInBounds(
      const PP_PdfPageCharacterIndex& index) const;

  // Gets the height of the top toolbar in screen coordinates. This is
  // independent of whether it is hidden or not at the moment.
  float GetToolbarHeightInScreenCoords();

  void ScheduleTouchTimer(const TouchInputEvent& event);
  void KillTouchTimer();
  void HandleLongPress(const TouchInputEvent& event);

  // Returns a VarDictionary (representing a bookmark), which in turn contains
  // child VarDictionaries (representing the child bookmarks).
  // If nullptr is passed in as the bookmark then we traverse from the "root".
  // Note that the "root" bookmark contains no useful information.
  pp::VarDictionary TraverseBookmarks(FPDF_BOOKMARK bookmark,
                                      unsigned int depth);

  void ScrollBasedOnScrollAlignment(
      const gfx::Rect& scroll_rect,
      const PP_PdfAccessibilityScrollAlignment& horizontal_scroll_alignment,
      const PP_PdfAccessibilityScrollAlignment& vertical_scroll_alignment);

  // Scrolls top left of a rect in page |target_rect| to |global_point|.
  // Global point is point relative to viewport in screen.
  void ScrollToGlobalPoint(const gfx::Rect& target_rect,
                           const gfx::Point& global_point);

  // Set if the document has any local edits.
  void EnteredEditMode();

  // Navigates to a link destination depending on the type of destination.
  // Returns false if |area| is not a link.
  bool NavigateToLinkDestination(PDFiumPage::Area area,
                                 const PDFiumPage::LinkTarget& target,
                                 WindowOpenDisposition disposition);

  // IFSDK_PAUSE callbacks
  static FPDF_BOOL Pause_NeedToPauseNow(IFSDK_PAUSE* param);

  // Used for text selection. Given the start and end of selection, sets the
  // text range in |selection_|.
  void SetSelection(const PP_PdfPageCharacterIndex& selection_start_index,
                    const PP_PdfPageCharacterIndex& selection_end_index);

  // Scroll the current focused annotation into view if not already in view.
  void ScrollFocusedAnnotationIntoView();

  // Given |annot|, scroll the |annot| into view if not already in view.
  void ScrollAnnotationIntoView(FPDF_ANNOTATION annot, int page_index);

  void OnFocusedAnnotationUpdated(FPDF_ANNOTATION annot, int page_index);

  // Read the attachments' information inside the PDF document, and set
  // |doc_attachment_info_list_|. To be called after the document is loaded.
  void LoadDocumentAttachmentInfoList();

  // Fetches and populates the fields of |doc_metadata_|. To be called after the
  // document is loaded.
  void LoadDocumentMetadata();

  // Retrieves the unparsed value of |field| in the document information
  // dictionary.
  std::string GetMetadataByField(FPDF_BYTESTRING field) const;

  // Retrieves the version of the PDF (e.g. 1.4 or 2.0) as an enum.
  PdfVersion GetDocumentVersion() const;

  // This is a layer between OnKeyDown() and actual tab handling to facilitate
  // testing.
  bool HandleTabEvent(uint32_t modifiers);

  // Helper functions to handle tab events.
  bool HandleTabEventWithModifiers(uint32_t modifiers);
  bool HandleTabForward(uint32_t modifiers);
  bool HandleTabBackward(uint32_t modifiers);

  // Updates the currently focused object stored in |focus_item_type_|. Notifies
  // |client_| about document focus change, if any.
  void UpdateFocusItemType(FocusElementType focus_item_type);

  void UpdateLinkUnderCursor(const std::string& target_url);
  void SetLinkUnderCursorForAnnotation(FPDF_ANNOTATION annot, int page_index);

  PDFEngine::Client* const client_;

  // The current document layout.
  DocumentLayout layout_;

  // The options for the desired document layout.
  DocumentLayout::Options desired_layout_options_;

  // The scroll position in screen coordinates.
  gfx::Point position_;
  // The offset of the page into the viewport.
  gfx::Vector2d page_offset_;
  // The plugin size in screen coordinates.
  gfx::Size plugin_size_;
  double current_zoom_ = 1.0;

  std::unique_ptr<DocumentLoader> doc_loader_;  // Main document's loader.
  bool doc_loader_set_for_testing_ = false;
  std::string url_;
  std::string headers_;

  // Set to true if the user is being prompted for their password. Will be set
  // to false after the user finishes getting their password.
  bool getting_password_ = false;
  int password_tries_remaining_ = 0;

  // Needs to be above pages_, as destroying a page may call some methods of
  // form filler.
  PDFiumFormFiller form_filler_;

  std::unique_ptr<PDFiumDocument> document_;
  bool document_loaded_ = false;

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
  gfx::Point mouse_middle_button_last_position_;

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

  std::unique_ptr<PDFiumPermissions> permissions_;

  gfx::Size default_page_size_;

  // Timer for touch long press detection.
  base::OneShotTimer touch_timer_;

  // Set to true when handling long touch press.
  bool handling_long_press_ = false;

  // Set to true when updating plugin focus.
  bool updating_focus_ = false;

  // The focus item type for the currently focused object.
  FocusElementType focus_item_type_ = FocusElementType::kNone;

  // Stores the last focused object's focus item type before PDF loses focus.
  FocusElementType last_focused_item_type_ = FocusElementType::kNone;

  // Stores the last focused annotation's index before PDF loses focus.
  int last_focused_annot_index_ = -1;

  // Holds the zero-based page index of the last page that had the focused
  // object.
  int last_focused_page_ = -1;

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
  std::vector<gfx::Rect> form_highlights_;

  // Whether to render in grayscale or in color.
  bool render_grayscale_ = false;

  // Whether to render PDF annotations.
  bool render_annots_ = true;

  // The link currently under the cursor.
  std::string link_under_cursor_;

  // Pending progressive paints.
  class ProgressivePaint {
   public:
    ProgressivePaint(int page_index, const gfx::Rect& rect);
    ProgressivePaint(ProgressivePaint&& that);
    ProgressivePaint& operator=(ProgressivePaint&& that);
    ~ProgressivePaint();

    int page_index() const { return page_index_; }
    const gfx::Rect& rect() const { return rect_; }
    FPDF_BITMAP bitmap() const { return bitmap_.get(); }
    bool painted() const { return painted_; }

    void set_painted(bool enable) { painted_ = enable; }
    void SetBitmapAndImageData(ScopedFPDFBitmap bitmap, SkBitmap image_data);

   private:
    int page_index_;
    gfx::Rect rect_;            // In screen coordinates.
    SkBitmap image_data_;       // Maintains reference while |bitmap_| exists.
    ScopedFPDFBitmap bitmap_;   // Must come after |image_data_|.
    // Temporary used to figure out if in a series of Paint() calls whether this
    // pending paint was updated or not.
    bool painted_ = false;
  };
  std::vector<ProgressivePaint> progressive_paints_;

  // Keeps track of when we started the last progressive paint, so that in our
  // callback we can determine if we need to pause.
  base::Time last_progressive_start_time_;

  // The timeout to use for the current progressive paint.
  base::TimeDelta progressive_paint_timeout_;

  // Shadow matrix for generating the page shadow bitmap.
  std::unique_ptr<draw_utils::ShadowMatrix> page_shadow_;

  // A list of information of document attachments.
  std::vector<DocumentAttachmentInfo> doc_attachment_info_list_;

  // Stores parsed document metadata.
  DocumentMetadata doc_metadata_;

  // While true, the document try to be opened and parsed after download each
  // part. Else the document will be opened and parsed only on finish of
  // downloading.
  bool process_when_pending_request_complete_ = true;

  enum class RangeSelectionDirection { Left, Right };
  RangeSelectionDirection range_selection_direction_ =
      RangeSelectionDirection::Right;

  gfx::Point range_selection_base_;

  bool edit_mode_ = false;

  PDFiumPrint print_;

  base::WeakPtrFactory<PDFiumEngine> weak_factory_{this};

  // Weak pointers from this factory are used to bind the ContinueFind()
  // function. This allows those weak pointers to be invalidated during
  // StopFind(), and keeps the invalidation separated from |weak_factory_|.
  base::WeakPtrFactory<PDFiumEngine> find_weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_H_
