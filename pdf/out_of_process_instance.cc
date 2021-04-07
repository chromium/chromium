// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <list>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "net/base/escape.h"
#include "pdf/accessibility.h"
#include "pdf/accessibility_structs.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_metadata.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/image.h"
#include "pdf/ppapi_migration/input_event_conversions.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/ppapi_migration/value_conversions.h"
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
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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
// Reset print preview mode (Page -> Plugin)
constexpr char kJSResetPrintPreviewModeType[] = "resetPrintPreviewMode";
constexpr char kJSPrintPreviewUrl[] = "url";
constexpr char kJSPrintPreviewGrayscale[] = "grayscale";
constexpr char kJSPrintPreviewPageCount[] = "pageCount";
// Load preview page (Page -> Plugin)
constexpr char kJSLoadPreviewPageType[] = "loadPreviewPage";
constexpr char kJSPreviewPageUrl[] = "url";
constexpr char kJSPreviewPageIndex[] = "index";

// Editing forms in document (Plugin -> Page)
constexpr char kJSSetIsEditingType[] = "setIsEditing";

constexpr base::TimeDelta kFindResultCooldown =
    base::TimeDelta::FromMilliseconds(100);

// Same value as printing::COMPLETE_PREVIEW_DOCUMENT_INDEX.
constexpr int kCompletePDFIndex = -1;
// A different negative value to differentiate itself from |kCompletePDFIndex|.
constexpr int kInvalidPDFIndex = -2;

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

PageCharacterIndex ToPageCharacterIndex(
    const PP_PdfPageCharacterIndex& pp_page_char_index) {
  return {pp_page_char_index.page_index, pp_page_char_index.char_index};
}

AccessibilityActionData ToAccessibilityActionData(
    const PP_PdfAccessibilityActionData& pp_action_data) {
  return {
      static_cast<AccessibilityAction>(pp_action_data.action),
      static_cast<AccessibilityAnnotationType>(pp_action_data.annotation_type),
      PointFromPPPoint(pp_action_data.target_point),
      RectFromPPRect(pp_action_data.target_rect),
      pp_action_data.annotation_index,
      pp_action_data.page_index,
      static_cast<AccessibilityScrollAlignment>(
          pp_action_data.horizontal_scroll_alignment),
      static_cast<AccessibilityScrollAlignment>(
          pp_action_data.vertical_scroll_alignment),
      ToPageCharacterIndex(pp_action_data.selection_start_index),
      ToPageCharacterIndex(pp_action_data.selection_end_index)};
}

void HandleAccessibilityAction(
    PP_Instance instance,
    const PP_PdfAccessibilityActionData& action_data) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->HandleAccessibilityAction(
        ToAccessibilityActionData(action_data));
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

PP_PrivateAccessibilityPageInfo ToPrivateAccessibilityPageInfo(
    const AccessibilityPageInfo& page_info) {
  PP_PrivateAccessibilityPageInfo pp_page_info;
  pp_page_info.page_index = page_info.page_index;
  pp_page_info.bounds = PPRectFromRect(page_info.bounds);
  pp_page_info.text_run_count = page_info.text_run_count;
  pp_page_info.char_count = page_info.char_count;
  return pp_page_info;
}

std::vector<PP_PrivateAccessibilityCharInfo> ToPrivateAccessibilityCharInfo(
    const std::vector<AccessibilityCharInfo>& chars) {
  std::vector<PP_PrivateAccessibilityCharInfo> pp_chars;
  pp_chars.reserve(chars.size());
  for (const auto& char_object : chars)
    pp_chars.push_back({char_object.unicode_character, char_object.char_width});
  return pp_chars;
}

pp::PDF::PrivateAccessibilityTextStyleInfo ToPrivateAccessibilityTextStyleInfo(
    const AccessibilityTextStyleInfo& style) {
  pp::PDF::PrivateAccessibilityTextStyleInfo pp_style;
  pp_style.font_name = style.font_name;
  pp_style.font_weight = style.font_weight;
  pp_style.render_mode = static_cast<PP_TextRenderingMode>(style.render_mode);
  pp_style.font_size = style.font_size;
  pp_style.fill_color = style.fill_color;
  pp_style.stroke_color = style.stroke_color;
  pp_style.is_italic = style.is_italic;
  pp_style.is_bold = style.is_bold;
  return pp_style;
}

std::vector<pp::PDF::PrivateAccessibilityTextRunInfo>
ToPrivateAccessibilityCharInfo(
    const std::vector<AccessibilityTextRunInfo>& text_runs) {
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> pp_text_runs;
  pp_text_runs.reserve(text_runs.size());
  for (const auto& text_run : text_runs) {
    pp::PDF::PrivateAccessibilityTextRunInfo pp_text_run = {
        text_run.len, PPFloatRectFromRectF(text_run.bounds),
        static_cast<PP_PrivateDirection>(text_run.direction),
        ToPrivateAccessibilityTextStyleInfo(text_run.style)};
    pp_text_runs.push_back(std::move(pp_text_run));
  }
  return pp_text_runs;
}

pp::PDF::PrivateAccessibilityPageObjects ToPrivateAccessibilityPageObjects(
    const AccessibilityPageObjects& page_objects) {
  pp::PDF::PrivateAccessibilityPageObjects pp_page_objects;

  pp_page_objects.links.reserve(page_objects.links.size());
  for (const auto& link_info : page_objects.links) {
    pp_page_objects.links.push_back(
        {link_info.url, link_info.index_in_page, link_info.text_range.index,
         link_info.text_range.count, PPFloatRectFromRectF(link_info.bounds)});
  }

  pp_page_objects.images.reserve(page_objects.images.size());
  for (const auto& image_info : page_objects.images) {
    pp_page_objects.images.push_back({image_info.alt_text,
                                      image_info.text_run_index,
                                      PPFloatRectFromRectF(image_info.bounds)});
  }

  pp_page_objects.highlights.reserve(page_objects.highlights.size());
  for (const auto& highlight_info : page_objects.highlights) {
    pp_page_objects.highlights.push_back(
        {highlight_info.note_text, highlight_info.index_in_page,
         highlight_info.text_range.index, highlight_info.text_range.count,
         PPFloatRectFromRectF(highlight_info.bounds), highlight_info.color});
  }

  pp_page_objects.form_fields.text_fields.reserve(
      page_objects.form_fields.text_fields.size());
  for (const auto& text_field_info : page_objects.form_fields.text_fields) {
    pp_page_objects.form_fields.text_fields.push_back(
        {text_field_info.name, text_field_info.value,
         text_field_info.is_read_only, text_field_info.is_required,
         text_field_info.is_password, text_field_info.index_in_page,
         text_field_info.text_run_index,
         PPFloatRectFromRectF(text_field_info.bounds)});
  }

  pp_page_objects.form_fields.choice_fields.reserve(
      page_objects.form_fields.choice_fields.size());
  for (const auto& choice_field_info : page_objects.form_fields.choice_fields) {
    std::vector<pp::PDF::PrivateAccessibilityChoiceFieldOptionInfo>
        pp_choice_field_option_infos;
    pp_choice_field_option_infos.reserve(choice_field_info.options.size());
    for (const auto& option : choice_field_info.options) {
      pp_choice_field_option_infos.push_back(
          {option.name, option.is_selected,
           PPFloatRectFromRectF(option.bounds)});
    }
    pp_page_objects.form_fields.choice_fields.push_back(
        {choice_field_info.name, pp_choice_field_option_infos,
         static_cast<PP_PrivateChoiceFieldType>(choice_field_info.type),
         choice_field_info.is_read_only, choice_field_info.is_multi_select,
         choice_field_info.has_editable_text_box,
         choice_field_info.index_in_page, choice_field_info.text_run_index,
         PPFloatRectFromRectF(choice_field_info.bounds)});
  }

  pp_page_objects.form_fields.buttons.reserve(
      page_objects.form_fields.buttons.size());
  for (const auto& button_info : page_objects.form_fields.buttons) {
    pp_page_objects.form_fields.buttons.push_back(
        {button_info.name, button_info.value,
         static_cast<PP_PrivateButtonType>(button_info.type),
         button_info.is_read_only, button_info.is_checked,
         button_info.control_count, button_info.control_index,
         button_info.index_in_page, button_info.text_run_index,
         PPFloatRectFromRectF(button_info.bounds)});
  }

  return pp_page_objects;
}

}  // namespace

OutOfProcessInstance::OutOfProcessInstance(PP_Instance instance)
    : pp::Instance(instance), pp::Find_Private(this), pp::Printing_Dev(this) {
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

  // Allow the plugin to handle find requests.
  SetPluginToHandleFindRequests();

  text_input_ = std::make_unique<pp::TextInput_Dev>(this);

  PDFiumFormFiller::ScriptOption script_option =
      PDFiumFormFiller::DefaultScriptOption();
  bool has_edits = false;
  const char* stream_url = nullptr;
  const char* original_url = nullptr;
  const char* top_level_url = nullptr;
  const char* headers = nullptr;
  for (uint32_t i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "src") == 0) {
      original_url = argv[i];
    } else if (strcmp(argn[i], "stream-url") == 0) {
      stream_url = argv[i];
    } else if (strcmp(argn[i], "top-level-url") == 0) {
      top_level_url = argv[i];
    } else if (strcmp(argn[i], "headers") == 0) {
      headers = argv[i];
    } else if (strcmp(argn[i], "full-frame") == 0) {
      set_full_frame(true);
    } else if (strcmp(argn[i], "background-color") == 0) {
      SkColor background_color;
      if (!base::StringToUint(argv[i], &background_color))
        return false;
      SetBackgroundColor(background_color);
    } else if (strcmp(argn[i], "javascript") == 0) {
      if (strcmp(argv[i], "allow") != 0)
        script_option = PDFiumFormFiller::ScriptOption::kNoJavaScript;
    } else if (strcmp(argn[i], "has-edits") == 0) {
      has_edits = true;
    }
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
  set_url(original_url);

  // Not all edits go through the PDF plugin's form filler. The plugin instance
  // can be restarted by exiting annotation mode on ChromeOS, which can set the
  // document to an edited state.
  set_edit_mode(has_edits);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!edit_mode());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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

  if (type == kJSPrintType) {
    Print();
  } else if (type == kJSSaveAttachmentType) {
    HandleSaveAttachmentMessage(dict);
  } else if (type == kJSSaveType) {
    HandleSaveMessage(dict);
  } else if (type == kJSResetPrintPreviewModeType) {
    HandleResetPrintPreviewModeMessage(dict);
  } else if (type == kJSLoadPreviewPageType) {
    HandleLoadPreviewPageMessage(dict);
  } else {
    PdfViewPluginBase::HandleMessage(ValueFromVar(message));
  }
}

bool OutOfProcessInstance::HandleInputEvent(const pp::InputEvent& event) {
  // Ignore user input in read-only mode.
  // TODO(dhoss): Add a test for ignored input events. It is currently difficult
  // to unit test certain `OutOfProcessInstance` methods.
  if (engine()->IsReadOnly())
    return false;

  // To simplify things, convert the event into device coordinates.
  pp::InputEvent event_device_res(event);
  {
    pp::MouseInputEvent mouse_event(event);
    if (!mouse_event.is_null()) {
      pp::Point point = mouse_event.GetPosition();
      pp::Point movement = mouse_event.GetMovement();
      ScalePoint(device_scale(), &point);
      point.set_x(point.x() - available_area().x());

      ScalePoint(device_scale(), &movement);
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
        ScaleFloatPoint(device_scale(), &point);
        point.set_x(point.x() - available_area().x());

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
  UpdateGeometryOnViewChanged(RectFromPPRect(view.GetRect()),
                              view.GetDeviceScale());

  if (is_print_preview_ && !stop_scrolling()) {
    set_scroll_position(PointFromPPPoint(view.GetScrollOffset()));
    UpdateScroll();
  }

  // Scrolling in the main PDF Viewer UI is already handled by
  // HandleUpdateScrollMessage().
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

  base::Optional<gfx::Size> uniform_page_size =
      engine()->GetUniformPageSizePoints();
  options->is_page_size_uniform = PP_FromBool(uniform_page_size.has_value());
  options->uniform_page_size = PPSizeFromSize(
      uniform_page_size.has_value() ? uniform_page_size.value() : gfx::Size());
}

void OutOfProcessInstance::SelectionChanged(const gfx::Rect& left,
                                            const gfx::Rect& right) {
  pp::Point l(left.x() + available_area().x(), left.y());
  pp::Point r(right.x() + available_area().x(), right.y());

  float inverse_scale = 1.0f / device_scale();
  ScalePoint(inverse_scale, &l);
  ScalePoint(inverse_scale, &r);

  pp::PDF::SelectionChanged(GetPluginInstance(),
                            PP_MakeFloatPoint(l.x(), l.y()), left.height(),
                            PP_MakeFloatPoint(r.x(), r.y()), right.height());
  if (accessibility_state() == AccessibilityState::kLoaded)
    PrepareAndSetAccessibilityViewportInfo();
}

void OutOfProcessInstance::SetCaretPosition(const pp::FloatPoint& position) {
  pp::Point new_position(position.x(), position.y());
  ScalePoint(device_scale(), &new_position);
  new_position.set_x(new_position.x() - available_area().x());
  engine()->SetCaretPosition(PointFromPPPoint(new_position));
}

void OutOfProcessInstance::MoveRangeSelectionExtent(
    const pp::FloatPoint& extent) {
  pp::Point new_extent(extent.x(), extent.y());
  ScalePoint(device_scale(), &new_extent);
  new_extent.set_x(new_extent.x() - available_area().x());
  engine()->MoveRangeSelectionExtent(PointFromPPPoint(new_extent));
}

void OutOfProcessInstance::SetSelectionBounds(const pp::FloatPoint& base,
                                              const pp::FloatPoint& extent) {
  pp::Point new_base_point(base.x(), base.y());
  ScalePoint(device_scale(), &new_base_point);
  new_base_point.set_x(new_base_point.x() - available_area().x());

  pp::Point new_extent_point(extent.x(), extent.y());
  ScalePoint(device_scale(), &new_extent_point);
  new_extent_point.set_x(new_extent_point.x() - available_area().x());

  engine()->SetSelectionBounds(PointFromPPPoint(new_base_point),
                               PointFromPPPoint(new_extent_point));
}

pp::Var OutOfProcessInstance::GetLinkAtPosition(const pp::Point& point) {
  pp::Point offset_point(point);
  ScalePoint(device_scale(), &offset_point);
  offset_point.set_x(offset_point.x() - available_area().x());
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

void OutOfProcessInstance::DidOpen(std::unique_ptr<UrlLoader> loader,
                                   int32_t result) {
  if (result == PP_OK) {
    if (!engine()->HandleDocumentLoad(std::move(loader))) {
      set_document_load_state(DocumentLoadState::kLoading);
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

void OutOfProcessInstance::SendMessage(base::Value message) {
  PostMessage(VarFromValue(message));
}

void OutOfProcessInstance::InitImageData(const gfx::Size& size) {
  pepper_image_data_ =
      pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, PPSizeFromSize(size),
                    /*init_to_zero=*/false);
  mutable_image_data() = SkBitmapFromPPImageData(
      std::make_unique<pp::ImageData>(pepper_image_data_));
}

void OutOfProcessInstance::SetFormFieldInFocus(bool in_focus) {
  if (!text_input_)
    return;

  text_input_->SetTextInputType(in_focus ? PP_TEXTINPUT_TYPE_DEV_TEXT
                                         : PP_TEXTINPUT_TYPE_DEV_NONE);
}

void OutOfProcessInstance::UpdateCursor(ui::mojom::CursorType cursor_type) {
  if (cursor_type == cursor_type_)
    return;
  cursor_type_ = cursor_type;

  const PPB_CursorControl_Dev* cursor_interface =
      reinterpret_cast<const PPB_CursorControl_Dev*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_CURSOR_CONTROL_DEV_INTERFACE));
  if (!cursor_interface) {
    NOTREACHED();
    return;
  }

  cursor_interface->SetCursor(pp_instance(),
                              PPCursorTypeFromCursorType(cursor_type_),
                              pp::ImageData().pp_resource(), nullptr);
}

void OutOfProcessInstance::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  float inverse_scale = 1.0f / device_scale();
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
  ScheduleTaskOnMainThread(
      FROM_HERE,
      base::BindOnce(&OutOfProcessInstance::ResetRecentlySentFindUpdate,
                     weak_factory_.GetWeakPtr()),
      /*result=*/0, kFindResultCooldown);
}

void OutOfProcessInstance::NotifySelectedFindResultChanged(
    int current_find_index) {
  DCHECK_GE(current_find_index, -1);
  SelectedFindResultChanged(current_find_index);
}

void OutOfProcessInstance::SaveToFile(const std::string& token) {
  engine()->KillFormFocus();
  ConsumeSaveToken(token);
  pp::PDF::SaveAs(this);
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

void OutOfProcessInstance::Print() {
  if (!engine() ||
      (!engine()->HasPermission(PDFEngine::PERMISSION_PRINT_LOW_QUALITY) &&
       !engine()->HasPermission(PDFEngine::PERMISSION_PRINT_HIGH_QUALITY))) {
    return;
  }

  ScheduleTaskOnMainThread(FROM_HERE,
                           base::BindOnce(&OutOfProcessInstance::OnPrint,
                                          weak_factory_.GetWeakPtr()),
                           /*result=*/0, base::TimeDelta());
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

std::vector<PDFEngine::Client::SearchStringResult>
OutOfProcessInstance::SearchString(const char16_t* string,
                                   const char16_t* term,
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

void OutOfProcessInstance::RotateClockwise() {
  engine()->RotateClockwise();
}

void OutOfProcessInstance::RotateCounterclockwise() {
  engine()->RotateCounterclockwise();
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
  set_url(url);
  preview_pages_info_ = base::queue<PreviewPageInfo>();
  preview_document_load_state_ = DocumentLoadState::kComplete;
  set_document_load_state(DocumentLoadState::kLoading);
  LoadUrl(GetURL(), /*is_print_preview=*/false);
  preview_engine_.reset();
  InitializeEngine(PDFiumFormFiller::ScriptOption::kNoJavaScript);
  engine()->SetGrayscale(dict.Get(pp::Var(kJSPrintPreviewGrayscale)).AsBool());
  engine()->New(GetURL().c_str(), /*headers=*/nullptr);

  paint_manager().InvalidateRect(gfx::Rect(plugin_rect().size()));
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
      pp::PDF::SetPluginCanSave(this, edit_mode());
      break;
    case SaveRequestType::kEdited:
      SaveToBuffer(dict.Get(pp::Var(kJSToken)).AsString());
      break;
  }
}

void OutOfProcessInstance::PreviewDocumentLoadComplete() {
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kComplete;

  int dest_page_index = preview_pages_info_.front().second;
  DCHECK_GT(dest_page_index, 0);
  preview_pages_info_.pop();
  DCHECK(preview_engine_);
  engine()->AppendPage(preview_engine_.get(), dest_page_index);

  ++print_preview_loaded_page_count_;
  LoadNextPreviewPage();
}

void OutOfProcessInstance::PreviewDocumentLoadFailed() {
  UserMetricsRecordAction("PDF.PreviewDocumentLoadFailure");
  if (preview_document_load_state_ != DocumentLoadState::kLoading ||
      preview_pages_info_.empty()) {
    return;
  }

  // Even if a print preview page failed to load, keep going.
  preview_document_load_state_ = DocumentLoadState::kFailed;
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
  if (!full_frame())
    return;

  if (told_browser_about_unsupported_feature_)
    return;
  told_browser_about_unsupported_feature_ = true;

  pp::PDF::HasUnsupportedFeature(this);
}

void OutOfProcessInstance::ResetRecentlySentFindUpdate(int32_t /* unused */) {
  recently_sent_find_update_ = false;
}

Image OutOfProcessInstance::GetPluginImageData() const {
  return Image(pepper_image_data_);
}

void OutOfProcessInstance::SetAccessibilityDocInfo(
    const AccessibilityDocInfo& doc_info) {
  PP_PrivateAccessibilityDocInfo pp_doc_info = {
      doc_info.page_count, PP_FromBool(doc_info.text_accessible),
      PP_FromBool(doc_info.text_copyable)};
  pp::PDF::SetAccessibilityDocInfo(GetPluginInstance(), &pp_doc_info);
}

void OutOfProcessInstance::SetAccessibilityPageInfo(
    AccessibilityPageInfo page_info,
    std::vector<AccessibilityTextRunInfo> text_runs,
    std::vector<AccessibilityCharInfo> chars,
    AccessibilityPageObjects page_objects) {
  PP_PrivateAccessibilityPageInfo pp_page_info =
      ToPrivateAccessibilityPageInfo(page_info);
  std::vector<PP_PrivateAccessibilityCharInfo> pp_chars =
      ToPrivateAccessibilityCharInfo(chars);
  std::vector<pp::PDF::PrivateAccessibilityTextRunInfo> pp_text_runs =
      ToPrivateAccessibilityCharInfo(text_runs);
  pp::PDF::PrivateAccessibilityPageObjects pp_page_objects =
      ToPrivateAccessibilityPageObjects(page_objects);
  pp::PDF::SetAccessibilityPageInfo(GetPluginInstance(), &pp_page_info,
                                    pp_text_runs, pp_chars, pp_page_objects);
}

void OutOfProcessInstance::SetAccessibilityViewportInfo(
    const AccessibilityViewportInfo& viewport_info) {
  PP_PrivateAccessibilityViewportInfo pp_viewport_info = {
      viewport_info.zoom,
      viewport_info.scale,
      pp::Point(viewport_info.scroll.x(), viewport_info.scroll.y()),
      pp::Point(viewport_info.offset.x(), viewport_info.offset.y()),
      viewport_info.selection_start_page_index,
      viewport_info.selection_start_char_index,
      viewport_info.selection_end_page_index,
      viewport_info.selection_end_char_index,
      {static_cast<PP_PrivateFocusObjectType>(
           viewport_info.focus_info.focused_object_type),
       viewport_info.focus_info.focused_object_page_index,
       viewport_info.focus_info.focused_annotation_index_in_page}};
  pp::PDF::SetAccessibilityViewportInfo(GetPluginInstance(), &pp_viewport_info);
}

base::WeakPtr<PdfViewPluginBase> OutOfProcessInstance::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<UrlLoader> OutOfProcessInstance::CreateUrlLoaderInternal() {
  auto loader = std::make_unique<PepperUrlLoader>(this);
  loader->GrantUniversalAccess();
  return loader;
}

void OutOfProcessInstance::AppendBlankPrintPreviewPages() {
  engine()->AppendBlankPages(print_preview_page_count_);
  LoadNextPreviewPage();
}

bool OutOfProcessInstance::IsPrintPreview() {
  return is_print_preview_;
}

void OutOfProcessInstance::EnteredEditMode() {
  set_edit_mode(true);
  pp::PDF::SetPluginCanSave(this, true);

  pp::VarDictionary message;
  message.Set(kType, kJSSetIsEditingType);
  PostMessage(message);
}

void OutOfProcessInstance::SetSelectedText(const std::string& selected_text) {
  pp::PDF::SetSelectedText(this, selected_text.c_str());
}

void OutOfProcessInstance::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  pp::PDF::SetLinkUnderCursor(this, link_under_cursor.c_str());
}

bool OutOfProcessInstance::IsValidLink(const std::string& url) {
  return pp::Var(url).is_string();
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

void OutOfProcessInstance::ScheduleTaskOnMainThread(
    const base::Location& from_here,
    ResultCallback callback,
    int32_t result,
    base::TimeDelta delay) {
  int64_t delay_in_msec = delay.InMilliseconds();
  DCHECK_LE(delay_in_msec, INT32_MAX);
  pp::Module::Get()->core()->CallOnMainThread(
      static_cast<int32_t>(delay_in_msec),
      PPCompletionCallbackFromResultCallback(std::move(callback)), result);
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
      document_load_state() != DocumentLoadState::kComplete ||
      preview_document_load_state_ == DocumentLoadState::kLoading) {
    return;
  }

  preview_document_load_state_ = DocumentLoadState::kLoading;
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

void OutOfProcessInstance::DidStartLoading() {
  if (did_call_start_loading_)
    return;

  pp::PDF::DidStartLoading(this);
  did_call_start_loading_ = true;
}

void OutOfProcessInstance::DidStopLoading() {
  if (!did_call_start_loading_)
    return;

  pp::PDF::DidStopLoading(this);
  did_call_start_loading_ = false;
}

void OutOfProcessInstance::OnPrintPreviewLoaded() {
  // Scroll location is retained across document loads in print preview mode, so
  // there's no need to override the scroll position by scrolling again.
  if (IsPreviewingPDF(print_preview_page_count_)) {
    SendPrintPreviewLoadedNotification();
  } else {
    DCHECK_EQ(0, print_preview_loaded_page_count_);
    print_preview_loaded_page_count_ = 1;
    AppendBlankPrintPreviewPages();
  }
  OnGeometryChanged(0, 0);
}

void OutOfProcessInstance::SetContentRestrictions(int content_restrictions) {
  pp::PDF::SetContentRestriction(this, content_restrictions);
}

void OutOfProcessInstance::UserMetricsRecordAction(const std::string& action) {
  // TODO(raymes): Move this function to PPB_UMA_Private.
  pp::PDF::UserMetricsRecordAction(this, pp::Var(action));
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

void OutOfProcessInstance::OnPrint(int32_t /*unused_but_required*/) {
  pp::PDF::Print(this);
}

void OutOfProcessInstance::PrintSettings::Clear() {
  is_printing = false;
  print_pages_called = false;
  memset(&pepper_print_settings, 0, sizeof(pepper_print_settings));
  memset(&pdf_print_settings, 0, sizeof(pdf_print_settings));
}

}  // namespace chrome_pdf
