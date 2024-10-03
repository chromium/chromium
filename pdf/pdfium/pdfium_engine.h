// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "pdf/buildflags.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/loader/document_loader.h"
#include "pdf/pdfium/pdfium_engine_client.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_range.h"
#include "printing/mojom/print.mojom-forward.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_progressive.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "pdf/flatten_pdf_result.h"
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "pdf/pdfium/pdfium_searchify.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#endif

namespace blink {
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebTouchEvent;
struct WebPrintParams;
}  // namespace blink

namespace chrome_pdf {

class PDFiumDocument;
class PDFiumPermissions;
class Thumbnail;
enum class AccessibilityScrollAlignment;
struct AccessibilityActionData;
struct AccessibilityFocusInfo;
struct DocumentAttachmentInfo;
struct DocumentMetadata;
struct PageCharacterIndex;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class PDFiumOnDemandSearchifier;
#endif

namespace draw_utils {
class ShadowMatrix;
}  // namespace draw_utils

enum class FontMappingMode {
  // Do not perform font mapping.
  kNoMapping,
  // Perform font mapping in renderer processes using Blink/content APIs.
  kBlink,
};

enum class DocumentPermission {
  kCopy,
  kCopyAccessible,
  kPrintLowQuality,
  kPrintHighQuality,
};

// Do one time initialization of the SDK.
// If `enable_v8` is false, then PDFiumEngine will not be able to run
// JavaScript.
// When `use_skia` is true, the PDFiumEngine will use Skia renderer. Otherwise,
// it will use AGG renderer.
void InitializeSDK(bool enable_v8,
                   bool use_skia,
                   FontMappingMode font_mapping_mode);
// Tells the SDK that we're shutting down.
void ShutdownSDK();

using SendThumbnailCallback = base::OnceCallback<void(Thumbnail)>;

// This class implements a PDF rendering engine using the PDFium library.
//
// Many methods in this class are virtual to facilitate testing.
class PDFiumEngine : public DocumentLoader::Client, public IFSDK_PAUSE {
 public:
  // Maximum number of parameters a nameddest view can contain.
  static constexpr size_t kMaxViewParams = 4;

  // State transition when tabbing forward:
  // None -> Document -> Page -> None (when focusable annotations on all pages
  // are done).
  // Exposed for testing.
  enum class FocusElementType { kNone, kDocument, kPage };

  // Named destination in a document.
  struct NamedDestination {
    // 0-based page number.
    unsigned long page;

    // View fit type (see table 8.2 "Destination syntax" on page 582 of PDF
    // Reference 1.7). Empty string if not present.
    std::string view;

    // Number of parameters for the view.
    unsigned long num_params;

    // Parameters for the view. Their meaning depends on the `view` and their
    // number is defined by `num_params` but is at most `kMaxViewParams`. Note:
    // If a parameter stands for the x/y coordinates, it should be transformed
    // into the corresponding in-screen coordinates before it's sent to the
    // viewport.
    float params[kMaxViewParams];

    // A string of parameters for view fit type XYZ in the format of "x,y,zoom",
    // where x and y parameters are the in-screen coordinates and zoom is the
    // zoom level. If a parameter is "null", then current value of that
    // parameter in the viewport should be retained. Note: This string is empty
    // if the view's fit type is not XYZ.
    std::string xyz_params;
  };

  // NOTE: `script_option` is ignored when PDF_ENABLE_V8 is not defined.
  PDFiumEngine(PDFiumEngineClient* client,
               PDFiumFormFiller::ScriptOption script_option);
  PDFiumEngine(const PDFiumEngine&) = delete;
  PDFiumEngine& operator=(const PDFiumEngine&) = delete;
  ~PDFiumEngine() override;

  // Replaces the normal DocumentLoader for testing. Must be called before
  // HandleDocumentLoad().
  void SetDocumentLoaderForTesting(std::unique_ptr<DocumentLoader> loader);

  // Returns the FontMappingMode set during PDFium SDK initialization.
  static FontMappingMode GetFontMappingMode();

  // Starts loading the document from `loader`. Follow-up requests (such as for
  // partial loading) will use `original_url`.
  bool HandleDocumentLoad(std::unique_ptr<UrlLoader> loader,
                          const std::string& original_url);

  // Most of these functions are similar to the Pepper functions of the same
  // name, so not repeating the description here unless it's different.
  virtual void PageOffsetUpdated(const gfx::Vector2d& page_offset);
  virtual void PluginSizeUpdated(const gfx::Size& size);
  virtual void ScrolledToXPosition(int position);
  virtual void ScrolledToYPosition(int position);
  // Paint is called a series of times. Before these n calls are made, PrePaint
  // is called once. After Paint is called n times, PostPaint is called once.
  void PrePaint();
  virtual void Paint(const gfx::Rect& rect,
                     SkBitmap& image_data,
                     std::vector<gfx::Rect>& ready,
                     std::vector<gfx::Rect>& pending);
  void PostPaint();
  virtual bool HandleInputEvent(const blink::WebInputEvent& event);
  void PrintBegin();
  virtual std::vector<uint8_t> PrintPages(
      const std::vector<int>& page_indices,
      const blink::WebPrintParams& print_params);
  void PrintEnd();
  void StartFind(const std::u16string& text, bool case_sensitive);
  bool SelectFindResult(bool forward);
  void StopFind();
  virtual void ZoomUpdated(double new_zoom_level);
  void RotateClockwise();
  void RotateCounterclockwise();
  bool IsReadOnly() const;
  void SetReadOnly(bool read_only);
  void SetDocumentLayout(DocumentLayout::PageSpread page_spread);
  void DisplayAnnotations(bool display);

  // Applies the document layout options proposed by a call to
  // PDFiumEngineClient::ProposeDocumentLayout(), returning the overall size of
  // the new effective layout.
  virtual gfx::Size ApplyDocumentLayout(const DocumentLayout::Options& options);

  std::string GetSelectedText();
  // Returns true if focus is within an editable form text area.
  virtual bool CanEditText() const;

  // Returns true if focus is within an editable form text area and the text
  // area has text.
  bool HasEditableText() const;

  // Replace selected text within an editable form text area with another
  // string. If there is no selected text, append the replacement text after the
  // current caret position.
  void ReplaceSelection(const std::string& text);

  // Methods to check if undo/redo is possible, and to perform them.
  bool CanUndo() const;
  bool CanRedo() const;
  void Undo();
  void Redo();

  // Handles actions invoked by Accessibility clients.
  void HandleAccessibilityAction(const AccessibilityActionData& action_data);

  std::string GetLinkAtPosition(const gfx::Point& point);

  // Checks the permissions associated with this document.
  virtual bool HasPermission(DocumentPermission permission) const;

  virtual void SelectAll();

  virtual void ClearTextSelection();

  // Gets the list of DocumentAttachmentInfo from the document.
  virtual const std::vector<DocumentAttachmentInfo>&
  GetDocumentAttachmentInfoList() const;

  // Gets the content of an attachment by the attachment's `index`. `index`
  // must be in the range of [0, attachment_count-1), where `attachment_count`
  // is the number of attachments embedded in the document.
  // The caller of this method is responsible for checking whether the
  // attachment is readable, attachment size is not 0 byte, and the return
  // value's size matches the corresponding DocumentAttachmentInfo's
  // `size_bytes`.
  std::vector<uint8_t> GetAttachmentData(size_t index);

  // Gets metadata about the document.
  virtual const DocumentMetadata& GetDocumentMetadata() const;

  // Gets the number of pages in the document.
  virtual int GetNumberOfPages() const;

  // Returns a list of Values of Bookmarks. Each Bookmark is a dictionary Value
  // which contains the following key/values:
  // - "title" - a string Value.
  // - "page" - an int Value.
  // - "children" - a list of Values, with each entry containing
  //   a dictionary Value of the same structure.
  virtual base::Value::List GetBookmarks();

  // Gets the named destination by name.
  std::optional<NamedDestination> GetNamedDestination(
      const std::string& destination);

  // Gets the index of the most visible page, or -1 if none are visible.
  int GetMostVisiblePage();

  // Returns whether the page at `page_index` is visible or not.
  virtual bool IsPageVisible(int page_index) const;

  // Gets the current layout orientation.
  PageOrientation GetCurrentOrientation() const;

  // Gets the rectangle of the page excluding any additional areas.
  virtual gfx::Rect GetPageContentsRect(int index);

  // Returns a page's rect in screen coordinates, as well as its surrounding
  // border areas and bottom separator.
  virtual gfx::Rect GetPageScreenRect(int page_index) const;

  // Set color / grayscale rendering modes.
  virtual void SetGrayscale(bool grayscale);

  // Returns the image as a 32-bit bitmap format for OCR.
  SkBitmap GetImageForOcr(int page_index, int image_index);

  // Gets the PDF document's print scaling preference. True if the document can
  // be scaled to fit.
  bool GetPrintScaling();

  // Returns number of copies to be printed.
  int GetCopiesToPrint();

  // Returns the duplex setting.
  printing::mojom::DuplexMode GetDuplexMode();

  // Returns the uniform page size of the document in points. Returns
  // `std::nullopt` if the document has more than one page size.
  virtual std::optional<gfx::Size> GetUniformPageSizePoints();

  // Append blank pages to make a 1-page document to a `num_pages` document.
  // Always retain the first page data.
  void AppendBlankPages(size_t num_pages);

  // Append the first page of the document loaded with the `engine` to this
  // document at page `index`.
  void AppendPage(PDFiumEngine* engine, int index);

  virtual std::vector<uint8_t> GetSaveData();

  virtual void SetCaretPosition(const gfx::Point& position);

  void MoveRangeSelectionExtent(const gfx::Point& extent);

  void SetSelectionBounds(const gfx::Point& base, const gfx::Point& extent);

  void GetSelection(uint32_t* selection_start_page_index,
                    uint32_t* selection_start_char_index,
                    uint32_t* selection_end_page_index,
                    uint32_t* selection_end_char_index);

  // Remove focus from form widgets, consolidating the user input.
  void KillFormFocus();

  // Notify whether the PDF currently has the focus or not.
  void UpdateFocus(bool has_focus);

  // Returns the focus info of current focus item.
  AccessibilityFocusInfo GetFocusInfo();

  bool IsPDFDocTagged();

  virtual uint32_t GetLoadedByteSize();

  virtual bool ReadLoadedBytes(uint32_t length, void* buffer);

  // Requests for a thumbnail to be sent using a callback when the page is ready
  // to be rendered. `send_callback` is run with the thumbnail data when ready.
  void RequestThumbnail(int page_index,
                        float device_pixel_ratio,
                        SendThumbnailCallback send_callback);
#if BUILDFLAG(ENABLE_PDF_INK2)
  // Virtual to support testing.
  virtual gfx::Size GetThumbnailSize(int page_index, float device_pixel_ratio);
#endif

  // DocumentLoader::Client:
  std::unique_ptr<URLLoaderWrapper> CreateURLLoader() override;
  void OnPendingRequestComplete() override;
  void OnNewDataReceived() override;
  void OnDocumentComplete() override;
  void OnDocumentCanceled() override;

#if defined(PDF_ENABLE_XFA)
  void UpdatePageCount();
#endif  // defined(PDF_ENABLE_XFA)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Starts the searchify process and passes a callback to a function that
  // performs OCR. This function is expected to be called only once.
  void StartSearchify(PerformOcrCallbackAsync perform_ocr_callback);

  // Returns a function to pass OCR disconnection events to the searchifier.
  base::RepeatingClosure GetOcrDisconnectHandler();

  // Tells if the page is waiting to be searchified.
  bool PageNeedsSearchify(int page_index) const;

  // Schedules searchify for the page if it has no text.
  void ScheduleSearchifyIfNeeded(PDFiumPage* page);

  // Cancels a pending searchify if it has not started yet. Ignores the request
  // if the page is not scheduled for searchify.
  void CancelPendingSearchify(int page_index);

  PDFiumOnDemandSearchifier* GetSearchifierForTesting() {
    return searchifier_.get();
  }
#endif

  void UnsupportedFeature(const std::string& feature);

  FPDF_AVAIL fpdf_availability() const;
  FPDF_DOCUMENT doc() const;
  FPDF_FORMHANDLE form() const;

  // Returns the PDFiumPage pointer of a given index. Returns nullptr if `index`
  // is out of range.
  PDFiumPage* GetPage(size_t index);

  bool IsValidLink(const std::string& url);

  // Sets whether form highlight should be enabled or cleared.
  virtual void SetFormHighlight(bool enable_form);

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

    // Invalidates `selection`, but with `selection` slightly expanded to
    // compensate for any rounding errors.
    void Invalidate(const gfx::Rect& selection);

    const raw_ptr<PDFiumEngine> engine_;
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

  struct RegionData {
    RegionData(base::span<uint8_t> buffer, size_t stride);
    RegionData(RegionData&&) noexcept;
    RegionData& operator=(RegionData&&) noexcept;
    ~RegionData();

    base::raw_span<uint8_t> buffer;  // Never empty.
    size_t stride;
  };

  friend class FormFillerTest;
  friend class PDFiumEngineTabbingTest;
  friend class PDFiumEngineTest;
  friend class PDFiumFormFiller;
  friend class PDFiumTestBase;
  friend class SelectionChangeInvalidator;

  gfx::Size plugin_size() const;

  // We finished getting the pdf file, so load it. This will complete
  // asynchronously (due to password fetching) and may be run multiple times.
  void LoadDocument();

  // Try loading the document. Returns true if the document is successfully
  // loaded or is already loaded otherwise it will return false. If there is a
  // password, then `password` is non-empty. If the document could not be loaded
  // and needs a password, `needs_password` will be set to true.
  bool TryLoadingDoc(const std::string& password, bool* needs_password);

  // Asks the user for the document password and then continue loading the
  // document.
  void GetPasswordAndLoad();

  // Called when the password has been retrieved.
  void OnGetPasswordComplete(const std::string& password);

  // Continues loading the document when the password has been retrieved, or if
  // there is no password. If there is no password, then `password` is empty.
  void ContinueLoadingDocument(const std::string& password);

  // Finishes loading the document. Recalculate the document size if there were
  // pages that were not previously available.
  // Also notifies the client that the document has been loaded.
  // This should only be called after `doc_` has been loaded and the document is
  // fully downloaded.
  // If this has been run once, it will not notify the client again.
  void FinishLoadingDocument();

  // Loads information about the pages in the document and performs layout.
  void LoadPageInfo();

  // Refreshes the document layout using the current pages and layout options.
  void RefreshCurrentDocumentLayout();

  // Proposes the next document layout using the current pages and
  // `desired_layout_options_`.
  void ProposeNextDocumentLayout();

  // Updates `layout` using the current page sizes.
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

  // Checks whether the document is optimized by linearization.
  bool IsLinearized();

  // Calculates which pages should be displayed right now.
  void CalculateVisiblePages();

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
  // the position of the page, specified by `page_index` and `num_of_pages`.
  gfx::Insets GetInsets(const DocumentLayout::Options& layout_options,
                        size_t page_index,
                        size_t num_of_pages) const;

  // If two-up view is enabled, returns the index of the page beside
  // `page_index` page. Returns std::nullopt if there is no adjacent page or
  // if two-up view is disabled.
  std::optional<size_t> GetAdjacentPageIndexForTwoUpView(
      size_t page_index,
      size_t num_of_pages) const;

  std::vector<gfx::Rect> GetAllScreenRectsUnion(
      const std::vector<PDFiumRange>& rect_range,
      const gfx::Point& point) const;

  void UpdateTickMarks();

  // Called to continue searching so we don't block the main thread.
  void ContinueFind(bool case_sensitive);

  // Inserts a find result into `find_results_`, which is sorted.
  void AddFindResult(PDFiumRange result);

  // Search a page using PDFium's methods.  Doesn't work with unicode.  This
  // function is just kept arount in case PDFium code is fixed.
  void SearchUsingPDFium(const std::u16string& term,
                         bool case_sensitive,
                         bool first_search,
                         int character_to_start_searching_from,
                         int current_page);

  // Search a page ourself using ICU.
  void SearchUsingICU(const std::u16string& term,
                      bool case_sensitive,
                      bool first_search,
                      int character_to_start_searching_from,
                      int current_page);

  // Input event handlers.
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);
  void OnMouseEnter(const blink::WebMouseEvent& event);
  bool OnKeyDown(const blink::WebKeyboardEvent& event);
  bool OnKeyUp(const blink::WebKeyboardEvent& event);
  bool OnChar(const blink::WebKeyboardEvent& event);

  // Decide what cursor should be displayed.
  ui::mojom::CursorType DetermineCursorType(PDFiumPage::Area area,
                                            int form_type) const;

  bool ExtendSelection(int page_index, int char_index);

  std::vector<uint8_t> PrintPagesAsRasterPdf(
      const std::vector<int>& page_indices,
      const blink::WebPrintParams& print_params);

  std::vector<uint8_t> PrintPagesAsPdf(
      const std::vector<int>& page_indices,
      const blink::WebPrintParams& print_params);

  // Checks if `page` has selected text in a form element. If so, sets that as
  // the plugin's text selection.
  void SetFormSelectedText(FPDF_FORMHANDLE form_handle, FPDF_PAGE page);

  // Given `point`, returns which page and character location it's closest to,
  // as well as extra information about objects at that point.
  PDFiumPage::Area GetCharIndex(const gfx::Point& point,
                                int* page_index,
                                int* char_index,
                                int* form_type,
                                PDFiumPage::LinkTarget* target);

  void OnSingleClick(int page_index, int char_index);
  void OnMultipleClick(int click_count, int page_index, int char_index);
  bool OnLeftMouseDown(const blink::WebMouseEvent& event);
  bool OnMiddleMouseDown(const blink::WebMouseEvent& event);
  bool OnRightMouseDown(const blink::WebMouseEvent& event);

  // Starts a progressive paint operation given a rectangle in screen
  // coordinates. Returns the index in `progressive_paints_`.
  size_t StartPaint(int page_index, const gfx::Rect& dirty);

  // Continues a paint operation that was started earlier.  Returns true if the
  // paint is done, or false if it needs to be continued.
  bool ContinuePaint(size_t progressive_index, SkBitmap& image_data);

  // Called once PDFium is finished rendering a page so that we draw our
  // borders, highlighting etc.
  void FinishPaint(size_t progressive_index, SkBitmap& image_data);

  // Stops any paints that are in progress.
  void CancelPaints();

  // Invalidates all pages. Use this when some global parameter, such as page
  // orientation, has changed.
  void InvalidateAllPages();

  // If the page is narrower than the document size, paint the extra space
  // with the page background.
  void FillPageSides(int progressive_index);

  void PaintPageShadow(size_t progressive_index, SkBitmap& image_data);

  // Highlight visible find results and selections.
  void DrawSelections(size_t progressive_index, SkBitmap& image_data) const;

  // Paints an page that hasn't finished downloading.
  void PaintUnavailablePage(int page_index,
                            const gfx::Rect& dirty,
                            SkBitmap& image_data);

  // Given a page index, returns the corresponding index in
  // `progressive_paints_`, or nullopt if it does not exist.
  std::optional<size_t> GetProgressiveIndex(int page_index) const;

  // Creates a FPDF_BITMAP from a rectangle in screen coordinates.
  ScopedFPDFBitmap CreateBitmap(const gfx::Rect& rect,
                                bool has_alpha,
                                SkBitmap& image_data) const;

  // Given a rectangle in screen coordinates, returns the coordinates in the
  // units that PDFium rendering functions expect.
  gfx::Rect GetPDFiumRect(int page_index, const gfx::Rect& rect) const;

  // Returns the rendering flags to pass to PDFium.
  int GetRenderingFlags() const;

  // Returns the currently visible rectangle in document coordinates.
  gfx::Rect GetVisibleRect() const;

  // Given `rect` in document coordinates, returns the rectangle in screen
  // coordinates. (i.e. 0,0 is top left corner of plugin area)
  gfx::Rect GetScreenRect(const gfx::Rect& rect) const;

  // Given an image `region`, highlights `rect`.
  // `highlighted_rects` contains the already highlighted rectangles and will be
  // updated to include `rect` if `rect` has not already been highlighted.
  void Highlight(const RegionData& region,
                 const gfx::Rect& rect,
                 SkColor color,
                 std::vector<gfx::Rect>& highlighted_rects) const;

  // Helper function to convert device coordinates to page coordinates.  If the
  // page is not yet loaded, `page_x` and `page_y` will be set to 0.
  void DeviceToPage(int page_index,
                    const gfx::Point& device_point,
                    double* page_x,
                    double* page_y);

  // Helper function to convert device coordinates to screen coordinates.
  // Normalizes `device_point` based on `position_` and `current_zoom_`.
  gfx::Point DeviceToScreen(const gfx::Point& device_point) const;

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

  std::optional<RegionData> GetRegion(const gfx::Point& location,
                                      SkBitmap& image_data) const;

  // Called when the selection changes.
  void OnSelectionTextChanged();
  void OnSelectionPositionChanged();

  // Sets text selection status of document. This does not include text
  // within form text fields.
  void SetSelecting(bool selecting);

  // Sets the type of field that has focus.
  void SetFieldFocus(PDFiumEngineClient::FocusFieldType type);

  // Sets whether or not left mouse button is currently being held down.
  void SetMouseLeftButtonDown(bool is_mouse_left_button_down);

  // Given an annotation which is a form of `form_type` which is known to be a
  // form text area, check if it is an editable form text area.
  bool IsAnnotationAnEditableFormTextArea(FPDF_ANNOTATION annot,
                                          int form_type) const;

  bool PageIndexInBounds(int index) const;
  bool IsPageCharacterIndexInBounds(const PageCharacterIndex& index) const;

  void ScheduleTouchTimer(const blink::WebTouchEvent& event);
  void KillTouchTimer();
  void HandleLongPress(const blink::WebTouchEvent& event);

  // Returns a dictionary representing a bookmark, which in turn contains child
  // dictionaries representing the child bookmarks. If `bookmark` is null, then
  // this method traverses from the root of the bookmarks tree. Note that the
  // root bookmark contains no useful information.
  base::Value::Dict TraverseBookmarks(FPDF_BOOKMARK bookmark,
                                      unsigned int depth);

  void ScrollBasedOnScrollAlignment(
      const gfx::Rect& scroll_rect,
      const AccessibilityScrollAlignment& horizontal_scroll_alignment,
      const AccessibilityScrollAlignment& vertical_scroll_alignment);

  // Scrolls top left of a rect in page `target_rect` to `global_point`.
  // Global point is point relative to viewport in screen.
  void ScrollToGlobalPoint(const gfx::Rect& target_rect,
                           const gfx::Point& global_point);

  // Set if the document has any local edits.
  void EnteredEditMode();

  // Navigates to a link destination depending on the type of destination.
  // Returns false if `area` is not a link.
  bool NavigateToLinkDestination(PDFiumPage::Area area,
                                 const PDFiumPage::LinkTarget& target,
                                 WindowOpenDisposition disposition);

  // IFSDK_PAUSE callbacks
  static FPDF_BOOL Pause_NeedToPauseNow(IFSDK_PAUSE* param);

  // Used for text selection. Given the start and end of selection, sets the
  // text range in `selection_`.
  void SetSelection(const PageCharacterIndex& selection_start_index,
                    const PageCharacterIndex& selection_end_index);

  void SaveSelection();
  void RestoreSelection();

  // Scroll the current focused annotation into view if not already in view.
  void ScrollFocusedAnnotationIntoView();

  // Given `annot`, scroll the `annot` into view if not already in view.
  void ScrollAnnotationIntoView(FPDF_ANNOTATION annot, int page_index);

  void OnFocusedAnnotationUpdated(FPDF_ANNOTATION annot, int page_index);

  // Read the attachments' information inside the PDF document, and set
  // `doc_attachment_info_list_`. To be called after the document is loaded.
  void LoadDocumentAttachmentInfoList();

  // Fetches and populates the fields of `doc_metadata_`. To be called after the
  // document is loaded.
  void LoadDocumentMetadata();

  // This is a layer between OnKeyDown() and actual tab handling to facilitate
  // testing.
  bool HandleTabEvent(int modifiers);

  // Helper functions to handle tab events.
  bool HandleTabEventWithModifiers(int modifiers);
  bool HandleTabForward(int modifiers);
  bool HandleTabBackward(int modifiers);

  // Updates the currently focused object stored in `focus_element_type_`.
  // Notifies `client_` about document focus change, if any.
  void UpdateFocusElementType(FocusElementType focus_element_type);

  void UpdateLinkUnderCursor(const std::string& target_url);
  void SetLinkUnderCursorForAnnotation(FPDF_ANNOTATION annot, int page_index);

  // Checks whether a given `page_index` exists in `pending_thumbnails_`. If so,
  // requests the thumbnail for that page.
  void MaybeRequestPendingThumbnail(int page_index);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Called if OCR service gets disconnected.
  void OnOcrDisconnected();
#endif

  const raw_ptr<PDFiumEngineClient> client_;

  // The current document layout.
  DocumentLayout layout_;

  // The options for the desired document layout.
  DocumentLayout::Options desired_layout_options_;

  // The scroll position in screen coordinates.
  gfx::Point position_;
  // The offset of the page into the viewport.
  gfx::Vector2d page_offset_;
  // The plugin size in screen coordinates.
  std::optional<gfx::Size> plugin_size_;
  double current_zoom_ = 1.0;
  // The caret position and bound in plugin viewport coordinates.
  gfx::Rect caret_rect_;

  std::unique_ptr<DocumentLoader> doc_loader_;  // Main document's loader.
  bool doc_loader_set_for_testing_ = false;

  // Set to true if the user is being prompted for their password. Will be set
  // to false after the user finishes getting their password.
  bool getting_password_ = false;
  int password_tries_remaining_ = 0;

  // Needs to be above pages_, as destroying a page may call some methods of
  // form filler.
  PDFiumFormFiller form_filler_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  std::unique_ptr<PDFiumOnDemandSearchifier> searchifier_;
#endif

  std::unique_ptr<PDFiumDocument> document_;
  bool document_pending_ = false;
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

  // When rotating the page or updating the PageSpread mode, used to store the
  // contents of `selection_`. After the page change is completed, the contents
  // of `selection_` are restored.
  std::vector<PDFiumRange> saved_selection_;

  // True if we're in the middle of text selection.
  bool selecting_ = false;

  MouseDownState mouse_down_state_;

  // Text selection within form text fields and form combobox text fields.
  std::string selected_form_text_;

  // True if left mouse button is currently being held down.
  bool mouse_left_button_down_ = false;

  // True if middle mouse button is currently being held down.
  bool mouse_middle_button_down_ = false;

  // Last known position while performing middle mouse button pan.
  gfx::Point mouse_middle_button_last_position_;

  // The current text used for searching.
  std::u16string current_find_text_;
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
  std::optional<size_t> current_find_index_;
  // Where to resume searching. (0-based)
  std::optional<size_t> resume_find_index_;

  std::unique_ptr<PDFiumPermissions> permissions_;

  gfx::Size default_page_size_;

  // Timer for touch long press detection.
  base::OneShotTimer touch_timer_;

  // Set to true when handling long touch press.
  bool handling_long_press_ = false;

  // Set to true when updating plugin focus.
  bool updating_focus_ = false;

  // True if `focus_field_type_` is currently set to `FocusFieldType::kText` and
  // the focused form text area is not read-only.
  bool editable_form_text_area_ = false;

  // The type of the currently focused form field.
  PDFiumEngineClient::FocusFieldType focus_field_type_ =
      PDFiumEngineClient::FocusFieldType::kNoFocus;

  // The focus element type for the currently focused object.
  FocusElementType focus_element_type_ = FocusElementType::kNone;

  // Stores the last focused object's focus element type before PDF loses focus.
  FocusElementType last_focused_element_type_ = FocusElementType::kNone;

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
  std::optional<int> in_flight_visible_page_;

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

  // Pending progressive paints.
  class ProgressivePaint {
   public:
    ProgressivePaint(int page_index, const gfx::Rect& rect);
    ProgressivePaint(ProgressivePaint&& that) noexcept;
    ProgressivePaint& operator=(ProgressivePaint&& that) noexcept;
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

  // Pending thumbnail requests.
  struct PendingThumbnail {
    PendingThumbnail();
    PendingThumbnail(PendingThumbnail&& that) noexcept;
    PendingThumbnail& operator=(PendingThumbnail&& that) noexcept;
    ~PendingThumbnail();

    float device_pixel_ratio = 1.0f;
    SendThumbnailCallback send_callback;
  };

  // Map of page indices to pending thumbnail requests.
  base::flat_map<int, PendingThumbnail> pending_thumbnails_;

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

  // When true, interactive portions of the content, such as forms and links,
  // are restricted.
  bool read_only_ = false;

  PDFiumPrint print_;

  base::WeakPtrFactory<PDFiumEngine> weak_factory_{this};

  // Weak pointers from this factory are used to bind the ContinueFind()
  // function. This allows those weak pointers to be invalidated during
  // StopFind(), and keeps the invalidation separated from `weak_factory_`.
  base::WeakPtrFactory<PDFiumEngine> find_weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_H_
