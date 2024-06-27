// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_form_filler.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8-isolate.h"

namespace chrome_pdf {

namespace {

int g_last_timer_id = 0;

#if defined(PDF_ENABLE_V8)
std::string WideStringToString(FPDF_WIDESTRING wide_string) {
  return base::UTF16ToUTF8(reinterpret_cast<const char16_t*>(wide_string));
}
#endif

}  // namespace

// static
PDFiumFormFiller::ScriptOption PDFiumFormFiller::DefaultScriptOption() {
#if defined(PDF_ENABLE_XFA)
  if (base::FeatureList::IsEnabled(features::kPdfXfaSupport))
    return PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA;
#endif  // defined(PDF_ENABLE_XFA)
  return PDFiumFormFiller::ScriptOption::kJavaScript;
}

PDFiumFormFiller::PDFiumFormFiller(PDFiumEngine* engine,
                                   ScriptOption script_option)
    : engine_in_isolate_scope_factory_(engine, script_option),
      script_option_(script_option) {
  // Initialize FPDF_FORMFILLINFO member variables.  Deriving from this struct
  // allows the static callbacks to be able to cast the FPDF_FORMFILLINFO in
  // callbacks to ourself instead of maintaining a map of them to
  // PDFiumEngine.
  FPDF_FORMFILLINFO::version = 2;
  FPDF_FORMFILLINFO::Release = nullptr;
  FPDF_FORMFILLINFO::FFI_Invalidate = Form_Invalidate;
  FPDF_FORMFILLINFO::FFI_OutputSelectedRect = Form_OutputSelectedRect;
  FPDF_FORMFILLINFO::FFI_SetCursor = Form_SetCursor;
  FPDF_FORMFILLINFO::FFI_SetTimer = Form_SetTimer;
  FPDF_FORMFILLINFO::FFI_KillTimer = Form_KillTimer;
  FPDF_FORMFILLINFO::FFI_GetLocalTime = Form_GetLocalTime;
  FPDF_FORMFILLINFO::FFI_OnChange = Form_OnChange;
  FPDF_FORMFILLINFO::FFI_GetPage = Form_GetPage;
  FPDF_FORMFILLINFO::FFI_GetCurrentPage = Form_GetCurrentPage;
  FPDF_FORMFILLINFO::FFI_GetRotation = Form_GetRotation;
  FPDF_FORMFILLINFO::FFI_ExecuteNamedAction = Form_ExecuteNamedAction;
  FPDF_FORMFILLINFO::FFI_SetTextFieldFocus = Form_SetTextFieldFocus;
  FPDF_FORMFILLINFO::FFI_DoURIAction = Form_DoURIAction;
  FPDF_FORMFILLINFO::FFI_DoGoToAction = Form_DoGoToAction;
  FPDF_FORMFILLINFO::FFI_OnFocusChange = Form_OnFocusChange;
  FPDF_FORMFILLINFO::FFI_DoURIActionWithKeyboardModifier =
      Form_DoURIActionWithKeyboardModifier;
  FPDF_FORMFILLINFO::xfa_disabled = true;
  FPDF_FORMFILLINFO::FFI_EmailTo = nullptr;
  FPDF_FORMFILLINFO::FFI_DisplayCaret = nullptr;
  FPDF_FORMFILLINFO::FFI_SetCurrentPage = nullptr;
  FPDF_FORMFILLINFO::FFI_GetCurrentPageIndex = nullptr;
  FPDF_FORMFILLINFO::FFI_GetPageViewRect = nullptr;
  FPDF_FORMFILLINFO::FFI_GetPlatform = nullptr;
  FPDF_FORMFILLINFO::FFI_PageEvent = nullptr;
  FPDF_FORMFILLINFO::FFI_PopupMenu = nullptr;
  FPDF_FORMFILLINFO::FFI_PostRequestURL = nullptr;
  FPDF_FORMFILLINFO::FFI_PutRequestURL = nullptr;
  FPDF_FORMFILLINFO::FFI_UploadTo = nullptr;
  FPDF_FORMFILLINFO::FFI_DownloadFromURL = nullptr;
  FPDF_FORMFILLINFO::FFI_OpenFile = nullptr;
  FPDF_FORMFILLINFO::FFI_GotoURL = nullptr;
  FPDF_FORMFILLINFO::FFI_GetLanguage = nullptr;
  FPDF_FORMFILLINFO::m_pJsPlatform = nullptr;

#if defined(PDF_ENABLE_V8)
  if (script_option != ScriptOption::kNoJavaScript) {
    FPDF_FORMFILLINFO::m_pJsPlatform = this;
    IPDF_JSPLATFORM::version = 3;
    IPDF_JSPLATFORM::app_alert = Form_Alert;
    IPDF_JSPLATFORM::app_beep = Form_Beep;
    IPDF_JSPLATFORM::app_response = Form_Response;
    IPDF_JSPLATFORM::Doc_getFilePath = Form_GetFilePath;
    IPDF_JSPLATFORM::Doc_mail = Form_Mail;
    IPDF_JSPLATFORM::Doc_print = Form_Print;
    IPDF_JSPLATFORM::Doc_submitForm = Form_SubmitForm;
    IPDF_JSPLATFORM::Doc_gotoPage = Form_GotoPage;
    IPDF_JSPLATFORM::Field_browse = nullptr;
  }
#if defined(PDF_ENABLE_XFA)
  if (script_option == ScriptOption::kJavaScriptAndXFA) {
    FPDF_FORMFILLINFO::xfa_disabled = false;
    FPDF_FORMFILLINFO::FFI_EmailTo = Form_EmailTo;
    FPDF_FORMFILLINFO::FFI_DisplayCaret = Form_DisplayCaret;
    FPDF_FORMFILLINFO::FFI_SetCurrentPage = Form_SetCurrentPage;
    FPDF_FORMFILLINFO::FFI_GetCurrentPageIndex = Form_GetCurrentPageIndex;
    FPDF_FORMFILLINFO::FFI_GetPageViewRect = Form_GetPageViewRect;
    FPDF_FORMFILLINFO::FFI_GetPlatform = Form_GetPlatform;
    FPDF_FORMFILLINFO::FFI_PageEvent = Form_PageEvent;
    FPDF_FORMFILLINFO::FFI_PopupMenu = Form_PopupMenu;
    FPDF_FORMFILLINFO::FFI_PostRequestURL = Form_PostRequestURL;
    FPDF_FORMFILLINFO::FFI_PutRequestURL = Form_PutRequestURL;
    FPDF_FORMFILLINFO::FFI_UploadTo = Form_UploadTo;
    FPDF_FORMFILLINFO::FFI_DownloadFromURL = Form_DownloadFromURL;
    FPDF_FORMFILLINFO::FFI_OpenFile = Form_OpenFile;
    FPDF_FORMFILLINFO::FFI_GotoURL = Form_GotoURL;
    FPDF_FORMFILLINFO::FFI_GetLanguage = Form_GetLanguage;
  }
#endif  // defined(PDF_ENABLE_XFA)
#endif  // defined(PDF_ENABLE_V8)
}

PDFiumFormFiller::~PDFiumFormFiller() = default;

// static
void PDFiumFormFiller::Form_Invalidate(FPDF_FORMFILLINFO* param,
                                       FPDF_PAGE page,
                                       double left,
                                       double top,
                                       double right,
                                       double bottom) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  int page_index = engine->GetVisiblePageIndex(page);
  if (page_index == -1) {
    // This can sometime happen when the page is closed because it went off
    // screen, and PDFium invalidates the control as it's being deleted.
    return;
  }

  gfx::Rect rect = engine->pages_[page_index]->PageToScreen(
      engine->GetVisibleRect().origin(), engine->current_zoom_, left, top,
      right, bottom, engine->layout_.options().default_page_orientation());
  engine->client_->Invalidate(rect);
}

// static
void PDFiumFormFiller::Form_OutputSelectedRect(FPDF_FORMFILLINFO* param,
                                               FPDF_PAGE page,
                                               double left,
                                               double top,
                                               double right,
                                               double bottom) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  int page_index = engine->GetVisiblePageIndex(page);
  if (page_index == -1)
    return;

  gfx::Rect rect = engine->pages_[page_index]->PageToScreen(
      engine->GetVisibleRect().origin(), engine->current_zoom_, left, top,
      right, bottom, engine->layout_.options().default_page_orientation());
  if (rect.IsEmpty())
    return;

  engine->form_highlights_.push_back(rect);
}

// static
void PDFiumFormFiller::Form_SetCursor(FPDF_FORMFILLINFO* param,
                                      int cursor_type) {
  // We don't need this since it's not enough to change the cursor in all
  // scenarios.  Instead, we check which form field we're under in OnMouseMove.
}

// static
int PDFiumFormFiller::Form_SetTimer(FPDF_FORMFILLINFO* param,
                                    int elapse,
                                    TimerCallback timer_func) {
  auto* form_filler = static_cast<PDFiumFormFiller*>(param);
  return form_filler->SetTimer(base::Milliseconds(elapse), timer_func);
}

// static
void PDFiumFormFiller::Form_KillTimer(FPDF_FORMFILLINFO* param, int timer_id) {
  auto* form_filler = static_cast<PDFiumFormFiller*>(param);
  form_filler->KillTimer(timer_id);
}

// static
FPDF_SYSTEMTIME PDFiumFormFiller::Form_GetLocalTime(FPDF_FORMFILLINFO* param) {
  base::Time time = base::Time::Now();
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return FPDF_SYSTEMTIME{
      .wYear = static_cast<unsigned short>(exploded.year),
      .wMonth = static_cast<unsigned short>(exploded.month),
      .wDayOfWeek = static_cast<unsigned short>(exploded.day_of_week),
      .wDay = static_cast<unsigned short>(exploded.day_of_month),
      .wHour = static_cast<unsigned short>(exploded.hour),
      .wMinute = static_cast<unsigned short>(exploded.minute),
      .wSecond = static_cast<unsigned short>(exploded.second),
      .wMilliseconds = static_cast<unsigned short>(exploded.millisecond)};
}

// static
void PDFiumFormFiller::Form_OnChange(FPDF_FORMFILLINFO* param) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->EnteredEditMode();
}

// static
FPDF_PAGE PDFiumFormFiller::Form_GetPage(FPDF_FORMFILLINFO* param,
                                         FPDF_DOCUMENT document,
                                         int page_index) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  if (!engine->PageIndexInBounds(page_index))
    return nullptr;
  return engine->pages_[page_index]->GetPage();
}

// static
FPDF_PAGE PDFiumFormFiller::Form_GetCurrentPage(FPDF_FORMFILLINFO* param,
                                                FPDF_DOCUMENT document) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  int index = engine->last_focused_page_;
  if (index == -1) {
    index = engine->GetMostVisiblePage();
    if (index == -1)
      return nullptr;
  }

  DCHECK_NE(index, -1);
  return engine->pages_[index]->GetPage();
}

// static
int PDFiumFormFiller::Form_GetRotation(FPDF_FORMFILLINFO* param,
                                       FPDF_PAGE page) {
  return 0;
}

// static
void PDFiumFormFiller::Form_ExecuteNamedAction(FPDF_FORMFILLINFO* param,
                                               FPDF_BYTESTRING named_action) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  std::string action(named_action);
  if (action == "Print") {
    engine->client_->Print();
    return;
  }

  int index = engine->last_focused_page_;
  /* Don't try to calculate the most visible page if we don't have a left click
     before this event (this code originally copied Form_GetCurrentPage which of
     course needs to do that and which doesn't have recursion). This can end up
     causing infinite recursion. See http://crbug.com/240413 for more
     information. Either way, it's not necessary for the spec'd list of named
     actions.
  if (index == -1)
    index = engine->GetMostVisiblePage();
  */
  if (index == -1)
    return;

  // This is the only list of named actions per the spec (see 12.6.4.11). Adobe
  // Reader supports more, like FitWidth, but since they're not part of the spec
  // and we haven't got bugs about them, no need to now.
  if (action == "NextPage") {
    engine->ScrollToPage(index + 1);
  } else if (action == "PrevPage") {
    engine->ScrollToPage(index - 1);
  } else if (action == "FirstPage") {
    engine->ScrollToPage(0);
  } else if (action == "LastPage") {
    engine->ScrollToPage(engine->pages_.size() - 1);
  }
}

// static
void PDFiumFormFiller::Form_SetTextFieldFocus(FPDF_FORMFILLINFO* param,
                                              FPDF_WIDESTRING value,
                                              FPDF_DWORD valueLen,
                                              FPDF_BOOL is_focus) {
  // Do nothing for now.
  // TODO(gene): use this signal to trigger OSK.
}

// static
void PDFiumFormFiller::Form_OnFocusChange(FPDF_FORMFILLINFO* param,
                                          FPDF_ANNOTATION annot,
                                          int page_index) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  if (!engine->PageIndexInBounds(page_index))
    return;

  base::AutoReset<bool> defer_page_unload_guard(&engine->defer_page_unload_,
                                                true);

  // Maintain viewport if we are updating focus. This is to ensure that we don't
  // scroll the focused annotation into view when focus is regained.
  if (!engine->updating_focus_)
    engine->ScrollAnnotationIntoView(annot, page_index);

  engine->OnFocusedAnnotationUpdated(annot, page_index);
}

// static
void PDFiumFormFiller::Form_DoURIAction(FPDF_FORMFILLINFO* param,
                                        FPDF_BYTESTRING uri) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->NavigateTo(std::string(uri),
                              WindowOpenDisposition::CURRENT_TAB);
}

// static
void PDFiumFormFiller::Form_DoGoToAction(FPDF_FORMFILLINFO* param,
                                         int page_index,
                                         int zoom_mode,
                                         float* position_array,
                                         int size_of_array) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->ScrollToPage(page_index);
}

// static
void PDFiumFormFiller::Form_DoURIActionWithKeyboardModifier(
    FPDF_FORMFILLINFO* param,
    FPDF_BYTESTRING uri,
    int modifiers) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  bool middle_button =
      !!(modifiers & blink::WebInputEvent::Modifiers::kMiddleButtonDown);
  bool alt_key = !!(modifiers & blink::WebInputEvent::Modifiers::kAltKey);
  bool ctrl_key = !!(modifiers & blink::WebInputEvent::Modifiers::kControlKey);
  bool meta_key = !!(modifiers & blink::WebInputEvent::Modifiers::kMetaKey);
  bool shift_key = !!(modifiers & blink::WebInputEvent::Modifiers::kShiftKey);

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      middle_button, alt_key, ctrl_key, meta_key, shift_key);

  engine->client_->NavigateTo(std::string(uri), disposition);
}

#if defined(PDF_ENABLE_V8)
#if defined(PDF_ENABLE_XFA)

// static
void PDFiumFormFiller::Form_EmailTo(FPDF_FORMFILLINFO* param,
                                    FPDF_FILEHANDLER* file_handler,
                                    FPDF_WIDESTRING to,
                                    FPDF_WIDESTRING subject,
                                    FPDF_WIDESTRING cc,
                                    FPDF_WIDESTRING bcc,
                                    FPDF_WIDESTRING message) {
  std::string to_str = WideStringToString(to);
  std::string subject_str = WideStringToString(subject);
  std::string cc_str = WideStringToString(cc);
  std::string bcc_str = WideStringToString(bcc);
  std::string message_str = WideStringToString(message);

  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->Email(to_str, cc_str, bcc_str, subject_str, message_str);
}

// static
void PDFiumFormFiller::Form_DisplayCaret(FPDF_FORMFILLINFO* param,
                                         FPDF_PAGE page,
                                         FPDF_BOOL visible,
                                         double left,
                                         double top,
                                         double right,
                                         double bottom) {}

// static
void PDFiumFormFiller::Form_SetCurrentPage(FPDF_FORMFILLINFO* param,
                                           FPDF_DOCUMENT document,
                                           int page) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->ScrollToPage(page);
}

// static
int PDFiumFormFiller::Form_GetCurrentPageIndex(FPDF_FORMFILLINFO* param,
                                               FPDF_DOCUMENT document) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  return engine->GetMostVisiblePage();
}

// static
void PDFiumFormFiller::Form_GetPageViewRect(FPDF_FORMFILLINFO* param,
                                            FPDF_PAGE page,
                                            double* left,
                                            double* top,
                                            double* right,
                                            double* bottom) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  int page_index = engine->GetVisiblePageIndex(page);
  if (!engine->PageIndexInBounds(page_index)) {
    *left = 0;
    *right = 0;
    *top = 0;
    *bottom = 0;
    return;
  }

  gfx::Rect page_view_rect = engine->GetPageContentsRect(page_index);

  float page_width = FPDF_GetPageWidth(page);
  float page_height = FPDF_GetPageHeight(page);

  // To convert from a screen scale to a page scale, we multiply by
  // (page_height / page_view_rect.height()) and
  // (page_width / page_view_rect.width()),
  // The base point of the page in screen coords is (page_view_rect.x(),
  // page_view_rect.y()).
  // Therefore, to convert an x position from screen to page
  // coords, we use (page_width * (x - base_x) / page_view_rect.width()).
  // For y positions, (page_height * (y - base_y) / page_view_rect.height()).

  // The top-most x position that is visible on the screen is the top of the
  // plugin area, which is y = 0.
  float screen_top_in_page_coords =
      page_height * (0 - page_view_rect.y()) / page_view_rect.height();
  // The bottom-most y position that is visible on the screen is the bottom of
  // the plugin area, which is y = engine->plugin_size().height().
  float screen_bottom_in_page_coords =
      page_height * (engine->plugin_size().height() - page_view_rect.y()) /
      page_view_rect.height();
  // The left-most x position that is visible on the screen is the left of the
  // plugin area, which is x = 0.
  float screen_left_in_page_coords =
      page_width * (0 - page_view_rect.x()) / page_view_rect.width();
  // The right-most x position that is visible on the screen is the right of the
  // plugin area, which is x = engine->plugin_size().width().
  float screen_right_in_page_coords =
      page_width * (engine->plugin_size().width() - page_view_rect.x()) /
      page_view_rect.width();

  // Return the edge of the screen or of the page, since we're restricted to
  // both.
  *left = std::max(screen_left_in_page_coords, 0.0f);
  *right = std::min(screen_right_in_page_coords, page_width);
  *top = std::max(screen_top_in_page_coords, 0.0f);
  *bottom = std::min(screen_bottom_in_page_coords, page_height);
}

// static
int PDFiumFormFiller::Form_GetPlatform(FPDF_FORMFILLINFO* param,
                                       void* platform,
                                       int length) {
  int platform_flag = -1;

#if defined(WIN32)
  platform_flag = 0;
#elif defined(__linux__)
  platform_flag = 1;
#else
  platform_flag = 2;
#endif

  std::string javascript =
      "alert(\"Platform:" + base::NumberToString(platform_flag) + "\")";

  return platform_flag;
}

// static
void PDFiumFormFiller::Form_PageEvent(FPDF_FORMFILLINFO* param,
                                      int page_count,
                                      unsigned long event_type) {
  DCHECK(page_count != 0);
  DCHECK(event_type == FXFA_PAGEVIEWEVENT_POSTADDED ||
         event_type == FXFA_PAGEVIEWEVENT_POSTREMOVED);

  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->UpdatePageCount();
}

// static
FPDF_BOOL PDFiumFormFiller::Form_PopupMenu(FPDF_FORMFILLINFO* param,
                                           FPDF_PAGE page,
                                           FPDF_WIDGET widget,
                                           int menu_flag,
                                           float x,
                                           float y) {
  return false;
}

// static
FPDF_BOOL PDFiumFormFiller::Form_PostRequestURL(FPDF_FORMFILLINFO* param,
                                                FPDF_WIDESTRING url,
                                                FPDF_WIDESTRING data,
                                                FPDF_WIDESTRING content_type,
                                                FPDF_WIDESTRING encode,
                                                FPDF_WIDESTRING header,
                                                FPDF_BSTR* response) {
  // NOTE: Think hard about the privacy implications before allowing
  // a PDF file to perform this action, as it might be used for beaconing.
  return true;
}

// static
FPDF_BOOL PDFiumFormFiller::Form_PutRequestURL(FPDF_FORMFILLINFO* param,
                                               FPDF_WIDESTRING url,
                                               FPDF_WIDESTRING data,
                                               FPDF_WIDESTRING encode) {
  // NOTE: Think hard about the privacy implications before allowing
  // a PDF file to perform this action, as it might be used for beaconing.
  return true;
}

// static
void PDFiumFormFiller::Form_UploadTo(FPDF_FORMFILLINFO* param,
                                     FPDF_FILEHANDLER* file_handle,
                                     int file_flag,
                                     FPDF_WIDESTRING to) {
  // NOTE: Think hard about the privacy implications before allowing
  // a PDF file to perform this action, as it might be used for beaconing.
}

// static
FPDF_FILEHANDLER* PDFiumFormFiller::Form_DownloadFromURL(
    FPDF_FORMFILLINFO* param,
    FPDF_WIDESTRING url) {
  // NOTE: Think hard about the security implications before allowing
  // a PDF file to perform this action. Also think hard about the privacy
  // implications, as it might be used for beaconing.
  return nullptr;
}

// static
FPDF_FILEHANDLER* PDFiumFormFiller::Form_OpenFile(FPDF_FORMFILLINFO* param,
                                                  int file_flag,
                                                  FPDF_WIDESTRING url,
                                                  const char* mode) {
  // NOTE: Think hard about the security implications before allowing
  // a PDF file to perform this action.
  return nullptr;
}

// static
void PDFiumFormFiller::Form_GotoURL(FPDF_FORMFILLINFO* param,
                                    FPDF_DOCUMENT document,
                                    FPDF_WIDESTRING url) {
  // NOTE: Think hard about the security implications before allowing
  // a PDF file to perform this action. Also think hard about the privacy
  // implications, as it might be used for beaconing.
}

// static
int PDFiumFormFiller::Form_GetLanguage(FPDF_FORMFILLINFO* param,
                                       void* language,
                                       int length) {
  return 0;
}

#endif  // defined(PDF_ENABLE_XFA)

// static
int PDFiumFormFiller::Form_Alert(IPDF_JSPLATFORM* param,
                                 FPDF_WIDESTRING message,
                                 FPDF_WIDESTRING title,
                                 int type,
                                 int icon) {
  // See fpdfformfill.h for these values.
  enum AlertType {
    ALERT_TYPE_OK = 0,
    ALERT_TYPE_OK_CANCEL,
    ALERT_TYPE_YES_ON,
    ALERT_TYPE_YES_NO_CANCEL
  };

  enum AlertResult {
    ALERT_RESULT_OK = 1,
    ALERT_RESULT_CANCEL,
    ALERT_RESULT_NO,
    ALERT_RESULT_YES
  };

  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  std::string message_str = WideStringToString(message);
  if (type == ALERT_TYPE_OK) {
    engine->client_->Alert(message_str);
    return ALERT_RESULT_OK;
  }

  bool rv = engine->client_->Confirm(message_str);
  if (type == ALERT_TYPE_OK_CANCEL)
    return rv ? ALERT_RESULT_OK : ALERT_RESULT_CANCEL;
  return rv ? ALERT_RESULT_YES : ALERT_RESULT_NO;
}

// static
void PDFiumFormFiller::Form_Beep(IPDF_JSPLATFORM* param, int type) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->Beep();
}

// static
int PDFiumFormFiller::Form_Response(IPDF_JSPLATFORM* param,
                                    FPDF_WIDESTRING question,
                                    FPDF_WIDESTRING title,
                                    FPDF_WIDESTRING default_response,
                                    FPDF_WIDESTRING label,
                                    FPDF_BOOL password,
                                    void* response,
                                    int length) {
  std::string question_str = WideStringToString(question);
  std::string default_str = WideStringToString(default_response);

  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  std::string rv = engine->client_->Prompt(question_str, default_str);
  std::u16string rv_16 = base::UTF8ToUTF16(rv);
  int rv_bytes = rv_16.size() * sizeof(char16_t);
  if (response) {
    int bytes_to_copy = rv_bytes < length ? rv_bytes : length;
    memcpy(response, rv_16.c_str(), bytes_to_copy);
  }
  return rv_bytes;
}

// static
int PDFiumFormFiller::Form_GetFilePath(IPDF_JSPLATFORM* param,
                                       void* file_path,
                                       int length) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  std::string rv = engine->client_->GetURL();

  // Account for the trailing null.
  int necessary_length = rv.size() + 1;
  if (file_path && necessary_length <= length)
    memcpy(file_path, rv.c_str(), necessary_length);
  return necessary_length;
}

// static
void PDFiumFormFiller::Form_Mail(IPDF_JSPLATFORM* param,
                                 void* mail_data,
                                 int length,
                                 FPDF_BOOL ui,
                                 FPDF_WIDESTRING to,
                                 FPDF_WIDESTRING subject,
                                 FPDF_WIDESTRING cc,
                                 FPDF_WIDESTRING bcc,
                                 FPDF_WIDESTRING message) {
  // Note: `mail_data` and `length` are ignored. We don't handle attachments;
  // there is no way with mailto.
  std::string to_str = WideStringToString(to);
  std::string cc_str = WideStringToString(cc);
  std::string bcc_str = WideStringToString(bcc);
  std::string subject_str = WideStringToString(subject);
  std::string message_str = WideStringToString(message);

  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->Email(to_str, cc_str, bcc_str, subject_str, message_str);
}

// static
void PDFiumFormFiller::Form_Print(IPDF_JSPLATFORM* param,
                                  FPDF_BOOL ui,
                                  int start,
                                  int end,
                                  FPDF_BOOL silent,
                                  FPDF_BOOL shrink_to_fit,
                                  FPDF_BOOL print_as_image,
                                  FPDF_BOOL reverse,
                                  FPDF_BOOL annotations) {
  // No way to pass the extra information to the print dialog using JavaScript.
  // Just opening it is fine for now.
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->Print();
}

// static
void PDFiumFormFiller::Form_SubmitForm(IPDF_JSPLATFORM* param,
                                       void* form_data,
                                       int length,
                                       FPDF_WIDESTRING url) {
  std::string url_str = WideStringToString(url);
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->client_->SubmitForm(url_str, form_data, length);
}

// static
void PDFiumFormFiller::Form_GotoPage(IPDF_JSPLATFORM* param, int page_number) {
  EngineInIsolateScope engine_scope = GetEngineInIsolateScope(param);
  PDFiumEngine* engine = engine_scope.engine();
  engine->ScrollToPage(page_number);
}

#endif  // defined(PDF_ENABLE_V8)

PDFiumFormFiller::EngineInIsolateScope::EngineInIsolateScope(
    PDFiumEngine* engine,
    v8::Isolate* isolate)
    : isolate_scope_(isolate ? std::make_unique<v8::Isolate::Scope>(isolate)
                             : nullptr),
      engine_(engine) {
  DCHECK(engine_);
}

PDFiumFormFiller::EngineInIsolateScope::EngineInIsolateScope(
    EngineInIsolateScope&&) noexcept = default;

PDFiumFormFiller::EngineInIsolateScope&
PDFiumFormFiller::EngineInIsolateScope::operator=(
    EngineInIsolateScope&&) noexcept = default;

PDFiumFormFiller::EngineInIsolateScope::~EngineInIsolateScope() = default;

PDFiumFormFiller::EngineInIsolateScopeFactory::EngineInIsolateScopeFactory(
    PDFiumEngine* engine,
    ScriptOption script_option)
    : engine_(engine),
      callback_isolate_(script_option !=
                                PDFiumFormFiller::ScriptOption::kNoJavaScript
                            ? v8::Isolate::TryGetCurrent()
                            : nullptr) {
  if (callback_isolate_) {
    CHECK_EQ(engine_->client_->GetIsolate(), callback_isolate_);
  }
}

PDFiumFormFiller::EngineInIsolateScopeFactory::~EngineInIsolateScopeFactory() =
    default;

PDFiumFormFiller::EngineInIsolateScope
PDFiumFormFiller::EngineInIsolateScopeFactory::GetEngineInIsolateScope() const {
  return EngineInIsolateScope(engine_, callback_isolate_);
}

// static
PDFiumFormFiller::EngineInIsolateScope
PDFiumFormFiller::GetEngineInIsolateScope(FPDF_FORMFILLINFO* info) {
  auto* form_filler = static_cast<PDFiumFormFiller*>(info);
  return form_filler->engine_in_isolate_scope_factory_
      .GetEngineInIsolateScope();
}

// static
PDFiumFormFiller::EngineInIsolateScope
PDFiumFormFiller::GetEngineInIsolateScope(IPDF_JSPLATFORM* platform) {
  auto* form_filler = static_cast<PDFiumFormFiller*>(platform);
  return form_filler->engine_in_isolate_scope_factory_
      .GetEngineInIsolateScope();
}

int PDFiumFormFiller::SetTimer(const base::TimeDelta& delay,
                               TimerCallback timer_func) {
  const int timer_id = ++g_last_timer_id;
  DCHECK(!base::Contains(timers_, timer_id));

  auto timer = std::make_unique<base::RepeatingTimer>();
  timer->Start(FROM_HERE, delay, base::BindRepeating(timer_func, timer_id));
  timers_[timer_id] = std::move(timer);
  return timer_id;
}

void PDFiumFormFiller::KillTimer(int timer_id) {
  size_t erased = timers_.erase(timer_id);
  DCHECK_EQ(1u, erased);
}

}  // namespace chrome_pdf
