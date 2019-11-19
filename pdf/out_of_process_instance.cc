// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>  // for min/max()
#include <cmath>      // for log() and pow()
#include <list>
#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/content_restriction.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "pdf/accessibility.h"
#include "pdf/document_layout.h"
#include "pdf/pdf.h"
#include "pdf/pdf_features.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/dev/text_input_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace chrome_pdf {

namespace {

constexpr char kChromePrint[] = "chrome://print/";
constexpr char kChromeExtension[] =
    "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai";

// Constants used in handling postMessage() messages.
constexpr char kType[] = "type";
// Beep messge arguments. (Plugin -> Page).
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
// Save (Page -> Plugin)
constexpr char kJSSaveType[] = "save";
constexpr char kJSToken[] = "token";
constexpr char kJSForce[] = "force";
// Save Data (Plugin -> Page)
constexpr char kJSSaveDataType[] = "saveData";
constexpr char kJSFileName[] = "fileName";
constexpr char kJSDataToSave[] = "dataToSave";
constexpr char kJSHasUnsavedChanges[] = "hasUnsavedChanges";
// Consume save token (Plugin -> Page)
constexpr char kJSConsumeSaveTokenType[] = "consumeSaveToken";
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

// Selecting text in document (Plugin -> Page)
constexpr char kJSSetIsSelectingType[] = "setIsSelecting";
constexpr char kJSIsSelecting[] = "isSelecting";

// Notify when a form field is focused (Plugin -> Page)
constexpr char kJSFieldFocusType[] = "formFocusChange";
constexpr char kJSFieldFocus[] = "focused";

constexpr int kFindResultCooldownMs = 100;

// Do not save forms with over 100 MB. This cap should be kept in sync with and
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

// Used for UMA. Do not delete entries, and keep in sync with histograms.xml.
enum PDFFeatures {
  LOADED_DOCUMENT = 0,
  HAS_TITLE = 1,
  HAS_BOOKMARKS = 2,
  FEATURES_COUNT
};

// Used for UMA. Do not delete entries, and keep in sync with histograms.xml
// and third_party/pdfium/public/fpdf_annot.h.
constexpr int kAnnotationTypesCount = 28;

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
  return url.starts_with(kChromePrint);
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

void ScaleRect(float scale, pp::Rect* rect) {
  int left = static_cast<int>(floorf(rect->x() * scale));
  int top = static_cast<int>(floorf(rect->y() * scale));
  int right = static_cast<int>(ceilf((rect->x() + rect->width()) * scale));
  int bottom = static_cast<int>(ceilf((rect->y() + rect->height()) * scale));
  rect->SetRect(left, top, right - left, bottom - top);
}

bool IsSaveDataSizeValid(size_t size) {
  return size > 0 && size <= kMaximumSavedFileSize;
}

}  // namespace

OutOfProcessInstance::OutOfProcessInstance(PP_Instance instance)
    : pp::Instance(instance),
      pp::Find_Private(this),
      pp::Printing_Dev(this),
      cursor_(PP_CURSORTYPE_POINTER),
      zoom_(1.0),
      needs_reraster_(true),
      last_bitmap_smaller_(false),
      device_scale_(1.0),
      full_(false),
      paint_manager_(this, this, true),
      first_paint_(true),
      document_load_state_(LOAD_STATE_LOADING),
      preview_document_load_state_(LOAD_STATE_COMPLETE),
      uma_(this),
      told_browser_about_unsupported_feature_(false),
      print_preview_page_count_(-1),
      print_preview_loaded_page_count_(-1),
      last_progress_sent_(0),
      recently_sent_find_update_(false),
      received_viewport_message_(false),
      did_call_start_loading_(false),
      stop_scrolling_(false),
      background_color_(0),
      top_toolbar_height_in_viewport_coords_(0),
      accessibility_state_(ACCESSIBILITY_STATE_OFF),
      is_print_preview_(false) {
  callback_factory_.Initialize(this);
  pp::Module::Get()->AddPluginInterface(kPPPPdfInterface, &ppp_private);
  AddPerInstanceObject(kPPPPdfInterface, this);

  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_TOUCH);
}

OutOfProcessInstance::~OutOfProcessInstance() {
  RemovePerInstanceObject(kPPPPdfInterface, this);
  // Explicitly reset the PDFEngine during destruction as it may call back into
  // this object.
  engine_.reset();
}

bool OutOfProcessInstance::Init(uint32_t argc,
                                const char* argn[],
                                const char* argv[]) {
  // Check if the PDF is being loaded in the PDF chrome extension. We only allow
  // the plugin to be loaded in the extension and print preview to avoid
  // exposing sensitive APIs directly to external websites.
  pp::Var document_url_var = pp::URLUtil_Dev::Get()->GetDocumentURL(this);
  if (!document_url_var.is_string())
    return false;

  std::string document_url = document_url_var.AsString();
  base::StringPiece document_url_piece(document_url);
  is_print_preview_ = IsPrintPreviewUrl(document_url_piece);
  if (!document_url_piece.starts_with(kChromeExtension) && !is_print_preview_)
    return false;

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

  bool enable_javascript = false;
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
      enable_javascript = (strcmp(argv[i], "allow") == 0);
    }
    if (!success)
      return false;
  }

  if (!original_url)
    return false;

  if (!stream_url)
    stream_url = original_url;

  if (!engine_) {
    // TODO(tsepez): fix lifetime issue, conditionalize javascript.
    engine_ = PDFEngine::Create(this, true);
  }

  // If we're in print preview mode we don't need to load the document yet.
  // A |kJSResetPrintPreviewModeType| message will be sent to the plugin letting
  // it know the url to load. By not loading here we avoid loading the same
  // document twice.
  if (IsPrintPreview())
    return true;

  LoadUrl(stream_url, /*is_print_preview=*/false);
  url_ = original_url;
  pp::PDF::SetCrashData(GetPluginInstance(), original_url, top_level_url);
  return engine_->New(original_url, headers);
}

void OutOfProcessInstance::HandleMessage(const pp::Var& message) {
  pp::VarDictionary dict(message);
  if (!dict.Get(kType).is_string()) {
    NOTREACHED();
    return;
  }

  std::string type = dict.Get(kType).AsString();

  if (type == kJSViewportType) {
    pp::Var layout_options_var = dict.Get(kJSLayoutOptions);
    if (!layout_options_var.is_undefined()) {
      DocumentLayout::Options layout_options;
      layout_options.FromVar(layout_options_var);
      // TODO(crbug.com/1013800): Eliminate need to get document size from here.
      document_size_ = engine_->ApplyDocumentLayout(layout_options);
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
      pp::Point pinch_vector =
          pp::Point(dict.Get(kJSPinchVectorX).AsDouble() * zoom_ratio,
                    dict.Get(kJSPinchVectorY).AsDouble() * zoom_ratio);
      pp::Point scroll_delta;
      // If the rendered document doesn't fill the display area we will
      // use |paint_offset| to anchor the paint vertically into the same place.
      // We use the scroll bars instead of the pinch vector to get the actual
      // position on screen of the paint.
      pp::Point paint_offset;

      if (plugin_size_.width() > GetDocumentPixelWidth() * zoom_ratio) {
        // We want to keep the paint in the middle but it must stay in the same
        // position relative to the scroll bars.
        paint_offset = pp::Point(0, (1 - zoom_ratio) * pinch_center.y());
        scroll_delta =
            pp::Point(0, (scroll_offset.y() -
                          scroll_offset_at_last_raster_.y() * zoom_ratio));

        pinch_vector = pp::Point();
        last_bitmap_smaller_ = true;
      } else if (last_bitmap_smaller_) {
        pinch_center = pp::Point((plugin_size_.width() / device_scale_) / 2,
                                 (plugin_size_.height() / device_scale_) / 2);
        const double zoom_when_doc_covers_plugin_width =
            zoom_ * plugin_size_.width() / GetDocumentPixelWidth();
        paint_offset = pp::Point(
            (1 - zoom / zoom_when_doc_covers_plugin_width) * pinch_center.x(),
            (1 - zoom_ratio) * pinch_center.y());
        pinch_vector = pp::Point();
        scroll_delta =
            pp::Point((scroll_offset.x() -
                       scroll_offset_at_last_raster_.x() * zoom_ratio),
                      (scroll_offset.y() -
                       scroll_offset_at_last_raster_.y() * zoom_ratio));
      }

      paint_manager_.SetTransform(zoom_ratio, pinch_center,
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
    engine_->ScrolledToXPosition(scroll_offset.x() * device_scale_);
    engine_->ScrolledToYPosition(scroll_offset.y() * device_scale_);
  } else if (type == kJSGetPasswordCompleteType) {
    if (!dict.Get(pp::Var(kJSPassword)).is_string()) {
      NOTREACHED();
      return;
    }
    if (password_callback_) {
      pp::CompletionCallbackWithOutput<pp::Var> callback = *password_callback_;
      password_callback_.reset();
      *callback.output() = dict.Get(pp::Var(kJSPassword)).pp_var();
      callback.Run(PP_OK);
    } else {
      NOTREACHED();
    }
  } else if (type == kJSPrintType) {
    Print();
  } else if (type == kJSSaveType) {
    if (!(dict.Get(pp::Var(kJSToken)).is_string() &&
          dict.Get(pp::Var(kJSForce)).is_bool())) {
      NOTREACHED();
      return;
    }
    const bool force = dict.Get(pp::Var(kJSForce)).AsBool();
    if (force) {
      // |force| being true means the user has entered annotation mode. In which
      // case, assume the user will make edits and prefer saving using the
      // plugin data.
      pp::PDF::SetPluginCanSave(this, true);
      SaveToBuffer(dict.Get(pp::Var(kJSToken)).AsString());
    } else {
      SaveToFile(dict.Get(pp::Var(kJSToken)).AsString());
    }
  } else if (type == kJSRotateClockwiseType) {
    RotateClockwise();
  } else if (type == kJSRotateCounterclockwiseType) {
    RotateCounterclockwise();
  } else if (type == kJSSelectAllType) {
    engine_->SelectAll();
  } else if (type == kJSBackgroundColorChangedType) {
    if (!dict.Get(pp::Var(kJSBackgroundColor)).is_string()) {
      NOTREACHED();
      return;
    }
    base::HexStringToUInt(dict.Get(pp::Var(kJSBackgroundColor)).AsString(),
                          &background_color_);
  } else if (type == kJSResetPrintPreviewModeType) {
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
    if (is_previewing_pdf) {
      if (page_index != kCompletePDFIndex) {
        NOTREACHED();
        return;
      }
    } else {
      if (page_index < 0) {
        NOTREACHED();
        return;
      }
    }

    print_preview_page_count_ = print_preview_page_count;
    print_preview_loaded_page_count_ = 0;
    url_ = url;
    preview_pages_info_ = base::queue<PreviewPageInfo>();
    preview_document_load_state_ = LOAD_STATE_COMPLETE;
    document_load_state_ = LOAD_STATE_LOADING;
    LoadUrl(url_, /*is_print_preview=*/false);
    preview_engine_.reset();
    engine_ = PDFEngine::Create(this, false);
    engine_->SetGrayscale(dict.Get(pp::Var(kJSPrintPreviewGrayscale)).AsBool());
    engine_->New(url_.c_str(), nullptr /* empty header */);

    paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));
  } else if (type == kJSLoadPreviewPageType) {
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
  } else if (type == kJSStopScrollingType) {
    stop_scrolling_ = true;
  } else if (type == kJSGetSelectedTextType) {
    std::string selected_text = engine_->GetSelectedText();
    // Always return unix newlines to JS.
    base::ReplaceChars(selected_text, "\r", std::string(), &selected_text);
    pp::VarDictionary reply;
    reply.Set(pp::Var(kType), pp::Var(kJSGetSelectedTextReplyType));
    reply.Set(pp::Var(kJSSelectedText), selected_text);
    PostMessage(reply);
  } else if (type == kJSGetNamedDestinationType) {
    if (!dict.Get(pp::Var(kJSGetNamedDestination)).is_string()) {
      NOTREACHED();
      return;
    }
    base::Optional<PDFEngine::NamedDestination> named_destination =
        engine_->GetNamedDestination(
            dict.Get(pp::Var(kJSGetNamedDestination)).AsString());
    pp::VarDictionary reply;
    reply.Set(pp::Var(kType), pp::Var(kJSGetNamedDestinationReplyType));
    reply.Set(
        pp::Var(kJSNamedDestinationPageNumber),
        named_destination ? static_cast<int>(named_destination->page) : -1);
    PostMessage(reply);
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

  if (engine_->HandleEvent(event_device_res))
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

  if (view_device_size != plugin_size_ || device_scale != device_scale_) {
    device_scale_ = device_scale;
    plugin_dip_size_ = view_rect.size();
    plugin_size_ = view_device_size;

    paint_manager_.SetSize(view_device_size, device_scale_);

    pp::Size new_image_data_size =
        PaintManager::GetNewContextSize(image_data_.size(), plugin_size_);
    if (new_image_data_size != image_data_.size()) {
      image_data_ = pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                                  new_image_data_size, false);
      first_paint_ = true;
    }

    if (image_data_.is_null()) {
      DCHECK(plugin_size_.IsEmpty());
      return;
    }

    OnGeometryChanged(zoom_, old_device_scale);
  }

  if (!stop_scrolling_) {
    scroll_offset_ = view.GetScrollOffset();
    // Because view messages come from the DOM, the coordinates of the viewport
    // are 0-based (i.e. they do not correspond to the viewport's coordinates in
    // JS), so we need to subtract the toolbar height to convert them into
    // viewport coordinates.
    pp::FloatPoint scroll_offset_float(
        scroll_offset_.x(),
        scroll_offset_.y() - top_toolbar_height_in_viewport_coords_);
    scroll_offset_float = BoundScrollOffsetToDocument(scroll_offset_float);
    engine_->ScrolledToXPosition(scroll_offset_float.x() * device_scale_);
    engine_->ScrolledToYPosition(scroll_offset_float.y() * device_scale_);
  }
}

void OutOfProcessInstance::DidChangeFocus(bool has_focus) {
  if (!has_focus)
    engine_->KillFormFocus();
}

void OutOfProcessInstance::GetPrintPresetOptionsFromDocument(
    PP_PdfPrintPresetOptions_Dev* options) {
  options->is_scaling_disabled = PP_FromBool(IsPrintScalingDisabled());
  options->duplex =
      static_cast<PP_PrivateDuplexMode_Dev>(engine_->GetDuplexType());
  options->copies = engine_->GetCopiesToPrint();
  pp::Size uniform_page_size;
  options->is_page_size_uniform =
      PP_FromBool(engine_->GetPageSizeAndUniformity(&uniform_page_size));
  options->uniform_page_size = uniform_page_size;
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
  doc_info.page_count = engine_->GetNumberOfPages();
  doc_info.text_accessible = PP_FromBool(
      engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE));
  doc_info.text_copyable =
      PP_FromBool(engine_->HasPermission(PDFEngine::PERMISSION_COPY));

  pp::PDF::SetAccessibilityDocInfo(GetPluginInstance(), &doc_info);

  // If the document contents isn't accessible, don't send anything more.
  if (!(engine_->HasPermission(PDFEngine::PERMISSION_COPY) ||
        engine_->HasPermission(PDFEngine::PERMISSION_COPY_ACCESSIBLE))) {
    return;
  }

  SendAccessibilityViewportInfo();

  // Schedule loading the first page.
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &OutOfProcessInstance::SendNextAccessibilityPage);
  pp::Module::Get()->core()->CallOnMainThread(kAccessibilityPageDelayMs,
                                              callback, 0);
}

void OutOfProcessInstance::SendNextAccessibilityPage(int32_t page_index) {
  PP_PrivateAccessibilityPageInfo page_info;
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> text_runs;
  std::vector<PP_PrivateAccessibilityCharInfo> chars;
  pp::PDF::PrivateAccessibilityPageObjects page_objects;

  if (!GetAccessibilityInfo(engine_.get(), page_index, &page_info, &text_runs,
                            &chars, &page_objects)) {
    return;
  }

  pp::PDF::SetAccessibilityPageInfo(GetPluginInstance(), &page_info, text_runs,
                                    chars, page_objects);

  // Schedule loading the next page.
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &OutOfProcessInstance::SendNextAccessibilityPage);
  pp::Module::Get()->core()->CallOnMainThread(kAccessibilityPageDelayMs,
                                              callback, page_index + 1);
}

void OutOfProcessInstance::SendAccessibilityViewportInfo() {
  PP_PrivateAccessibilityViewportInfo viewport_info;
  viewport_info.scroll.x = 0;
  viewport_info.scroll.y =
      -top_toolbar_height_in_viewport_coords_ * device_scale_;
  viewport_info.offset = available_area_.point();
  viewport_info.zoom_device_scale_factor = zoom_ * device_scale_;

  engine_->GetSelection(&viewport_info.selection_start_page_index,
                        &viewport_info.selection_start_char_index,
                        &viewport_info.selection_end_page_index,
                        &viewport_info.selection_end_char_index);

  pp::PDF::SetAccessibilityViewportInfo(GetPluginInstance(), &viewport_info);
}

void OutOfProcessInstance::SelectionChanged(const pp::Rect& left,
                                            const pp::Rect& right) {
  pp::Point l(left.point().x() + available_area_.x(), left.point().y());
  pp::Point r(right.x() + available_area_.x(), right.point().y());

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
  engine_->SetCaretPosition(new_position);
}

void OutOfProcessInstance::MoveRangeSelectionExtent(
    const pp::FloatPoint& extent) {
  pp::Point new_extent(extent.x(), extent.y());
  ScalePoint(device_scale_, &new_extent);
  new_extent.set_x(new_extent.x() - available_area_.x());
  engine_->MoveRangeSelectionExtent(new_extent);
}

void OutOfProcessInstance::SetSelectionBounds(const pp::FloatPoint& base,
                                              const pp::FloatPoint& extent) {
  pp::Point new_base_point(base.x(), base.y());
  ScalePoint(device_scale_, &new_base_point);
  new_base_point.set_x(new_base_point.x() - available_area_.x());

  pp::Point new_extent_point(extent.x(), extent.y());
  ScalePoint(device_scale_, &new_extent_point);
  new_extent_point.set_x(new_extent_point.x() - available_area_.x());

  engine_->SetSelectionBounds(new_base_point, new_extent_point);
}

pp::Var OutOfProcessInstance::GetLinkAtPosition(const pp::Point& point) {
  pp::Point offset_point(point);
  ScalePoint(device_scale_, &offset_point);
  offset_point.set_x(offset_point.x() - available_area_.x());
  return engine_->GetLinkAtPosition(offset_point);
}

bool OutOfProcessInstance::CanEditText() {
  return engine_->CanEditText();
}

bool OutOfProcessInstance::HasEditableText() {
  return engine_->HasEditableText();
}

void OutOfProcessInstance::ReplaceSelection(const std::string& text) {
  engine_->ReplaceSelection(text);
}

bool OutOfProcessInstance::CanUndo() {
  return engine_->CanUndo();
}

bool OutOfProcessInstance::CanRedo() {
  return engine_->CanRedo();
}

void OutOfProcessInstance::Undo() {
  engine_->Undo();
}

void OutOfProcessInstance::Redo() {
  engine_->Redo();
}

void OutOfProcessInstance::HandleAccessibilityAction(
    const PP_PdfAccessibilityActionData& action_data) {
  engine_->HandleAccessibilityAction(action_data);
}

int32_t OutOfProcessInstance::PdfPrintBegin(
    const PP_PrintSettings_Dev* print_settings,
    const PP_PdfPrintSettings_Dev* pdf_print_settings) {
  // For us num_pages is always equal to the number of pages in the PDF
  // document irrespective of the printable area.
  int32_t ret = engine_->GetNumberOfPages();
  if (!ret)
    return 0;

  uint32_t supported_formats = engine_->QuerySupportedPrintOutputFormats();
  if ((print_settings->format & supported_formats) == 0)
    return 0;

  print_settings_.is_printing = true;
  print_settings_.pepper_print_settings = *print_settings;
  print_settings_.pdf_print_settings = *pdf_print_settings;
  engine_->PrintBegin();
  return ret;
}

uint32_t OutOfProcessInstance::QuerySupportedPrintOutputFormats() {
  return engine_->QuerySupportedPrintOutputFormats();
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
  return engine_->PrintPages(page_ranges, page_range_count,
                             print_settings_.pepper_print_settings,
                             print_settings_.pdf_print_settings);
}

void OutOfProcessInstance::PrintEnd() {
  if (print_settings_.print_pages_called)
    UserMetricsRecordAction("PDF.PrintPage");
  print_settings_.Clear();
  engine_->PrintEnd();
}

bool OutOfProcessInstance::IsPrintScalingDisabled() {
  return !engine_->GetPrintScaling();
}

bool OutOfProcessInstance::StartFind(const std::string& text,
                                     bool case_sensitive) {
  engine_->StartFind(text, case_sensitive);
  return true;
}

void OutOfProcessInstance::SelectFindResult(bool forward) {
  engine_->SelectFindResult(forward);
}

void OutOfProcessInstance::StopFind() {
  engine_->StopFind();
  tickmarks_.clear();
  SetTickmarks(tickmarks_);
}

void OutOfProcessInstance::OnPaint(const std::vector<pp::Rect>& paint_rects,
                                   std::vector<PaintManager::ReadyRect>* ready,
                                   std::vector<pp::Rect>* pending) {
  if (image_data_.is_null()) {
    DCHECK(plugin_size_.IsEmpty());
    return;
  }
  if (first_paint_) {
    first_paint_ = false;
    pp::Rect rect = pp::Rect(pp::Point(), image_data_.size());
    FillRect(rect, background_color_);
    ready->push_back(PaintManager::ReadyRect(rect, image_data_, true));
  }

  if (!received_viewport_message_ || !needs_reraster_)
    return;

  engine_->PrePaint();

  for (const auto& paint_rect : paint_rects) {
    // Intersect with plugin area since there could be pending invalidates from
    // when the plugin area was larger.
    pp::Rect rect = paint_rect.Intersect(pp::Rect(pp::Point(), plugin_size_));
    if (rect.IsEmpty())
      continue;

    pp::Rect pdf_rect = available_area_.Intersect(rect);
    if (!pdf_rect.IsEmpty()) {
      pdf_rect.Offset(available_area_.x() * -1, 0);

      std::vector<pp::Rect> pdf_ready;
      std::vector<pp::Rect> pdf_pending;
      engine_->Paint(pdf_rect, &image_data_, &pdf_ready, &pdf_pending);
      for (auto& ready_rect : pdf_ready) {
        ready_rect.Offset(available_area_.point());
        ready->push_back(
            PaintManager::ReadyRect(ready_rect, image_data_, false));
      }
      for (auto& pending_rect : pdf_pending) {
        pending_rect.Offset(available_area_.point());
        pending->push_back(pending_rect);
      }
    }

    // Ensure the region above the first page (if any) is filled;
    int32_t first_page_ypos = engine_->GetNumberOfPages() == 0
                                  ? 0
                                  : engine_->GetPageScreenRect(0).y();
    if (rect.y() < first_page_ypos) {
      pp::Rect region = rect.Intersect(pp::Rect(
          pp::Point(), pp::Size(plugin_size_.width(), first_page_ypos)));
      ready->push_back(PaintManager::ReadyRect(region, image_data_, false));
      FillRect(region, background_color_);
    }

    for (const auto& background_part : background_parts_) {
      pp::Rect intersection = background_part.location.Intersect(rect);
      if (!intersection.IsEmpty()) {
        FillRect(intersection, background_part.color);
        ready->push_back(
            PaintManager::ReadyRect(intersection, image_data_, false));
      }
    }
  }

  engine_->PostPaint();
}

void OutOfProcessInstance::DidOpen(int32_t result) {
  if (result == PP_OK) {
    if (!engine_->HandleDocumentLoad(embed_loader_)) {
      document_load_state_ = LOAD_STATE_LOADING;
      DocumentLoadFailed();
    }
  } else if (result != PP_ERROR_ABORTED) {  // Can happen in tests.
    DocumentLoadFailed();
  }
}

void OutOfProcessInstance::DidOpenPreview(int32_t result) {
  if (result == PP_OK) {
    preview_client_ = std::make_unique<PreviewModeClient>(this);
    preview_engine_ = PDFEngine::Create(preview_client_.get(), false);
    preview_engine_->HandleDocumentLoad(embed_preview_loader_);
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
  dimensions.Set(kJSLayoutOptions, layout.options().ToVar());
  pp::VarArray page_dimensions_array;
  size_t num_pages = layout.page_count();
  if (page_is_processed_.size() < num_pages)
    page_is_processed_.resize(num_pages);

  for (size_t i = 0; i < num_pages; ++i) {
    pp::Rect page_rect = layout.page_rect(i);
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

void OutOfProcessInstance::Invalidate(const pp::Rect& rect) {
  pp::Rect offset_rect(rect);
  offset_rect.Offset(available_area_.point());
  paint_manager_.InvalidateRect(offset_rect);
}

void OutOfProcessInstance::DidScroll(const pp::Point& point) {
  if (!image_data_.is_null())
    paint_manager_.ScrollRect(available_area_, point);
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

void OutOfProcessInstance::ScrollBy(const pp::Point& point) {
  pp::VarDictionary position;
  position.Set(kType, kJSScrollByType);
  position.Set(kJSPositionX, pp::Var(point.x() / device_scale_));
  position.Set(kJSPositionY, pp::Var(point.y() / device_scale_));
  PostMessage(position);
}

void OutOfProcessInstance::ScrollToPage(int page) {
  if (!engine_ || engine_->GetNumberOfPages() == 0)
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
    const std::vector<pp::Rect>& tickmarks) {
  float inverse_scale = 1.0f / device_scale_;
  std::vector<pp::Rect> scaled_tickmarks = tickmarks;
  for (auto& tickmark : scaled_tickmarks)
    ScaleRect(inverse_scale, &tickmark);
  tickmarks_ = scaled_tickmarks;
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
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &OutOfProcessInstance::ResetRecentlySentFindUpdate);
  pp::Module::Get()->core()->CallOnMainThread(kFindResultCooldownMs, callback,
                                              0);
}

void OutOfProcessInstance::NotifySelectedFindResultChanged(
    int current_find_index) {
  DCHECK_GE(current_find_index, -1);
  SelectedFindResultChanged(current_find_index);
}

void OutOfProcessInstance::NotifyPageBecameVisible(
    const PDFEngine::PageFeatures* page_features) {
  if (!page_features || !page_features->IsInitialized() ||
      page_features->index >= static_cast<int>(page_is_processed_.size()) ||
      page_is_processed_[page_features->index]) {
    return;
  }

  for (const int annotation_type : page_features->annotation_types) {
    if (annotation_type < 0 || annotation_type >= kAnnotationTypesCount) {
      NOTREACHED();
      continue;
    }

    bool inserted = annotation_types_counted_.insert(annotation_type).second;
    if (inserted) {
      HistogramEnumeration("PDF.AnnotationType", annotation_type,
                           kAnnotationTypesCount);
    }
  }
  page_is_processed_[page_features->index] = true;
}

void OutOfProcessInstance::GetDocumentPassword(
    pp::CompletionCallbackWithOutput<pp::Var> callback) {
  if (password_callback_) {
    NOTREACHED();
    return;
  }

  password_callback_ =
      std::make_unique<pp::CompletionCallbackWithOutput<pp::Var>>(callback);
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSGetPasswordType));
  PostMessage(message);
}

bool OutOfProcessInstance::ShouldSaveEdits() const {
  return edit_mode_ &&
         base::FeatureList::IsEnabled(features::kSaveEditedPDFForm);
}

void OutOfProcessInstance::SaveToBuffer(const std::string& token) {
  engine_->KillFormFocus();

  pp::VarDictionary message;
  message.Set(kType, kJSSaveDataType);
  message.Set(kJSToken, pp::Var(token));
  message.Set(kJSFileName, pp::Var(GetFileNameFromUrl(url_)));
  // This will be overwritten if the save is successful.
  message.Set(kJSDataToSave, pp::Var(pp::Var::Null()));
  const bool has_unsaved_changes =
      edit_mode_ && !base::FeatureList::IsEnabled(features::kSaveEditedPDFForm);
  message.Set(kJSHasUnsavedChanges, pp::Var(has_unsaved_changes));

  if (ShouldSaveEdits()) {
    std::vector<uint8_t> data = engine_->GetSaveData();
    if (IsSaveDataSizeValid(data.size())) {
      pp::VarArrayBuffer buffer(data.size());
      std::copy(data.begin(), data.end(),
                reinterpret_cast<char*>(buffer.Map()));
      message.Set(kJSDataToSave, buffer);
    }
  } else {
    DCHECK(base::FeatureList::IsEnabled(features::kPDFAnnotations));
    uint32_t length = engine_->GetLoadedByteSize();
    if (IsSaveDataSizeValid(length)) {
      pp::VarArrayBuffer buffer(length);
      if (engine_->ReadLoadedBytes(length, buffer.Map())) {
        message.Set(kJSDataToSave, buffer);
      }
    }
  }

  PostMessage(message);
}

void OutOfProcessInstance::SaveToFile(const std::string& token) {
  if (!ShouldSaveEdits()) {
    engine_->KillFormFocus();
    ConsumeSaveToken(token);
    pp::PDF::SaveAs(this);
    return;
  }

  SaveToBuffer(token);
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
  if (!engine_ ||
      (!engine_->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
       !engine_->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY))) {
    return;
  }

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&OutOfProcessInstance::OnPrint);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
}

void OutOfProcessInstance::OnPrint(int32_t) {
  pp::PDF::Print(this);
}

void OutOfProcessInstance::SubmitForm(const std::string& url,
                                      const void* data,
                                      int length) {
  pp::URLRequestInfo request(this);
  request.SetURL(url);
  request.SetMethod("POST");
  request.AppendDataToBody(reinterpret_cast<const char*>(data), length);

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&OutOfProcessInstance::FormDidOpen);
  form_loader_ = CreateURLLoaderInternal();
  int rv = form_loader_.Open(request, callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

void OutOfProcessInstance::FormDidOpen(int32_t result) {
  // TODO: inform the user of success/failure.
  if (result != PP_OK) {
    LOG(ERROR) << "FormDidOpen failed: " << result;
  }
}

pp::URLLoader OutOfProcessInstance::CreateURLLoader() {
  if (full_) {
    if (!did_call_start_loading_) {
      did_call_start_loading_ = true;
      pp::PDF::DidStartLoading(this);
    }

    // Disable save and print until the document is fully loaded, since they
    // would generate an incomplete document.  Need to do this each time we
    // call DidStartLoading since that resets the content restrictions.
    pp::PDF::SetContentRestriction(
        this, CONTENT_RESTRICTION_SAVE | CONTENT_RESTRICTION_PRINT);
  }

  return CreateURLLoaderInternal();
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
  HistogramEnumeration("PDF.DocumentFeature", LOADED_DOCUMENT, FEATURES_COUNT);

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

  pp::VarDictionary metadata_message;
  metadata_message.Set(pp::Var(kType), pp::Var(kJSMetadataType));
  std::string title = engine_->GetMetadata("Title");
  if (!base::TrimWhitespace(base::UTF8ToUTF16(title), base::TRIM_ALL).empty()) {
    metadata_message.Set(pp::Var(kJSTitle), pp::Var(title));
    HistogramEnumeration("PDF.DocumentFeature", HAS_TITLE, FEATURES_COUNT);
  }
  metadata_message.Set(
      pp::Var(kJSCanSerializeDocument),
      pp::Var(IsSaveDataSizeValid(engine_->GetLoadedByteSize())));

  pp::VarArray bookmarks = engine_->GetBookmarks();
  metadata_message.Set(pp::Var(kJSBookmarks), bookmarks);
  if (bookmarks.GetLength() > 0)
    HistogramEnumeration("PDF.DocumentFeature", HAS_BOOKMARKS, FEATURES_COUNT);
  PostMessage(metadata_message);

  pp::VarDictionary progress_message;
  progress_message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
  progress_message.Set(pp::Var(kJSProgressPercentage), pp::Var(100));
  PostMessage(progress_message);

  if (accessibility_state_ == ACCESSIBILITY_STATE_PENDING)
    LoadAccessibility();

  if (!full_)
    return;

  if (did_call_start_loading_) {
    pp::PDF::DidStopLoading(this);
    did_call_start_loading_ = false;
  }

  int content_restrictions =
      CONTENT_RESTRICTION_CUT | CONTENT_RESTRICTION_PASTE;
  if (!engine_->HasPermission(PDFEngine::PERMISSION_COPY))
    content_restrictions |= CONTENT_RESTRICTION_COPY;

  if (!engine_->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
      !engine_->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY)) {
    content_restrictions |= CONTENT_RESTRICTION_PRINT;
  }

  pp::PDF::SetContentRestriction(this, content_restrictions);
  HistogramCustomCounts("PDF.PageCount", document_features.page_count, 1,
                        1000000, 50);
  HistogramEnumeration("PDF.HasAttachment",
                       document_features.has_attachments ? 1 : 0, 2);
  HistogramEnumeration("PDF.IsTagged", document_features.is_tagged ? 1 : 0, 2);
  HistogramEnumeration("PDF.FormType",
                       static_cast<int32_t>(document_features.form_type),
                       static_cast<int32_t>(PDFEngine::FormType::kCount));
}

void OutOfProcessInstance::RotateClockwise() {
  engine_->RotateClockwise();
}

void OutOfProcessInstance::RotateCounterclockwise() {
  engine_->RotateCounterclockwise();
}

std::string OutOfProcessInstance::GetFileNameFromUrl(const std::string& url) {
  // Generate a file name. Unfortunately, MIME type can't be provided, since it
  // requires IO.
  base::string16 file_name = net::GetSuggestedFilename(
      GURL(url), std::string() /* content_disposition */,
      std::string() /* referrer_charset */, std::string() /* suggested_name */,
      std::string() /* mime_type */, std::string() /* default_name */);
  return base::UTF16ToUTF8(file_name);
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
  engine_->AppendPage(preview_engine_.get(), dest_page_index);

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
  paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));

  // Send a progress value of -1 to indicate a failure.
  pp::VarDictionary message;
  message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
  message.Set(pp::Var(kJSProgressPercentage), pp::Var(-1));
  PostMessage(message);
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
    pp::VarDictionary message;
    message.Set(pp::Var(kType), pp::Var(kJSLoadProgressType));
    message.Set(pp::Var(kJSProgressPercentage), pp::Var(progress));
    PostMessage(message);
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
    engine_->ZoomUpdated(zoom_ * device_scale_);

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
  engine_->PageOffsetUpdated(available_area_.point());
  engine_->PluginSizeUpdated(available_area_.size());

  if (document_size_.IsEmpty())
    return;
  paint_manager_.InvalidateRect(pp::Rect(pp::Point(), plugin_size_));

  if (accessibility_state_ == ACCESSIBILITY_STATE_LOADED)
    SendAccessibilityViewportInfo();
}

void OutOfProcessInstance::LoadUrl(const std::string& url,
                                   bool is_print_preview) {
  pp::URLRequestInfo request(this);
  request.SetURL(url);
  request.SetMethod("GET");
  request.SetFollowRedirects(false);

  pp::URLLoader* loader =
      is_print_preview ? &embed_preview_loader_ : &embed_loader_;
  *loader = CreateURLLoaderInternal();
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      is_print_preview ? &OutOfProcessInstance::DidOpenPreview
                       : &OutOfProcessInstance::DidOpen);
  int rv = loader->Open(request, callback);
  if (rv != PP_OK_COMPLETIONPENDING)
    callback.Run(rv);
}

pp::URLLoader OutOfProcessInstance::CreateURLLoaderInternal() {
  pp::URLLoader loader(this);

  const PPB_URLLoaderTrusted* trusted_interface =
      reinterpret_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(loader.pp_resource());
  return loader;
}

void OutOfProcessInstance::SetZoom(double scale) {
  double old_zoom = zoom_;
  zoom_ = scale;
  OnGeometryChanged(old_zoom, device_scale_);
}

void OutOfProcessInstance::AppendBlankPrintPreviewPages() {
  engine_->AppendBlankPages(print_preview_page_count_);
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

void OutOfProcessInstance::IsEditModeChanged(bool is_edit_mode) {
  edit_mode_ = is_edit_mode;
  pp::PDF::SetPluginCanSave(this, ShouldSaveEdits());
}

float OutOfProcessInstance::GetToolbarHeightInScreenCoords() {
  return top_toolbar_height_in_viewport_coords_ * device_scale_;
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

void OutOfProcessInstance::HistogramCustomCounts(const std::string& name,
                                                 int32_t sample,
                                                 int32_t min,
                                                 int32_t max,
                                                 uint32_t bucket_count) {
  if (IsPrintPreview())
    return;

  uma_.HistogramCustomCounts(name, sample, min, max, bucket_count);
}

void OutOfProcessInstance::HistogramEnumeration(const std::string& name,
                                                int32_t sample,
                                                int32_t boundary_value) {
  if (IsPrintPreview())
    return;
  uma_.HistogramEnumeration(name, sample, boundary_value);
}

void OutOfProcessInstance::PrintSettings::Clear() {
  is_printing = false;
  print_pages_called = false;
  memset(&pepper_print_settings, 0, sizeof(pepper_print_settings));
  memset(&pdf_print_settings, 0, sizeof(pdf_print_settings));
}

}  // namespace chrome_pdf
