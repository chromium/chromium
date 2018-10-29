// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ENGINE_H_
#define PDF_PDF_ENGINE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/var_array.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_f.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)
typedef void (*PDFEnsureTypefaceCharactersAccessible)(const LOGFONT* font,
                                                      const wchar_t* text,
                                                      size_t text_length);
#endif

struct PP_PdfPrintSettings_Dev;

namespace gfx {
class Rect;
class Size;
}

namespace pp {
class InputEvent;
class VarDictionary;
}

namespace chrome_pdf {

// Do one time initialization of the SDK.
bool InitializeSDK();
// Tells the SDK that we're shutting down.
void ShutdownSDK();

// This class encapsulates a PDF rendering engine.
class PDFEngine {
 public:
  enum DocumentPermission {
    PERMISSION_COPY,
    PERMISSION_COPY_ACCESSIBLE,
    PERMISSION_PRINT_LOW_QUALITY,
    PERMISSION_PRINT_HIGH_QUALITY,
  };

  // Values other then |kCount| are persisted to logs as part of metric
  // collection, so should not be changed.
  enum class FormType {
    kNone = 0,
    kAcroForm = 1,
    kXFAFull = 2,
    kXFAForeground = 3,
    kCount = 4,
  };

  // Maximum number of parameters a nameddest view can contain.
  static constexpr size_t kMaxViewParams = 4;

  // Named destination in a document.
  struct NamedDestination {
    // 0-based page number.
    unsigned long page;

    // View fit type (see table 8.2 "Destination syntax" on page 582 of PDF
    // Reference 1.7). Empty string if not present.
    std::string view;

    // Number of parameters for the view.
    unsigned long num_params;

    // Parameters for the view. Their meaning depends on the |view| and their
    // number is defined by |num_params| but is at most |kMaxViewParams|.
    float params[kMaxViewParams];
  };

  // Features in a document that are relevant to measure.
  struct DocumentFeatures {
    // Number of pages in document.
    size_t page_count = 0;
    // Whether any files are attached to document (see "File Attachment
    // Annotations" on page 637 of PDF Reference 1.7).
    bool has_attachments = false;
    // Whether the document is linearized (see Appendix F "Linearized PDF" of
    // PDF Reference 1.7).
    bool is_linearized = false;
    // Whether the PDF is Tagged (see 10.7 "Tagged PDF" in PDF Reference 1.7).
    bool is_tagged = false;
    // What type of form the document contains.
    FormType form_type = FormType::kNone;
  };

  // Features in a page that are relevant to measure.
  struct PageFeatures {
    PageFeatures();
    PageFeatures(const PageFeatures& other);
    ~PageFeatures();

    // Whether the instance has been initialized and filled.
    bool IsInitialized() const;

    // 0-based page index in the document. < 0 when uninitialized.
    int index = -1;

    // Set of annotation types found in page.
    std::set<int> annotation_types;
  };

  // The interface that's provided to the rendering engine.
  class Client {
   public:
    virtual ~Client() {}

    // Informs the client about the document's size in pixels.
    virtual void DocumentSizeUpdated(const pp::Size& size) {}

    // Informs the client that the given rect needs to be repainted.
    virtual void Invalidate(const pp::Rect& rect) {}

    // Informs the client to scroll the plugin area by the given offset.
    virtual void DidScroll(const pp::Point& point) {}

    // Scroll the horizontal/vertical scrollbars to a given position.
    // Values are in screen coordinates, where 0 is the top/left of the document
    // and a positive value is the distance in pixels from that line.
    // For ScrollToY, setting |compensate_for_toolbar| will align the position
    // with the bottom of the toolbar so the given position is always visible.
    virtual void ScrollToX(int x_in_screen_coords) {}
    virtual void ScrollToY(int y_in_screen_coords,
                           bool compensate_for_toolbar) {}

    // Scroll by a given delta relative to the current position.
    virtual void ScrollBy(const pp::Point& point) {}

    // Scroll to zero-based |page|.
    virtual void ScrollToPage(int page) {}

    // Navigate to the given url.
    virtual void NavigateTo(const std::string& url,
                            WindowOpenDisposition disposition) {}

    // Updates the cursor.
    virtual void UpdateCursor(PP_CursorType_Dev cursor) {}

    // Updates the tick marks in the vertical scrollbar.
    virtual void UpdateTickMarks(const std::vector<pp::Rect>& tickmarks) {}

    // Updates the number of find results for the current search term.  If
    // there are no matches 0 should be passed in.  Only when the plugin has
    // finished searching should it pass in the final count with final_result
    // set to true.
    virtual void NotifyNumberOfFindResultsChanged(int total,
                                                  bool final_result) {}

    // Updates the index of the currently selected search item.
    virtual void NotifySelectedFindResultChanged(int current_find_index) {}

    // Notifies a page became visible.
    virtual void NotifyPageBecameVisible(
        const PDFEngine::PageFeatures* page_features) {}

    // Prompts the user for a password to open this document. The callback is
    // called when the password is retrieved.
    virtual void GetDocumentPassword(
        pp::CompletionCallbackWithOutput<pp::Var> callback) {}

    // Play a "beeping" sound.
    virtual void Beep() {}

    // Puts up an alert with the given message.
    virtual void Alert(const std::string& message) {}

    // Puts up a confirm with the given message, and returns true if the user
    // presses OK, or false if they press cancel.
    virtual bool Confirm(const std::string& message) = 0;

    // Puts up a prompt with the given message and default answer and returns
    // the answer.
    virtual std::string Prompt(const std::string& question,
                               const std::string& default_answer) = 0;

    // Returns the url of the pdf.
    virtual std::string GetURL() = 0;

    // Send an email.
    virtual void Email(const std::string& to,
                       const std::string& cc,
                       const std::string& bcc,
                       const std::string& subject,
                       const std::string& body) {}

    // Put up the print dialog.
    virtual void Print() {}

    // Submit the data using HTTP POST.
    virtual void SubmitForm(const std::string& url,
                            const void* data,
                            int length) {}

    // Creates and returns new URL loader for partial document requests.
    virtual pp::URLLoader CreateURLLoader() = 0;

    // Searches the given string for "term" and returns the results.  Unicode-
    // aware.
    struct SearchStringResult {
      int start_index;
      int length;
    };
    virtual std::vector<SearchStringResult> SearchString(
        const base::char16* string,
        const base::char16* term,
        bool case_sensitive) = 0;

    // Notifies the client that the engine has painted a page from the document.
    virtual void DocumentPaintOccurred() {}

    // Notifies the client that the document has finished loading.
    virtual void DocumentLoadComplete(const DocumentFeatures& document_features,
                                      uint32_t file_size) {}

    // Notifies the client that the document has failed to load.
    virtual void DocumentLoadFailed() {}

    // Notifies the client that the document has requested substitute fonts.
    virtual void FontSubstituted() {}

    virtual pp::Instance* GetPluginInstance() = 0;

    // Notifies that an unsupported feature in the PDF was encountered.
    virtual void DocumentHasUnsupportedFeature(const std::string& feature) {}

    // Notifies the client about document load progress.
    virtual void DocumentLoadProgress(uint32_t available, uint32_t doc_size) {}

    // Notifies the client about focus changes for form text fields.
    virtual void FormTextFieldFocusChange(bool in_focus) {}

    // Returns true if the plugin has been opened within print preview.
    virtual bool IsPrintPreview() = 0;

    // Get the background color of the PDF.
    virtual uint32_t GetBackgroundColor() = 0;

    // Cancel browser initiated document download.
    virtual void CancelBrowserDownload() {}

    // Sets selection status.
    virtual void IsSelectingChanged(bool is_selecting) {}

    virtual void SelectionChanged(const pp::Rect& left, const pp::Rect& right) {
    }

    // Sets edit mode state.
    virtual void IsEditModeChanged(bool is_edit_mode) {}

    // Gets the height of the top toolbar in screen coordinates. This is
    // independent of whether it is hidden or not at the moment.
    virtual float GetToolbarHeightInScreenCoords() = 0;
  };

  // Factory method to create an instance of the PDF Engine.
  static std::unique_ptr<PDFEngine> Create(Client* client,
                                           bool enable_javascript);

  virtual ~PDFEngine() {}

  // Most of these functions are similar to the Pepper functions of the same
  // name, so not repeating the description here unless it's different.
  virtual bool New(const char* url, const char* headers) = 0;
  virtual void PageOffsetUpdated(const pp::Point& page_offset) = 0;
  virtual void PluginSizeUpdated(const pp::Size& size) = 0;
  virtual void ScrolledToXPosition(int position) = 0;
  virtual void ScrolledToYPosition(int position) = 0;
  // Paint is called a series of times. Before these n calls are made, PrePaint
  // is called once. After Paint is called n times, PostPaint is called once.
  virtual void PrePaint() = 0;
  virtual void Paint(const pp::Rect& rect,
                     pp::ImageData* image_data,
                     std::vector<pp::Rect>* ready,
                     std::vector<pp::Rect>* pending) = 0;
  virtual void PostPaint() = 0;
  virtual bool HandleDocumentLoad(const pp::URLLoader& loader) = 0;
  virtual bool HandleEvent(const pp::InputEvent& event) = 0;
  virtual uint32_t QuerySupportedPrintOutputFormats() = 0;
  virtual void PrintBegin() = 0;
  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings) = 0;
  virtual void PrintEnd() = 0;
  virtual void StartFind(const std::string& text, bool case_sensitive) = 0;
  virtual bool SelectFindResult(bool forward) = 0;
  virtual void StopFind() = 0;
  virtual void ZoomUpdated(double new_zoom_level) = 0;
  virtual void RotateClockwise() = 0;
  virtual void RotateCounterclockwise() = 0;
  virtual std::string GetSelectedText() = 0;
  // Returns true if focus is within an editable form text area.
  virtual bool CanEditText() = 0;
  // Returns true if focus is within an editable form text area and the text
  // area has text.
  virtual bool HasEditableText() = 0;
  // Replace selected text within an editable form text area with another
  // string. If there is no selected text, append the replacement text after the
  // current caret position.
  virtual void ReplaceSelection(const std::string& text) = 0;
  // Methods to check if undo/redo is possible, and to perform them.
  virtual bool CanUndo() = 0;
  virtual bool CanRedo() = 0;
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual std::string GetLinkAtPosition(const pp::Point& point) = 0;
  // Checks the permissions associated with this document.
  virtual bool HasPermission(DocumentPermission permission) const = 0;
  virtual void SelectAll() = 0;
  // Gets the number of pages in the document.
  virtual int GetNumberOfPages() = 0;
  // Gets the named destination by name.
  virtual base::Optional<PDFEngine::NamedDestination> GetNamedDestination(
      const std::string& destination) = 0;
  // Transforms an (x, y) point in page coordinates to screen coordinates.
  virtual gfx::PointF TransformPagePoint(int page_index,
                                         const gfx::PointF& page_xy) = 0;
  // Gets the index of the most visible page, or -1 if none are visible.
  virtual int GetMostVisiblePage() = 0;
  // Gets the rectangle of the page including shadow.
  virtual pp::Rect GetPageRect(int index) = 0;
  // Gets the rectangle of the page not including the shadow.
  virtual pp::Rect GetPageBoundsRect(int index) = 0;
  // Gets the rectangle of the page excluding any additional areas.
  virtual pp::Rect GetPageContentsRect(int index) = 0;
  // Returns a page's rect in screen coordinates, as well as its surrounding
  // border areas and bottom separator.
  virtual pp::Rect GetPageScreenRect(int page_index) const = 0;
  // Gets the offset of the vertical scrollbar from the top in document
  // coordinates.
  virtual int GetVerticalScrollbarYPosition() = 0;
  // Set color / grayscale rendering modes.
  virtual void SetGrayscale(bool grayscale) = 0;
  // Get the number of characters on a given page.
  virtual int GetCharCount(int page_index) = 0;
  // Get the bounds in page pixels of a character on a given page.
  virtual pp::FloatRect GetCharBounds(int page_index, int char_index) = 0;
  // Get a given unicode character on a given page.
  virtual uint32_t GetCharUnicode(int page_index, int char_index) = 0;
  // Given a start char index, find the longest continuous run of text that's
  // in a single direction and with the same style and font size. Return the
  // length of that sequence and its font size and bounding box.
  virtual void GetTextRunInfo(int page_index,
                              int start_char_index,
                              uint32_t* out_len,
                              double* out_font_size,
                              pp::FloatRect* out_bounds) = 0;
  // Gets the PDF document's print scaling preference. True if the document can
  // be scaled to fit.
  virtual bool GetPrintScaling() = 0;
  // Returns number of copies to be printed.
  virtual int GetCopiesToPrint() = 0;
  // Returns the duplex setting.
  virtual int GetDuplexType() = 0;
  // Returns true if all the pages are the same size.
  virtual bool GetPageSizeAndUniformity(pp::Size* size) = 0;

  // Returns a VarArray of Bookmarks, each a VarDictionary containing the
  // following key/values:
  // - "title" - a string Var.
  // - "page" - an int Var.
  // - "children" - a VarArray(), with each entry containing a VarDictionary of
  //   the same structure.
  virtual pp::VarArray GetBookmarks() = 0;

  // Append blank pages to make a 1-page document to a |num_pages| document.
  // Always retain the first page data.
  virtual void AppendBlankPages(int num_pages) = 0;
  // Append the first page of the document loaded with the |engine| to this
  // document at page |index|.
  virtual void AppendPage(PDFEngine* engine, int index) = 0;

  virtual std::string GetMetadata(const std::string& key) = 0;
  virtual std::vector<uint8_t> GetSaveData() = 0;

  virtual void SetCaretPosition(const pp::Point& position) = 0;
  virtual void MoveRangeSelectionExtent(const pp::Point& extent) = 0;
  virtual void SetSelectionBounds(const pp::Point& base,
                                  const pp::Point& extent) = 0;
  virtual void GetSelection(uint32_t* selection_start_page_index,
                            uint32_t* selection_start_char_index,
                            uint32_t* selection_end_page_index,
                            uint32_t* selection_end_char_index) = 0;

  // Remove focus from form widgets, consolidating the user input.
  virtual void KillFormFocus() = 0;
};

// Interface for exports that wrap the PDF engine.
class PDFEngineExports {
 public:
  struct RenderingSettings {
    RenderingSettings(int dpi_x,
                      int dpi_y,
                      const pp::Rect& bounds,
                      bool fit_to_bounds,
                      bool stretch_to_bounds,
                      bool keep_aspect_ratio,
                      bool center_in_bounds,
                      bool autorotate,
                      bool use_color);
    RenderingSettings(const RenderingSettings& that);

    int dpi_x;
    int dpi_y;
    pp::Rect bounds;
    bool fit_to_bounds;
    bool stretch_to_bounds;
    bool keep_aspect_ratio;
    bool center_in_bounds;
    bool autorotate;
    bool use_color;
  };

  PDFEngineExports() {}
  virtual ~PDFEngineExports() {}

  static PDFEngineExports* Get();

#if defined(OS_WIN)
  // See the definition of RenderPDFPageToDC in pdf.cc for details.
  virtual bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                                 int page_number,
                                 const RenderingSettings& settings,
                                 HDC dc) = 0;

  virtual void SetPDFEnsureTypefaceCharactersAccessible(
      PDFEnsureTypefaceCharactersAccessible func) = 0;

  virtual void SetPDFUseGDIPrinting(bool enable) = 0;
  virtual void SetPDFUsePrintMode(int mode) = 0;
#endif  // defined(OS_WIN)

  // See the definition of RenderPDFPageToBitmap in pdf.cc for details.
  virtual bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                                     int page_number,
                                     const RenderingSettings& settings,
                                     void* bitmap_buffer) = 0;

  // See the definition of ConvertPdfPagesToNupPdf in pdf.cc for details.
  virtual std::vector<uint8_t> ConvertPdfPagesToNupPdf(
      std::vector<base::span<const uint8_t>> input_buffers,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) = 0;

  // See the definition of ConvertPdfDocumentToNupPdf in pdf.cc for details.
  virtual std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
      base::span<const uint8_t> input_buffer,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) = 0;

  virtual bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                             int* page_count,
                             double* max_page_width) = 0;

  // See the definition of GetPDFPageSizeByIndex in pdf.cc for details.
  virtual bool GetPDFPageSizeByIndex(base::span<const uint8_t> pdf_buffer,
                                     int page_number,
                                     double* width,
                                     double* height) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ENGINE_H_
