// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PDF_H_
#define PPAPI_C_PRIVATE_PPB_PDF_H_

#include <stdint.h>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/pp_private_font_charset.h"

#define PPB_PDF_INTERFACE "PPB_PDF;1"

typedef enum {
  PP_PDFFEATURE_HIDPI = 0,
  PP_PDFFEATURE_PRINTING = 1
} PP_PDFFeature;

typedef enum {
  PP_CONTENT_RESTRICTION_COPY = 1 << 0,
  PP_CONTENT_RESTRICTION_CUT = 1 << 1,
  PP_CONTENT_RESTRICTION_PASTE = 1 << 2,
  PP_CONTENT_RESTRICTION_PRINT = 1 << 3,
  PP_CONTENT_RESTRICTION_SAVE = 1 << 4
} PP_ContentRestriction;

struct PP_PrivateFontFileDescription {
  const char* face;
  uint32_t weight;
  bool italic;
};

struct PP_PrivateFindResult {
  int start_index;
  int length;
};

typedef enum {
  PP_PRIVATEFOCUSOBJECT_NONE = 0,
  PP_PRIVATEFOCUSOBJECT_DOCUMENT = 1,
  PP_PRIVATEFOCUSOBJECT_LINK = 2,
  PP_PRIVATEFOCUSOBJECT_HIGHLIGHT = 3,
  PP_PRIVATEFOCUSOBJECT_TEXT_FIELD = 4,
  PP_PRIVATEFOCUSOBJECT_LAST = PP_PRIVATEFOCUSOBJECT_TEXT_FIELD
} PP_PrivateFocusObjectType;

// Represents the information to uniquely identify the focused object
// in PDF.
struct PP_PrivateAccessibilityFocusInfo {
  // Holds the type of the focused object in PDFiumEngine.
  PP_PrivateFocusObjectType focused_object_type;
  // Holds the PDF page index in the which the focused annotation is present.
  // When |focused_object_type| is PP_PRIVATEFOCUSOBJECT_NONE or
  // PP_PRIVATEFOCUSOBJECT_DOCUMENT then the value of this member shouldn't
  // be used, set to zero as a sentinel value.
  uint32_t focused_object_page_index;
  // Holds the focused annotation's index in page's annotations array.
  // When |focused_object_type| is PP_PRIVATEFOCUSOBJECT_NONE or
  // PP_PRIVATEFOCUSOBJECT_DOCUMENT then the value of this member shouldn't
  // be used, set to zero as a sentinel value.
  uint32_t focused_annotation_index_in_page;
};

struct PP_PrivateAccessibilityViewportInfo {
  double zoom;
  double scale;
  struct PP_Point scroll;
  struct PP_Point offset;
  uint32_t selection_start_page_index;
  uint32_t selection_start_char_index;
  uint32_t selection_end_page_index;
  uint32_t selection_end_char_index;
  struct PP_PrivateAccessibilityFocusInfo focus_info;
};

struct PP_PrivateAccessibilityDocInfo {
  uint32_t page_count;
  PP_Bool text_accessible;
  PP_Bool text_copyable;
};

typedef enum {
  PP_PRIVATEDIRECTION_NONE = 0,
  PP_PRIVATEDIRECTION_LTR = 1,
  PP_PRIVATEDIRECTION_RTL = 2,
  PP_PRIVATEDIRECTION_TTB = 3,
  PP_PRIVATEDIRECTION_BTT = 4,
  PP_PRIVATEDIRECTION_LAST = PP_PRIVATEDIRECTION_BTT
} PP_PrivateDirection;

struct PP_PrivateAccessibilityPageInfo {
  uint32_t page_index;
  struct PP_Rect bounds;
  uint32_t text_run_count;
  uint32_t char_count;
};

// See PDF Reference 1.7, page 402, table 5.3.
typedef enum {
  PP_TEXTRENDERINGMODE_UNKNOWN = -1,
  PP_TEXTRENDERINGMODE_FIRST = PP_TEXTRENDERINGMODE_UNKNOWN,
  PP_TEXTRENDERINGMODE_FILL = 0,
  PP_TEXTRENDERINGMODE_STROKE = 1,
  PP_TEXTRENDERINGMODE_FILLSTROKE = 2,
  PP_TEXTRENDERINGMODE_INVISIBLE = 3,
  PP_TEXTRENDERINGMODE_FILLCLIP = 4,
  PP_TEXTRENDERINGMODE_STROKECLIP = 5,
  PP_TEXTRENDERINGMODE_FILLSTROKECLIP = 6,
  PP_TEXTRENDERINGMODE_CLIP = 7,
  PP_TEXTRENDERINGMODE_LAST = PP_TEXTRENDERINGMODE_CLIP
} PP_TextRenderingMode;

// This holds the text style information provided by the PDF and will be used
// in accessibility to provide the text style information. Needs to stay in
// sync with C++ version. (PrivateAccessibilityTextStyleInfo and
// PdfAccessibilityTextStyleInfo)
struct PP_PrivateAccessibilityTextStyleInfo {
  const char* font_name;
  uint32_t font_name_length;
  int font_weight;
  PP_TextRenderingMode render_mode;
  float font_size;
  // Colors are ARGB.
  uint32_t fill_color;
  uint32_t stroke_color;
  bool is_italic;
  bool is_bold;
};

// This holds the text run information provided by the PDF and will be used in
// accessibility to provide the text run information.
// Needs to stay in sync with C++ version. (PrivateAccessibilityTextRunInfo and
// PdfAccessibilityTextRunInfo)
struct PP_PrivateAccessibilityTextRunInfo {
  uint32_t len;
  struct PP_FloatRect bounds;
  PP_PrivateDirection direction;
  struct PP_PrivateAccessibilityTextStyleInfo style;
};

struct PP_PrivateAccessibilityCharInfo {
  uint32_t unicode_character;
  double char_width;
};

// This holds the link information provided by the PDF and will be used in
// accessibility to provide the link information. Needs to stay in sync with
// C++ versions (PdfAccessibilityLinkInfo and PrivateAccessibilityLinkInfo).
// This struct contains index state that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityLinkInfo {
  // URL of the link.
  const char* url;
  uint32_t url_length;
  // Index of the link in the page. This will be used to identify the link on
  // which action has to be performed in the page.
  // |index_in_page| is populated and used in plugin process to handle
  // accessiility actions from mimehandler process. It's value should be
  // validated in plugin before usage.
  uint32_t index_in_page;
  // Link can either be part of the page text or not. If the link is part of the
  // page text, then |text_run_index| denotes the text run which contains the
  // start_index of the link and the |text_run_count| equals the number of text
  // runs the link spans in the page text. If the link is not part of the page
  // text then |text_run_count| should be 0 and the |text_run_index| should
  // contain the nearest char index to the bounding rectangle of the link.
  uint32_t text_run_index;
  uint32_t text_run_count;
  // Bounding box of the link.
  struct PP_FloatRect bounds;
};

// This holds the image information provided by the PDF and will be used in
// accessibility to provide the image information. Needs to stay in sync with
// C++ versions (PdfAccessibilityImageInfo and PrivateAccessibilityImageInfo).
// This struct contains index state that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityImageInfo {
  // Alternate text for the image provided by PDF.
  const char* alt_text;
  uint32_t alt_text_length;
  // We anchor the image to a char index, this denotes the text run before
  // which the image should be inserted in the accessibility tree. The text run
  // at this index should contain the anchor char index.
  uint32_t text_run_index;
  // Bounding box of the image.
  struct PP_FloatRect bounds;
};

// This holds text highlight information provided by the PDF and will be
// used in accessibility to expose it. Text highlights can have an associated
// popup note, the data of which is also captured here.
// Needs to stay in sync with C++ versions (PdfAccessibilityHighlightInfo and
// PrivateAccessibilityHighlightInfo).
// This struct contains index state that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityHighlightInfo {
  // Represents the text of the associated popup note, if present.
  const char* note_text;
  uint32_t note_text_length;
  // Index of the highlight in the page annotation list. Used to identify the
  // annotation on which action needs to be performed.
  // |index_in_page| is populated and used in plugin process to handle
  // accessiility actions from mimehandler process. It's value should be
  // validated in plugin before usage.
  uint32_t index_in_page;
  // Highlights are annotations over existing page text.  |text_run_index|
  // denotes the index of the text run where the highlight starts and
  // |text_run_count| denotes the number of text runs which the highlight spans
  // across.
  uint32_t text_run_index;
  uint32_t text_run_count;
  // Bounding box of the highlight.
  struct PP_FloatRect bounds;
  // Color of the highlight in ARGB. Alpha is stored in the first 8 MSBs. RGB
  // follows after it with each using 8 bytes.
  uint32_t color;
};

// This holds text form field information provided by the PDF and will be used
// in accessibility to expose it. Needs to stay in sync with C++ versions
// (PdfAccessibilityTextFieldInfo and PrivateAccessibilityTextFieldInfo).
// This struct contains index state that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityTextFieldInfo {
  // Represents the name property of text field, if present.
  const char* name;
  uint32_t name_length;
  // Represents the value property of text field, if present.
  const char* value;
  uint32_t value_length;
  // Represents if the text field is non-editable.
  bool is_read_only;
  // Represents if the field should have value at the time it is exported by a
  // submit form action.
  bool is_required;
  // Represents if the text field is a password text field type.
  bool is_password;
  // Index of the text field in the collection of text fields in the page. Used
  // to identify the annotation on which action needs to be performed.
  // |index_in_page| is populated and used in plugin process to handle
  // accessiility actions from mimehandler process. It's value should be
  // validated in plugin before usage.
  uint32_t index_in_page;
  // We anchor the text field to a text run index, this denotes the text run
  // before which the text field should be inserted in the accessibility tree.
  uint32_t text_run_index;
  // Bounding box of the text field.
  struct PP_FloatRect bounds;
};

// This holds choice form field option information provided by the PDF and
// will be used in accessibility to expose it. Needs to stay in sync with C++
// versions (PdfAccessibilityChoiceFieldOptionInfo and
// PrivateAccessibilityChoiceFieldOptionInfo).
struct PP_PrivateAccessibilityChoiceFieldOptionInfo {
  // Represents the name property of choice field option.
  const char* name;
  uint32_t name_length;
  // Represents if a choice field option is selected or not.
  bool is_selected;
  // Bounding box of the choice field option.
  struct PP_FloatRect bounds;
};

typedef enum {
  PP_PRIVATECHOICEFIELD_LISTBOX = 0,
  PP_PRIVATECHOICEFIELD_COMBOBOX = 1,
  PP_PRIVATECHOICEFIELD_LAST = PP_PRIVATECHOICEFIELD_COMBOBOX
} PP_PrivateChoiceFieldType;

// This holds choice form field information provided by the PDF and will be used
// in accessibility to expose it. Needs to stay in sync with C++ versions
// (PdfAccessibilityChoiceFieldInfo and PrivateAccessibilityChoiceFieldInfo).
// This struct contains index state that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityChoiceFieldInfo {
  // Represents the name property of choice field, if present.
  const char* name;
  uint32_t name_length;
  // Represents list of options in choice field, if present.
  struct PP_PrivateAccessibilityChoiceFieldOptionInfo* options;
  uint32_t options_length;
  // Represents type of choice field.
  PP_PrivateChoiceFieldType type;
  // Represents if the choice field is non-editable.
  bool is_read_only;
  // Represents if the choice field is multi-selectable.
  bool is_multi_select;
  // Represents if the choice field includes an editable text box.
  bool has_editable_text_box;
  // Index of the choice field in the collection of choice fields in the page.
  // Used to identify the annotation on which action needs to be performed.
  // |index_in_page| is populated and used in plugin process to handle
  // accessiility actions from mimehandler process. It's value should be
  // validated in plugin before usage.
  uint32_t index_in_page;
  // We anchor the choice field to a text run index, this denotes the text run
  // before which the choice field should be inserted in the accessibility tree.
  uint32_t text_run_index;
  // Bounding box of the choice field.
  struct PP_FloatRect bounds;
};

typedef enum {
  PP_PRIVATEBUTTON_PUSHBUTTON = 1,
  PP_PRIVATEBUTTON_FIRST = PP_PRIVATEBUTTON_PUSHBUTTON,
  PP_PRIVATEBUTTON_CHECKBOX = 2,
  PP_PRIVATEBUTTON_RADIOBUTTON = 3,
  PP_PRIVATEBUTTON_LAST = PP_PRIVATEBUTTON_RADIOBUTTON
} PP_PrivateButtonType;

// This holds button form field information provided by the PDF and will be
// used in accessibility to expose it. Needs to stay in sync with C++ versions
// (PdfAccessibilityButtonInfo and PrivateAccessibilityButtonInfo).
// This struct contains index states that should be validated using
// PdfAccessibilityTree::IsDataFromPluginValid() before usage.
struct PP_PrivateAccessibilityButtonInfo {
  // Represents the name property of button, if present.
  const char* name;
  uint32_t name_length;
  // Represents the value property of button, if present.
  const char* value;
  uint32_t value_length;
  // Represents the button type.
  PP_PrivateButtonType type;
  // Represents if the button is non-editable.
  bool is_read_only;
  // Represents if the radio button or check box is checked or not.
  bool is_checked;
  // Represents count of controls in the control group. A group of interactive
  // form annotations is collectively called a form control group. Here, an
  // interactive form annotation, should be either a radio button or a checkbox.
  // Value of |control_count| is >= 1.
  uint32_t control_count;
  // Represents index of the control in the control group. A group of
  // interactive form annotations is collectively called a form control group.
  // Here, an interactive form annotation, should be either a radio button or a
  // checkbox. Value of |control_index| should always be less than
  // |control_count|.
  uint32_t control_index;
  // Index of the button in the collection of buttons in the page. Used
  // to identify the annotation on which action needs to be performed.
  // |index_in_page| is populated and used in plugin process to handle
  // accessiility actions from mimehandler process. It's value should be
  // validated in plugin before usage.
  uint32_t index_in_page;
  // We anchor the button to a text run index, this denotes the text run
  // before which the button should be inserted in the accessibility tree.
  uint32_t text_run_index;
  // Bounding box of the button.
  struct PP_FloatRect bounds;
};

// This holds form fields within a PDF page. Needs to stay in sync with C++
// versions (PdfAccessibilityFormFieldInfo and
// PrivateAccessibilityFormFieldInfo).
struct PP_PrivateAccessibilityFormFieldInfo {
  struct PP_PrivateAccessibilityTextFieldInfo* text_fields;
  uint32_t text_field_count;
  struct PP_PrivateAccessibilityChoiceFieldInfo* choice_fields;
  uint32_t choice_field_count;
  struct PP_PrivateAccessibilityButtonInfo* buttons;
  uint32_t button_count;
};

// This holds different PDF page objects - links, images, highlights and
// form fields within a PDF page so that IPC messages passing accessibility
// objects do not have too many parameters. Needs to stay in sync with C++
// versions (PdfAccessibilityPageObjects and PrivateAccessibilityPageObjects).
struct PP_PrivateAccessibilityPageObjects {
  struct PP_PrivateAccessibilityLinkInfo* links;
  uint32_t link_count;
  struct PP_PrivateAccessibilityImageInfo* images;
  uint32_t image_count;
  struct PP_PrivateAccessibilityHighlightInfo* highlights;
  uint32_t highlight_count;
  struct PP_PrivateAccessibilityFormFieldInfo form_fields;
};

struct PPB_PDF {
  // Returns a resource identifying a font file corresponding to the given font
  // request after applying the browser-specific fallback.
  //
  // Currently Linux-only.
  PP_Resource (*GetFontFileWithFallback)(
      PP_Instance instance,
      const struct PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);

  // Given a resource previously returned by GetFontFileWithFallback, returns
  // a pointer to the requested font table. Linux only.
  bool (*GetFontTableForPrivateFontFile)(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length);

  // Search the given string using ICU.  Use PPB_Core's MemFree on results when
  // done.
  void (*SearchString)(PP_Instance instance,
                       const unsigned short* string,
                       const unsigned short* term,
                       bool case_sensitive,
                       struct PP_PrivateFindResult** results,
                       uint32_t* count);

  // Since WebFrame doesn't know about PPAPI requests, it'll think the page has
  // finished loading even if there are outstanding requests by the plugin.
  // Take this out once WebFrame knows about requests by PPAPI plugins.
  void (*DidStartLoading)(PP_Instance instance);
  void (*DidStopLoading)(PP_Instance instance);

  // Sets content restriction for a full-page plugin (i.e. can't copy/print).
  // The value is a bitfield of PP_ContentRestriction enums.
  void (*SetContentRestriction)(PP_Instance instance, int restrictions);

  // Notifies the browser that the given action has been performed.
  void (*UserMetricsRecordAction)(PP_Instance instance, struct PP_Var action);

  // Notifies the browser that the PDF has an unsupported feature.
  void (*HasUnsupportedFeature)(PP_Instance instance);

  // Invoke SaveAs... dialog, similar to the right-click or wrench menu.
  void (*SaveAs)(PP_Instance instance);

  // Invoke Print dialog for plugin.
  void (*Print)(PP_Instance instance);

  PP_Bool(*IsFeatureEnabled)(PP_Instance instance, PP_PDFFeature feature);

  // Sets the selected text of the plugin.
  void(*SetSelectedText)(PP_Instance instance, const char* selected_text);

  // Sets the link currently under the cursor.
  void (*SetLinkUnderCursor)(PP_Instance instance, const char* url);

  // Gets pointers to the mmap'd V8 snapshot file and its size.
  // This is needed when loading V8's initial snapshot from an external file.
  void (*GetV8ExternalSnapshotData)(PP_Instance instance,
                                    const char** snapshot_data_out,
                                    int* snapshot_size_out);

  // Sends information about the viewport to the renderer for accessibility
  // support.
  void (*SetAccessibilityViewportInfo)(
      PP_Instance instance,
      const struct PP_PrivateAccessibilityViewportInfo* viewport_info);

  // Sends information about the PDF document to the renderer for accessibility
  // support.
  void (*SetAccessibilityDocInfo)(
      PP_Instance instance,
      const struct PP_PrivateAccessibilityDocInfo* doc_info);

  // Sends information about one page in a PDF document to the renderer for
  // accessibility support.
  void (*SetAccessibilityPageInfo)(
      PP_Instance instance,
      const struct PP_PrivateAccessibilityPageInfo* page_info,
      const struct PP_PrivateAccessibilityTextRunInfo text_runs[],
      const struct PP_PrivateAccessibilityCharInfo chars[],
      const struct PP_PrivateAccessibilityPageObjects* page_objects);

  // Sends information about the PDF's URL and the embedder's URL.
  void (*SetCrashData)(PP_Instance instance,
                       const char* pdf_url,
                       const char* top_level_url);

  // Sets the current selection bounding edges.
  void (*SelectionChanged)(PP_Instance instance,
                           const struct PP_FloatPoint* left,
                           int32_t left_height,
                           const struct PP_FloatPoint* right,
                           int32_t right_height);

  // Sets whether the PDF viewer can handle save commands internally.
  void (*SetPluginCanSave)(PP_Instance instance, bool can_save);

  // Displays an alert dialog.
  void (*ShowAlertDialog)(PP_Instance instance, const char* message);

  // Displays a confirmation dialog. This method is synchronous.
  bool (*ShowConfirmDialog)(PP_Instance instance, const char* message);

  // Displays a prompt dialog. This method is synchronous.
  struct PP_Var (*ShowPromptDialog)(PP_Instance instance,
                                    const char* message,
                                    const char* default_answer);
};

#endif  // PPAPI_C_PRIVATE_PPB_PDF_H_
