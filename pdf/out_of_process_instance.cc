// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>  // for min/max()
#include <cmath>      // for log() and pow()
#include <list>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "pdf/accessibility.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/input_event_conversions.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/ppapi_migration/value_conversions.h"
#include "pdf/thumbnail.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/dev/text_input_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "url/gurl.h"

namespace chrome_pdf {

namespace {

constexpr char kChromePrint[] = "chrome://print/";
constexpr char kChromeExtension[] =
    "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai";

// Constants used in handling postMessage() messages.
constexpr char kType[] = "type";
// Name of identifier field passed from JS to the plugin and back, to associate
// Page->Plugin messages to Plugin->Page responses.
constexpr char kJSMessageId[] = "messageId";
// Beep message arguments. (Plugin -> Page).
constexpr char kJSBeepType[] = "beep";
// Viewport message arguments. (Page -> Plugin).
constexpr char kJSViewportType[] = "viewport";
constexpr char kJSUserInitiated[] = "userInitiated";
constexpr char kJSXOffset[] = "xOffset";
constexpr char kJSYOffset[] = "yOffset";
constexpr char kJSZoom[] = "zoom";
constexpr char kJSPinchPhase[] = "pinchPhase";
// kJSPinchX and kJSPinchY represent the center of the pinch gesture.
constexpr char kJSPinchX[] = "pinchX";
constexpr char kJSPinchY[] = "pinchY";
// kJSPinchVector represents the amount of panning caused by the pinch gesture.
constexpr char kJSPinchVectorX[] = "pinchVectorX";
constexpr char kJSPinchVectorY[] = "pinchVectorY";
// UpdateScroll message arguments. (Page -> Plugin).
constexpr char kJSUpdateScrollType[] = "updateScroll";
constexpr char kJSUpdateScrollX[] = "x";
constexpr char kJSUpdateScrollY[] = "y";
// Stop scrolling message (Page -> Plugin)
constexpr char kJSStopScrollingType[] = "stopScrolling";
// Document dimension arguments (Plugin -> Page).
constexpr char kJSDocumentDimensionsType[] = "documentDimensions";
constexpr char kJSDocumentWidth[] = "width";
constexpr char kJSDocumentHeight[] = "height";
constexpr char kJSLayoutOptions[] = "layoutOptions";
constexpr char kJSPageDimensions[] = "pageDimensions";
constexpr char kJSPageX[] = "x";
constexpr char kJSPageY[] = "y";
constexpr char kJSPageWidth[] = "width";
constexpr char kJSPageHeight[] = "height";
// Document load progress arguments (Plugin -> Page)
constexpr char kJSLoadProgressType[] = "loadProgress";
constexpr char kJSProgressPercentage[] = "progress";
// Document print preview loaded (Plugin -> Page)
constexpr char kJSPreviewLoadedType[] = "printPreviewLoaded";
// Metadata
constexpr char kJSMetadataType[] = "metadata";
constexpr char kJSAttachments[] = "attachments";
constexpr char kJSBookmarks[] = "bookmarks";
constexpr char kJSTitle[] = "title";
constexpr char kJSCanSerializeDocument[] = "canSerializeDocument";
// Get password (Plugin -> Page)
constexpr char kJSGetPasswordType[] = "getPassword";
// Get password complete arguments (Page -> Plugin)
constexpr char kJSGetPasswordCompleteType[] = "getPasswordComplete";
constexpr char kJSPassword[] = "password";
// Print (Page -> Plugin)
constexpr char kJSPrintType[] = "print";
// Save attachment (Page -> Plugin)
constexpr char kJSSaveAttachmentType[] = "saveAttachment";
constexpr char kJSAttachmentIndex[] = "attachmentIndex";
// Save attachment data (Plugin -> Page)
constexpr char kJSSaveAttachmentDataType[] = "saveAttachmentData";
constexpr char kJSAttachmentDataToSave[] = "dataToSave";
// Save (Page -> Plugin)
constexpr char kJSSaveType[] = "save";
constexpr char kJSToken[] = "token";
constexpr char kJSSaveRequestType[] = "saveRequestType";
// Save data (Plugin -> Page)
constexpr char kJSSaveDataType[] = "saveData";
constexpr char kJSFileName[] = "fileName";
constexpr char kJSDataToSave[] = "dataToSave";
constexpr char kJSHasUnsavedChanges[] = "hasUnsavedChanges";
// Consume save token (Plugin -> Page)
constexpr char kJSConsumeSaveTokenType[] = "consumeSaveToken";
// Notify when touch selection occurs (Plugin -> Page)
constexpr char kJSTouchSelectionOccurredType[] = "touchSelectionOccurred";
// Go to page (Plugin -> Page)
constexpr char kJSGoToPageType[] = "goToPage";
constexpr char kJSPageNumber[] = "page";
// Reset print preview mode (Page -> Plugin)
constexpr char kJSResetPrintPreviewModeType[] = "resetPrintPreviewMode";
constexpr char kJSPrintPreviewUrl[] = "url";
constexpr char kJSPrintPreviewGrayscale[] = "grayscale";
constexpr char kJSPrintPreviewPageCount[] = "pageCount";
// Background color changed (Page -> Plugin)
constexpr char kJSBackgroundColorChangedType[] = "backgroundColorChanged";
constexpr char kJSBackgroundColor[] = "backgroundColor";
// Load preview page (Page -> Plugin)
constexpr char kJSLoadPreviewPageType[] = "loadPreviewPage";
constexpr char kJSPreviewPageUrl[] = "url";
constexpr char kJSPreviewPageIndex[] = "index";
// Set scroll position (Plugin -> Page)
constexpr char kJSSetScrollPositionType[] = "setScrollPosition";
constexpr char kJSPositionX[] = "x";
constexpr char kJSPositionY[] = "y";
// Scroll by (Plugin -> Page)
constexpr char kJSScrollByType[] = "scrollBy";
// Navigate to the given URL (Plugin -> Page)
constexpr char kJSNavigateType[] = "navigate";
constexpr char kJSNavigateUrl[] = "url";
constexpr char kJSNavigateWindowOpenDisposition[] = "disposition";
// Navigate to the given destination (Plugin -> Page)
constexpr char kJSNavigateToDestinationType[] = "navigateToDestination";
constexpr char kJSNavigateToDestinationPage[] = "page";
constexpr char kJSNavigateToDestinationXOffset[] = "x";
constexpr char kJSNavigateToDestinationYOffset[] = "y";
constexpr char kJSNavigateToDestinationZoom[] = "zoom";
// Open the email editor with the given parameters (Plugin -> Page)
constexpr char kJSEmailType[] = "email";
constexpr char kJSEmailTo[] = "to";
constexpr char kJSEmailCc[] = "cc";
constexpr char kJSEmailBcc[] = "bcc";
constexpr char kJSEmailSubject[] = "subject";
constexpr char kJSEmailBody[] = "body";
// Rotation (Page -> Plugin)
constexpr char kJSRotateClockwiseType[] = "rotateClockwise";
constexpr char kJSRotateCounterclockwiseType[] = "rotateCounterclockwise";
// Toggle two-up view (Page -> Plugin)
constexpr char kJSSetTwoUpViewType[] = "setTwoUpView";
constexpr char kJSEnableTwoUpView[] = "enableTwoUpView";
// Display annotations (Page -> Plugin)
constexpr char kJSDisplayAnnotationsType[] = "displayAnnotations";
constexpr char kJSDisplayAnnotations[] = "display";
// Select all text in the document (Page -> Plugin)
constexpr char kJSSelectAllType[] = "selectAll";
// Get the selected text in the document (Page -> Plugin)
constexpr char kJSGetSelectedTextType[] = "getSelectedText";
// Reply with selected text (Plugin -> Page)
constexpr char kJSGetSelectedTextReplyType[] = "getSelectedTextReply";
constexpr char kJSSelectedText[] = "selectedText";

// Get the named destination with the given name (Page -> Plugin)
constexpr char kJSGetNamedDestinationType[] = "getNamedDestination";
constexpr char kJSGetNamedDestination[] = "namedDestination";
// Reply with the page number of the named destination (Plugin -> Page)
constexpr char kJSGetNamedDestinationReplyType[] = "getNamedDestinationReply";
constexpr char kJSNamedDestinationPageNumber[] = "pageNumber";
constexpr char kJSNamedDestinationView[] = "namedDestinationView";

// Selecting text in document (Plugin -> Page)
constexpr char kJSSetIsSelectingType[] = "setIsSelecting";
constexpr char kJSIsSelecting[] = "isSelecting";

// Editing forms in document (Plugin -> Page)
constexpr char kJSSetIsEditingType[] = "setIsEditing";

// Notify when a form field is focused (Plugin -> Page)
constexpr char kJSFieldFocusType[] = "formFocusChange";
constexpr char kJSFieldFocus[] = "focused";

// Notify when document is focused (Plugin -> Page)
constexpr char kJSDocumentFocusChangedType[] = "documentFocusChanged";
constexpr char kJSDocumentHasFocus[] = "hasFocus";

// Request the thumbnail image for a particular page (Page -> Plugin)
constexpr char kJSGetThumbnailType[] = "getThumbnail";
constexpr char kJSGetThumbnailPage[] = "page";
// Reply with the image data of the requested thumbnail (Plugin -> Page)
constexpr char kJSGetThumbnailReplyType[] = "getThumbnailReply";
constexpr char kJSGetThumbnailImageData[] = "imageData";
constexpr char kJSGetThumbnailWidth[] = "width";
constexpr char kJSGetThumbnailHeight[] = "height";

constexpr int kFindResultCooldownMs = 100;

// Do not save files with over 100 MB. This cap should be kept in sync with and
// is also enforced in chrome/browser/resources/pdf/pdf_viewer.js.
constexpr size_t kMaximumSavedFileSize = 100u * 1000u * 1000u;

// Same value as printing::COMPLETE_PREVIEW_DOCUMENT_INDEX.
constexpr int kCompletePDFIndex = -1;
// A different negative value to differentiate itself from |kCompletePDFIndex|.
constexpr int kInvalidPDFIndex = -2;

// A delay to wait between each accessibility page to keep the system
// responsive.
constexpr int kAccessibilityPageDelayMs = 100;

constexpr double kMinZoom = 0.01;

constexpr char kPPPPdfInterface[] = PPP_PDF_INTERFACE_1;

PP_Var GetLinkAtPosition(PP_Instance instance, PP_Point point) {
  pp::Var var;
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    var = static_cast<OutOfProcessInstance*>(object)->GetLinkAtPosition(
        pp::Point(point));
  }
  return var.Detach();
}

void Transform(PP_Instance instance, PP_PrivatePageTransformType type) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    switch (type) {
      case PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW:
        obj_instance->RotateClockwise();
        break;
      case PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW:
        obj_instance->RotateCounterclockwise();
        break;
    }
  }
}

PP_Bool GetPrintPresetOptionsFromDocument(
    PP_Instance instance,
    PP_PdfPrintPresetOptions_Dev* options) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->GetPrintPresetOptionsFromDocument(options);
  }
  return PP_TRUE;
}

void EnableAccessibility(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->EnableAccessibility();
  }
}

void SetCaretPosition(PP_Instance instance, const PP_FloatPoint* position) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->SetCaretPosition(*position);
  }
}

void MoveRangeSelectionExtent(PP_Instance instance,
                              const PP_FloatPoint* extent) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->MoveRangeSelectionExtent(*extent);
  }
}

void SetSelectionBounds(PP_Instance instance,
                        const PP_FloatPoint* base,
                        const PP_FloatPoint* extent) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->SetSelectionBounds(*base, *extent);
  }
}

PP_Bool CanEditText(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (!object)
    return PP_FALSE;

  auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
  return PP_FromBool(obj_instance->CanEditText());
}

PP_Bool HasEditableText(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (!object)
    return PP_FALSE;

  auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
  return PP_FromBool(obj_instance->HasEditableText());
}

void ReplaceSelection(PP_Instance instance, const char* text) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->ReplaceSelection(text);
  }
}

PP_Bool CanUndo(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (!object)
    return PP_FALSE;

  auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
  return PP_FromBool(obj_instance->CanUndo());
}

PP_Bool CanRedo(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (!object)
    return PP_FALSE;

  auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
  return PP_FromBool(obj_instance->CanRedo());
}

void Undo(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->Undo();
  }
}

void Redo(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->Redo();
  }
}

void HandleAccessibilityAction(
    PP_Instance instance,
    const PP_PdfAccessibilityActionData& action_data) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->HandleAccessibilityAction(action_data);
  }
}

int32_t PdfPrintBegin(PP_Instance instance,
                      const PP_PrintSettings_Dev* print_settings,
                      const PP_PdfPrintSettings_Dev* pdf_print_settings) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (!object)
    return 0;

  auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
  return obj_instance->PdfPrintBegin(print_settings, pdf_print_settings);
}

const PPP_Pdf ppp_private = {
    &GetLinkAtPosition,
    &Transform,
    &GetPrintPresetOptionsFromDocument,
    &EnableAccessibility,
    &SetCaretPosition,
    &MoveRangeSelectionExtent,
    &SetSelectionBounds,
    &CanEditText,
    &HasEditableText,
    &ReplaceSelection,
    &CanUndo,
    &CanRedo,
    &Undo,
    &Redo,
    &HandleAccessibilityAction,
    &PdfPrintBegin,
};

int ExtractPrintPreviewPageIndex(base::StringPiece src_url) {
  // Sample |src_url| format: chrome://print/id/page_index/print.pdf
  // The page_index is zero-based, but can be negative with special meanings.
  std::vector<base::StringPiece> url_substr =
      base::SplitStringPiece(src_url.substr(strlen(kChromePrint)), "/",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (url_substr.size() != 3)
    return kInvalidPDFIndex;

  if (url_substr[2] != "print.pdf")
    return kInvalidPDFIndex;

  int page_index = 0;
  if (!base::StringToInt(url_substr[1], &page_index))
    return kInvalidPDFIndex;
  return page_index;
}

bool IsPrintPreviewUrl(base::StringPiece url) {
  return base::StartsWith(url, kChromePrint);
}

bool IsPreviewingPDF(int print_preview_page_count) {
  return print_preview_page_count == 0;
}

void ScaleFloatPoint(float scale, pp::FloatPoint* point) {
  point->set_x(point->x() * scale);
  point->set_y(point->y() * scale);
}

void ScalePoint(float scale, pp::Point* point) {
  point->set_x(static_cast<int>(point->x() * scale));
  point->set_y(static_cast<int>(point->y() * scale));
}

bool IsSaveDataSizeValid(size_t size) {
  return size > 0 && size <= kMaximumSavedFileSize;
}

PDFiumFormFiller::ScriptOption DefaultScriptOption() {
#if defined(PDF_ENABLE_XFA)
  return PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA;
#else   // defined(PDF_ENABLE_XFA)
  return PDFiumFormFiller::ScriptOption::kJavaScript;
#endif  // defined(PDF_ENABLE_XFA)
}

}  // namespace

OutOfProcessInstance::OutOfProcessInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::Find_Private(this),
      pp::Printing_Dev(this),
      paint_manager_(this) {
  pp::Module::Get()->AddPluginInterface(kPPPPdfInterface, &ppp_private);
  AddPerInstanceObject(kPPPPdfInterface, this);

  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_TOUCH);
}

OutOfProcessInstance::~OutOfProcessInstance() {
  RemovePerInstanceObject(kPPPPdfInterface, this);
  // Explicitly destroy the PDFEngine during destruction as it may call back
  // into this object.
  DestroyEngine();
}

bool OutOfProcessInstance::Init(uint32_t argc,
                                const char* argn[],
                                const char* argv[]) {
  DCHECK(!engine());

  pp::Var document_url_var = pp::URLUtil_Dev::Get()->GetDocumentURL(this);
  if (!document_url_var.is_string())
    return false;

  // Check if the PDF is being loaded in the PDF chrome extension. We only allow
  // the plugin to be loaded in the extension and print preview to avoid
  // exposing sensitive APIs directly to external websites.
  //
  // This is enforced before launching the plugin process (see
  // ChromeContentBrowserClient::ShouldAllowPluginCreation), so below we just do
  // a CHECK as a defense-in-depth.
  std::string document_url = document_url_var.AsString();
  base::StringPiece document_url_piece(document_url);
  is_print_preview_ = IsPrintPreviewUrl(document_url_piece);
  CHECK(base::StartsWith(document_url_piece, kChromeExtension) ||
        is_print_preview_);

  // Check if the plugin is full frame. This is passed in from JS.
  for (uint32_t i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "full-frame") == 0) {
      full_ = true;
      break;
    }
  }

  // Allow the plugin to handle find requests.
  SetPluginToHandleFindRequests();

  text_input_ = std::make_unique<pp::TextInput_Dev>(this);

  PDFiumFormFiller::ScriptOption script_option = DefaultScriptOption();
  bool has_edits = false;
  const char* stream_url = nullptr;
  const char* original_url = nullptr;
  const char* top_level_url = nullptr;
  const char* headers = nullptr;
  for (uint32_t i = 0; i < argc; ++i) {
    bool success = true;
    if (strcmp(argn[i], "src") == 0) {
      original_url = argv[i];
    } else if (strcmp(argn[i], "stream-url") == 0) {
      stream_url = argv[i];
    } else if (strcmp(argn[i], "top-level-url") == 0) {
      top_level_url = argv[i];
    } else if (strcmp(argn[i], "headers") == 0) {
      headers = argv[i];
    } else if (strcmp(argn[i], "background-color") == 0) {
      success = base::HexStringToUInt(argv[i], &background_color_);
    } else if (strcmp(argn[i], "top-toolbar-height") == 0) {
      success =
          base::StringToInt(argv[i], &top_toolbar_height_in_viewport_coords_);
    } else if (strcmp(argn[i], "javascript") == 0) {
      if (base::FeatureList::IsEnabled(features::kPdfHonorJsContentSettings)) {
        if (strcmp(argv[i], "allow") != 0)
          script_option = PDFiumFormFiller::ScriptOption::kNoJavaScript;
      }
    } else if (strcmp(argn[i], "has-edits") == 0) {
      has_edits = true;
    }
    if (!success)
      return false;
  }

  if (!original_url)
    return false;

  if (!stream_url)
    stream_url = original_url;

  InitializeEngine(script_option);

  // If we're in print preview mode we don't need to load the document yet.
  // A |kJSResetPrintPreviewModeType| message will be sent to the plugin letting
  // it know the url to load. By not loading here we avoid loading the same
  // document twice.
  if (IsPrintPreview())
    return true;

  LoadUrl(stream_url, /*is_print_preview=*/false);
  url_ = original_url;
  edit_mode_ = has_edits;
  pp::PDF::SetCrashData(GetPluginInstance(), original_url, top_level_url);
  return engine()->New(original_url, headers);
}

void OutOfProcessInstance::HandleMessage(const pp::Var& message) {
  pp::VarDictionary dict(message);
  if (!dict.Get(kType).is_string()) {
    NOTREACHED();
    return;
  }

  std::string type = dict.Get(kType).AsString();

  if (type == kJSViewportType) {
    HandleViewportMessage(dict);
  } else if (type == kJSUpdateScrollType) {
    HandleUpdateScrollMessage(dict);
  } else if (type == kJSGetPasswordCompleteType) {
    HandleGetPasswordCompleteMessage(dict);
  } else if (type == kJSPrintType) {
    Print();
  } else if (type == kJSSaveAttachmentType) {
    HandleSaveAttachmentMessage(dict);
  } else if (type == kJSSaveType) {
    HandleSaveMessage(dict);
  } else if (type == kJSRotateClockwiseType) {
    RotateClockwise();
  } else if (type == kJSRotateCounterclockwiseType) {
    RotateCounterclockwise();
  } else if (type == kJSSetTwoUpViewType) {
    HandleSetTwoUpViewMessage(dict);
  } else if (type == kJSDisplayAnnotationsType) {
    HandleDisplayAnnotations(dict);
  } else if (type == kJSSelectAllType) {
    engine()->SelectAll();
  } else if (type == kJSBackgroundColorChangedType) {
    HandleBackgroundColorChangedMessage(dict);
  } else if (type == kJSResetPrintPreviewModeType) {
    HandleResetPrintPreviewModeMessage(dict);
  } else if (type == kJSLoadPreviewPageType) {
    HandleLoadPreviewPageMessage(dict);
  } else if (type == kJSStopScrollingType) {
    stop_scrolling_ = true;
  } else if (type == kJSGetSelectedTextType) {
    HandleGetSelectedTextMessage(dict);
  } else if (type == kJSGetNamedDestinationType) {
    HandleGetNamedDestinationMessage(dict);
  } else if (type == kJSGetThumbnailType) {
    HandleGetThumbnailMessage(dict);
  } else {
    NOTREACHED();
  }
}

bool OutOfProcessInstance::HandleInputEvent(const pp::InputEvent& event) {
  // To simplify things, convert the event into device coordinates.
  pp::InputEvent event_device_res(event);
  {
    pp::MouseInputEvent mouse_event(event);
    if (!mouse_event.is_null()) {
      pp::Point point = mouse_event.GetPosition();
      pp::Point movement = mouse_event.GetMovement();
      ScalePoint(device_scale_, &point);
      point.set_x(point.x() - available_area_.x());

      ScalePoint(device_scale_, &movement);
      mouse_event =
          pp::MouseInputEvent(this, event.GetType(), event.GetTimeStamp(),
                              event.GetModifiers(), mouse_event.GetButton(),
                              point, mouse_event.GetClickCount(), movement);
      event_device_res = mouse_event;
    }
  }
  {
    pp::TouchInputEvent touch_event(event);
    if (!touch_event.is_null()) {
      pp::TouchInputEvent new_touch_event = pp::TouchInputEvent(
          this, touch_event.GetType(), touch_event.GetTimeStamp(),
          touch_event.GetModifiers());

      for (uint32_t i = 0;
           i < touch_event.GetTouchCount(PP_TOUCHLIST_TYPE_TARGETTOUCHES);
           i++) {
        pp::TouchPoint touch_point =
            touch_event.GetTouchByIndex(PP_TOUCHLIST_TYPE_TARGETTOUCHES, i);

        pp::FloatPoint point = touch_point.position();
        ScaleFloatPoint(device_scale_, &point);
        point.set_x(point.x() - available_area_.x());

        new_touch_event.AddTouchPoint(
            PP_TOUCHLIST_TYPE_TARGETTOUCHES,
            {touch_point.id(), point, touch_point.radii(),
             touch_point.rotation_angle(), touch_point.pressure()});
      }
      event_device_res = new_touch_event;
    }
  }

  if (SendInputEventToEngine(event_device_res))
    return true;

  // Middle click is used for scrolling and is handled by the container page.
  pp::MouseInputEvent mouse_event(event_device_res);
  if (!mouse_event.is_null() &&
      mouse_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    return false;
  }

  // Return true for unhandled clicks so the plugin takes focus.
  return (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN);
}

void OutOfProcessInstance::DidChangeView(const pp::View& view) {
  pp::Rect view_rect(view.GetRect());
  float old_device_scale = device_scale_;
  float device_scale = view.GetDeviceScale();
  pp::Size view_device_size(view_rect.width() * device_scale,
                            view_rect.height() * device_scale);

  if (view_device_size != plugin_size_ || device_scale != device_scale_ ||
      view_rect.point() != plugin_offset_) {
    device_scale_ = device_scale;
    plugin_dip_size_ = view_rect.size();
    plugin_size_ = view_device_size;
    plugin_offset_ = view_rect.point();

    paint_manager_.SetSize(SizeFromPPSize(view_device_size), device_scale_);

    const gfx::Size old_image_data_size = SizeFromPPSize(image_data_.size());
    gfx::Size new_image_data_size = PaintManager::GetNewContextSize(
        old_image_data_size, SizeFromPPSize(plugin_size_));
    if (new_image_data_size != old_image_data_size) {
      image_data_ = pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                  PPSizeFromSize(new_image_data_size), false);
      skia_image_data_ =
          SkBitmapFromPPImageData(std::make_unique<pp::ImageData>(image_data_));
      first_paint_ = true;
    }

    if (image_data_.is_null()) {
      DCHECK(plugin_size_.IsEmpty());
      return;
    }

    OnGeometryChanged(zoom_, old_device_scale);
  }

  if (!is_print_preview_ &&
      base::FeatureList::IsEnabled(features::kPDFViewerUpdate)) {
    // Scrolling in the new PDF Viewer UI is already handled by
    // HandleUpdateScrollMessage().
    return;
  }

  if (!stop_scrolling_) {
    scroll_offset_ = view.GetScrollOffset();
    UpdateScroll();
  }
}

void OutOfProcessInstance::UpdateScroll() {
  DCHECK(!stop_scrolling_);

  // Because view messages come from the DOM, the coordinates of the viewport
  // are 0-based (i.e. they do not correspond to the viewport's coordinates in
  // JS), so we need to subtract the toolbar height to convert them into
  // viewport coordinates.
  pp::FloatPoint scroll_offset_float(
      scroll_offset_.x(),
      scroll_offset_.y() - top_toolbar_height_in_viewport_coords_);
  scroll_offset_float = BoundScrollOffsetToDocument(scroll_offset_float);
  engine()->ScrolledToXPosition(scroll_offset_float.x() * device_scale_);
  engine()->ScrolledToYPosition(scroll_offset_float.y() * device_scale_);
}

void OutOfProcessInstance::DidChangeFocus(bool has_focus) {
  engine()->UpdateFocus(has_focus);
}

void OutOfProcessInstance::GetPrintPresetOptionsFromDocument(
    PP_PdfPrintPresetOptions_Dev* options) {
  options->is_scaling_disabled = PP_FromBool(IsPrintScalingDisabled());
  options->duplex =
      static_cast<PP_PrivateDuplexMode_Dev>(engine()->GetDuplexType());
  options->copies = engine()->GetCopiesToPrint();
  gfx::Size uniform_page_size;
  options->is_page_size_uniform =
      PP_FromBool(engine()->GetPageSizeAndUniformity(&uniform_page_size));
  options->uniform_page_size = PPSizeFromSize(uniform_page_size);
}

void OutOfProcessInstance::EnableAccessibility() {
  if (accessibility_state_ == ACCESSIBILITY_STATE_LOADED)
    return;

  if (accessibility_state_ == ACCESSIBILITY_STATE_OFF)
    accessibility_state_ = ACCESSIBILITY_STATE_PENDING;

  if (document_load_state_ == LOAD_STATE_COMPLETE)
    LoadAccessibility();
}

void OutOfProcessInstance::LoadAccessibility() {
  accessibility_state_ = ACCESSIBILITY_STATE_LOADED;
  PP_PrivateAccessibilityDocInfo doc_info;
  doc_info.page_count = engine()->GetNumberOfPages();
  doc_info.text_accessible = PP_FromBool(
      engine()->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE));
  doc_info.text_copyable =
      PP_FromBool(engine()->HasPermission(PDFEngine::PERMISSION_COPY));

  pp::PDF::SetAccessibilityDocInfo(GetPluginInstance(), &doc_info);

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine()->HasPermission(PDFEngine::PERMISSION_COPY) ||
        engine()->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE))) {
    return;
  }

  SendAccessibilityViewportInfo();

  // Schedule loading the first page.
  pp::Module::Get()->core()->CallOnMainThread(
      kAccessibilityPageDelayMs,
      PPCompletionCallbackFromResultCallback(
          base::BindOnce(&OutOfProcessInstance::SendNextAccessibilityPage,
                         weak_factory_.GetWeakPtr())),
      0);
}

void OutOfProcessInstance::SendNextAccessibilityPage(int32_t page_index) {
  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;

  if (!GetAccessibilityInfo(engine(), page_index, &page_info, &text_runs,
                            &chars, &page_objects)) {
    return;
  }

  pp::PDF::SetAccessibilityPageInfo(GetPluginInstance(), &page_info, text_runs,
                                    chars, page_objects);

  // Schedule loading the next page.
  pp::Module::Get()->core()->CallOnMainThread(
      kAccessibilityPageDelayMs,
      PPCompletionCallbackFromResultCallback(
          base::BindOnce(&OutOfProcessInstance::SendNextAccessibilityPage,
                         weak_factory_.GetWeakPtr())),
      page_index + 1);
}

void OutOfProcessInstance::SendAccessibilityViewportInfo() {
  PP_PrivateAccessibilityViewportInfo viewport_info;
  viewport_info.scroll.x = -plugin_offset_.x();
  viewport_info.scroll.y =
      -top_toolbar_height_in_viewport_coords_ - plugin_offset_.y();
  viewport_info.offset.x =
      available_area_.point().x() / (device_scale_ * zoom_);
  viewport_info.offset.y =
      available_area_.point().y() / (device_scale_ * zoom_);

  viewport_info.zoom = zoom_;
  viewport_info.scale = device_scale_;
  viewport_info.focus_info = {
      PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_NONE, 0, 0};

  engine()->GetSelection(&viewport_info.selection_start_page_index,
                         &viewport_info.selection_start_char_index,
                         &viewport_info.selection_end_page_index,
                         &viewport_info.selection_end_char_index);

  pp::PDF::SetAccessibilityViewportInfo(GetPluginInstance(), &viewport_info);
}

void OutOfProcessInstance::SelectionChanged(const gfx::Rect& left,
                                            const gfx::Rect& right) {
  pp::Point l(left.x() + available_area_.x(), left.y());
  pp::Point r(right.x() + available_area_.x(), right.y());

  float inverse_scale = 1.0f / device_scale_;
  ScalePoint(inverse_scale, &l);
  ScalePoint(inverse_scale, &r);

  pp::PDF::SelectionChanged(GetPluginInstance(),
                            PP_MakeFloatPoint(l.x(), l.y()), left.height(),
                            PP_MakeFloatPoint(r.x(), r.y()), right.height());
  if (accessibility_state_ == ACCESSIBILITY_STATE_LOADED)
    SendAccessibilityViewportInfo();
}

void OutOfProcessInstance::SetCaretPosition(const pp::FloatPoint& position) {
  pp::Point new_position(position.x(), position.y());
  ScalePoint(device_scale_, &new_position);
  new_position.set_x(new_position.x() - available_area_.x());
  engine()->SetCaretPosition(PointFromPPPoint(new_position));
}

void OutOfProcessInstance::MoveRangeSelectionExtent(
    const pp::FloatPoint& extent) {
  pp::Point new_extent(extent.x(), extent.y());
  ScalePoint(device_scale_, &new_extent);
  new_extent.set_x(new_extent.x() - available_area_.x());
  engine()->MoveRangeSelectionExtent(PointFromPPPoint(new_extent));
}

void OutOfProcessInstance::SetSelectionBounds(const pp::FloatPoint& base,
                                              const pp::FloatPoint& extent) {
  pp::Point new_base_point(base.x(), base.y());
  ScalePoint(device_scale_, &new_base_point);
  new_base_point.set_x(new_base_point.x() - available_area_.x());

  pp::Point new_extent_point(extent.x(), extent.y());
  ScalePoint(device_scale_, &new_extent_point);
  new_extent_point.set_x(new_extent_point.x() - available_area_.x());

  engine()->SetSelectionBounds(PointFromPPPoint(new_base_point),
                               PointFromPPPoint(new_extent_point));
}

pp::Var OutOfProcessInstance::GetLinkAtPosition(const pp::Point& point) {
  pp::Point offset_point(point);
  ScalePoint(device_scale_, &offset_point);
  offset_point.set_x(offset_point.x() - available_area_.x());
  return engine()->GetLinkAtPosition(PointFromPPPoint(offset_point));
}

bool OutOfProcessInstance::CanEditText() {
  return engine()->CanEditText();
}

bool OutOfProcessInstance::HasEditableText() {
  return engine()->HasEditableText();
}

void OutOfProcessInstance::ReplaceSelection(const std::string& text) {
  engine()->ReplaceSelection(text);
}

bool OutOfProcessInstance::CanUndo() {
  return engine()->CanUndo();
}

bool OutOfProcessInstance::CanRedo() {
  return engine()->CanRedo();
}

void OutOfProcessInstance::Undo() {
  engine()->Undo();
}

void OutOfProcessInstance::Redo() {
  engine()->Redo();
}

void OutOfProcessInstance::HandleAccessibilityAction(
    const PP_PdfAccessibilityActionData& action_data) {
  engine()->HandleAccessibilityAction(action_data);
}

int32_t OutOfProcessInstance::PdfPrintBegin(
    const PP_PrintSettings_Dev* print_settings,
    const PP_PdfPrintSettings_Dev* pdf_print_settings) {
  // For us num_pages is always equal to the number of pages in the PDF
  // document irrespective of the printable area.
  int32_t ret = engine()->GetNumberOfPages();
  if (!ret)
    return 0;

  uint32_t supported_formats = engine()->QuerySupportedPrintOutputFormats();
  if ((print_settings->format & supported_formats) == 0)
    return 0;

  print_settings_.is_printing = true;
  print_settings_.pepper_print_settings = *print_settings;
  print_settings_.pdf_print_settings = *pdf_print_settings;
  engine()->PrintBegin();
  return ret;
}

uint32_t OutOfProcessInstance::QuerySupportedPrintOutputFormats() {
  return engine()->QuerySupportedPrintOutputFormats();
}

int32_t OutOfProcessInstance::PrintBegin(
    const PP_PrintSettings_Dev& print_settings) {
  // Replaced with PdfPrintBegin();
  NOTREACHED();
  return 0;
}

pp::Resource OutOfProcessInstance::PrintPages(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  if (!print_settings_.is_printing)
    return pp::Resource();

  print_settings_.print_pages_called = true;
  return engine()->PrintPages(page_ranges, page_range_count,
                              print_settings_.pepper_print_settings,
                              print_settings_.pdf_print_settings);
}

void OutOfProcessInstance::PrintEnd() {
  if (print_settings_.print_pages_called)
    UserMetricsRecordAction("PDF.PrintPage");
  print_settings_.Clear();
  engine()->PrintEnd();
}

bool OutOfProcessInstance::IsPrintScalingDisabled() {
  return !engine()->GetPrintScaling();
}

bool OutOfProcessInstance::StartFind(const std::string& text,
                                     bool case_sensitive) {
  engine()->StartFind(text, case_sensitive);
  return true;
}

void OutOfProcessInstance::SelectFindResult(bool forward) {
  engine()->SelectFindResult(forward);
}

void OutOfProcessInstance::StopFind() {
  engine()->StopFind();
  tickmarks_.clear();
  SetTickmarks(tickmarks_);
}

std::unique_ptr<Graphics> OutOfProcessInstance::CreatePaintGraphics(
    const gfx::Size& size) {
  auto graphics = std::make_unique<PepperGraphics>(this, size);
  DCHECK(!graphics->pepper_graphics().is_null());
  return graphics;
}

bool OutOfProcessInstance::BindPaintGraphics(Graphics& graphics) {
  return BindGraphics(static_cast<PepperGraphics&>(graphics).pepper_graphics());
}

void OutOfProcessInstance::OnPaint(const std::vector<gfx::Rect>& paint_rects,
                                   std::vector<PaintReadyRect>* ready,
                                   std::vector<gfx::Rect>* pending) {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);
  if (image_data_.is_null()) {
    DCHECK(plugin_size_.IsEmpty());
    return;
  }
  if (first_paint_) {
    first_paint_ = false;
    pp::Rect rect = pp::Rect(pp::Point(), image_data_.size());
    FillRect(rect, background_color_);
    ready->push_back(PaintReadyRect(rect, image_data_, /*flush_now=*/true));
  }

  if (!received_viewport_message_ || !needs_reraster_)
    return;

  engine()->PrePaint();

  for (const auto& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    pp::Rect rect = PPRectFromRect(paint_rect);
    rect = rect.Intersect(pp::Rect(pp::Point(), plugin_size_));
    if (rect.IsEmpty())
      continue;

    pp::Rect pdf_rect = available_area_.Intersect(rect);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(available_area_.x() * -1, 0);

      std::vector<gfx::Rect> pdf_ready;
      std::vector<gfx::Rect> pdf_pending;
      engine()->Paint(RectFromPPRect(pdf_rect), skia_image_data_, pdf_ready,
                      pdf_pending);
      for (auto& ready_rect : pdf_ready) {
        ready_rect.Offset(VectorFromPPPoint(available_area_.point()));
        ready->push_back(
            PaintReadyRect(PPRectFromRect(ready_rect), image_data_));
      }
      for (auto& pending_rect : pdf_pending) {
        pending_rect.Offset(VectorFromPPPoint(available_area_.point()));
        pending->push_back(pending_rect);
      }
    }

    // Ensure the region above the first page (if any) is filled;
    int32_t first_page_ypos = engine()->GetNumberOfPages() == 0
                                  ? 0
                                  : engine()->GetPageScreenRect(0).y();
    if (rect.y() < first_page_ypos) {
      pp::Rect region = rect.Intersect(pp::Rect(
          pp::Point(), pp::Size(plugin_size_.width(), first_page_ypos)));
      ready->push_back(PaintReadyRect(region, image_data_));
      FillRect(region, background_color_);
    }

    for (const auto& background_part : background_parts_) {
      pp::Rect intersection = background_part.location.Intersect(rect);
      if (!intersection.IsEmpty()) {
        FillRect(intersection, background_part.color);
        ready->push_back(PaintReadyRect(intersection, image_data_));
      }
    }
  }

  engine()->PostPaint();

  if (!deferred_invalidates_.empty()) {
    pp::Module::Get()->core()->CallOnMainThread(
        0, PPCompletionCallbackFromResultCallback(
               base::BindOnce(&OutOfProcessInstance::InvalidateAfterPaintDone,
                              weak_factory_.GetWeakPtr())));
  }
}

void OutOfProcessInstance::DidOpen(std::unique_ptr<UrlLoader> loader,
                                   int32_t result) {
  if (result == PP_OK) {
    if (!engine()->HandleDocumentLoad(std::move(loader))) {
      document_load_state_ = LOAD_STATE_LOADING;
      DocumentLoadFailed();
    }
  } else if (result != PP_ERROR_ABORTED) {  // Can happen in tests.
    DocumentLoadFailed();
  }
}

void OutOfProcessInstance::DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                                          int32_t result) {
  if (result == PP_OK) {
    preview_client_ = std::make_unique<PreviewModeClient>(this);
    preview_engine_ = std::make_unique<PDFiumEngine>(
        preview_client_.get(), PDFiumFormFiller::ScriptOption::kNoJavaScript);
    preview_engine_->HandleDocumentLoad(std::move(loader));
  } else {
    NOTREACHED();
  }
}

void OutOfProcessInstance::CalculateBackgroundParts() {
  background_parts_.clear();
  int left_width = available_area_.x();
  int right_start = available_area_.right();
  int right_width = abs(plugin_size_.width() - available_area_.right());
  int bottom = std::min(available_area_.bottom(), plugin_size_.height());

  // Add the left, right, and bottom rectangles.  Note: we assume only
  // horizontal centering.
  BackgroundPart part = {pp::Rect(0, 0, left_width, bottom), background_color_};
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
  part.location = pp::Rect(right_start, 0, right_width, bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
  part.location =
      pp::Rect(0, bottom, plugin_size_.width(), plugin_size_.height() - bottom);
  if (!part.location.IsEmpty())
    background_parts_.push_back(part);
}

pp::VarArray OutOfProcessInstance::GetDocumentAttachments() {
  const std::vector<DocumentAttachmentInfo>& list =
      engine()->GetDocumentAttachmentInfoList();
  pp::VarArray attachments;
  attachments.SetLength(list.size());

  for (size_t i = 0; i < list.size(); ++i) {
    const DocumentAttachmentInfo& attachment_info = list[i];
    pp::VarDictionary dict;
    dict.Set(pp::Var("name"), pp::Var(base::UTF16ToUTF8(attachment_info.name)));
    // Set |size| to -1 to indicate that the attachment is too big to be
    // downloaded.
    int32_t size = attachment_info.size_bytes <= kMaximumSavedFileSize
                       ? static_cast<int32_t>(attachment_info.size_bytes)
                       : -1;
    dict.Set(pp::Var("size"), pp::Var(size));
    dict.Set(pp::Var("readable"), pp::Var(attachment_info.is_readable));
    attachments.Set(i, dict);
  }
  return attachments;
}

int OutOfProcessInstance::GetDocumentPixelWidth() const {
  return static_cast<int>(ceil(document_size_.width() * zoom_ * device_scale_));
}

int OutOfProcessInstance::GetDocumentPixelHeight() const {
  return static_cast<int>(
      ceil(document_size_.height() * zoom_ * device_scale_));
}

void OutOfProcessInstance::FillRect(const pp::Rect& rect, uint32_t color) {
  DCHECK(!image_data_.is_null() || rect.IsEmpty());
  uint32_t* buffer_start = static_cast<uint32_t*>(image_data_.data());
  int stride = image_data_.stride();
  uint32_t* ptr = buffer_start + rect.y() * stride / 4 + rect.x();
  int height = rect.height();
  int width = rect.width();
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x)
      *(ptr + x) = color;
    ptr += stride / 4;
  }
}

void OutOfProcessInstance::ProposeDocumentLayout(const DocumentLayout& layout) {
  pp::VarDictionary dimensions;
  dimensions.Set(kType, kJSDocumentDimensionsType);
  dimensions.Set(kJSDocumentWidth, pp::Var(layout.size().width()));
  dimensions.Set(kJSDocumentHeight, pp::Var(layout.size().height()));
  dimensions.Set(kJSLayoutOptions, VarFromValue(layout.options().ToValue()));
  pp::VarArray page_dimensions_array;
  for (size_t i = 0; i < layout.page_count(); ++i) {
    const gfx::Rect& page_rect = layout.page_rect(i);
    pp::VarDictionary page_dimensions;
    page_dimensions.Set(kJSPageX, pp::Var(page_rect.x()));
    page_dimensions.Set(kJSPageY, pp::Var(page_rect.y()));
    page_dimensions.Set(kJSPageWidth, pp::Var(page_rect.width()));
    page_dimensions.Set(kJSPageHeight, pp::Var(page_rect.height()));
    page_dimensions_array.Set(i, page_dimensions);
  }
  dimensions.Set(kJSPageDimensions, page_dimensions_array);
  PostMessage(dimensions);
}

void OutOfProcessInstance::Invalidate(const gfx::Rect& rect) {
  if (in_paint_) {
    deferred_invalidates_.push_back(rect);
    return;
  }

  gfx::Rect offset_rect(rect);
  offset_rect.Offset(VectorFromPPPoint(available_area_.point()));
  paint_manager_.InvalidateRect(offset_rect);
}

void OutOfProcessInstance::DidScroll(const gfx::Vector2d& offset) {
  if (!image_data_.is_null())
    paint_manager_.ScrollRect(RectFromPPRect(available_area_), offset);
}

void OutOfProcessInstance::ScrollToX(int x_in_screen_coords) {
  pp::VarDictionary position;
  position.Set(kType, kJSSetScrollPositionType);
  position.Set(kJSPositionX, pp::Var(x_in_screen_coords / device_scale_));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollToY(int y_in_screen_coords,
                                     bool compensate_for_toolbar) {
  pp::VarDictionary position;
  position.Set(kType, kJSSetScrollPositionType);
  float new_y_viewport_coords = y_in_screen_coords / device_scale_;
  if (compensate_for_toolbar) {
    new_y_viewport_coords -= top_toolbar_height_in_viewport_coords_;
  }
  position.Set(kJSPositionY, pp::Var(new_y_viewport_coords));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollBy(const gfx::Vector2d& scroll_delta) {
  pp::VarDictionary position;
  position.Set(kType, kJSScrollByType);
  position.Set(kJSPositionX, pp::Var(scroll_delta.x() / device_scale_));
  position.Set(kJSPositionY, pp::Var(scroll_delta.y() / device_scale_));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollToPage(int page) {
  if (!engine() || engine()->GetNumberOfPages() == 0)
    return;

  pp::VarDictionary message;
  message.Set(kType, kJSGoToPageType);
  message.Set(kJSPageNumber, pp::Var(page));
  PostMessage(message);
}

void OutOfProcessInstance::NavigateTo(const std::string& url,
                                      WindowOpenDisposition disposition) {
  pp::VarDictionary message;
  message.Set(kType, kJSNavigateType);
  message.Set(kJSNavigateUrl, url);
  message.Set(kJSNavigateWindowOpenDisposition,
              pp::Var(static_cast<int32_t>(disposition)));
  PostMessage(message);
}

void OutOfProcessInstance::NavigateToDestination(int page,
                                                 const float* x,
                                                 const float* y,
                                                 const float* zoom) {
  pp::VarDictionary message;
  message.Set(kType, kJSNavigateToDestinationType);
  message.Set(kJSNavigateToDestinationPage, pp::Var(page));
  if (x)
    message.Set(kJSNavigateToDestinationXOffset, pp::Var(*x));
  if (y)
    message.Set(kJSNavigateToDestinationYOffset, pp::Var(*y));
  if (zoom)
    message.Set(kJSNavigateToDestinationZoom, pp::Var(*zoom));
  PostMessage(message);
}

void OutOfProcessInstance::UpdateCursor(PP_CursorType_Dev cursor) {
  if (cursor == cursor_)
    return;
  cursor_ = cursor;

  const PPB_CursorControl_Dev* cursor_interface =
      reinterpret_cast<const PPB_CursorControl_Dev*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_CURSOR_CONTROL_DEV_INTERFACE));
  if (!cursor_interface) {
    NOTREACHED();
    return;
  }

  cursor_interface->SetCursor(pp_instance(), cursor_,
                              pp::ImageData().pp_resource(), nullptr);
}

void OutOfProcessInstance::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  float inverse_scale = 1.0f / device_scale_;
  tickmarks_.clear();
  tickmarks_.reserve(tickmarks.size());
  for (auto& tickmark : tickmarks) {
    tickmarks_.emplace_back(
        PPRectFromRect(gfx::ScaleToEnclosingRect(tickmark, inverse_scale)));
  }
}

void OutOfProcessInstance::NotifyNumberOfFindResultsChanged(int total,
                                                            bool final_result) {
  // We don't want to spam the renderer with too many updates to the number of
  // find results. Don't send an update if we sent one too recently. If it's the
  // final update, we always send it though.
  if (final_result) {
    NumberOfFindResultsChanged(total, final_result);
    SetTickmarks(tickmarks_);
    return;
  }

  if (recently_sent_find_update_)
    return;

  NumberOfFindResultsChanged(total, final_result);
  SetTickmarks(tickmarks_);
  recently_sent_find_update_ = true;
  pp::Module::Get()->core()->CallOnMainThread(
      kFindResultCooldownMs,
      PPCompletionCallbackFromResultCallback(
          base::BindOnce(&OutOfProcessInstance::ResetRecentlySentFindUpdate,
                         weak_factory_.GetWeakPtr())),
      0);
}

void OutOfProcessInstance::NotifySelectedFindResultChanged(
    int current_find_index) {
  DCHECK_GE(current_find_index, -1);
  SelectedFindResultChanged(current_find_index);
}

void OutOfProcessInstance::NotifyTouchSelectionOccurred() {
  pp::VarDictionary message;
  message.Set(kType, kJSTouchSelectionOccurredType);
  PostMessage(message);
}

void OutOfProcessInstance::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {
  if (password_callback_) {
    NOTREACHED();
    return;
  }

  password_callback_ = std::move(callback);
  pp::VarDictionary message;
  message.Set(kType, kJSGetPasswordType);
  PostMessage(message);
}

bool OutOfProcessInstance::CanSaveEdits() const {
  return edit_mode_ &&
         base::FeatureList::IsEnabled(features::kSaveEditedPDFForm);
}

void OutOfProcessInstance::SaveToBuffer(const std::string& token) {
  engine()->KillFormFocus();

  pp::VarDictionary message;
  message.Set(kType, kJSSaveDataType);
  message.Set(kJSToken, pp::Var(token));
  message.Set(kJSFileName, pp::Var(GetFileNameFromUrl(url_)));
  // This will be overwritten if the save is successful.
  message.Set(kJSDataToSave, pp::Var(pp::Var::Null()));
  const bool has_unsaved_changes =
      edit_mode_ && !base::FeatureList::IsEnabled(features::kSaveEditedPDFForm);
  message.Set(kJSHasUnsavedChanges, pp::Var(has_unsaved_changes));

  if (CanSaveEdits()) {
    std::vector<uint8_t> data = engine()->GetSaveData();
    if (IsSaveDataSizeValid(data.size())) {
      pp::VarArrayBuffer buffer(data.size());
      std::copy(data.begin(), data.end(),
                reinterpret_cast<char*>(buffer.Map()));
      message.Set(kJSDataToSave, buffer);
    }
  } else {
#if defined(OS_CHROMEOS)
    uint32_t length = engine()->GetLoadedByteSize();
    if (IsSaveDataSizeValid(length)) {
      pp::VarArrayBuffer buffer(length);
      if (engine()->ReadLoadedBytes(length, buffer.Map())) {
        message.Set(kJSDataToSave, buffer);
      }
    }
#else
    NOTREACHED();
#endif
  }

  PostMessage(message);
}

void OutOfProcessInstance::SaveToFile(const std::string& token) {
  engine()->KillFormFocus();
  ConsumeSaveToken(token);
  pp::PDF::SaveAs(this);
}

void OutOfProcessInstance::ConsumeSaveToken(const std::string& token) {
  pp::VarDictionary message;
  message.Set(kType, kJSConsumeSaveTokenType);
  message.Set(kJSToken, pp::Var(token));
  PostMessage(message);
}

void OutOfProcessInstance::Beep() {
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSBeepType));
  PostMessage(message);
}

void OutOfProcessInstance::Alert(const std::string& message) {
  pp::PDF::ShowAlertDialog(this, message.c_str());
}

bool OutOfProcessInstance::Confirm(const std::string& message) {
  return pp::PDF::ShowConfirmDialog(this, message.c_str());
}

std::string OutOfProcessInstance::Prompt(const std::string& question,
                                         const std::string& default_answer) {
  pp::Var result =
      pp::PDF::ShowPromptDialog(this, question.c_str(), default_answer.c_str());
  return result.is_string() ? result.AsString() : std::string();
}

std::string OutOfProcessInstance::GetURL() {
  return url_;
}

void OutOfProcessInstance::Email(const std::string& to,
                                 const std::string& cc,
                                 const std::string& bcc,
                                 const std::string& subject,
                                 const std::string& body) {
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSEmailType));
  message.Set(pp::Var(kJSEmailTo),
              pp::Var(net::EscapeUrlEncodedData(to, false)));
  message.Set(pp::Var(kJSEmailCc),
              pp::Var(net::EscapeUrlEncodedData(cc, false)));
  message.Set(pp::Var(kJSEmailBcc),
              pp::Var(net::EscapeUrlEncodedData(bcc, false)));
  message.Set(pp::Var(kJSEmailSubject),
              pp::Var(net::EscapeUrlEncodedData(subject, false)));
  message.Set(pp::Var(kJSEmailBody),
              pp::Var(net::EscapeUrlEncodedData(body, false)));
  PostMessage(message);
}

void OutOfProcessInstance::Print() {
  if (!engine() ||
      (!engine()->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
       !engine()->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY))) {
    return;
  }

  pp::Module::Get()->core()->CallOnMainThread(
      0, PPCompletionCallbackFromResultCallback(base::BindOnce(
             &OutOfProcessInstance::OnPrint, weak_factory_.GetWeakPtr())));
}

void OutOfProcessInstance::SubmitForm(const std::string& url,
                                      const void* data,
                                      int length) {
  UrlRequest request;
  request.url = url;
  request.method = "POST";
  request.body.assign(static_cast<const char*>(data), length);

  form_loader_ = CreateUrlLoaderInternal();
  form_loader_->Open(request, base::BindOnce(&OutOfProcessInstance::FormDidOpen,
                                             weak_factory_.GetWeakPtr()));
}

void OutOfProcessInstance::FormDidOpen(int32_t result) {
  // TODO(crbug.com/719344): Process response.
  LOG_IF(ERROR, result != PP_OK) << "FormDidOpen failed: " << result;
}

std::unique_ptr<UrlLoader> OutOfProcessInstance::CreateUrlLoader() {
  if (full_) {
    if (!did_call_start_loading_) {
      did_call_start_loading_ = true;
      pp::PDF::DidStartLoading(this);
    }

    // Disable save and print until the document is fully loaded, since they
    // would generate an incomplete document.  Need to do this each time we
    // call DidStartLoading since that resets the content restrictions.
    pp::PDF::SetContentRestriction(
        this, PP_CONTENT_RESTRICTION_SAVE | PP_CONTENT_RESTRICTION_PRINT);
  }

  return CreateUrlLoaderInternal();
}

std::vector<PDFEngine::Client::SearchStringResult>
OutOfProcessInstance::SearchString(const base::char16* string,
                                   const base::char16* term,
                                   bool case_sensitive) {
  PP_PrivateFindResult* pp_results;
  uint32_t count = 0;
  pp::PDF::SearchString(this, reinterpret_cast<const unsigned short*>(string),
                        reinterpret_cast<const unsigned short*>(term),
                        case_sensitive, &pp_results, &count);

  std::vector<SearchStringResult> results(count);
  for (uint32_t i = 0; i < count; ++i) {
    results[i].start_index = pp_results[i].start_index;
    results[i].length = pp_results[i].length;
  }

  pp::Memory_Dev memory;
  memory.MemFree(pp_results);

  return results;
}

void OutOfProcessInstance::DocumentLoadComplete(
    const PDFEngine::DocumentFeatures& document_features) {
  // Clear focus state for OSK.
  FormTextFieldFocusChange(false);

  DCHECK_EQ(LOAD_STATE_LOADING, document_load_state_);
  document_load_state_ = LOAD_STATE_COMPLETE;
  UserMetricsRecordAction("PDF.LoadSuccess");

  // Note: If we are in print preview mode the scroll location is retained
  // across document loads so we don't want to scroll again and override it.
  if (IsPrintPreview()) {
    if (IsPreviewingPDF(print_preview_page_count_)) {
      SendPrintPreviewLoadedNotification();
    } else {
      DCHECK_EQ(0, print_preview_loaded_page_count_);
      print_preview_loaded_page_count_ = 1;
      AppendBlankPrintPreviewPages();
    }
    OnGeometryChanged(0, 0);
  }

  SendDocumentMetadata();
  SendLoadingProgress(/*percentage=*/100);

  if (accessibility_state_ == ACCESSIBILITY_STATE_PENDING)
    LoadAccessibility();

  if (!full_)
    return;

  if (did_call_start_loading_) {
    pp::PDF::DidStopLoading(this);
    did_call_start_loading_ = false;
  }

  int content_restrictions =
      PP_CONTENT_RESTRICTION_CUT | PP_CONTENT_RESTRICTION_PASTE;
  if (!engine()->HasPermission(PDFEngine::PERMISSION_COPY))
    content_restrictions |= PP_CONTENT_RESTRICTION_COPY;

  if (!engine()->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
      !engine()->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY)) {
    content_restrictions |= PP_CONTENT_RESTRICTION_PRINT;
  }

  pp::PDF::SetContentRestriction(this, content_restrictions);
  HistogramCustomCounts("PDF.PageCount", document_features.page_count, 1,
                        1000000, 50);
  HistogramEnumeration("PDF.HasAttachment", document_features.has_attachments
                                                ? PdfHasAttachment::kYes
                                                : PdfHasAttachment::kNo);
  HistogramEnumeration("PDF.IsTagged", document_features.is_tagged
                                           ? PdfIsTagged::kYes
                                           : PdfIsTagged::kNo);
  HistogramEnumeration("PDF.FormType", document_features.form_type,
                       PDFEngine::FormType::kCount);
  HistogramEnumeration("PDF.Version", engine()->GetDocumentMetadata().version);
}

void OutOfProcessInstance::RotateClockwise() {
  engine()->RotateClockwise();
}

void OutOfProcessInstance::RotateCounterclockwise() {
  engine()->RotateCounterclockwise();
}

// static
std::string OutOfProcessInstance::GetFileNameFromUrl(const std::string& url) {
  // Generate a file name. Unfortunately, MIME type can't be provided, since it
  // requires IO.
  base::string16 file_name = net::GetSuggestedFilename(
      GURL(url), /*content_disposition=*/std::string(),
      /*referrer_charset=*/std::string(), /*suggested_name=*/std::string(),
      /*mime_type=*/std::string(), /*default_name=*/std::string());
  return base::UTF16ToUTF8(file_name);
}

void OutOfProcessInstance::HandleBackgroundColorChangedMessage(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSBackgroundColor)).is_string()) {
    NOTREACHED();
    return;
  }
  base::HexStringToUInt(dict.Get(pp::Var(kJSBackgroundColor)).AsString(),
                        &background_color_);
}

void OutOfProcessInstance::HandleDisplayAnnotations(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSDisplayAnnotations)).is_bool()) {
    NOTREACHED();
    return;
  }

  engine()->DisplayAnnotations(
      dict.Get(pp::Var(kJSDisplayAnnotations)).AsBool());
}

void OutOfProcessInstance::HandleGetNamedDestinationMessage(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSGetNamedDestination)).is_string() ||
      !dict.Get(pp::Var(kJSMessageId)).is_string()) {
    NOTREACHED();
    return;
  }
  base::Optional<PDFEngine::NamedDestination> named_destination =
      engine()->GetNamedDestination(
          dict.Get(pp::Var(kJSGetNamedDestination)).AsString());
  pp::VarDictionary reply;
  reply.Set(pp::Var(kType), pp::Var(kJSGetNamedDestinationReplyType));
  reply.Set(pp::Var(kJSNamedDestinationPageNumber),
            named_destination ? static_cast<int>(named_destination->page) : -1);
  reply.Set(pp::Var(kJSMessageId), dict.Get(pp::Var(kJSMessageId)).AsString());

  // Handle named destination view.
  if (named_destination && !named_destination->view.empty()) {
    std::ostringstream view_stream;
    view_stream << named_destination->view;
    for (unsigned long i = 0; i < named_destination->num_params; ++i)
      view_stream << "," << named_destination->params[i];

    reply.Set(pp::Var(kJSNamedDestinationView), view_stream.str());
  }
  PostMessage(reply);
}

void OutOfProcessInstance::HandleGetPasswordCompleteMessage(
    const pp::VarDictionary& dict) {
  if (!password_callback_ || !dict.Get(kJSPassword).is_string()) {
    NOTREACHED();
    return;
  }

  std::move(password_callback_).Run(dict.Get(kJSPassword).AsString());
}

void OutOfProcessInstance::HandleGetSelectedTextMessage(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSMessageId)).is_string()) {
    NOTREACHED();
    return;
  }

  std::string selected_text = engine()->GetSelectedText();
  // Always return unix newlines to JS.
  base::ReplaceChars(selected_text, "\r", std::string(), &selected_text);
  pp::VarDictionary reply;
  reply.Set(pp::Var(kType), pp::Var(kJSGetSelectedTextReplyType));
  reply.Set(pp::Var(kJSSelectedText), selected_text);
  reply.Set(pp::Var(kJSMessageId), dict.Get(pp::Var(kJSMessageId)).AsString());
  PostMessage(reply);
}

void OutOfProcessInstance::HandleGetThumbnailMessage(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSGetThumbnailPage)).is_number() ||
      !dict.Get(pp::Var(kJSMessageId)).is_string()) {
    NOTREACHED();
    return;
  }

  const int page_index = dict.Get(pp::Var(kJSGetThumbnailPage)).AsInt();
  engine()->RequestThumbnail(
      page_index, device_scale_,
      base::BindOnce(&OutOfProcessInstance::SendThumbnail,
                     weak_factory_.GetWeakPtr(),
                     dict.Get(pp::Var(kJSMessageId)).AsString()));
}

void OutOfProcessInstance::HandleLoadPreviewPageMessage(
    const pp::VarDictionary& dict) {
  if (!(dict.Get(pp::Var(kJSPreviewPageUrl)).is_string() &&
        dict.Get(pp::Var(kJSPreviewPageIndex)).is_int())) {
    NOTREACHED();
    return;
  }

  std::string url = dict.Get(pp::Var(kJSPreviewPageUrl)).AsString();
  // For security reasons we crash if the URL that is trying to be loaded here
  // isn't a print preview one.
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));
  ProcessPreviewPageInfo(url, dict.Get(pp::Var(kJSPreviewPageIndex)).AsInt());
}

void OutOfProcessInstance::HandleResetPrintPreviewModeMessage(
    const pp::VarDictionary& dict) {
  if (!(dict.Get(pp::Var(kJSPrintPreviewUrl)).is_string() &&
        dict.Get(pp::Var(kJSPrintPreviewGrayscale)).is_bool() &&
        dict.Get(pp::Var(kJSPrintPreviewPageCount)).is_int())) {
    NOTREACHED();
    return;
  }

  // For security reasons, crash if the URL that is trying to be loaded here
  // isn't a print preview one.
  std::string url = dict.Get(pp::Var(kJSPrintPreviewUrl)).AsString();
  CHECK(IsPrintPreview());
  CHECK(IsPrintPreviewUrl(url));

  int print_preview_page_count =
      dict.Get(pp::Var(kJSPrintPreviewPageCount)).AsInt();
  if (print_preview_page_count < 0) {
    NOTREACHED();
    return;
  }

  // The page count is zero if the print preview source is a PDF. In which
  // case, the page index for |url| should be at |kCompletePDFIndex|.
  // When the page count is not zero, then the source is not PDF. In which
  // case, the page index for |url| should be non-negative.
  bool is_previewing_pdf = IsPreviewingPDF(print_preview_page_count);
  int page_index = ExtractPrintPreviewPageIndex(url);
  if ((is_previewing_pdf && page_index != kCompletePDFIndex) ||
      (!is_previewing_pdf && page_index < 0)) {
    NOTREACHED();
    return;
  }

  print_preview_page_count_ = print_preview_page_count;
  print_preview_loaded_page_count_ = 0;
  url_ = url;
  preview_pages_info_ = base::queue<PreviewPageInfo>();
  preview_document_load_state_ = LOAD_STATE_COMPLETE;
  document_load_state_ = LOAD_STATE_LOADING;
  LoadUrl(url_, /*is_print_preview=*/false);
  preview_engine_.reset();
  InitializeEngine(PDFiumFormFiller::ScriptOption::kNoJavaScript);
  engine()->SetGrayscale(dict.Get(pp::Var(kJSPrintPreviewGrayscale)).AsBool());
  engine()->New(url_.c_str(), /*headers=*/nullptr);

  paint_manager_.InvalidateRect(gfx::Rect(SizeFromPPSize(plugin_size_)));
}

void OutOfProcessInstance::HandleSaveAttachmentMessage(
    const pp::VarDictionary& dict) {
  if (!dict.Get(pp::Var(kJSMessageId)).is_string() ||
      !dict.Get(pp::Var(kJSAttachmentIndex)).is_int() ||
      dict.Get(pp::Var(kJSAttachmentIndex)).AsInt() < 0) {
    NOTREACHED();
    return;
  }

  int index = dict.Get(pp::Var(kJSAttachmentIndex)).AsInt();
  const std::vector<DocumentAttachmentInfo>& list =
      engine()->GetDocumentAttachmentInfoList();
  if (static_cast<size_t>(index) >= list.size() || !list[index].is_readable ||
      !IsSaveDataSizeValid(list[index].size_bytes)) {
    NOTREACHED();
    return;
  }

  pp::VarDictionary message;
  message.Set(kType, kJSSaveAttachmentDataType);
  message.Set(kJSMessageId, dict.Get(pp::Var(kJSMessageId)));
  // This will be overwritten if the save is successful.
  message.Set(kJSAttachmentDataToSave, pp::Var(pp::Var::Null()));

  std::vector<uint8_t> data = engine()->GetAttachmentData(index);
  if (data.size() != list[index].size_bytes) {
    NOTREACHED();
    return;
  }

  if (IsSaveDataSizeValid(data.size())) {
    pp::VarArrayBuffer buffer(data.size());
    std::copy(data.begin(), data.end(), reinterpret_cast<char*>(buffer.Map()));
    message.Set(kJSAttachmentDataToSave, buffer);
  }
  PostMessage(message);
}

void OutOfProcessInstance::HandleSaveMessage(const pp::VarDictionary& dict) {
  if (!(dict.Get(pp::Var(kJSToken)).is_string() &&
        dict.Get(pp::Var(kJSSaveRequestType)).is_int())) {
    NOTREACHED();
    return;
  }
  const SaveRequestType request_type = static_cast<SaveRequestType>(
      dict.Get(pp::Var(kJSSaveRequestType)).AsInt());
  switch (request_type) {
    case SaveRequestType::kAnnotation:
      // In annotation mode, assume the user will make edits and prefer saving
      // using the plugin data.
      pp::PDF::SetPluginCanSave(this, true);
      SaveToBuffer(dict.Get(pp::Var(kJSToken)).AsString());
      break;
    case SaveRequestType::kOriginal:
      pp::PDF::SetPluginCanSave(this, false);
      SaveToFile(dict.Get(pp::Var(kJSToken)).AsString());
      pp::PDF::SetPluginCanSave(this, CanSaveEdits());
      break;
    case SaveRequestType::kEdited:
      SaveToBuffer(dict.Get(pp::Var(kJSToken)).AsString());
      break;
  }
}

void OutOfProcessInstance::HandleSetTwoUpViewMessage(
    const pp::VarDictionary& dict) {
  if (!base::FeatureList::IsEnabled(features::kPDFViewerUpdate) ||
      !dict.Get(pp::Var(kJSEnableTwoUpView)).is_bool()) {
    NOTREACHED();
    return;
  }

  engine()->SetTwoUpView(dict.Get(pp::Var(kJSEnableTwoUpView)).AsBool());
}

void OutOfProcessInstance::HandleUpdateScrollMessage(
    const pp::VarDictionary& dict) {
  if (!base::FeatureList::IsEnabled(features::kPDFViewerUpdate) ||
      !dict.Get(pp::Var(kJSUpdateScrollX)).is_number() ||
      !dict.Get(pp::Var(kJSUpdateScrollY)).is_number()) {
    NOTREACHED();
    return;
  }

  if (stop_scrolling_) {
    return;
  }

  int x = dict.Get(pp::Var(kJSUpdateScrollX)).AsInt();
  int y = dict.Get(pp::Var(kJSUpdateScrollY)).AsInt();
  scroll_offset_ = pp::Point(x, y);
  UpdateScroll();
}

void OutOfProcessInstance::HandleViewportMessage(
    const pp::VarDictionary& dict) {
  pp::Var layout_options_var = dict.Get(kJSLayoutOptions);
  if (!layout_options_var.is_undefined()) {
    DocumentLayout::Options layout_options;
    layout_options.FromValue(ValueFromVar(layout_options_var));
    // TODO(crbug.com/1013800): Eliminate need to get document size from here.
    document_size_ =
        PPSizeFromSize(engine()->ApplyDocumentLayout(layout_options));
    OnGeometryChanged(zoom_, device_scale_);
  }

  if (!(dict.Get(pp::Var(kJSXOffset)).is_number() &&
        dict.Get(pp::Var(kJSYOffset)).is_number() &&
        dict.Get(pp::Var(kJSZoom)).is_number() &&
        dict.Get(pp::Var(kJSPinchPhase)).is_number())) {
    NOTREACHED();
    return;
  }
  received_viewport_message_ = true;
  stop_scrolling_ = false;
  PinchPhase pinch_phase =
      static_cast<PinchPhase>(dict.Get(pp::Var(kJSPinchPhase)).AsInt());
  double zoom = dict.Get(pp::Var(kJSZoom)).AsDouble();
  double zoom_ratio = zoom / zoom_;

  pp::FloatPoint scroll_offset(dict.Get(pp::Var(kJSXOffset)).AsDouble(),
                               dict.Get(pp::Var(kJSYOffset)).AsDouble());

  if (pinch_phase == PINCH_START) {
    scroll_offset_at_last_raster_ = scroll_offset;
    last_bitmap_smaller_ = false;
    needs_reraster_ = false;
    return;
  }

  // When zooming in, we set a layer transform to avoid unneeded rerasters.
  // Also, if we're zooming out and the last time we rerastered was when
  // we were even further zoomed out (i.e. we pinch zoomed in and are now
  // pinch zooming back out in the same gesture), we update the layer
  // transform instead of rerastering.
  if (pinch_phase == PINCH_UPDATE_ZOOM_IN ||
      (pinch_phase == PINCH_UPDATE_ZOOM_OUT && zoom_ratio > 1.0)) {
    if (!(dict.Get(pp::Var(kJSPinchX)).is_number() &&
          dict.Get(pp::Var(kJSPinchY)).is_number() &&
          dict.Get(pp::Var(kJSPinchVectorX)).is_number() &&
          dict.Get(pp::Var(kJSPinchVectorY)).is_number())) {
      NOTREACHED();
      return;
    }

    pp::Point pinch_center(dict.Get(pp::Var(kJSPinchX)).AsDouble(),
                           dict.Get(pp::Var(kJSPinchY)).AsDouble());
    // Pinch vector is the panning caused due to change in pinch
    // center between start and end of the gesture.
    gfx::Vector2d pinch_vector =
        gfx::Vector2d(dict.Get(kJSPinchVectorX).AsDouble() * zoom_ratio,
                      dict.Get(kJSPinchVectorY).AsDouble() * zoom_ratio);
    gfx::Vector2d scroll_delta;
    // If the rendered document doesn't fill the display area we will
    // use |paint_offset| to anchor the paint vertically into the same place.
    // We use the scroll bars instead of the pinch vector to get the actual
    // position on screen of the paint.
    gfx::Vector2d paint_offset;

    if (plugin_size_.width() > GetDocumentPixelWidth() * zoom_ratio) {
      // We want to keep the paint in the middle but it must stay in the same
      // position relative to the scroll bars.
      paint_offset = gfx::Vector2d(0, (1 - zoom_ratio) * pinch_center.y());
      scroll_delta = gfx::Vector2d(
          0,
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));

      pinch_vector = gfx::Vector2d();
      last_bitmap_smaller_ = true;
    } else if (last_bitmap_smaller_) {
      pinch_center = pp::Point((plugin_size_.width() / device_scale_) / 2,
                               (plugin_size_.height() / device_scale_) / 2);
      const double zoom_when_doc_covers_plugin_width =
          zoom_ * plugin_size_.width() / GetDocumentPixelWidth();
      paint_offset = gfx::Vector2d(
          (1 - zoom / zoom_when_doc_covers_plugin_width) * pinch_center.x(),
          (1 - zoom_ratio) * pinch_center.y());
      pinch_vector = gfx::Vector2d();
      scroll_delta = gfx::Vector2d(
          (scroll_offset.x() - scroll_offset_at_last_raster_.x() * zoom_ratio),
          (scroll_offset.y() - scroll_offset_at_last_raster_.y() * zoom_ratio));
    }

    paint_manager_.SetTransform(zoom_ratio, PointFromPPPoint(pinch_center),
                                pinch_vector + paint_offset + scroll_delta,
                                true);
    needs_reraster_ = false;
    return;
  }

  if (pinch_phase == PINCH_UPDATE_ZOOM_OUT || pinch_phase == PINCH_END) {
    // We reraster on pinch zoom out in order to solve the invalid regions
    // that appear after zooming out.
    // On pinch end the scale is again 1.f and we request a reraster
    // in the new position.
    paint_manager_.ClearTransform();
    last_bitmap_smaller_ = false;
    needs_reraster_ = true;

    // If we're rerastering due to zooming out, we need to update
    // |scroll_offset_at_last_raster_|, in case the user continues the
    // gesture by zooming in.
    scroll_offset_at_last_raster_ = scroll_offset;
  }

  // Bound the input parameters.
  zoom = std::max(kMinZoom, zoom);
  DCHECK(dict.Get(pp::Var(kJSUserInitiated)).is_bool());

  SetZoom(zoom);
  scroll_offset = BoundScrollOffsetToDocument(scroll_offset);
  engine()->ScrolledToXPosition(scroll_offset.x() * device_scale_);
  engine()->ScrolledToYPosition(scroll_offset.y() * device_scale_);
}

void OutOfProcessInstance::PreviewDocumentLoadComplete() {
  if (preview_document_load_state_ != LOAD_STATE_LOADING ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = LOAD_STATE_COMPLETE;

  int dest_page_index = preview_pages_info_.front().second;
  DCHECK_GT(dest_page_index, 0);
  preview_pages_info_.pop();
  DCHECK(preview_engine_);
  engine()->AppendPage(preview_engine_.get(), dest_page_index);

  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

void OutOfProcessInstance::DocumentLoadFailed() {
  DCHECK_EQ(LOAD_STATE_LOADING, document_load_state_);
  UserMetricsRecordAction("PDF.LoadFailure");

  if (did_call_start_loading_) {
    pp::PDF::DidStopLoading(this);
    did_call_start_loading_ = false;
  }

  document_load_state_ = LOAD_STATE_FAILED;
  paint_manager_.InvalidateRect(gfx::Rect(SizeFromPPSize(plugin_size_)));

  // Send a progress value of -1 to indicate a failure.
  SendLoadingProgress(-1);
}

void OutOfProcessInstance::PreviewDocumentLoadFailed() {
  UserMetricsRecordAction("PDF.PreviewDocumentLoadFailure");
  if (preview_document_load_state_ != LOAD_STATE_LOADING ||
      preview_pages_info_.empty()) {
    return;
  }

  // Even if a print preview page failed to load, keep going.
  preview_document_load_state_ = LOAD_STATE_FAILED;
  preview_pages_info_.pop();
  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

pp::Instance* OutOfProcessInstance::GetPluginInstance() {
  return this;
}

void OutOfProcessInstance::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  DCHECK(!feature.empty());
  std::string metric("PDF_Unsupported_");
  metric += feature;
  if (!unsupported_features_reported_.count(metric)) {
    unsupported_features_reported_.insert(metric);
    UserMetricsRecordAction(metric);
  }

  // Since we use an info bar, only do this for full frame plugins..
  if (!full_)
    return;

  if (told_browser_about_unsupported_feature_)
    return;
  told_browser_about_unsupported_feature_ = true;

  pp::PDF::HasUnsupportedFeature(this);
}

void OutOfProcessInstance::DocumentLoadProgress(uint32_t available,
                                                uint32_t doc_size) {
  double progress = 0.0;
  if (doc_size) {
    progress = 100.0 * static_cast<double>(available) / doc_size;
  } else {
    // Document size is unknown. Use heuristics.
    // We'll make progress logarithmic from 0 to 100M.
    static const double kFactor = log(100000000.0) / 100.0;
    if (available > 0)
      progress = std::min(log(static_cast<double>(available)) / kFactor, 100.0);
  }

  // We send 100% load progress in DocumentLoadComplete.
  if (progress >= 100)
    return;

  // Avoid sending too many progress messages over PostMessage.
  if (progress > last_progress_sent_ + 1) {
    last_progress_sent_ = progress;
    SendLoadingProgress(progress);
  }
}

void OutOfProcessInstance::FormTextFieldFocusChange(bool in_focus) {
  if (!text_input_)
    return;

  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSFieldFocusType));
  message.Set(pp::Var(kJSFieldFocus), pp::Var(in_focus));
  PostMessage(message);

  text_input_->SetTextInputType(in_focus ? PP_TEXTINPUT_TYPE_DEV_TEXT
                                         : PP_TEXTINPUT_TYPE_DEV_NONE);
}

void OutOfProcessInstance::ResetRecentlySentFindUpdate(int32_t /* unused */) {
  recently_sent_find_update_ = false;
}

void OutOfProcessInstance::OnGeometryChanged(double old_zoom,
                                             float old_device_scale) {
  if (zoom_ != old_zoom || device_scale_ != old_device_scale)
    engine()->ZoomUpdated(zoom_ * device_scale_);

  available_area_ = pp::Rect(plugin_size_);
  int doc_width = GetDocumentPixelWidth();
  if (doc_width < available_area_.width()) {
    available_area_.Offset((available_area_.width() - doc_width) / 2, 0);
    available_area_.set_width(doc_width);
  }
  int bottom_of_document =
      GetDocumentPixelHeight() +
      (top_toolbar_height_in_viewport_coords_ * device_scale_);
  if (bottom_of_document < available_area_.height())
    available_area_.set_height(bottom_of_document);

  CalculateBackgroundParts();
  engine()->PageOffsetUpdated(VectorFromPPPoint(available_area_.point()));
  engine()->PluginSizeUpdated(SizeFromPPSize(available_area_.size()));

  if (document_size_.IsEmpty())
    return;
  paint_manager_.InvalidateRect(gfx::Rect(SizeFromPPSize(plugin_size_)));

  if (accessibility_state_ == ACCESSIBILITY_STATE_LOADED)
    SendAccessibilityViewportInfo();
}

base::WeakPtr<PdfViewPluginBase> OutOfProcessInstance::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<UrlLoader> OutOfProcessInstance::CreateUrlLoaderInternal() {
  auto loader = std::make_unique<PepperUrlLoader>(this);
  loader->GrantUniversalAccess();
  return loader;
}

void OutOfProcessInstance::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;
  OnGeometryChanged(old_zoom, device_scale_);
}

void OutOfProcessInstance::AppendBlankPrintPreviewPages() {
  engine()->AppendBlankPages(print_preview_page_count_);
  LoadNextPreviewPage();
}

bool OutOfProcessInstance::IsPrintPreview() {
  return is_print_preview_;
}

uint32_t OutOfProcessInstance::GetBackgroundColor() {
  return background_color_;
}

void OutOfProcessInstance::IsSelectingChanged(bool is_selecting) {
  pp::VarDictionary message;
  message.Set(kType, kJSSetIsSelectingType);
  message.Set(kJSIsSelecting, pp::Var(is_selecting));
  PostMessage(message);
}

void OutOfProcessInstance::EnteredEditMode() {
  edit_mode_ = true;
  pp::PDF::SetPluginCanSave(this, CanSaveEdits());
  if (CanSaveEdits()) {
    pp::VarDictionary message;
    message.Set(kType, kJSSetIsEditingType);
    PostMessage(message);
  }
}

float OutOfProcessInstance::GetToolbarHeightInScreenCoords() {
  return top_toolbar_height_in_viewport_coords_ * device_scale_;
}

void OutOfProcessInstance::DocumentFocusChanged(bool document_has_focus) {
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSDocumentFocusChangedType));
  message.Set(pp::Var(kJSDocumentHasFocus), pp::Var(document_has_focus));
  PostMessage(message);
}

void OutOfProcessInstance::ProcessPreviewPageInfo(const std::string& url,
                                                  int dest_page_index) {
  DCHECK(IsPrintPreview());

  if (dest_page_index < 0 || dest_page_index >= print_preview_page_count_) {
    NOTREACHED();
    return;
  }

  // Print Preview JS will send the loadPreviewPage message for every page,
  // including the first page in the print preview, which has already been
  // loaded when handing the resetPrintPreviewMode message. Just ignore it.
  if (dest_page_index == 0)
    return;

  int src_page_index = ExtractPrintPreviewPageIndex(url);
  if (src_page_index < 0) {
    NOTREACHED();
    return;
  }

  preview_pages_info_.push(std::make_pair(url, dest_page_index));
  LoadAvailablePreviewPage();
}

void OutOfProcessInstance::LoadAvailablePreviewPage() {
  if (preview_pages_info_.empty() ||
      document_load_state_ != LOAD_STATE_COMPLETE ||
      preview_document_load_state_ == LOAD_STATE_LOADING) {
    return;
  }

  preview_document_load_state_ = LOAD_STATE_LOADING;
  const std::string& url = preview_pages_info_.front().first;
  LoadUrl(url, /*is_print_preview=*/true);
}

void OutOfProcessInstance::LoadNextPreviewPage() {
  if (!preview_pages_info_.empty()) {
    DCHECK_LT(print_preview_loaded_page_count_, print_preview_page_count_);
    LoadAvailablePreviewPage();
    return;
  }

  if (print_preview_loaded_page_count_ == print_preview_page_count_) {
    SendPrintPreviewLoadedNotification();
  }
}

void OutOfProcessInstance::SendPrintPreviewLoadedNotification() {
  pp::VarDictionary loaded_message;
  loaded_message.Set(pp::Var(kType), pp::Var(kJSPreviewLoadedType));
  PostMessage(loaded_message);
}

void OutOfProcessInstance::SendDocumentMetadata() {
  pp::VarDictionary metadata_message;
  metadata_message.Set(pp::Var(kType), pp::Var(kJSMetadataType));

  const std::string& title = engine()->GetDocumentMetadata().title;
  if (!base::TrimWhitespace(base::UTF8ToUTF16(title), base::TRIM_ALL).empty())
    metadata_message.Set(pp::Var(kJSTitle), pp::Var(title));

  metadata_message.Set(pp::Var(kJSAttachments), GetDocumentAttachments());

  pp::VarArray bookmarks = engine()->GetBookmarks();
  metadata_message.Set(pp::Var(kJSBookmarks), bookmarks);

  metadata_message.Set(
      pp::Var(kJSCanSerializeDocument),
      pp::Var(IsSaveDataSizeValid(engine()->GetLoadedByteSize())));

  PostMessage(metadata_message);
}

void OutOfProcessInstance::SendLoadingProgress(double percentage) {
  DCHECK(percentage == -1 || (percentage >= 0 && percentage <= 100));
  pp::VarDictionary progress_message;
  progress_message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
  progress_message.Set(pp::Var(kJSProgressPercentage), pp::Var(percentage));
  PostMessage(progress_message);
}

void OutOfProcessInstance::SendThumbnail(const std::string& message_id,
                                         Thumbnail thumbnail) {
  pp::VarDictionary reply;
  reply.Set(pp::Var(kType), pp::Var(kJSGetThumbnailReplyType));
  reply.Set(pp::Var(kJSMessageId), message_id);

  const SkBitmap& bitmap = thumbnail.bitmap();
  const size_t buffer_size = bitmap.computeByteSize();
  pp::VarArrayBuffer buffer(buffer_size);
  memcpy(buffer.Map(), bitmap.getPixels(), buffer_size);
  reply.Set(pp::Var(kJSGetThumbnailImageData), buffer);
  buffer.Unmap();

  reply.Set(pp::Var(kJSGetThumbnailWidth), bitmap.width());
  reply.Set(pp::Var(kJSGetThumbnailHeight), bitmap.height());

  PostMessage(reply);
}

void OutOfProcessInstance::UserMetricsRecordAction(const std::string& action) {
  // TODO(raymes): Move this function to PPB_UMA_Private.
  pp::PDF::UserMetricsRecordAction(this, pp::Var(action));
}

pp::FloatPoint OutOfProcessInstance::BoundScrollOffsetToDocument(
    const pp::FloatPoint& scroll_offset) {
  float max_x = std::max(
      document_size_.width() * float{zoom_} - plugin_dip_size_.width(), 0.0f);
  float x = base::ClampToRange(scroll_offset.x(), 0.0f, max_x);
  float min_y = -top_toolbar_height_in_viewport_coords_;
  float max_y = std::max(
      document_size_.height() * float{zoom_} - plugin_dip_size_.height(),
      min_y);
  float y = base::ClampToRange(scroll_offset.y(), min_y, max_y);
  return pp::FloatPoint(x, y);
}

bool OutOfProcessInstance::SendInputEventToEngine(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      return engine()->HandleEvent(
          GetMouseInputEvent(pp::MouseInputEvent(event)));
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
    case PP_INPUTEVENT_TYPE_CHAR:
      return engine()->HandleEvent(
          GetKeyboardInputEvent(pp::KeyboardInputEvent(event)));
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
    case PP_INPUTEVENT_TYPE_TOUCHEND:
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      return engine()->HandleEvent(
          GetTouchInputEvent(pp::TouchInputEvent(event)));
    case PP_INPUTEVENT_TYPE_WHEEL:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      // These event types are not used in PDFiumEngine, so there are no
      // functions to convert them from pp::InputEvent to
      // chrome_pdf::InputEvent. As such just send a dummy NoneInputEvent
      // instead.
      return engine()->HandleEvent(NoneInputEvent());
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      return false;
  }
}

template <typename T>
void OutOfProcessInstance::HistogramEnumeration(const char* name, T sample) {
  if (IsPrintPreview())
    return;
  base::UmaHistogramEnumeration(name, sample);
}

template <typename T>
void OutOfProcessInstance::HistogramEnumeration(const char* name,
                                                T sample,
                                                T enum_size) {
  if (IsPrintPreview())
    return;
  base::UmaHistogramEnumeration(name, sample, enum_size);
}

void OutOfProcessInstance::HistogramCustomCounts(const char* name,
                                                 int32_t sample,
                                                 int32_t min,
                                                 int32_t max,
                                                 uint32_t bucket_count) {
  if (IsPrintPreview())
    return;
  base::UmaHistogramCustomCounts(name, sample, min, max, bucket_count);
}

void OutOfProcessInstance::OnPrint(int32_t /*unused_but_required*/) {
  pp::PDF::Print(this);
}

void OutOfProcessInstance::InvalidateAfterPaintDone(
    int32_t /*unused_but_required*/) {
  DCHECK(!in_paint_);
  for (const gfx::Rect& rect : deferred_invalidates_)
    Invalidate(rect);
  deferred_invalidates_.clear();
}

void OutOfProcessInstance::PrintSettings::Clear() {
  is_printing = false;
  print_pages_called = false;
  memset(&pepper_print_settings, 0, sizeof(pepper_print_settings));
  memset(&pdf_print_settings, 0, sizeof(pdf_print_settings));
}

}  // namespace chrome_pdf
