// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/accessibility_structs.h"
#include "pdf/content_restriction.h"
#include "pdf/ppapi_migration/result_codes.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/private/ppp_pdf.h"

namespace chrome_pdf {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kUnknown,
                   PP_TEXTRENDERINGMODE_UNKNOWN);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kFill,
                   PP_TEXTRENDERINGMODE_FILL);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kStroke,
                   PP_TEXTRENDERINGMODE_STROKE);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kFillStroke,
                   PP_TEXTRENDERINGMODE_FILLSTROKE);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kInvisible,
                   PP_TEXTRENDERINGMODE_INVISIBLE);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kFillClip,
                   PP_TEXTRENDERINGMODE_FILLCLIP);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kStrokeClip,
                   PP_TEXTRENDERINGMODE_STROKECLIP);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kFillStrokeClip,
                   PP_TEXTRENDERINGMODE_FILLSTROKECLIP);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kClip,
                   PP_TEXTRENDERINGMODE_CLIP);
STATIC_ASSERT_ENUM(AccessibilityTextRenderMode::kMaxValue,
                   PP_TEXTRENDERINGMODE_LAST);

STATIC_ASSERT_ENUM(AccessibilityTextDirection::kNone, PP_PRIVATEDIRECTION_NONE);
STATIC_ASSERT_ENUM(AccessibilityTextDirection::kLeftToRight,
                   PP_PRIVATEDIRECTION_LTR);
STATIC_ASSERT_ENUM(AccessibilityTextDirection::kRightToLeft,
                   PP_PRIVATEDIRECTION_RTL);
STATIC_ASSERT_ENUM(AccessibilityTextDirection::kTopToBottom,
                   PP_PRIVATEDIRECTION_TTB);
STATIC_ASSERT_ENUM(AccessibilityTextDirection::kBottomToTop,
                   PP_PRIVATEDIRECTION_BTT);
STATIC_ASSERT_ENUM(AccessibilityTextDirection::kMaxValue,
                   PP_PRIVATEDIRECTION_LAST);

STATIC_ASSERT_ENUM(ChoiceFieldType::kListBox, PP_PRIVATECHOICEFIELD_LISTBOX);
STATIC_ASSERT_ENUM(ChoiceFieldType::kComboBox, PP_PRIVATECHOICEFIELD_COMBOBOX);
STATIC_ASSERT_ENUM(ChoiceFieldType::kMaxValue, PP_PRIVATECHOICEFIELD_LAST);

STATIC_ASSERT_ENUM(ButtonType::kPushButton, PP_PRIVATEBUTTON_PUSHBUTTON);
STATIC_ASSERT_ENUM(ButtonType::kPushButton, PP_PRIVATEBUTTON_FIRST);
STATIC_ASSERT_ENUM(ButtonType::kCheckBox, PP_PRIVATEBUTTON_CHECKBOX);
STATIC_ASSERT_ENUM(ButtonType::kRadioButton, PP_PRIVATEBUTTON_RADIOBUTTON);
STATIC_ASSERT_ENUM(ButtonType::kMaxValue, PP_PRIVATEBUTTON_LAST);

STATIC_ASSERT_ENUM(FocusObjectType::kNone, PP_PRIVATEFOCUSOBJECT_NONE);
STATIC_ASSERT_ENUM(FocusObjectType::kDocument, PP_PRIVATEFOCUSOBJECT_DOCUMENT);
STATIC_ASSERT_ENUM(FocusObjectType::kLink, PP_PRIVATEFOCUSOBJECT_LINK);
STATIC_ASSERT_ENUM(FocusObjectType::kHighlight,
                   PP_PRIVATEFOCUSOBJECT_HIGHLIGHT);
STATIC_ASSERT_ENUM(FocusObjectType::kTextField,
                   PP_PRIVATEFOCUSOBJECT_TEXT_FIELD);
STATIC_ASSERT_ENUM(FocusObjectType::kMaxValue, PP_PRIVATEFOCUSOBJECT_LAST);

STATIC_ASSERT_ENUM(AccessibilityAction::kNone, PP_PDF_ACTION_NONE);
STATIC_ASSERT_ENUM(AccessibilityAction::kScrollToMakeVisible,
                   PP_PDF_SCROLL_TO_MAKE_VISIBLE);
STATIC_ASSERT_ENUM(AccessibilityAction::kDoDefaultAction,
                   PP_PDF_DO_DEFAULT_ACTION);
STATIC_ASSERT_ENUM(AccessibilityAction::kScrollToGlobalPoint,
                   PP_PDF_SCROLL_TO_GLOBAL_POINT);
STATIC_ASSERT_ENUM(AccessibilityAction::kSetSelection, PP_PDF_SET_SELECTION);
STATIC_ASSERT_ENUM(AccessibilityAction::kMaxValue,
                   PP_PDF_ACCESSIBILITYACTION_LAST);

STATIC_ASSERT_ENUM(AccessibilityAnnotationType::kNone, PP_PDF_TYPE_NONE);
STATIC_ASSERT_ENUM(AccessibilityAnnotationType::kLink, PP_PDF_LINK);
STATIC_ASSERT_ENUM(AccessibilityAnnotationType::kMaxValue,
                   PP_PDF_ACCESSIBILITY_ANNOTATIONTYPE_LAST);

STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kNone, PP_PDF_SCROLL_NONE);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kCenter,
                   PP_PDF_SCROLL_ALIGNMENT_CENTER);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kTop,
                   PP_PDF_SCROLL_ALIGNMENT_TOP);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kBottom,
                   PP_PDF_SCROLL_ALIGNMENT_BOTTOM);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kLeft,
                   PP_PDF_SCROLL_ALIGNMENT_LEFT);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kRight,
                   PP_PDF_SCROLL_ALIGNMENT_RIGHT);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kClosestToEdge,
                   PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE);
STATIC_ASSERT_ENUM(AccessibilityScrollAlignment::kMaxValue,
                   PP_PDF_ACCESSIBILITYSCROLLALIGNMENT_LAST);

STATIC_ASSERT_ENUM(kContentRestrictionCopy, PP_CONTENT_RESTRICTION_COPY);
STATIC_ASSERT_ENUM(kContentRestrictionCut, PP_CONTENT_RESTRICTION_CUT);
STATIC_ASSERT_ENUM(kContentRestrictionPaste, PP_CONTENT_RESTRICTION_PASTE);
STATIC_ASSERT_ENUM(kContentRestrictionPrint, PP_CONTENT_RESTRICTION_PRINT);
STATIC_ASSERT_ENUM(kContentRestrictionSave, PP_CONTENT_RESTRICTION_SAVE);

STATIC_ASSERT_ENUM(Result::kSuccess, PP_OK);
STATIC_ASSERT_ENUM(Result::kErrorFailed, PP_ERROR_FAILED);
STATIC_ASSERT_ENUM(Result::kErrorAborted, PP_ERROR_ABORTED);
STATIC_ASSERT_ENUM(Result::kErrorBadArgument, PP_ERROR_BADARGUMENT);
STATIC_ASSERT_ENUM(Result::kErrorNoAccess, PP_ERROR_NOACCESS);

}  // namespace chrome_pdf
