// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_ACCESSIBILITY_STRUCTS_H_
#define PDF_ACCESSIBILITY_STRUCTS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

struct AccessibilityDocInfo {
  bool operator==(const AccessibilityDocInfo& other) const;
  bool operator!=(const AccessibilityDocInfo& other) const;

  uint32_t page_count = 0;
  bool text_accessible = false;
  bool text_copyable = false;
};

struct AccessibilityPageInfo {
  uint32_t page_index = 0;
  gfx::Rect bounds;
  uint32_t text_run_count = 0;
  uint32_t char_count = 0;
};

// See PDF Reference 1.7, page 402, table 5.3.
enum class AccessibilityTextRenderMode {
  kUnknown = -1,
  kFill = 0,
  kStroke = 1,
  kFillStroke = 2,
  kInvisible = 3,
  kFillClip = 4,
  kStrokeClip = 5,
  kFillStrokeClip = 6,
  kClip = 7,
  kMaxValue = kClip,
};

struct AccessibilityTextStyleInfo {
  AccessibilityTextStyleInfo();
  AccessibilityTextStyleInfo(const std::string& font_name,
                             int font_weight,
                             AccessibilityTextRenderMode render_mode,
                             float font_size,
                             uint32_t fill_color,
                             uint32_t stroke_color,
                             bool is_italic,
                             bool is_bold);
  AccessibilityTextStyleInfo(const AccessibilityTextStyleInfo& other);
  ~AccessibilityTextStyleInfo();

  std::string font_name;
  int font_weight = 0;
  AccessibilityTextRenderMode render_mode =
      AccessibilityTextRenderMode::kUnknown;
  float font_size = 0.0f;
  // Colors are ARGB.
  uint32_t fill_color = 0;
  uint32_t stroke_color = 0;
  bool is_italic = false;
  bool is_bold = false;
};

enum class AccessibilityTextDirection {
  kNone = 0,
  kLeftToRight = 1,
  kRightToLeft = 2,
  kTopToBottom = 3,
  kBottomToTop = 4,
  kMaxValue = kBottomToTop,
};

struct AccessibilityTextRunInfo {
  AccessibilityTextRunInfo();
  AccessibilityTextRunInfo(uint32_t len,
                           const gfx::RectF& bounds,
                           AccessibilityTextDirection direction,
                           const AccessibilityTextStyleInfo& style);
  AccessibilityTextRunInfo(uint32_t len,
                           const gfx::RectF& bounds,
                           AccessibilityTextDirection direction,
                           const AccessibilityTextStyleInfo& style,
                           bool is_searchified);
  AccessibilityTextRunInfo(const AccessibilityTextRunInfo& other);
  ~AccessibilityTextRunInfo();

  uint32_t len = 0;
  gfx::RectF bounds;
  AccessibilityTextDirection direction = AccessibilityTextDirection::kNone;
  AccessibilityTextStyleInfo style;
  bool is_searchified = false;
};

struct AccessibilityCharInfo {
  uint32_t unicode_character = 0;
  double char_width = 0.0;
};

struct AccessibilityTextRunRangeInfo {
  // Index of the starting text run of the annotation in the collection of all
  // text runs in the page.
  size_t index = 0;
  // Count of the text runs spanning the annotation.
  uint32_t count = 0;
};

struct AccessibilityLinkInfo {
  AccessibilityLinkInfo();
  AccessibilityLinkInfo(const std::string& url,
                        uint32_t index_in_page,
                        const gfx::RectF& bounds,
                        const AccessibilityTextRunRangeInfo& text_range);
  AccessibilityLinkInfo(const AccessibilityLinkInfo& other);
  ~AccessibilityLinkInfo();

  // URL of the link.
  std::string url;
  // Index of this link in the collection of links in the page.
  uint32_t index_in_page = 0;
  // Bounding box of the link.
  gfx::RectF bounds;
  AccessibilityTextRunRangeInfo text_range;
};

struct AccessibilityImageInfo {
  AccessibilityImageInfo();
  AccessibilityImageInfo(const std::string& alt_text,
                         uint32_t text_run_index,
                         const gfx::RectF& bounds,
                         int32_t page_object_index);
  AccessibilityImageInfo(const AccessibilityImageInfo& other);
  ~AccessibilityImageInfo();

  // Alternate text for the image provided by PDF.
  std::string alt_text;

  // We anchor the image to a char index, this denotes the text run before
  // which the image should be inserted in the accessibility tree. The text run
  // at this index should contain the anchor char index.
  uint32_t text_run_index = 0;

  // Bounding box of the image.
  gfx::RectF bounds;

  // Index of the image object in its page.
  int32_t page_object_index;
};

struct AccessibilityHighlightInfo {
  AccessibilityHighlightInfo();
  AccessibilityHighlightInfo(const std::string& note_text,
                             uint32_t index_in_page,
                             uint32_t color,
                             const gfx::RectF& bounds,
                             const AccessibilityTextRunRangeInfo& text_range);
  AccessibilityHighlightInfo(const AccessibilityHighlightInfo& other);
  ~AccessibilityHighlightInfo();

  // Represents the text of the associated popup note, if present.
  std::string note_text;
  // Index of this highlight in the collection of highlights in the page.
  uint32_t index_in_page = 0;
  // Color of the highlight in ARGB. Alpha is stored in the first 8 MSBs. RGB
  // follows after it with each using 8 bytes.
  uint32_t color = 0;
  // Bounding box of the highlight.
  gfx::RectF bounds;
  AccessibilityTextRunRangeInfo text_range;
};

struct AccessibilityTextFieldInfo {
  AccessibilityTextFieldInfo();
  AccessibilityTextFieldInfo(const std::string& name,
                             const std::string& value,
                             bool is_read_only,
                             bool is_required,
                             bool is_password,
                             uint32_t index_in_page,
                             uint32_t text_run_index,
                             const gfx::RectF& bounds);
  AccessibilityTextFieldInfo(const AccessibilityTextFieldInfo& other);
  ~AccessibilityTextFieldInfo();

  // Represents the name property of text field, if present.
  std::string name;
  // Represents the value property of text field, if present.
  std::string value;
  // Represents if the text field is non-editable.
  bool is_read_only = false;
  // Represents if the field should have value at the time it is exported by a
  // submit form action.
  bool is_required = false;
  // Represents if the text field is a password text field type.
  bool is_password = false;
  // Index of this text field in the collection of text fields in the page.
  uint32_t index_in_page = 0;
  // We anchor the text field to a text run index, this denotes the text run
  // before which the text field should be inserted in the accessibility tree.
  uint32_t text_run_index = 0;
  // Bounding box of the text field.
  gfx::RectF bounds;
};

struct AccessibilityChoiceFieldOptionInfo {
  // Represents the name property of choice field option.
  std::string name;
  // Represents if a choice field option is selected or not.
  bool is_selected = false;
  // Bounding box of the choice field option.
  gfx::RectF bounds;
};

enum class ChoiceFieldType {
  kListBox = 0,
  kComboBox = 1,
  kMinValue = kListBox,
  kMaxValue = kComboBox,
};

struct AccessibilityChoiceFieldInfo {
  AccessibilityChoiceFieldInfo();
  AccessibilityChoiceFieldInfo(
      const std::string& name,
      const std::vector<AccessibilityChoiceFieldOptionInfo>& options,
      ChoiceFieldType type,
      bool is_read_only,
      bool is_multi_select,
      bool has_editable_text_box,
      uint32_t index_in_page,
      uint32_t text_run_index,
      const gfx::RectF& bounds);
  AccessibilityChoiceFieldInfo(const AccessibilityChoiceFieldInfo& other);
  ~AccessibilityChoiceFieldInfo();

  // Represents the name property of choice field, if present.
  std::string name;
  // Represents list of options in choice field, if present.
  std::vector<AccessibilityChoiceFieldOptionInfo> options;
  // Represents type of choice field.
  ChoiceFieldType type;
  // Represents if the choice field is non-editable.
  bool is_read_only = false;
  // Represents if the choice field is multi-selectable.
  bool is_multi_select = false;
  // Represents if the choice field includes an editable text box.
  bool has_editable_text_box = false;
  // Index of this choice field in the collection of choice fields in the
  // page.
  uint32_t index_in_page = 0;
  // We anchor the choice field to a text run index, this denotes the text run
  // before which the choice field should be inserted in the accessibility
  // tree.
  uint32_t text_run_index = 0;
  // Bounding box of the choice field.
  gfx::RectF bounds;
};

enum class ButtonType {
  kPushButton = 1,
  kCheckBox = 2,
  kRadioButton = 3,
  kMinValue = kPushButton,
  kMaxValue = kRadioButton,
};

struct AccessibilityButtonInfo {
  AccessibilityButtonInfo();
  AccessibilityButtonInfo(const std::string& name,
                          const std::string& value,
                          ButtonType type,
                          bool is_read_only,
                          bool is_checked,
                          uint32_t control_count,
                          uint32_t control_index,
                          uint32_t index_in_page,
                          uint32_t text_run_index,
                          const gfx::RectF& bounds);
  AccessibilityButtonInfo(const AccessibilityButtonInfo& other);
  ~AccessibilityButtonInfo();

  // Represents the name property of button, if present.
  std::string name;
  // Represents the value property of button, if present.
  std::string value;
  // Represents the button type.
  ButtonType type;
  // Represents if the button is non-editable.
  bool is_read_only = false;
  // Represents if the radio button or check box is checked or not.
  bool is_checked = false;
  // Represents count of controls in the control group. A group of interactive
  // form annotations is collectively called a form control group. Here, an
  // interactive form annotation, should be either a radio button or a
  // checkbox. Value of `control_count` is >= 1.
  uint32_t control_count = 0;
  // Represents index of the control in the control group. A group of
  // interactive form annotations is collectively called a form control group.
  // Here, an interactive form annotation, should be either a radio button or
  // a checkbox. Value of `control_index` should always be less than
  // `control_count`.
  uint32_t control_index = 0;
  // Index of this button in the collection of buttons in the page.
  uint32_t index_in_page = 0;
  // We anchor the button to a text run index, this denotes the text run
  // before which the button should be inserted in the accessibility tree.
  uint32_t text_run_index = 0;
  // Bounding box of the button.
  gfx::RectF bounds;
};

struct AccessibilityFormFieldInfo {
  AccessibilityFormFieldInfo();
  AccessibilityFormFieldInfo(
      const std::vector<AccessibilityTextFieldInfo>& text_fields,
      const std::vector<AccessibilityChoiceFieldInfo>& choice_fields,
      const std::vector<AccessibilityButtonInfo>& buttons);
  AccessibilityFormFieldInfo(const AccessibilityFormFieldInfo& other);
  ~AccessibilityFormFieldInfo();

  std::vector<AccessibilityTextFieldInfo> text_fields;
  std::vector<AccessibilityChoiceFieldInfo> choice_fields;
  std::vector<AccessibilityButtonInfo> buttons;
};

struct AccessibilityPageObjects {
  AccessibilityPageObjects();
  AccessibilityPageObjects(
      const std::vector<AccessibilityLinkInfo>& links,
      const std::vector<AccessibilityImageInfo>& images,
      const std::vector<AccessibilityHighlightInfo>& highlights,
      const AccessibilityFormFieldInfo& form_fields);
  AccessibilityPageObjects(const AccessibilityPageObjects& other);
  ~AccessibilityPageObjects();

  std::vector<AccessibilityLinkInfo> links;
  std::vector<AccessibilityImageInfo> images;
  std::vector<AccessibilityHighlightInfo> highlights;
  AccessibilityFormFieldInfo form_fields;
};

enum class FocusObjectType {
  kNone = 0,
  kDocument = 1,
  kLink = 2,
  kHighlight = 3,
  kTextField = 4,
  kMaxValue = kTextField,
};

struct AccessibilityFocusInfo {
  FocusObjectType focused_object_type = FocusObjectType::kNone;
  uint32_t focused_object_page_index = 0;
  uint32_t focused_annotation_index_in_page = 0;
};

struct AccessibilityViewportInfo {
  AccessibilityViewportInfo();
  AccessibilityViewportInfo(const AccessibilityViewportInfo& other);
  ~AccessibilityViewportInfo();

  double zoom = 0.0;
  double scale = 0.0;
  gfx::Point scroll;
  gfx::Point offset;
  uint32_t orientation = 0;
  uint32_t selection_start_page_index = 0;
  uint32_t selection_start_char_index = 0;
  uint32_t selection_end_page_index = 0;
  uint32_t selection_end_char_index = 0;
  AccessibilityFocusInfo focus_info;
};

enum class AccessibilityAction {
  // No action specified.
  kNone = 0,
  // Invoke the rect to scroll into the viewport.
  kScrollToMakeVisible = 1,
  // Invoke the default action on a node.
  kDoDefaultAction = 2,
  // Invoke the global point to scroll into the viewport.
  kScrollToGlobalPoint = 3,
  // Set the text selection.
  kSetSelection = 4,
  // Last enum value marker.
  kMaxValue = kSetSelection,
};

enum class AccessibilityAnnotationType {
  // No annotation type defined.
  kNone = 0,
  // Link annotation.
  kLink = 1,
  // Last enum value marker.
  kMaxValue = kLink,
};

enum class AccessibilityScrollAlignment {
  // No scroll alignment specified.
  kNone = 0,
  // Scroll the point to the center of the viewport.
  kCenter,
  // Scroll the point to the top of the viewport.
  kTop,
  // Scroll the point to the bottom of the viewport.
  kBottom,
  // Scroll the point to the left of the viewport.
  kLeft,
  // Scroll the point to the right of the viewport.
  kRight,
  // Scroll the point to the closest edge of the viewport.
  kClosestToEdge,
  // Last enum value marker.
  kMaxValue = kClosestToEdge,
};

struct PageCharacterIndex {
  // Index of PDF page.
  uint32_t page_index = 0;
  // Index of character within the PDF page.
  uint32_t char_index = 0;
};

struct AccessibilityActionData {
  AccessibilityActionData();
  AccessibilityActionData(
      AccessibilityAction action,
      AccessibilityAnnotationType annotation_type,
      const gfx::Point& target_point,
      const gfx::Rect& target_rect,
      uint32_t annotation_index,
      uint32_t page_index,
      AccessibilityScrollAlignment horizontal_scroll_alignment,
      AccessibilityScrollAlignment vertical_scroll_alignment,
      const PageCharacterIndex& selection_start_index,
      const PageCharacterIndex& selection_end_index);
  AccessibilityActionData(const AccessibilityActionData& other);
  ~AccessibilityActionData();

  // Accessibility action type.
  AccessibilityAction action = AccessibilityAction::kNone;
  // Annotation type on which the action is to be performed.
  AccessibilityAnnotationType annotation_type =
      AccessibilityAnnotationType::kNone;
  // Target point on which the action is to be performed.
  gfx::Point target_point;
  // Target rect on which the action is to be performed.
  gfx::Rect target_rect;
  // Index of annotation in page.
  uint32_t annotation_index = 0;
  // Page index on which the link is present.
  uint32_t page_index = 0;
  // Horizontal scroll alignment with respect to the viewport
  AccessibilityScrollAlignment horizontal_scroll_alignment =
      AccessibilityScrollAlignment::kNone;
  // Vertical scroll alignment with respect to the viewport
  AccessibilityScrollAlignment vertical_scroll_alignment =
      AccessibilityScrollAlignment::kNone;
  // Page and character index of start of selection.
  PageCharacterIndex selection_start_index;
  // Page and character index of exclusive end of selection.
  PageCharacterIndex selection_end_index;
};

}  // namespace chrome_pdf

#endif  // PDF_ACCESSIBILITY_STRUCTS_H_
