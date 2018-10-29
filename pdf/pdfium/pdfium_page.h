// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PAGE_H_
#define PDF_PDFIUM_PDFIUM_PAGE_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "pdf/pdf_engine.h"
#include "ppapi/cpp/rect.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_doc.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_text.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

class PDFiumEngine;

// Wrapper around a page from the document.
class PDFiumPage {
 public:
  PDFiumPage(PDFiumEngine* engine, int i, const pp::Rect& r, bool available);
  PDFiumPage(PDFiumPage&& that);
  ~PDFiumPage();

  // Unloads the PDFium data for this page from memory.
  void Unload();
  // Gets the FPDF_PAGE for this page, loading and parsing it if necessary.
  FPDF_PAGE GetPage();

  // Returns FPDF_TEXTPAGE for the page, loading and parsing it if necessary.
  FPDF_TEXTPAGE GetTextPage();

  // Given a start char index, find the longest continuous run of text that's
  // in a single direction and with the same style and font size. Return the
  // length of that sequence and its font size and bounding box.
  void GetTextRunInfo(int start_char_index,
                      uint32_t* out_len,
                      double* out_font_size,
                      pp::FloatRect* out_bounds);
  // Get a unicode character from the page.
  uint32_t GetCharUnicode(int char_index);
  // Get the bounds of a character in page pixels.
  pp::FloatRect GetCharBounds(int char_index);

  enum Area {
    NONSELECTABLE_AREA,
    TEXT_AREA,       // Area contains regular, selectable text not
                     // within form fields.
    WEBLINK_AREA,    // Area is a hyperlink.
    DOCLINK_AREA,    // Area is a link to a different part of the same
                     // document.
    FORM_TEXT_AREA,  // Area is a form text field or form combobox text
                     // field.
  };

  struct LinkTarget {
    LinkTarget();
    LinkTarget(const LinkTarget& other);
    ~LinkTarget();

    // Valid for WEBLINK_AREA only.
    std::string url;

    // Valid for DOCLINK_AREA only.
    int page;
    // Valid for DOCLINK_AREA only. From the top of the page.
    base::Optional<float> y_in_pixels;
  };

  // Returns the (x, y) position of a destination in page coordinates.
  base::Optional<gfx::PointF> GetPageXYTarget(FPDF_DEST destination);

  // Transforms an (x, y) position in page coordinates to screen coordinates.
  gfx::PointF TransformPageToScreenXY(const gfx::PointF& xy);

  // Given a point in the document that's in this page, returns its character
  // index if it's near a character, and also the type of text.
  // Target is optional. It will be filled in for WEBLINK_AREA or
  // DOCLINK_AREA only.
  Area GetCharIndex(const pp::Point& point,
                    int rotation,
                    int* char_index,
                    int* form_type,
                    LinkTarget* target);

  // Converts a form type to its corresponding Area.
  static Area FormTypeToArea(int form_type);

  // Gets the character at the given index.
  base::char16 GetCharAtIndex(int index);

  // Gets the number of characters in the page.
  int GetCharCount();

  // Converts from page coordinates to screen coordinates.
  pp::Rect PageToScreen(const pp::Point& offset,
                        double zoom,
                        double left,
                        double top,
                        double right,
                        double bottom,
                        int rotation) const;

  const PDFEngine::PageFeatures* GetPageFeatures();

  int index() const { return index_; }
  pp::Rect rect() const { return rect_; }
  void set_rect(const pp::Rect& r) { rect_ = r; }
  bool available() const { return available_; }
  void set_available(bool available) { available_ = available; }
  void set_calculated_links(bool calculated_links) {
    calculated_links_ = calculated_links;
  }

  FPDF_PAGE page() const { return page_.get(); }
  FPDF_TEXTPAGE text_page() const { return text_page_.get(); }

 private:
  // Returns a link index if the given character index is over a link, or -1
  // otherwise.
  int GetLink(int char_index, LinkTarget* target);
  // Returns the link indices if the given rect intersects a link rect, or an
  // empty vector otherwise.
  std::vector<int> GetLinks(pp::Rect text_area,
                            std::vector<LinkTarget>* targets);
  // Calculate the locations of any links on the page.
  void CalculateLinks();
  // Returns link type and fills target associated with a link. Returns
  // NONSELECTABLE_AREA if link detection failed.
  Area GetLinkTarget(FPDF_LINK link, LinkTarget* target);
  // Returns link type and fills target associated with a destination. Returns
  // NONSELECTABLE_AREA if detection failed.
  Area GetDestinationTarget(FPDF_DEST destination, LinkTarget* target);
  // Returns link type and fills target associated with a URI action. Returns
  // NONSELECTABLE_AREA if detection failed.
  Area GetURITarget(FPDF_ACTION uri_action, LinkTarget* target) const;

  class ScopedUnloadPreventer {
   public:
    explicit ScopedUnloadPreventer(PDFiumPage* page);
    ~ScopedUnloadPreventer();

   private:
    PDFiumPage* const page_;
  };

  struct Link {
    Link();
    Link(const Link& that);
    ~Link();

    std::string url;
    // Bounding rectangles of characters.
    std::vector<pp::Rect> rects;
  };

  PDFiumEngine* engine_;
  ScopedFPDFPage page_;
  ScopedFPDFTextPage text_page_;
  int index_;
  int preventing_unload_count_ = 0;
  pp::Rect rect_;
  bool calculated_links_ = false;
  std::vector<Link> links_;
  bool available_;
  PDFEngine::PageFeatures page_features_;

  DISALLOW_COPY_AND_ASSIGN(PDFiumPage);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PAGE_H_
