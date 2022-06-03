// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PDF_H_
#define PPAPI_CPP_PRIVATE_PDF_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/cpp/rect.h"

struct PP_BrowserFont_Trusted_Description;

namespace pp {

class InstanceHandle;
class Var;

class PDF {
 public:
  // C++ version of PP_PrivateAccessibilityTextStyleInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityTextStyleInfo {
    std::string font_name;
    int font_weight;
    PP_TextRenderingMode render_mode;
    float font_size;
    // Colors are ARGB.
    uint32_t fill_color;
    uint32_t stroke_color;
    bool is_italic;
    bool is_bold;
  };

  // C++ version of PP_PrivateAccessibilityTextRunInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityTextRunInfo {
    uint32_t len;
    struct PP_FloatRect bounds;
    PP_PrivateDirection direction;
    PrivateAccessibilityTextStyleInfo style;
  };

  // C++ version of PP_PrivateAccessibilityLinkInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityLinkInfo {
    std::string url;
    // Index of this link in the collection of links in the page.
    uint32_t index_in_page;
    // Index of the starting text run of this link in the collection of all
    // text runs in the page.
    uint32_t text_run_index;
    uint32_t text_run_count;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityImageInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityImageInfo {
    std::string alt_text;
    uint32_t text_run_index;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityHighlightInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityHighlightInfo {
    std::string note_text;
    // Index of this highlight in the collection of highlights in the page.
    uint32_t index_in_page;
    // Index of the starting text run of this highlight in the collection of all
    // text runs in the page.
    uint32_t text_run_index;
    uint32_t text_run_count;
    FloatRect bounds;
    // Color of the highlight in ARGB. Alpha is stored in the first 8 MSBs. RGB
    // follows after it with each using 8 bytes.
    uint32_t color;
  };

  // C++ version of PP_PrivateAccessibilityTextFieldInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityTextFieldInfo {
    std::string name;
    std::string value;
    bool is_read_only;
    bool is_required;
    bool is_password;
    // Index of this text field in the collection of text fields in the page.
    uint32_t index_in_page;
    // We anchor the text field to a text run index, this denotes the text run
    // before which the text field should be inserted in the accessibility tree.
    uint32_t text_run_index;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityChoiceFieldOptionInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityChoiceFieldOptionInfo {
    std::string name;
    bool is_selected;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityChoiceFieldInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityChoiceFieldInfo {
    std::string name;
    std::vector<PrivateAccessibilityChoiceFieldOptionInfo> options;
    PP_PrivateChoiceFieldType type;
    // Represents if the choice field is non-editable.
    bool is_read_only;
    // Represents if the choice field is multi-selectable.
    bool is_multi_select;
    // Represents if the choice field includes an editable text box.
    bool has_editable_text_box;
    // Index of this choice field in the collection of choice fields in the
    // page.
    uint32_t index_in_page;
    // We anchor the choice field to a text run index, this denotes the text run
    // before which the choice field should be inserted in the accessibility
    // tree.
    uint32_t text_run_index;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityButtonInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityButtonInfo {
    std::string name;
    std::string value;
    // Represents the button type.
    PP_PrivateButtonType type;
    // Represents if the button is non-editable.
    bool is_read_only;
    // Represents if the radio button or check box is checked or not.
    bool is_checked;
    // Represents count of controls in the control group. A group of interactive
    // form annotations is collectively called a form control group. Here, an
    // interactive form annotation, should be either a radio button or a
    // checkbox. Value of |control_count| is >= 1.
    uint32_t control_count;
    // Represents index of the control in the control group. A group of
    // interactive form annotations is collectively called a form control group.
    // Here, an interactive form annotation, should be either a radio button or
    // a checkbox. Value of |control_index| should always be less than
    // |control_count|.
    uint32_t control_index;
    // Index of this button in the collection of buttons in the page.
    uint32_t index_in_page;
    // We anchor the button to a text run index, this denotes the text run
    // before which the button should be inserted in the accessibility tree.
    uint32_t text_run_index;
    FloatRect bounds;
  };

  // C++ version of PP_PrivateAccessibilityFormFieldInfo.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityFormFieldInfo {
    std::vector<PrivateAccessibilityTextFieldInfo> text_fields;
    std::vector<PrivateAccessibilityChoiceFieldInfo> choice_fields;
    std::vector<PrivateAccessibilityButtonInfo> buttons;
  };

  // C++ version of PP_PrivateAccessibilityPageObjects.
  // Needs to stay in sync with the C version.
  struct PrivateAccessibilityPageObjects {
    std::vector<PrivateAccessibilityLinkInfo> links;
    std::vector<PrivateAccessibilityImageInfo> images;
    std::vector<PrivateAccessibilityHighlightInfo> highlights;
    PrivateAccessibilityFormFieldInfo form_fields;
  };

  // Returns true if the required interface is available.
  static bool IsAvailable();

  static PP_Resource GetFontFileWithFallback(
      const InstanceHandle& instance,
      const PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);
  static bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                             uint32_t table,
                                             void* output,
                                             uint32_t* output_length);
  static void SearchString(const InstanceHandle& instance,
                           const unsigned short* string,
                           const unsigned short* term,
                           bool case_sensitive,
                           PP_PrivateFindResult** results,
                           uint32_t* count);
  static void DidStartLoading(const InstanceHandle& instance);
  static void DidStopLoading(const InstanceHandle& instance);
  static void SetContentRestriction(const InstanceHandle& instance,
                                    int restrictions);
  static void UserMetricsRecordAction(const InstanceHandle& instance,
                                      const Var& action);
  static void HasUnsupportedFeature(const InstanceHandle& instance);
  static void ShowAlertDialog(const InstanceHandle& instance,
                              const char* message);
  static bool ShowConfirmDialog(const InstanceHandle& instance,
                                const char* message);
  static pp::Var ShowPromptDialog(const InstanceHandle& instance,
                                  const char* message,
                                  const char* default_answer);
  static void SaveAs(const InstanceHandle& instance);
  static void Print(const InstanceHandle& instance);
  static bool IsFeatureEnabled(const InstanceHandle& instance,
                               PP_PDFFeature feature);
  static void SetSelectedText(const InstanceHandle& instance,
                              const char* selected_text);
  static void SetLinkUnderCursor(const InstanceHandle& instance,
                                 const char* url);
  static void GetV8ExternalSnapshotData(const InstanceHandle& instance,
                                        const char** snapshot_data_out,
                                        int* snapshot_size_out);
  static void SetAccessibilityViewportInfo(
      const InstanceHandle& instance,
      const PP_PrivateAccessibilityViewportInfo* viewport_info);
  static void SetAccessibilityDocInfo(
      const InstanceHandle& instance,
      const PP_PrivateAccessibilityDocInfo* doc_info);
  static void SetAccessibilityPageInfo(
      const InstanceHandle& instance,
      const PP_PrivateAccessibilityPageInfo* page_info,
      const std::vector<PrivateAccessibilityTextRunInfo>& text_runs,
      const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
      const PrivateAccessibilityPageObjects& page_objects);
  static void SetCrashData(const InstanceHandle& instance,
                           const char* pdf_url,
                           const char* top_level_url);
  static void SelectionChanged(const InstanceHandle& instance,
                               const PP_FloatPoint& left,
                               int32_t left_height,
                               const PP_FloatPoint& right,
                               int32_t right_height);
  static void SetPluginCanSave(const InstanceHandle& instance, bool can_save);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PDF_H_
