// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PAGE_H_
#define PDF_PDFIUM_PDFIUM_PAGE_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "pdf/ui/thumbnail.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_doc.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_text.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "ui/gfx/geometry/size.h"
#endif

namespace gfx {
class Point;
class RectF;
}  // namespace gfx

namespace chrome_pdf {

class PDFiumEngine;
class PdfRect;
class Thumbnail;
struct AccessibilityCharInfo;
struct AccessibilityHighlightInfo;
struct AccessibilityImageInfo;
struct AccessibilityLinkInfo;
struct AccessibilityStructureElement;
struct AccessibilityTextFieldInfo;
struct AccessibilityTextRunInfo;

// Wrapper around a page from the document.
class PDFiumPage {
 public:
  class ScopedUnloadPreventer {
   public:
    explicit ScopedUnloadPreventer(PDFiumPage* page);
    ScopedUnloadPreventer(const ScopedUnloadPreventer& that);
    ScopedUnloadPreventer& operator=(const ScopedUnloadPreventer& that);
    ~ScopedUnloadPreventer();

   private:
    raw_ptr<PDFiumPage> page_;
  };

  PDFiumPage(PDFiumEngine* engine, uint32_t i);
  PDFiumPage(const PDFiumPage&) = delete;
  PDFiumPage& operator=(const PDFiumPage&) = delete;
  PDFiumPage(PDFiumPage&& that);
  ~PDFiumPage();

  // Unloads the PDFium data for this page from memory.
  void Unload();

  // Gets the FPDF_PAGE for this page, loading and parsing it if necessary.
  FPDF_PAGE GetPage();

  // Returns FPDF_TEXTPAGE for the page, loading and parsing it if necessary.
  FPDF_TEXTPAGE GetTextPage();

  // Gets the number of characters in the page.
  int GetCharCount();

  // Resets loaded text and loads it again.
  void ReloadTextPage();

  // Get all the chars from the page.
  std::vector<AccessibilityCharInfo> GetCharInfo();

  // Returns a copy of the structure tree which describes the logical
  // organization of the current PDF page, if present.
  std::unique_ptr<AccessibilityStructureElement> GetStructureTree();

  // Gets all the text runs from the page.
  std::vector<AccessibilityTextRunInfo> GetTextRunInfo();

  // Returns the text run that contains `char_index`. Return a filled out
  // AccessibilityTextRunInfo on success or std::nullopt on failure. e.g. When
  // `char_index` is out of bounds.
  std::optional<AccessibilityTextRunInfo> GetTextRunInfoAt(int char_index);

  // Get a unicode character from the page.
  uint32_t GetCharUnicode(int char_index);

  // Get the bounds of a character in page pixels.
  gfx::RectF GetCharBounds(int char_index);

  // Get the bounds of the page with the crop box applied, in page pixels.
  gfx::RectF GetCroppedRect();

  // Get the bounding box of the page in page pixels. The bounding box is the
  // largest rectangle containing all visible content in the effective crop box.
  // If the bounding box can't be calculated, returns the effective crop box.
  // The resulting bounding box is relative to the effective crop box.
  gfx::RectF GetBoundingBox();

  // Returns if the character at `char_index` is within `page_bounds`.
  bool IsCharInPageBounds(int char_index, const gfx::RectF& page_bounds);

  // For all the links on the page, get their urls, underlying text ranges and
  // bounding boxes.
  std::vector<AccessibilityLinkInfo> GetLinkInfo();

  // For all the images on the page, get their alt texts and bounding boxes. If
  // the alt text is empty or unavailable, and if the user has requested that
  // the OCR service tag the PDF so that it is made accessible, transfer the raw
  // image pixels in the `image_data` field. Otherwise do not populate the
  // `image_data` field.
  std::vector<AccessibilityImageInfo> GetImageInfo();

  // Returns the indices of image objects.
  std::vector<int> GetImageObjectIndices();

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Returns if the page has images.
  bool HasImages();

  // Returns the image as a 32-bit bitmap format for OCR. The image dimensions
  // will be at most `max_image_dimension`.
  SkBitmap GetImageForOcr(int page_object_index, int max_image_dimension);

  // Called to inform PDFiumPage that OCR operations performed on this page
  // added text into the page or not.
  // May only be called once per PDFiumPage instance.
  void OnSearchifyGotOcrResult(bool added_text);

  // Returns if Searchify has run on the page, regardless of whether it added
  // any text to the page or not.
  bool IsPageSearchified() const;

  // Returns if the page can be unloaded.
  bool PageCanBeUnloaded() const;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  // For all the highlights on the page, get their underlying text ranges and
  // bounding boxes.
  std::vector<AccessibilityHighlightInfo> GetHighlightInfo();

  // For all the text fields on the page, get their properties like name,
  // value, bounding boxes, etc.
  std::vector<AccessibilityTextFieldInfo> GetTextFieldInfo();

  // Traverses the entire struct tree of the page recursively and extracts the
  // text run type or the alt text from struct tree elements corresponding to
  // the marked content IDs present in `marked_content_id_to_text_runs_map_` or
  // `marked_content_id_to_images_map_` respectively.
  void PopulateTextRunTypeAndImageAltText();

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
    // Valid for DOCLINK_AREA only. From the top-left of the page.
    std::optional<float> x_in_pixels;
    std::optional<float> y_in_pixels;
    // Valid for DOCLINK_AREA only.
    std::optional<float> zoom;
  };

  // Given a `link_index`, returns the type of underlying area and the link
  // target. `target` must be valid. Returns NONSELECTABLE_AREA if
  // `link_index` is invalid.
  Area GetLinkTargetAtIndex(int link_index, LinkTarget* target);

  // Returns link type and fills target associated with a link. Returns
  // NONSELECTABLE_AREA if link detection failed.
  Area GetLinkTarget(FPDF_LINK link, LinkTarget* target);

  // Fills the output params with the PDF coordinates and the zoom value of the
  // destination.
  void GetPageDestinationTarget(FPDF_DEST destination,
                                std::optional<float>* dest_x,
                                std::optional<float>* dest_y,
                                std::optional<float>* zoom_value);

  // For a named destination with "XYZ" view fit type, pre-processes the PDF
  // x/y coordinate in case it's out of the range of the page dimension. Then
  // transform it to a screen coordinate.
  float PreProcessAndTransformInPageCoordX(float x);
  float PreProcessAndTransformInPageCoordY(float y);

  // Transforms an (x, y) position in PDF coordinates to screen coordinates.
  gfx::PointF TransformPageToScreenXY(const gfx::PointF& xy);

  // Transforms an PDF x coordinate to its value in screen coordinates.
  float TransformPageToScreenX(float x);

  // Transforms an PDF y coordinate to its value in screen coordinates.
  float TransformPageToScreenY(float y);

  // Given a point in this page in PDF coordinates, returns the area type, or
  // `NONSELECTABLE_AREA` if the area type cannot be determined.
  //
  // Also writes to the following non-optional out-parameters:
  // - `char_index`, if `point` is near a character.
  // - `char_bounds`, if `char_index` is valid.
  // - `form_type, if `point` is over a form.
  // - `target`, if the area is `WEBLINK_AREA` or `DOCLINK_AREA`.
  Area GetCharInfo(const gfx::PointF& point,
                   int* char_index,
                   PdfRect* char_bounds,
                   int* form_type,
                   LinkTarget* target);

  // Converts a form type to its corresponding Area.
  static Area FormTypeToArea(int form_type);

  // Returns true if the given `char_index` lies within the character range
  // of the page.
  bool IsCharIndexInBounds(int char_index);

  // Given a rectangle in PDF coordinates, computes the range of continuous
  // characters which lie inside that rectangle. Returns false without
  // modifying the out parameters if no character lies inside the rectangle.
  bool GetUnderlyingTextRangeForRect(const gfx::RectF& rect,
                                     int* start_index,
                                     int* char_len);

  // Converts from PDF coordinates to screen coordinates.
  gfx::Rect PageToScreen(const gfx::Point& page_point,
                         double zoom,
                         const PdfRect& rect,
                         PageOrientation orientation) const;
  gfx::Rect PageToScreen(const gfx::Point& page_point,
                         double zoom,
                         double left,
                         double top,
                         double right,
                         double bottom,
                         PageOrientation orientation) const;

  // Sets the callbacks for sending the thumbnail.
  void RequestThumbnail(float device_pixel_ratio,
                        SendThumbnailCallback send_callback);

  // Generates a page thumbnail accommodating a specific `device_pixel_ratio`.
  Thumbnail GenerateThumbnail(float device_pixel_ratio);

#if BUILDFLAG(ENABLE_PDF_INK2)
  gfx::Size GetThumbnailSize(float device_pixel_ratio);
#endif

  uint32_t index() const { return index_; }

  const gfx::Rect& rect() const { return rect_; }
  void set_rect(const gfx::Rect& r) { rect_ = r; }

  // Availability is a one-way transition: A page can become available, but it
  // cannot become unavailable (unless deleted entirely).
  bool available() const { return available_; }
  void MarkAvailable();

  void set_calculated_links(bool calculated_links) {
    calculated_links_ = calculated_links;
  }

  FPDF_PAGE page() const { return page_.get(); }
  FPDF_TEXTPAGE text_page() const { return text_page_.get(); }

 private:
  friend class PDFiumPageLinkTest;
  friend class PDFiumTestBase;

  FRIEND_TEST_ALL_PREFIXES(PDFiumPageButtonTest, PopulateButtons);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageChoiceFieldTest, PopulateChoiceFields);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageHighlightTest, PopulateHighlights);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageForOcrTest, LowResolutionImage);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageForOcrTest, HighResolutionImage);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageForOcrTest, RotatedPage);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageForOcrTest, NonImage);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageTest, ImagesWithAltText);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageImageTest, TextAndImagesWithAltText);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageLinkTest, AnnotLinkGeneration);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageLinkTest, GetLinkTarget);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageLinkTest, GetUTF8LinkTarget);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageLinkTest, LinkGeneration);
  FRIEND_TEST_ALL_PREFIXES(PDFiumPageTextFieldTest, PopulateTextFields);

  // Key: Marked content id for the text element as specified in the struct
  //      tree.
  // Value: A list of indices in the `text_runs_` vector.
  using MarkedContentIdToTextRunInfoMap = std::map<int, std::vector<size_t>>;

  // Key: Marked content id for the image element as specified in the struct
  //      tree.
  // Value: Index of the image in the `images_` vector.
  using MarkedContentIdToImageMap = std::map<int, size_t>;

  struct Link {
    Link();
    Link(const Link& that);
    ~Link();

    // Represents start index of underlying text range. Should be -1 if the link
    // is not over text.
    int32_t start_char_index = -1;
    // Represents the number of characters that the link overlaps with.
    int32_t char_count = 0;
    std::vector<gfx::Rect> bounding_rects;
    LinkTarget target;
  };

  // Represents an Image inside the page.
  struct Image {
    Image();
    Image(const Image& other);
    ~Image();

    // Index of the object in its page.
    int page_object_index;

    // Alt text is available only for PDFs that are tagged for accessibility.
    std::string alt_text;
    gfx::Rect bounding_rect;
  };

  // Represents a highlight within the page.
  struct Highlight {
    Highlight();
    Highlight(const Highlight& other);
    ~Highlight();

    // Start index of underlying text range. -1 indicates invalid value.
    int32_t start_char_index = -1;
    // Number of characters encompassed by this highlight.
    int32_t char_count = 0;
    gfx::Rect bounding_rect;

    // Color of the highlight in ARGB. Alpha is stored in the first 8 MSBs. RGB
    // follows after it with each using 8 bytes.
    uint32_t color;

    // Text of the popup note associated with highlight.
    std::string note_text;
  };

  // Represents a form field within the page.
  struct FormField {
    FormField();
    FormField(const FormField& other);
    ~FormField();

    gfx::Rect bounding_rect;
    // Represents the name of form field as defined in the field dictionary.
    std::string name;
    // Represents the flags of form field as defined in the field dictionary.
    int flags;
  };

  // Represents a text field within the page.
  struct TextField : FormField {
    TextField();
    TextField(const TextField& other);
    ~TextField();

    std::string value;
  };

  // Represents a choice field option.
  struct ChoiceFieldOption {
    ChoiceFieldOption();
    ChoiceFieldOption(const ChoiceFieldOption& other);
    ~ChoiceFieldOption();

    std::string name;
    bool is_selected;
  };

  // Represents a choice field within the page.
  struct ChoiceField : FormField {
    ChoiceField();
    ChoiceField(const ChoiceField& other);
    ~ChoiceField();

    std::vector<ChoiceFieldOption> options;
  };

  // Represents a button within the page.
  struct Button : FormField {
    Button();
    Button(const Button& other);
    ~Button();

    std::string value;
    // A button can be of type radio, checkbox or push button.
    int type;
    // Represents if the radio button or checkbox is checked.
    bool is_checked = false;
    // Represents count of controls in the control group. A group of
    // interactive form annotations is collectively called a form control
    // group. Here an interactive form annotation should be either a radio
    // button or a checkbox.
    uint32_t control_count = 0;
    // Represents index of the control in the control group. A group of
    // interactive form annotations is collectively called a form control
    // group. Here an interactive form annotation should be either a radio
    // button or a checkbox. Value of `control_index` is -1 for push button.
    int control_index = -1;
  };

  // Returns a link index if the given character index is over a link, or -1
  // otherwise.
  int GetLink(int char_index, LinkTarget* target);
  // Calculate the locations of any links on the page.
  void CalculateLinks();
  // Populates weblinks on the page.
  void PopulateWebLinks();
  // Populates annotation links on the page.
  void PopulateAnnotationLinks();
  // Calculate the locations of images on the page.
  void CalculateImages();
  // Populate annotations like highlight and text field on the page.
  void PopulateAnnotations();
  // Populate `highlights_` with `annot`.
  void PopulateHighlight(FPDF_ANNOTATION annot);
  // Populate `text_fields_` with `annot`.
  void PopulateTextField(FPDF_ANNOTATION annot);
  // Populate `choice_fields_` with `annot`.
  void PopulateChoiceField(FPDF_ANNOTATION annot);
  // Populate `buttons_` with `annot`.
  void PopulateButton(FPDF_ANNOTATION annot);
  // Populate form fields like text field, choice field and button on the page.
  void PopulateFormField(FPDF_ANNOTATION annot);
  // Returns link type and fills target associated with a destination. Returns
  // NONSELECTABLE_AREA if detection failed.
  Area GetDestinationTarget(FPDF_DEST destination, LinkTarget* target);
  // Returns link type and fills target associated with a URI action. Returns
  // NONSELECTABLE_AREA if detection failed.
  Area GetURITarget(FPDF_ACTION uri_action, LinkTarget* target) const;
  // Calculates the set of character indices on which text runs need to be
  // broken for page objects such as links and images.
  void CalculatePageObjectTextRunBreaks();
  // Calculate and caches all text runs and character information on the page.
  void CalculateTextRuns();
  // Calculates and return a text run that starts exactly at `start_char_index`.
  AccessibilityTextRunInfo CalculateTextRunInfoAt(uint32_t start_char_index,
                                                  uint32_t chars_count);

  // Traverses a struct element and its sub-tree recursively and extracts the
  // text run type or the alt text from struct elements corresponding to the
  // marked content IDs present in `marked_content_id_to_text_runs_map_` or
  // `marked_content_id_to_images_map_` respectively. Uses `visited_elements` to
  // guard against malformed struct trees.
  void PopulateTextRunTypeAndImageAltTextForStructElement(
      FPDF_STRUCTELEMENT current_element,
      std::set<FPDF_STRUCTELEMENT>& visited_elements);

  // Traverses a structure element and its subtree recursively and extracts all
  // information, storing it in a corresponding hierarchy of
  // `AccessibilityStructureElement`s. Also, extracts the text run type or the
  // alt text from structure elements corresponding to the marked content IDs
  // present in `marked_content_id_to_text_runs_map_` or
  // `marked_content_id_to_images_map_` respectively. Uses `visited_elements` to
  // guard against malformed structure trees.
  std::unique_ptr<AccessibilityStructureElement> GetStructureSubtree(
      FPDF_STRUCTELEMENT element,
      std::set<FPDF_STRUCTELEMENT>& visited_elements);

  bool PopulateFormFieldProperties(FPDF_ANNOTATION annot,
                                   FormField* form_field);

  // Generates and sends the thumbnail using `send_callback`.
  void GenerateAndSendThumbnail(float device_pixel_ratio,
                                SendThumbnailCallback send_callback);

  raw_ptr<PDFiumEngine> engine_;
  ScopedFPDFPage page_;
  ScopedFPDFTextPage text_page_;
  uint32_t index_;
  int preventing_unload_count_ = 0;
  gfx::Rect rect_;
  bool calculated_text_runs_ = false;
  MarkedContentIdToTextRunInfoMap marked_content_id_to_text_runs_map_;
  // Text runs are sorted by character index.
  std::vector<AccessibilityTextRunInfo> text_runs_;
  bool calculated_links_ = false;
  std::vector<Link> links_;
  bool calculated_images_ = false;
  MarkedContentIdToImageMap marked_content_id_to_images_map_;
  std::vector<Image> images_;
  bool calculated_annotations_ = false;
  std::vector<Highlight> highlights_;
  std::vector<TextField> text_fields_;
  std::vector<ChoiceField> choice_fields_;
  std::vector<Button> buttons_;
  bool calculated_page_object_text_run_breaks_ = false;
  // The set of character indices on which text runs need to be broken for page
  // objects.
  std::set<int> page_object_text_run_breaks_;
  base::OnceClosure thumbnail_callback_;
  bool available_ = false;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Indicates whether Searchify added text to this page or not. Note that if
  // this page has never been Searchified, then this is null.
  std::optional<bool> has_searchify_added_text_;
#endif
};

constexpr uint32_t MakeARGB(unsigned int a,
                            unsigned int r,
                            unsigned int g,
                            unsigned int b) {
  return (a << 24) | (r << 16) | (g << 8) | b;
}

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PAGE_H_
