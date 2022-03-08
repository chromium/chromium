// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPP_PDF_H_
#define PPAPI_C_PRIVATE_PPP_PDF_H_

#include <stdint.h>

#include "ppapi/c/dev/pp_print_settings_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_var.h"

#define PPP_PDF_INTERFACE_1 "PPP_Pdf;1"
#define PPP_PDF_INTERFACE PPP_PDF_INTERFACE_1

typedef enum {
  // Rotates the page 90 degrees clockwise from its current orientation.
  PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW,
  // Rotates the page 90 degrees counterclockwise from its current orientation.
  PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW
} PP_PrivatePageTransformType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrivatePageTransformType, 4);

typedef enum {
  PP_PRIVATEDUPLEXMODE_NONE = 0,
  PP_PRIVATEDUPLEXMODE_SIMPLEX = 1,
  PP_PRIVATEDUPLEXMODE_SHORT_EDGE = 2,
  PP_PRIVATEDUPLEXMODE_LONG_EDGE = 3,
  PP_PRIVATEDUPLEXMODE_LAST = PP_PRIVATEDUPLEXMODE_LONG_EDGE
} PP_PrivateDuplexMode_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_PrivateDuplexMode_Dev, 4);

struct PP_PdfPrintPresetOptions_Dev {
  // Returns whether scaling is disabled. Returns same information as the
  // PPP_Printing_Dev's method IsScalingDiabled().
  PP_Bool is_scaling_disabled;

  // Number of copies to be printed.
  int32_t copies;

  // DuplexMode to be used for printing.
  PP_PrivateDuplexMode_Dev duplex;

  // True if all the pages in the PDF are the same size.
  PP_Bool is_page_size_uniform;

  // Only valid if |is_page_size_uniform| is true. The page size.
  PP_Size uniform_page_size;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PdfPrintPresetOptions_Dev, 24);

struct PP_PdfPrintSettings_Dev {
  // Used for N-up mode.
  uint32_t pages_per_sheet;

  // The scale factor percentage, where 100 indicates default scaling.
  uint32_t scale_factor;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_PdfPrintSettings_Dev, 8);

struct PPP_Pdf_1_1 {
  // Returns an absolute URL if the position is over a link.
  PP_Var (*GetLinkAtPosition)(PP_Instance instance,
                              PP_Point point);

  // Requests that the plugin apply the given transform to its view.
  void (*Transform)(PP_Instance instance, PP_PrivatePageTransformType type);

  // Return true if print preset options are updated from document.
  PP_Bool (*GetPrintPresetOptionsFromDocument)(
      PP_Instance instance,
      PP_PdfPrintPresetOptions_Dev* options);

  void (*SetCaretPosition)(PP_Instance instance,
                           const struct PP_FloatPoint* position);
  void (*MoveRangeSelectionExtent)(PP_Instance instance,
                                   const struct PP_FloatPoint* extent);
  void (*SetSelectionBounds)(PP_Instance instance,
                             const struct PP_FloatPoint* base,
                             const struct PP_FloatPoint* extent);

  // Return true if plugin text can be edited. i.e. When focus is within an
  // editable form text area (a form text field or user-editable form combobox
  // text field.
  PP_Bool (*CanEditText)(PP_Instance instance);

  // Return true if plugin has editable text. i.e. When the focused editable
  // field has content.
  PP_Bool (*HasEditableText)(PP_Instance instance);

  // Replace the plugin's selected text (if focus is in an editable text area).
  // If there is no selected text, append the replacement text after the current
  // caret position.
  void (*ReplaceSelection)(PP_Instance instance, const char* text);

  // Perform a select all operation.
  void (*SelectAll)(PP_Instance instance);

  // Return true if plugin can perform an undo operation.
  PP_Bool (*CanUndo)(PP_Instance instance);

  // Return true if plugin can perform a redo operation.
  PP_Bool (*CanRedo)(PP_Instance instance);

  // Perform an undo operation.
  void (*Undo)(PP_Instance instance);

  // Perform a redo operation.
  void (*Redo)(PP_Instance instance);

  // This is a specialized version of PPP_Printing_Dev's Begin method.
  // It functions in the same way, but takes an additional |pdf_print_settings|
  // parameter. When the PPP_Pdf interface is available, use this instead of
  // PPP_Printing_Dev's Begin method, in conjuction with PPP_Printing_Dev's
  // other methods.
  int32_t (*PrintBegin)(
      PP_Instance instance,
      const struct PP_PrintSettings_Dev* print_settings,
      const struct PP_PdfPrintSettings_Dev* pdf_print_settings);
};

typedef PPP_Pdf_1_1 PPP_Pdf;

#endif  // PPAPI_C_PRIVATE_PPP_PDF_H_
