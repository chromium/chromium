// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ENGINE_H_
#define PDF_PDF_ENGINE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "pdf/document_layout.h"
#include "pdf/ui/thumbnail.h"
#include "printing/mojom/print.mojom-forward.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#endif

class SkBitmap;

namespace blink {
class WebInputEvent;
struct WebPrintParams;
}  // namespace blink

namespace gfx {
class Point;
class Size;
class Vector2d;
}  // namespace gfx

namespace chrome_pdf {

struct AccessibilityActionData;
struct AccessibilityFocusInfo;
struct AccessibilityLinkInfo;
struct AccessibilityHighlightInfo;
struct AccessibilityImageInfo;
struct AccessibilityTextFieldInfo;
struct AccessibilityTextRunInfo;
struct DocumentAttachmentInfo;
struct DocumentMetadata;

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
// If `enable_v8` is false, then the PDFEngine will not be able to run
// JavaScript.
// When `use_skia` is true, the PDFEngine will use Skia renderer. Otherwise, it
// will use AGG renderer.
void InitializeSDK(bool enable_v8,
                   bool use_skia,
                   FontMappingMode font_mapping_mode);
// Tells the SDK that we're shutting down.
void ShutdownSDK();

// This class encapsulates a PDF rendering engine.
class PDFEngine {
 public:
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

  virtual ~PDFEngine() = default;

  // Most of these functions are similar to the Pepper functions of the same
  // name, so not repeating the description here unless it's different.
  virtual void PageOffsetUpdated(const gfx::Vector2d& page_offset) = 0;
  virtual void PluginSizeUpdated(const gfx::Size& size) = 0;
  virtual void ScrolledToXPosition(int position) = 0;
  virtual void ScrolledToYPosition(int position) = 0;
  // Paint is called a series of times. Before these n calls are made, PrePaint
  // is called once. After Paint is called n times, PostPaint is called once.
  virtual void PrePaint() = 0;
  virtual void Paint(const gfx::Rect& rect,
                     SkBitmap& image_data,
                     std::vector<gfx::Rect>& ready,
                     std::vector<gfx::Rect>& pending) = 0;
  virtual void PostPaint() = 0;
  virtual bool HandleInputEvent(const blink::WebInputEvent& event) = 0;
  virtual void PrintBegin() = 0;
  virtual std::vector<uint8_t> PrintPages(
      const std::vector<int>& page_index,
      const blink::WebPrintParams& print_params) = 0;
  virtual void PrintEnd() = 0;
  virtual void StartFind(const std::u16string& text, bool case_sensitive) = 0;
  virtual bool SelectFindResult(bool forward) = 0;
  virtual void StopFind() = 0;
  virtual void ZoomUpdated(double new_zoom_level) = 0;
  virtual void RotateClockwise() = 0;
  virtual void RotateCounterclockwise() = 0;
  virtual bool IsReadOnly() const = 0;
  virtual void SetReadOnly(bool enable) = 0;
  virtual void SetDocumentLayout(DocumentLayout::PageSpread page_spread) = 0;
  virtual void DisplayAnnotations(bool display) = 0;

  // Applies the document layout options proposed by a call to
  // PDFiumEngineClient::ProposeDocumentLayout(), returning the overall size of
  // the new effective layout.
  virtual gfx::Size ApplyDocumentLayout(
      const DocumentLayout::Options& options) = 0;

  virtual std::string GetSelectedText() = 0;
  // Returns true if focus is within an editable form text area.
  virtual bool CanEditText() const = 0;
  // Returns true if focus is within an editable form text area and the text
  // area has text.
  virtual bool HasEditableText() const = 0;
  // Replace selected text within an editable form text area with another
  // string. If there is no selected text, append the replacement text after the
  // current caret position.
  virtual void ReplaceSelection(const std::string& text) = 0;
  // Methods to check if undo/redo is possible, and to perform them.
  virtual bool CanUndo() const = 0;
  virtual bool CanRedo() const = 0;
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  // Handles actions invoked by Accessibility clients.
  virtual void HandleAccessibilityAction(
      const AccessibilityActionData& action_data) = 0;
  virtual std::string GetLinkAtPosition(const gfx::Point& point) = 0;
  // Checks the permissions associated with this document.
  virtual bool HasPermission(DocumentPermission permission) const = 0;
  virtual void SelectAll() = 0;
  // Gets the list of DocumentAttachmentInfo from the document.
  virtual const std::vector<DocumentAttachmentInfo>&
  GetDocumentAttachmentInfoList() const = 0;
  // Gets the content of an attachment by the attachment's `index`. `index`
  // must be in the range of [0, attachment_count-1), where `attachment_count`
  // is the number of attachments embedded in the document.
  // The caller of this method is responsible for checking whether the
  // attachment is readable, attachment size is not 0 byte, and the return
  // value's size matches the corresponding DocumentAttachmentInfo's
  // `size_bytes`.
  virtual std::vector<uint8_t> GetAttachmentData(size_t index) = 0;
  // Gets metadata about the document.
  virtual const DocumentMetadata& GetDocumentMetadata() const = 0;
  // Gets the number of pages in the document.
  virtual int GetNumberOfPages() const = 0;
  // Gets the named destination by name.
  virtual std::optional<PDFEngine::NamedDestination> GetNamedDestination(
      const std::string& destination) = 0;
  // Gets the index of the most visible page, or -1 if none are visible.
  virtual int GetMostVisiblePage() = 0;
  // Returns whether the page at `index` is visible or not.
  virtual bool IsPageVisible(int index) const = 0;
  // Gets the current layout orientation.
  virtual PageOrientation GetCurrentOrientation() const = 0;
  // Gets the rectangle of the page not including the shadow.
  virtual gfx::Rect GetPageBoundsRect(int index) = 0;
  // Gets the rectangle of the page excluding any additional areas.
  virtual gfx::Rect GetPageContentsRect(int index) = 0;
  // Returns a page's rect in screen coordinates, as well as its surrounding
  // border areas and bottom separator.
  virtual gfx::Rect GetPageScreenRect(int page_index) const = 0;
  // Return a page's bounding box rectangle, or an empty rectangle if
  // `page_index` is invalid.
  virtual gfx::RectF GetPageBoundingBox(int page_index) = 0;
  // Set color / grayscale rendering modes.
  virtual void SetGrayscale(bool grayscale) = 0;
  // Get the number of characters on a given page.
  virtual int GetCharCount(int page_index) = 0;
  // Get the bounds in page pixels of a character on a given page.
  virtual gfx::RectF GetCharBounds(int page_index, int char_index) = 0;
  // Get a given unicode character on a given page.
  virtual uint32_t GetCharUnicode(int page_index, int char_index) = 0;
  // Given a start char index, find the longest continuous run of text that's
  // in a single direction and with the same text style. Return a filled out
  // AccessibilityTextRunInfo on success or std::nullopt on failure. e.g. When
  // `start_char_index` is out of bounds.
  virtual std::optional<AccessibilityTextRunInfo> GetTextRunInfo(
      int page_index,
      int start_char_index) = 0;
  // For all the links on page `page_index`, get their urls, underlying text
  // ranges and bounding boxes.
  virtual std::vector<AccessibilityLinkInfo> GetLinkInfo(
      int page_index,
      const std::vector<AccessibilityTextRunInfo>& text_runs) = 0;
  // For all the images in page `page_index`, get their alt texts and bounding
  // boxes. If the alt text is empty or unavailable, and if the user has
  // requested that the OCR service tag the PDF so that it is made accessible,
  // transfer the raw image pixels in the `image_data` field. Otherwise do not
  // populate the `image_data` field.
  virtual std::vector<AccessibilityImageInfo> GetImageInfo(
      int page_index,
      uint32_t text_run_count) = 0;
  // Returns the image as a 32-bit bitmap format for OCR.
  virtual SkBitmap GetImageForOcr(int page_index, int image_index) = 0;
  // For all the highlights in page `page_index`, get their underlying text
  // ranges and bounding boxes.
  virtual std::vector<AccessibilityHighlightInfo> GetHighlightInfo(
      int page_index,
      const std::vector<AccessibilityTextRunInfo>& text_runs) = 0;
  // For all the text fields in page `page_index`, get their properties like
  // name, value, bounding boxes etc.
  virtual std::vector<AccessibilityTextFieldInfo> GetTextFieldInfo(
      int page_index,
      uint32_t text_run_count) = 0;

  // Gets the PDF document's print scaling preference. True if the document can
  // be scaled to fit.
  virtual bool GetPrintScaling() = 0;
  // Returns number of copies to be printed.
  virtual int GetCopiesToPrint() = 0;
  // Returns the duplex setting.
  virtual printing::mojom::DuplexMode GetDuplexMode() = 0;
  // Returns the uniform page size of the document in points. Returns
  // `std::nullopt` if the document has more than one page size.
  virtual std::optional<gfx::Size> GetUniformPageSizePoints() = 0;

  // Returns a list of Values of Bookmarks. Each Bookmark is a dictionary Value
  // which contains the following key/values:
  // - "title" - a string Value.
  // - "page" - an int Value.
  // - "children" - a list of Values, with each entry containing
  //   a dictionary Value of the same structure.
  virtual base::Value::List GetBookmarks() = 0;

  // Append blank pages to make a 1-page document to a `num_pages` document.
  // Always retain the first page data.
  virtual void AppendBlankPages(size_t num_pages) = 0;
  // Append the first page of the document loaded with the `engine` to this
  // document at page `index`.
  virtual void AppendPage(PDFEngine* engine, int index) = 0;

  virtual std::vector<uint8_t> GetSaveData() = 0;

  virtual void SetCaretPosition(const gfx::Point& position) = 0;
  virtual void MoveRangeSelectionExtent(const gfx::Point& extent) = 0;
  virtual void SetSelectionBounds(const gfx::Point& base,
                                  const gfx::Point& extent) = 0;
  virtual void GetSelection(uint32_t* selection_start_page_index,
                            uint32_t* selection_start_char_index,
                            uint32_t* selection_end_page_index,
                            uint32_t* selection_end_char_index) = 0;

  // Remove focus from form widgets, consolidating the user input.
  virtual void KillFormFocus() = 0;

  // Notify whether the PDF currently has the focus or not.
  virtual void UpdateFocus(bool has_focus) = 0;

  // Returns the focus info of current focus item.
  virtual AccessibilityFocusInfo GetFocusInfo() = 0;

  virtual bool IsPDFDocTagged() = 0;

  virtual uint32_t GetLoadedByteSize() = 0;
  virtual bool ReadLoadedBytes(uint32_t length, void* buffer) = 0;

  // Requests for a thumbnail to be sent using a callback when the page is ready
  // to be rendered. `send_callback` is run with the thumbnail data when ready.
  virtual void RequestThumbnail(int page_index,
                                float device_pixel_ratio,
                                SendThumbnailCallback send_callback) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ENGINE_H_
