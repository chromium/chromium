// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/out_of_process_instance.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "pdf/ppapi_migration/printing_conversions.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/ppapi_migration/value_conversions.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/buffer_dev.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "pdf/ppapi_migration/pdfium_font_linux.h"
#endif

namespace chrome_pdf {

namespace {

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

void SelectAll(PP_Instance instance) {
  void* object = pp::Instance::GetPerInstanceObject(instance, kPPPPdfInterface);
  if (object) {
    auto* obj_instance = static_cast<OutOfProcessInstance*>(object);
    obj_instance->SelectAll();
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
    &SelectAll,
    &CanUndo,
    &CanRedo,
    &Undo,
    &Redo,
    &HandleAccessibilityAction,
    &PdfPrintBegin,
};

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
        {link_info.url, link_info.index_in_page,
         base::checked_cast<uint32_t>(link_info.text_range.index),
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
         base::checked_cast<uint32_t>(highlight_info.text_range.index),
         highlight_info.text_range.count,
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
  DestroyPreviewEngine();
  DestroyEngine();
}

bool OutOfProcessInstance::Init(uint32_t argc,
                                const char* argn[],
                                const char* argv[]) {
  DCHECK(!engine());

  pp::Var document_url_var = pp::URLUtil_Dev::Get()->GetDocumentURL(this);
  if (!document_url_var.is_string())
    return false;
  GURL document_url(document_url_var.AsString());

  // Allow the plugin to handle find requests.
  SetPluginToHandleFindRequests();

  text_input_ = std::make_unique<pp::TextInput_Dev>(this);

  // Parse attributes. Keep in sync with `ParseWebPluginParams()`.
  const char* src_url = nullptr;
  const char* original_url = nullptr;
  const char* top_level_url = nullptr;
  bool full_frame = false;
  SkColor background_color = SK_ColorTRANSPARENT;
  PDFiumFormFiller::ScriptOption script_option =
      PDFiumFormFiller::DefaultScriptOption();
  bool has_edits = false;
  for (uint32_t i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "src") == 0) {
      src_url = argv[i];
    } else if (strcmp(argn[i], "original-url") == 0) {
      original_url = argv[i];
    } else if (strcmp(argn[i], "top-level-url") == 0) {
      top_level_url = argv[i];
    } else if (strcmp(argn[i], "full-frame") == 0) {
      full_frame = true;
    } else if (strcmp(argn[i], "background-color") == 0) {
      if (!base::StringToUint(argv[i], &background_color))
        return false;
    } else if (strcmp(argn[i], "javascript") == 0) {
      if (strcmp(argv[i], "allow") != 0)
        script_option = PDFiumFormFiller::ScriptOption::kNoJavaScript;
    } else if (strcmp(argn[i], "has-edits") == 0) {
      has_edits = true;
    }
  }

  if (!src_url)
    return false;

  if (!original_url)
    original_url = src_url;

  pp::PDF::SetCrashData(this, original_url, top_level_url);
  InitializeBase(
      std::make_unique<PDFiumEngine>(this, script_option),
      /*embedder_origin=*/document_url.DeprecatedGetOriginAsURL().spec(),
      /*src_url=*/src_url,
      /*original_url=*/original_url,
      /*full_frame=*/full_frame,
      /*background_color=*/background_color,
      /*has_edits=*/has_edits);
  return true;
}

void OutOfProcessInstance::HandleMessage(const pp::Var& message) {
  PdfViewPluginBase::HandleMessage(ValueFromVar(message));
}

bool OutOfProcessInstance::HandleInputEvent(const pp::InputEvent& event) {
  std::unique_ptr<blink::WebInputEvent> web_event = GetWebInputEvent(event);
  if (!web_event)
    return false;

  return PdfViewPluginBase::HandleInputEvent(*web_event);
}

void OutOfProcessInstance::DidChangeView(const pp::View& view) {
  const gfx::Rect new_plugin_rect = gfx::ScaleToEnclosingRect(
      RectFromPPRect(view.GetRect()), view.GetDeviceScale());
  UpdateGeometryOnPluginRectChanged(new_plugin_rect, view.GetDeviceScale());
}

void OutOfProcessInstance::DidChangeFocus(bool has_focus) {
  engine()->UpdateFocus(has_focus);
}

void OutOfProcessInstance::GetPrintPresetOptionsFromDocument(
    PP_PdfPrintPresetOptions_Dev* options) {
  *options =
      PPPdfPrintPresetOptionsFromWebPrintPresetOptions(GetPrintPresetOptions());
}

void OutOfProcessInstance::SetCaretPosition(const pp::FloatPoint& position) {
  PdfViewPluginBase::SetCaretPosition(PointFFromPPFloatPoint(position));
}

void OutOfProcessInstance::MoveRangeSelectionExtent(
    const pp::FloatPoint& extent) {
  PdfViewPluginBase::MoveRangeSelectionExtent(PointFFromPPFloatPoint(extent));
}

void OutOfProcessInstance::SetSelectionBounds(const pp::FloatPoint& base,
                                              const pp::FloatPoint& extent) {
  PdfViewPluginBase::SetSelectionBounds(PointFFromPPFloatPoint(base),
                                        PointFFromPPFloatPoint(extent));
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

void OutOfProcessInstance::SelectAll() {
  engine()->SelectAll();
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
  return PdfViewPluginBase::PrintBegin(
      WebPrintParamsFromPPPrintSettings(*print_settings, *pdf_print_settings));
}

uint32_t OutOfProcessInstance::QuerySupportedPrintOutputFormats() {
  if (engine()->HasPermission(DocumentPermission::kPrintHighQuality))
    return PP_PRINTOUTPUTFORMAT_PDF | PP_PRINTOUTPUTFORMAT_RASTER;
  if (engine()->HasPermission(DocumentPermission::kPrintLowQuality))
    return PP_PRINTOUTPUTFORMAT_RASTER;
  return 0;
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
  const std::vector<uint8_t> pdf_data = PdfViewPluginBase::PrintPages(
      PageNumbersFromPPPrintPageNumberRange(page_ranges, page_range_count));

  // Convert buffer to Pepper type.
  pp::Buffer_Dev buffer;
  if (!pdf_data.empty()) {
    buffer = pp::Buffer_Dev(this, pdf_data.size());
    if (!buffer.is_null())
      memcpy(buffer.data(), pdf_data.data(), pdf_data.size());
  }
  return buffer;
}

void OutOfProcessInstance::PrintEnd() {
  PdfViewPluginBase::PrintEnd();
}

bool OutOfProcessInstance::IsPrintScalingDisabled() {
  return !engine()->GetPrintScaling();
}

bool OutOfProcessInstance::StartFind(const std::string& text,
                                     bool case_sensitive) {
  return PdfViewPluginBase::StartFind(text, case_sensitive);
}

void OutOfProcessInstance::SelectFindResult(bool forward) {
  PdfViewPluginBase::SelectFindResult(forward);
}

void OutOfProcessInstance::StopFind() {
  PdfViewPluginBase::StopFind();
}

void OutOfProcessInstance::SendMessage(base::Value message) {
  PostMessage(VarFromValue(message));
}

void OutOfProcessInstance::SaveAs() {
  pp::PDF::SaveAs(this);
}

void OutOfProcessInstance::InitImageData(const gfx::Size& size) {
  pepper_image_data_ =
      pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, PPSizeFromSize(size),
                    /*init_to_zero=*/false);
  mutable_image_data() = SkBitmapFromPPImageData(
      std::make_unique<pp::ImageData>(pepper_image_data_));
}

void OutOfProcessInstance::SetFormTextFieldInFocus(bool in_focus) {
  if (!text_input_)
    return;

  text_input_->SetTextInputType(in_focus ? PP_TEXTINPUT_TYPE_DEV_TEXT
                                         : PP_TEXTINPUT_TYPE_DEV_NONE);
}

void OutOfProcessInstance::UpdateCursor(ui::mojom::CursorType new_cursor_type) {
  if (cursor_type() == new_cursor_type)
    return;
  set_cursor_type(new_cursor_type);

  const PPB_CursorControl_Dev* cursor_interface =
      reinterpret_cast<const PPB_CursorControl_Dev*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_CURSOR_CONTROL_DEV_INTERFACE));

  cursor_interface->SetCursor(pp_instance(),
                              PPCursorTypeFromCursorType(cursor_type()),
                              pp::ImageData().pp_resource(), nullptr);
}

void OutOfProcessInstance::NotifySelectedFindResultChanged(
    int current_find_index) {
  DCHECK_GE(current_find_index, -1);
  SelectedFindResultChanged(current_find_index);
}

void OutOfProcessInstance::CaretChanged(const gfx::Rect& caret_rect) {
  PP_Rect caret_viewport =
      PPRectFromRect(caret_rect + available_area().OffsetFromOrigin());
  text_input_->UpdateCaretPosition(caret_viewport, caret_viewport);
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

void OutOfProcessInstance::SetLastPluginInstance() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetLastPepperInstance(this);
#endif
}

Image OutOfProcessInstance::GetPluginImageData() const {
  return Image(pepper_image_data_);
}

void OutOfProcessInstance::SetAccessibilityDocInfo(
    AccessibilityDocInfo doc_info) {
  PP_PrivateAccessibilityDocInfo pp_doc_info = {
      doc_info.page_count, PP_FromBool(doc_info.text_accessible),
      PP_FromBool(doc_info.text_copyable)};
  pp::PDF::SetAccessibilityDocInfo(this, &pp_doc_info);
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
  pp::PDF::SetAccessibilityPageInfo(this, &pp_page_info, pp_text_runs, pp_chars,
                                    pp_page_objects);
}

void OutOfProcessInstance::SetAccessibilityViewportInfo(
    AccessibilityViewportInfo viewport_info) {
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
  pp::PDF::SetAccessibilityViewportInfo(this, &pp_viewport_info);
}

void OutOfProcessInstance::NotifyFindResultsChanged(int total,
                                                    bool final_result) {
  NumberOfFindResultsChanged(total, final_result);
}

void OutOfProcessInstance::NotifyFindTickmarks(
    const std::vector<gfx::Rect>& tickmarks) {
  std::vector<pp::Rect> pp_tickmarks;
  pp_tickmarks.reserve(tickmarks.size());
  std::transform(tickmarks.begin(), tickmarks.end(),
                 std::back_inserter(pp_tickmarks), PPRectFromRect);
  SetTickmarks(pp_tickmarks);
}

void OutOfProcessInstance::SetPluginCanSave(bool can_save) {
  pp::PDF::SetPluginCanSave(this, can_save);
}

base::WeakPtr<PdfViewPluginBase> OutOfProcessInstance::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<UrlLoader> OutOfProcessInstance::CreateUrlLoaderInternal() {
  auto loader = std::make_unique<PepperUrlLoader>(this);
  loader->GrantUniversalAccess();
  return loader;
}

std::string OutOfProcessInstance::RewriteRequestUrl(
    base::StringPiece url) const {
  if (IsPrintPreview()) {
    // TODO(crbug.com/1238829): This is a workaround for Pepper not supporting
    // chrome-untrusted://print/ URLs. Pepper issues requests through the
    // embedder's URL loaders, but a WebUI loader only supports subresource
    // requests to the same scheme (so chrome: only can request chrome: URLs,
    // and chrome-untrusted: only can request chrome-untrusted: URLs).
    //
    // To work around this (for the Pepper plugin only), we'll issue
    // chrome-untrusted://print/ requests to the equivalent chrome://print/ URL,
    // since both schemes support the same PDF URLs.
    if (base::StartsWith(url, kChromeUntrustedPrintHost)) {
      return base::StrCat(
          {kChromePrintHost, url.substr(kChromeUntrustedPrintHost.size())});
    }

    NOTREACHED();
  }

  return PdfViewPluginBase::RewriteRequestUrl(url);
}

void OutOfProcessInstance::SetSelectedText(const std::string& selected_text) {
  pp::PDF::SetSelectedText(this, selected_text.c_str());
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

void OutOfProcessInstance::PluginDidStartLoading() {
  pp::PDF::DidStartLoading(this);
}

void OutOfProcessInstance::PluginDidStopLoading() {
  pp::PDF::DidStopLoading(this);
}

void OutOfProcessInstance::InvokePrintDialog() {
  pp::PDF::Print(this);
}

void OutOfProcessInstance::SetContentRestrictions(int content_restrictions) {
  pp::PDF::SetContentRestriction(this, content_restrictions);
}

void OutOfProcessInstance::NotifyLinkUnderCursor() {
  pp::PDF::SetLinkUnderCursor(this, link_under_cursor().c_str());
}

void OutOfProcessInstance::NotifySelectionChanged(const gfx::PointF& left,
                                                  int left_height,
                                                  const gfx::PointF& right,
                                                  int right_height) {
  pp::PDF::SelectionChanged(this, PPFloatPointFromPointF(left), left_height,
                            PPFloatPointFromPointF(right), right_height);
}

void OutOfProcessInstance::NotifyUnsupportedFeature() {
  DCHECK(full_frame());
  pp::PDF::HasUnsupportedFeature(this);
}

void OutOfProcessInstance::UserMetricsRecordAction(const std::string& action) {
  // TODO(raymes): Move this function to PPB_UMA_Private.
  pp::PDF::UserMetricsRecordAction(this, pp::Var(action));
}

}  // namespace chrome_pdf
