// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/v8_platform.h"
#include "pdf/document_loader_impl.h"
#include "pdf/draw_utils/coordinates.h"
#include "pdf/draw_utils/shadow.h"
#include "pdf/pdf_features.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_document.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_read.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_permissions.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/ppapi_migration/geometry_conversions.h"
#include "pdf/ppapi_migration/input_event_conversions.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/url_loader_wrapper_impl.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/var_dictionary.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_attachment.h"
#include "third_party/pdfium/public/fpdf_catalog.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdf_fwlevent.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "v8/include/v8.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "pdf/pdfium/pdfium_font_linux.h"
#endif

using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using printing::kPixelsPerInch;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

constexpr int32_t kHighlightColorR = 153;
constexpr int32_t kHighlightColorG = 193;
constexpr int32_t kHighlightColorB = 218;

constexpr uint32_t kPendingPageColor = 0xFFEEEEEE;

constexpr uint32_t kFormHighlightColor = 0xFFE4DD;
constexpr int32_t kFormHighlightAlpha = 100;

constexpr int kMaxPasswordTries = 3;

constexpr base::TimeDelta kTouchLongPressTimeout =
    base::TimeDelta::FromMilliseconds(300);

// Windows has native panning capabilities. No need to use our own.
#if defined(OS_WIN)
constexpr bool kViewerImplementedPanning = false;
#else
constexpr bool kViewerImplementedPanning = true;
#endif

constexpr int32_t kLoadingTextVerticalOffset = 50;

// The maximum amount of time we'll spend doing a paint before we give back
// control of the thread.
constexpr base::TimeDelta kMaxProgressivePaintTime =
    base::TimeDelta::FromMilliseconds(300);

// The maximum amount of time we'll spend doing the first paint. This is less
// than the above to keep things smooth if the user is scrolling quickly. This
// is set to 250 ms to give enough time for most PDFs to render, while avoiding
// adding too much latency to the display of the final image when the user
// stops scrolling.
// Setting a higher value has minimal benefit (scrolling at less than 4 fps will
// never be a great experience) and there is some cost, since when the user
// stops scrolling the in-progress painting has to complete or timeout before
// the final painting can start.
// The scrollbar will always be responsive since it is managed by a separate
// process.
constexpr base::TimeDelta kMaxInitialProgressivePaintTime =
    base::TimeDelta::FromMilliseconds(250);

template <class S>
bool IsAboveOrDirectlyLeftOf(const S& lhs, const S& rhs) {
  return lhs.y() < rhs.y() || (lhs.y() == rhs.y() && lhs.x() < rhs.x());
}

int CalculateCenterForZoom(int center, int length, double zoom) {
  int adjusted_center =
      static_cast<int>(center * zoom) - static_cast<int>(length * zoom / 2);
  return std::max(adjusted_center, 0);
}

// This formats a string with special 0xfffe end-of-line hyphens the same way
// as Adobe Reader. When a hyphen is encountered, the next non-CR/LF whitespace
// becomes CR+LF and the hyphen is erased. If there is no whitespace between
// two hyphens, the latter hyphen is erased and ignored.
void FormatStringWithHyphens(base::string16* text) {
  // First pass marks all the hyphen positions.
  struct HyphenPosition {
    HyphenPosition() : position(0), next_whitespace_position(0) {}
    size_t position;
    size_t next_whitespace_position;  // 0 for none
  };
  std::vector<HyphenPosition> hyphen_positions;
  HyphenPosition current_hyphen_position;
  bool current_hyphen_position_is_valid = false;
  constexpr base::char16 kPdfiumHyphenEOL = 0xfffe;

  for (size_t i = 0; i < text->size(); ++i) {
    const base::char16& current_char = (*text)[i];
    if (current_char == kPdfiumHyphenEOL) {
      if (current_hyphen_position_is_valid)
        hyphen_positions.push_back(current_hyphen_position);
      current_hyphen_position = HyphenPosition();
      current_hyphen_position.position = i;
      current_hyphen_position_is_valid = true;
    } else if (base::IsUnicodeWhitespace(current_char)) {
      if (current_hyphen_position_is_valid) {
        if (current_char != L'\r' && current_char != L'\n')
          current_hyphen_position.next_whitespace_position = i;
        hyphen_positions.push_back(current_hyphen_position);
        current_hyphen_position_is_valid = false;
      }
    }
  }
  if (current_hyphen_position_is_valid)
    hyphen_positions.push_back(current_hyphen_position);

  // With all the hyphen positions, do the search and replace.
  while (!hyphen_positions.empty()) {
    static constexpr base::char16 kCr[] = {L'\r', L'\0'};
    const HyphenPosition& position = hyphen_positions.back();
    if (position.next_whitespace_position != 0) {
      (*text)[position.next_whitespace_position] = L'\n';
      text->insert(position.next_whitespace_position, kCr);
    }
    text->erase(position.position, 1);
    hyphen_positions.pop_back();
  }

  // Adobe Reader also get rid of trailing spaces right before a CRLF.
  static constexpr base::char16 kSpaceCrCn[] = {L' ', L'\r', L'\n', L'\0'};
  static constexpr base::char16 kCrCn[] = {L'\r', L'\n', L'\0'};
  base::ReplaceSubstringsAfterOffset(text, 0, kSpaceCrCn, kCrCn);
}

// Replace CR/LF with just LF on POSIX.
void FormatStringForOS(base::string16* text) {
#if defined(OS_POSIX)
  static constexpr base::char16 kCr[] = {L'\r', L'\0'};
  static constexpr base::char16 kBlank[] = {L'\0'};
  base::ReplaceChars(*text, kCr, kBlank, text);
#elif defined(OS_WIN)
  // Do nothing
#else
  NOTIMPLEMENTED();
#endif
}

// Returns true if |cur| is a character to break on.
// For double clicks, look for work breaks.
// For triple clicks, look for line breaks.
// The actual algorithm used in Blink is much more complicated, so do a simple
// approximation.
bool FindMultipleClickBoundary(bool is_double_click, base::char16 cur) {
  if (!is_double_click)
    return cur == '\n';

  // Deal with ASCII characters.
  if (base::IsAsciiAlpha(cur) || base::IsAsciiDigit(cur) || cur == '_')
    return false;
  if (cur < 128)
    return true;

  if (cur == kZeroWidthSpace)
    return true;

  return false;
}

#if defined(PDF_ENABLE_V8)
gin::IsolateHolder* g_isolate_holder = nullptr;

bool IsV8Initialized() {
  return !!g_isolate_holder;
}

void SetUpV8() {
  const char* recommended = FPDF_GetRecommendedV8Flags();
  v8::V8::SetFlagsFromString(recommended, strlen(recommended));
  gin::IsolateHolder::Initialize(
      gin::IsolateHolder::kNonStrictMode,
      static_cast<v8::ArrayBuffer::Allocator*>(
          FPDF_GetArrayBufferAllocatorSharedInstance()));
  DCHECK(!g_isolate_holder);
  g_isolate_holder = new gin::IsolateHolder(
      base::ThreadTaskRunnerHandle::Get(), gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility);
  g_isolate_holder->isolate()->Enter();
}

void TearDownV8() {
  g_isolate_holder->isolate()->Exit();
  delete g_isolate_holder;
  g_isolate_holder = nullptr;
}
#endif  // defined(PDF_ENABLE_V8)

// Returns true if the given |area| and |form_type| combination from
// PDFiumEngine::GetCharIndex() indicates it is a form text area.
bool IsFormTextArea(PDFiumPage::Area area, int form_type) {
  if (form_type == FPDF_FORMFIELD_UNKNOWN)
    return false;

  DCHECK_EQ(area, PDFiumPage::FormTypeToArea(form_type));
  return area == PDFiumPage::FORM_TEXT_AREA;
}

// Checks whether or not focus is in an editable form text area given the
// form field annotation flags and form type.
bool CheckIfEditableFormTextArea(int flags, int form_type) {
  if (!!(flags & FPDF_FORMFLAG_READONLY))
    return false;
  if (form_type == FPDF_FORMFIELD_TEXTFIELD)
    return true;
  if (form_type == FPDF_FORMFIELD_COMBOBOX &&
      (!!(flags & FPDF_FORMFLAG_CHOICE_EDIT))) {
    return true;
  }
  return false;
}

bool IsLinkArea(PDFiumPage::Area area) {
  return area == PDFiumPage::WEBLINK_AREA || area == PDFiumPage::DOCLINK_AREA;
}

// Normalize a MouseInputEvent. For Mac, this means transforming ctrl + left
// button down events into a right button down events.
MouseInputEvent NormalizeMouseEvent(const MouseInputEvent& event) {
  MouseInputEvent normalized_event = event;
#if defined(OS_MAC)
  uint32_t modifiers = event.GetModifiers();
  if ((event.GetModifiers() & kInputEventModifierControlKey) &&
      event.GetButton() == InputEventMouseButtonType::kLeft &&
      event.GetEventType() == InputEventType::kMouseDown) {
    uint32_t new_modifiers = modifiers & ~kInputEventModifierControlKey;
    normalized_event =
        MouseInputEvent(InputEventType::kMouseDown, event.GetTimeStamp(),
                        new_modifiers, InputEventMouseButtonType::kRight,
                        event.GetPosition(), 1, event.GetMovement());
  }
#endif
  return normalized_event;
}

// These values are intended for the JS to handle, and it doesn't have access
// to the PDFDEST_VIEW_* defines.
std::string ConvertViewIntToViewString(unsigned long view_int) {
  switch (view_int) {
    case PDFDEST_VIEW_XYZ:
      return "XYZ";
    case PDFDEST_VIEW_FIT:
      return "Fit";
    case PDFDEST_VIEW_FITH:
      return "FitH";
    case PDFDEST_VIEW_FITV:
      return "FitV";
    case PDFDEST_VIEW_FITR:
      return "FitR";
    case PDFDEST_VIEW_FITB:
      return "FitB";
    case PDFDEST_VIEW_FITBH:
      return "FitBH";
    case PDFDEST_VIEW_FITBV:
      return "FitBV";
    case PDFDEST_VIEW_UNKNOWN_MODE:
      return "";
    default:
      NOTREACHED();
      return "";
  }
}

// Simplify to \" for searching
constexpr wchar_t kHebrewPunctuationGershayimCharacter = 0x05F4;
constexpr wchar_t kLeftDoubleQuotationMarkCharacter = 0x201C;
constexpr wchar_t kRightDoubleQuotationMarkCharacter = 0x201D;

// Simplify \' for searching
constexpr wchar_t kHebrewPunctuationGereshCharacter = 0x05F3;
constexpr wchar_t kLeftSingleQuotationMarkCharacter = 0x2018;
constexpr wchar_t kRightSingleQuotationMarkCharacter = 0x2019;

wchar_t SimplifyForSearch(wchar_t c) {
  switch (c) {
    case kHebrewPunctuationGershayimCharacter:
    case kLeftDoubleQuotationMarkCharacter:
    case kRightDoubleQuotationMarkCharacter:
      return L'\"';
    case kHebrewPunctuationGereshCharacter:
    case kLeftSingleQuotationMarkCharacter:
    case kRightSingleQuotationMarkCharacter:
      return L'\'';
    default:
      return c;
  }
}

PDFiumEngine::SetSelectedTextFunction g_set_selected_text_func_for_testing =
    nullptr;

void SetSelectedText(pp::Instance* instance, const std::string& selected_text) {
  pp::PDF::SetSelectedText(instance, selected_text.c_str());
}

PDFiumEngine::SetLinkUnderCursorFunction
    g_set_link_under_cursor_func_for_testing = nullptr;

void SetLinkUnderCursor(pp::Instance* instance,
                        const std::string& link_under_cursor) {
  pp::PDF::SetLinkUnderCursor(instance, link_under_cursor.c_str());
}

PP_PrivateFocusObjectType GetAnnotationFocusType(
    FPDF_ANNOTATION_SUBTYPE annot_type) {
  switch (annot_type) {
    case FPDF_ANNOT_LINK:
      return PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_LINK;
    case FPDF_ANNOT_HIGHLIGHT:
      return PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_HIGHLIGHT;
    case FPDF_ANNOT_WIDGET:
      return PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_TEXT_FIELD;
    default:
      return PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_NONE;
  }
}

base::string16 GetAttachmentAttribute(FPDF_ATTACHMENT attachment,
                                      FPDF_BYTESTRING field) {
  return CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFAttachment_GetStringValue, attachment, field),
      /*check_expected_size=*/true);
}

base::string16 GetAttachmentName(FPDF_ATTACHMENT attachment) {
  return CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFAttachment_GetName, attachment),
      /*check_expected_size=*/true);
}

}  // namespace

void InitializeSDK(bool enable_v8) {
  FPDF_LIBRARY_CONFIG config;
  config.version = 3;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = nullptr;
  config.m_pPlatform = nullptr;
  config.m_v8EmbedderSlot = gin::kEmbedderPDFium;

#if defined(PDF_ENABLE_V8)
  if (enable_v8) {
    SetUpV8();
    config.m_pIsolate = v8::Isolate::GetCurrent();
    // NOTE: static_cast<> prior to assigning to (void*) is safer since it
    // will manipulate the pointer value should gin::V8Platform someday have
    // multiple base classes.
    config.m_pPlatform = static_cast<v8::Platform*>(gin::V8Platform::Get());
  }
#endif  // defined(PDF_ENABLE_V8)

  FPDF_InitLibraryWithConfig(&config);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  InitializeLinuxFontMapper();
#endif

  InitializeUnsupportedFeaturesHandler();
}

void ShutdownSDK() {
  FPDF_DestroyLibrary();
#if defined(PDF_ENABLE_V8)
  if (IsV8Initialized())
    TearDownV8();
#endif  // defined(PDF_ENABLE_V8)
}

PDFEngine::AccessibilityLinkInfo::AccessibilityLinkInfo() = default;

PDFEngine::AccessibilityLinkInfo::AccessibilityLinkInfo(
    const AccessibilityLinkInfo& that) = default;

PDFEngine::AccessibilityLinkInfo::~AccessibilityLinkInfo() = default;

PDFEngine::AccessibilityImageInfo::AccessibilityImageInfo() = default;

PDFEngine::AccessibilityImageInfo::AccessibilityImageInfo(
    const AccessibilityImageInfo& that) = default;

PDFEngine::AccessibilityImageInfo::~AccessibilityImageInfo() = default;

PDFEngine::AccessibilityHighlightInfo::AccessibilityHighlightInfo() = default;

PDFEngine::AccessibilityHighlightInfo::AccessibilityHighlightInfo(
    const AccessibilityHighlightInfo& that) = default;

PDFEngine::AccessibilityHighlightInfo::~AccessibilityHighlightInfo() = default;

PDFEngine::AccessibilityTextFieldInfo::AccessibilityTextFieldInfo() = default;

PDFEngine::AccessibilityTextFieldInfo::AccessibilityTextFieldInfo(
    const AccessibilityTextFieldInfo& that) = default;

PDFEngine::AccessibilityTextFieldInfo::~AccessibilityTextFieldInfo() = default;

PDFiumEngine::PDFiumEngine(PDFEngine::Client* client,
                           PDFiumFormFiller::ScriptOption script_option)
    : client_(client),
      form_filler_(this, script_option),
      mouse_down_state_(PDFiumPage::NONSELECTABLE_AREA,
                        PDFiumPage::LinkTarget()),
      print_(this) {
#if defined(PDF_ENABLE_V8)
  if (script_option != PDFiumFormFiller::ScriptOption::kNoJavaScript)
    DCHECK(IsV8Initialized());
#endif  // defined(PDF_ENABLE_V8)

  IFSDK_PAUSE::version = 1;
  IFSDK_PAUSE::user = nullptr;
  IFSDK_PAUSE::NeedToPauseNow = Pause_NeedToPauseNow;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // PreviewModeClient does not know its pp::Instance.
  SetLastInstance(client_->GetPluginInstance());
#endif
}

PDFiumEngine::~PDFiumEngine() {
  for (auto& page : pages_)
    page->Unload();

  if (doc())
    FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_WC);
}

void PDFiumEngine::SetDocumentLoaderForTesting(
    std::unique_ptr<DocumentLoader> loader) {
  DCHECK(loader);
  DCHECK(!doc_loader_);
  doc_loader_ = std::move(loader);
  doc_loader_set_for_testing_ = true;
}

// static
void PDFiumEngine::OverrideSetSelectedTextFunctionForTesting(
    SetSelectedTextFunction function) {
  g_set_selected_text_func_for_testing = function;
}

// static
void PDFiumEngine::OverrideSetLinkUnderCursorFunctionForTesting(
    SetLinkUnderCursorFunction function) {
  g_set_link_under_cursor_func_for_testing = function;
}

bool PDFiumEngine::New(const char* url, const char* headers) {
  url_ = url;
  if (headers)
    headers_ = headers;
  else
    headers_.clear();
  return true;
}

void PDFiumEngine::PageOffsetUpdated(const gfx::Vector2d& page_offset) {
  page_offset_ = page_offset;
}

void PDFiumEngine::PluginSizeUpdated(const gfx::Size& size) {
  CancelPaints();

  plugin_size_ = size;
  CalculateVisiblePages();
}

void PDFiumEngine::ScrolledToXPosition(int position) {
  CancelPaints();

  int old_x = position_.x();
  position_.set_x(position);
  CalculateVisiblePages();
  client_->DidScroll(gfx::Vector2d(old_x - position, 0));
  OnSelectionPositionChanged();
}

void PDFiumEngine::ScrolledToYPosition(int position) {
  CancelPaints();

  int old_y = position_.y();
  position_.set_y(position);
  CalculateVisiblePages();
  client_->DidScroll(gfx::Vector2d(0, old_y - position));
  OnSelectionPositionChanged();
}

void PDFiumEngine::PrePaint() {
  for (auto& paint : progressive_paints_)
    paint.set_painted(false);
}

void PDFiumEngine::Paint(const gfx::Rect& rect,
                         SkBitmap& image_data,
                         std::vector<gfx::Rect>& ready,
                         std::vector<gfx::Rect>& pending) {
  gfx::Rect leftover = rect;
  for (size_t i = 0; i < visible_pages_.size(); ++i) {
    int index = visible_pages_[i];
    // Convert the current page's rectangle to screen rectangle.  We do this
    // instead of the reverse (converting the dirty rectangle from screen to
    // page coordinates) because then we'd have to convert back to screen
    // coordinates, and the rounding errors sometime leave pixels dirty or even
    // move the text up or down a pixel when zoomed.
    gfx::Rect page_rect_in_screen = GetPageScreenRect(index);
    gfx::Rect dirty_in_screen =
        gfx::IntersectRects(page_rect_in_screen, leftover);
    if (dirty_in_screen.IsEmpty())
      continue;

    // Compute the leftover dirty region. The first page may have blank space
    // above it, in which case we also need to subtract that space from the
    // dirty region.
    // If two-up view is enabled, we don't need to recompute |leftover| since
    // subtracting |leftover| with a two-up view page won't result in a
    // rectangle.
    if (!layout_.options().two_up_view_enabled()) {
      if (i == 0) {
        gfx::Rect blank_space_in_screen = dirty_in_screen;
        blank_space_in_screen.set_y(0);
        blank_space_in_screen.set_height(dirty_in_screen.y());
        leftover.Subtract(blank_space_in_screen);
      }

      leftover.Subtract(dirty_in_screen);
    }

    if (pages_[index]->available()) {
      int progressive = GetProgressiveIndex(index);
      if (progressive != -1) {
        DCHECK_GE(progressive, 0);
        DCHECK_LT(static_cast<size_t>(progressive), progressive_paints_.size());
        if (progressive_paints_[progressive].rect() != dirty_in_screen) {
          // The PDFium code can only handle one progressive paint at a time, so
          // queue this up. Previously we used to merge the rects when this
          // happened, but it made scrolling up on complex PDFs very slow since
          // there would be a damaged rect at the top (from scroll) and at the
          // bottom (from toolbar).
          pending.push_back(dirty_in_screen);
          continue;
        }
      }

      if (progressive == -1) {
        progressive = StartPaint(index, dirty_in_screen);
        progressive_paint_timeout_ = kMaxInitialProgressivePaintTime;
      } else {
        progressive_paint_timeout_ = kMaxProgressivePaintTime;
      }

      progressive_paints_[progressive].set_painted(true);
      if (ContinuePaint(progressive, image_data)) {
        FinishPaint(progressive, image_data);
        ready.push_back(dirty_in_screen);
      } else {
        pending.push_back(dirty_in_screen);
      }
    } else {
      PaintUnavailablePage(index, dirty_in_screen, image_data);
      ready.push_back(dirty_in_screen);
    }
  }
}

void PDFiumEngine::PostPaint() {
  for (size_t i = 0; i < progressive_paints_.size(); ++i) {
    if (progressive_paints_[i].painted())
      continue;

    // This rectangle must have been merged with another one, that's why we
    // weren't asked to paint it. Remove it or otherwise we'll never finish
    // painting.
    FPDF_RenderPage_Close(
        pages_[progressive_paints_[i].page_index()]->GetPage());
    progressive_paints_.erase(progressive_paints_.begin() + i);
    --i;
  }
}

bool PDFiumEngine::HandleDocumentLoad(std::unique_ptr<UrlLoader> loader) {
  password_tries_remaining_ = kMaxPasswordTries;
  process_when_pending_request_complete_ =
      base::FeatureList::IsEnabled(features::kPdfIncrementalLoading);

  if (!doc_loader_set_for_testing_) {
    auto loader_wrapper =
        std::make_unique<URLLoaderWrapperImpl>(std::move(loader));
    loader_wrapper->SetResponseHeaders(headers_);

    doc_loader_ = std::make_unique<DocumentLoaderImpl>(this);
    if (!doc_loader_->Init(std::move(loader_wrapper), url_))
      return false;
  }
  document_ = std::make_unique<PDFiumDocument>(doc_loader_.get());

  // request initial data.
  doc_loader_->RequestData(0, 1);
  return true;
}

pp::Instance* PDFiumEngine::GetPluginInstance() {
  return client_->GetPluginInstance();
}

std::unique_ptr<URLLoaderWrapper> PDFiumEngine::CreateURLLoader() {
  return std::make_unique<URLLoaderWrapperImpl>(client_->CreateUrlLoader());
}

void PDFiumEngine::AppendPage(PDFEngine* engine, int index) {
  // Unload and delete the blank page before appending.
  pages_[index]->Unload();
  pages_[index]->set_calculated_links(false);
  gfx::Size curr_page_size = GetPageSize(index);
  FPDFPage_Delete(doc(), index);
  FPDF_ImportPages(doc(), static_cast<PDFiumEngine*>(engine)->doc(), "1",
                   index);
  gfx::Size new_page_size = GetPageSize(index);
  if (curr_page_size != new_page_size) {
    DCHECK(document_loaded_);
    LoadPageInfo();
  }
  client_->Invalidate(GetPageScreenRect(index));
}

std::vector<uint8_t> PDFiumEngine::GetSaveData() {
  PDFiumMemBufferFileWrite output_file_write;
  if (!FPDF_SaveAsCopy(doc(), &output_file_write, 0))
    return std::vector<uint8_t>();
  return output_file_write.TakeBuffer();
}

void PDFiumEngine::OnPendingRequestComplete() {
  if (!process_when_pending_request_complete_)
    return;

  if (!fpdf_availability()) {
    document_->file_access().m_FileLen = doc_loader_->GetDocumentSize();
    document_->CreateFPDFAvailability();
    DCHECK(fpdf_availability());
    // Currently engine does not deal efficiently with some non-linearized
    // files.
    // See http://code.google.com/p/chromium/issues/detail?id=59400
    // To improve user experience we download entire file for non-linearized
    // PDF.
    if (FPDFAvail_IsLinearized(fpdf_availability()) != PDF_LINEARIZED) {
      // Wait complete document.
      process_when_pending_request_complete_ = false;
      document_->ResetFPDFAvailability();
      return;
    }
  }

  if (!doc()) {
    LoadDocument();
    return;
  }

  if (pages_.empty()) {
    LoadBody();
    return;
  }

  // LoadDocument() will result in |pending_pages_| being reset so there's no
  // need to run the code below in that case.
  bool update_pages = false;
  std::vector<int> still_pending;
  for (int pending_page : pending_pages_) {
    if (CheckPageAvailable(pending_page, &still_pending)) {
      update_pages = true;
      if (IsPageVisible(pending_page))
        client_->Invalidate(GetPageScreenRect(pending_page));
    }
  }
  pending_pages_.swap(still_pending);
  if (update_pages) {
    DCHECK(!document_loaded_);
    LoadPageInfo();
  }
}

void PDFiumEngine::OnNewDataReceived() {
  client_->DocumentLoadProgress(doc_loader_->BytesReceived(),
                                doc_loader_->GetDocumentSize());
}

void PDFiumEngine::OnDocumentComplete() {
  if (doc())
    return FinishLoadingDocument();

  document_->file_access().m_FileLen = doc_loader_->GetDocumentSize();
  if (!fpdf_availability()) {
    document_->CreateFPDFAvailability();
    DCHECK(fpdf_availability());
  }
  LoadDocument();
}

void PDFiumEngine::OnDocumentCanceled() {
  if (visible_pages_.empty())
    client_->DocumentLoadFailed();
  else
    OnDocumentComplete();
}

void PDFiumEngine::FinishLoadingDocument() {
  DCHECK(doc());
  DCHECK(doc_loader_->IsDocumentComplete());

  LoadBody();

  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  bool need_update = false;
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (pages_[i]->available())
      continue;

    pages_[i]->MarkAvailable();
    // We still need to call IsPageAvail() even if the whole document is
    // already downloaded.
    FPDFAvail_IsPageAvail(fpdf_availability(), i, &download_hints);
    need_update = true;
    if (IsPageVisible(i))
      client_->Invalidate(GetPageScreenRect(i));
  }

  // Transition |document_loaded_| to true after finishing any calls to
  // FPDFAvail_IsPageAvail(), since we no longer need to defer calls to this
  // function from LoadPageInfo(). Note that LoadBody() calls LoadPageInfo()
  // indirectly, so we cannot make this transition earlier.
  document_loaded_ = true;

  if (need_update)
    LoadPageInfo();

  LoadDocumentAttachmentInfoList();

  LoadDocumentMetadata();

  if (called_do_document_action_)
    return;
  called_do_document_action_ = true;

  // These can only be called now, as the JS might end up needing a page.
  FORM_DoDocumentJSAction(form());
  FORM_DoDocumentOpenAction(form());
  if (most_visible_page_ != -1) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form(), FPDFPAGE_AACTION_OPEN);
  }

  if (doc()) {
    DocumentFeatures document_features;
    document_features.page_count = pages_.size();
    document_features.has_attachments = !doc_attachment_info_list_.empty();
    document_features.is_tagged = FPDFCatalog_IsTagged(doc());
    document_features.form_type =
        static_cast<FormType>(FPDF_GetFormType(doc()));
    client_->DocumentLoadComplete(document_features);
  }
}

void PDFiumEngine::UnsupportedFeature(const std::string& feature) {
  client_->DocumentHasUnsupportedFeature(feature);
}

FPDF_AVAIL PDFiumEngine::fpdf_availability() const {
  return document_ ? document_->fpdf_availability() : nullptr;
}

FPDF_DOCUMENT PDFiumEngine::doc() const {
  return document_ ? document_->doc() : nullptr;
}

FPDF_FORMHANDLE PDFiumEngine::form() const {
  return document_ ? document_->form() : nullptr;
}

void PDFiumEngine::ContinueFind(int32_t result) {
  StartFind(current_find_text_, result != 0);
}

bool PDFiumEngine::HandleEvent(const InputEvent& event) {
  DCHECK(!defer_page_unload_);
  defer_page_unload_ = true;
  bool rv = false;
  switch (event.GetEventType()) {
    case InputEventType::kMouseDown:
      rv = OnMouseDown(static_cast<const MouseInputEvent&>(event));
      break;
    case InputEventType::kMouseUp:
      rv = OnMouseUp(static_cast<const MouseInputEvent&>(event));
      break;
    case InputEventType::kMouseMove:
      rv = OnMouseMove(static_cast<const MouseInputEvent&>(event));
      break;
    case InputEventType::kMouseEnter:
      OnMouseEnter(static_cast<const MouseInputEvent&>(event));
      break;
    case InputEventType::kKeyDown:
      rv = OnKeyDown(static_cast<const KeyboardInputEvent&>(event));
      break;
    case InputEventType::kKeyUp:
      rv = OnKeyUp(static_cast<const KeyboardInputEvent&>(event));
      break;
    case InputEventType::kChar:
      rv = OnChar(static_cast<const KeyboardInputEvent&>(event));
      break;
    case InputEventType::kTouchStart: {
      KillTouchTimer();

      const auto& touch_event = static_cast<const TouchInputEvent&>(event);
      if (touch_event.GetTouchCount() == 1)
        ScheduleTouchTimer(touch_event);
      break;
    }
    case InputEventType::kTouchEnd:
      KillTouchTimer();
      break;
    case InputEventType::kTouchMove:
      // TODO(dsinclair): This should allow a little bit of movement (up to the
      // touch radii) to account for finger jiggle.
      KillTouchTimer();
      break;
    default:
      break;
  }

  DCHECK(defer_page_unload_);
  defer_page_unload_ = false;

  // Store the pages to unload away because the act of unloading pages can cause
  // there to be more pages to unload. We leave those extra pages to be unloaded
  // on the next go around.
  std::vector<int> pages_to_unload;
  std::swap(pages_to_unload, deferred_page_unloads_);
  for (int page_index : pages_to_unload)
    pages_[page_index]->Unload();

  return rv;
}

uint32_t PDFiumEngine::QuerySupportedPrintOutputFormats() {
  if (HasPermission(PERMISSION_PRINT_HIGH_QUALITY))
    return PP_PRINTOUTPUTFORMAT_PDF | PP_PRINTOUTPUTFORMAT_RASTER;
  if (HasPermission(PERMISSION_PRINT_LOW_QUALITY))
    return PP_PRINTOUTPUTFORMAT_RASTER;
  return 0;
}

void PDFiumEngine::PrintBegin() {
  FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_WP);
}

pp::Resource PDFiumEngine::PrintPages(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  if (!page_range_count)
    return pp::Resource();

  if ((print_settings.format & PP_PRINTOUTPUTFORMAT_PDF) &&
      HasPermission(PERMISSION_PRINT_HIGH_QUALITY)) {
    return PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                           pdf_print_settings);
  }
  if (HasPermission(PERMISSION_PRINT_LOW_QUALITY)) {
    return PrintPagesAsRasterPdf(page_ranges, page_range_count, print_settings,
                                 pdf_print_settings);
  }
  return pp::Resource();
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsRasterPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  DCHECK(page_range_count);

  // If document is not downloaded yet, disable printing.
  if (doc() && !doc_loader_->IsDocumentComplete())
    return pp::Buffer_Dev();

  KillFormFocus();

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetLastInstance(client_->GetPluginInstance());
#endif

  return ConvertPdfToBufferDev(
      print_.PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                             pdf_print_settings, /*raster=*/true));
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  DCHECK(page_range_count);
  DCHECK(doc());

  KillFormFocus();

  std::vector<uint32_t> page_numbers =
      PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(page_ranges,
                                                          page_range_count);
  for (uint32_t page_number : page_numbers) {
    pages_[page_number]->GetPage();
    if (!IsPageVisible(page_number))
      pages_[page_number]->Unload();
  }

  return ConvertPdfToBufferDev(
      print_.PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                             pdf_print_settings, /*raster=*/false));
}

pp::Buffer_Dev PDFiumEngine::ConvertPdfToBufferDev(
    const std::vector<uint8_t>& pdf_data) {
  pp::Buffer_Dev buffer;
  if (!pdf_data.empty()) {
    buffer = pp::Buffer_Dev(GetPluginInstance(), pdf_data.size());
    if (!buffer.is_null())
      memcpy(buffer.data(), pdf_data.data(), pdf_data.size());
  }
  return buffer;
}

void PDFiumEngine::KillFormFocus() {
  FORM_ForceToKillFocus(form());
  SetInFormTextArea(false);
}

void PDFiumEngine::UpdateFocus(bool has_focus) {
  base::AutoReset<bool> updating_focus_guard(&updating_focus_, true);
  if (has_focus) {
    UpdateFocusItemType(last_focused_item_type_);
    if (focus_item_type_ == FocusElementType::kPage &&
        PageIndexInBounds(last_focused_page_) &&
        last_focused_annot_index_ != -1) {
      ScopedFPDFAnnotation last_focused_annot(FPDFPage_GetAnnot(
          pages_[last_focused_page_]->GetPage(), last_focused_annot_index_));
      if (last_focused_annot) {
        FPDF_BOOL ret = FORM_SetFocusedAnnot(form(), last_focused_annot.get());
        DCHECK(ret);
      }
    }
  } else {
    last_focused_item_type_ = focus_item_type_;
    if (focus_item_type_ == FocusElementType::kDocument) {
      UpdateFocusItemType(FocusElementType::kNone);
    } else if (focus_item_type_ == FocusElementType::kPage) {
      FPDF_ANNOTATION last_focused_annot = nullptr;
      FPDF_BOOL ret = FORM_GetFocusedAnnot(form(), &last_focused_page_,
                                           &last_focused_annot);
      DCHECK(ret);
      if (PageIndexInBounds(last_focused_page_) && last_focused_annot) {
        last_focused_annot_index_ = FPDFPage_GetAnnotIndex(
            pages_[last_focused_page_]->GetPage(), last_focused_annot);
      } else {
        last_focused_annot_index_ = -1;
      }
      FPDFPage_CloseAnnot(last_focused_annot);
    }
    KillFormFocus();
  }
}

PP_PrivateAccessibilityFocusInfo PDFiumEngine::GetFocusInfo() {
  PP_PrivateAccessibilityFocusInfo focus_info = {
      PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_NONE, 0, 0};

  switch (focus_item_type_) {
    case FocusElementType::kNone: {
      break;
    }
    case FocusElementType::kPage: {
      int page_index;
      FPDF_ANNOTATION focused_annot;
      FPDF_BOOL ret = FORM_GetFocusedAnnot(form(), &page_index, &focused_annot);
      DCHECK(ret);

      if (PageIndexInBounds(page_index) && focused_annot) {
        PP_PrivateFocusObjectType type =
            GetAnnotationFocusType(FPDFAnnot_GetSubtype(focused_annot));
        int annot_index = FPDFPage_GetAnnotIndex(pages_[page_index]->GetPage(),
                                                 focused_annot);
        if (type != PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_NONE &&
            annot_index >= 0) {
          focus_info.focused_object_type = type;
          focus_info.focused_object_page_index = page_index;
          focus_info.focused_annotation_index_in_page = annot_index;
        }
      }
      FPDFPage_CloseAnnot(focused_annot);
      break;
    }
    case FocusElementType::kDocument: {
      focus_info.focused_object_type =
          PP_PrivateFocusObjectType::PP_PRIVATEFOCUSOBJECT_DOCUMENT;
      break;
    }
  }
  return focus_info;
}

uint32_t PDFiumEngine::GetLoadedByteSize() {
  return doc_loader_->GetDocumentSize();
}

bool PDFiumEngine::ReadLoadedBytes(uint32_t length, void* buffer) {
  DCHECK_LE(length, GetLoadedByteSize());
  return doc_loader_->GetBlock(0, length, buffer);
}

void PDFiumEngine::SetFormSelectedText(FPDF_FORMHANDLE form_handle,
                                       FPDF_PAGE page) {
  base::string16 selected_form_text16 = CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FORM_GetSelectedText, form_handle, page),
      /*check_expected_size=*/false);

  // If form selected text is empty and there was no previous form text
  // selection, exit early because nothing has changed.
  if (selected_form_text16.empty() && selected_form_text_.empty())
    return;

  // Update previous and current selections, then compare them to check if
  // selection has changed. If so, set plugin text selection.
  std::string selected_form_text = selected_form_text_;
  selected_form_text_ = base::UTF16ToUTF8(selected_form_text16);
  if (selected_form_text != selected_form_text_) {
    DCHECK(in_form_text_area_);
    pp::PDF::SetSelectedText(GetPluginInstance(), selected_form_text_.c_str());
  }
}

void PDFiumEngine::PrintEnd() {
  FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_DP);
}

PDFiumPage::Area PDFiumEngine::GetCharIndex(const gfx::Point& point,
                                            int* page_index,
                                            int* char_index,
                                            int* form_type,
                                            PDFiumPage::LinkTarget* target) {
  int page = -1;
  gfx::Point point_in_page(
      static_cast<int>((point.x() + position_.x()) / current_zoom_),
      static_cast<int>((point.y() + position_.y()) / current_zoom_));
  for (int visible_page : visible_pages_) {
    if (pages_[visible_page]->rect().Contains(point_in_page)) {
      page = visible_page;
      break;
    }
  }
  if (page == -1)
    return PDFiumPage::NONSELECTABLE_AREA;

  // If the page hasn't finished rendering, calling into the page sometimes
  // leads to hangs.
  for (const auto& paint : progressive_paints_) {
    if (paint.page_index() == page)
      return PDFiumPage::NONSELECTABLE_AREA;
  }

  *page_index = page;
  PDFiumPage::Area result = pages_[page]->GetCharIndex(
      point_in_page, layout_.options().default_page_orientation(), char_index,
      form_type, target);
  return (client_->IsPrintPreview() && result == PDFiumPage::WEBLINK_AREA)
             ? PDFiumPage::NONSELECTABLE_AREA
             : result;
}

bool PDFiumEngine::OnMouseDown(const MouseInputEvent& event) {
  MouseInputEvent normalized_event = NormalizeMouseEvent(event);
  if (normalized_event.GetButton() == InputEventMouseButtonType::kLeft)
    return OnLeftMouseDown(normalized_event);
  if (normalized_event.GetButton() == InputEventMouseButtonType::kMiddle)
    return OnMiddleMouseDown(normalized_event);
  if (normalized_event.GetButton() == InputEventMouseButtonType::kRight)
    return OnRightMouseDown(normalized_event);
  return false;
}

void PDFiumEngine::OnSingleClick(int page_index, int char_index) {
  SetSelecting(true);
  selection_.push_back(PDFiumRange(pages_[page_index].get(), char_index, 0));
}

void PDFiumEngine::OnMultipleClick(int click_count,
                                   int page_index,
                                   int char_index) {
  DCHECK_GE(click_count, 2);
  bool is_double_click = click_count == 2;

  // It would be more efficient if the SDK could support finding a space, but
  // now it doesn't.
  int start_index = char_index;
  do {
    base::char16 cur = pages_[page_index]->GetCharAtIndex(start_index);
    if (FindMultipleClickBoundary(is_double_click, cur))
      break;
  } while (--start_index >= 0);
  if (start_index)
    start_index++;

  int end_index = char_index;
  int total = pages_[page_index]->GetCharCount();
  while (end_index++ <= total) {
    base::char16 cur = pages_[page_index]->GetCharAtIndex(end_index);
    if (FindMultipleClickBoundary(is_double_click, cur))
      break;
  }

  selection_.push_back(PDFiumRange(pages_[page_index].get(), start_index,
                                   end_index - start_index));

  if (handling_long_press_)
    client_->NotifyTouchSelectionOccurred();
}

bool PDFiumEngine::OnLeftMouseDown(const MouseInputEvent& event) {
  SetMouseLeftButtonDown(true);

  auto selection_invalidator =
      std::make_unique<SelectionChangeInvalidator>(this);
  selection_.clear();

  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  gfx::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);
  DCHECK_GE(form_type, FPDF_FORMFIELD_UNKNOWN);
  mouse_down_state_.Set(area, target);

  // Decide whether to open link or not based on user action in mouse up and
  // mouse move events.
  if (IsLinkArea(area))
    return true;

  if (page_index != -1) {
    UpdateFocusItemType(FocusElementType::kPage);
    last_focused_page_ = page_index;
    double page_x;
    double page_y;
    DeviceToPage(page_index, point, &page_x, &page_y);

    if (form_type != FPDF_FORMFIELD_UNKNOWN) {
      // FORM_OnLButton*() will trigger a callback to
      // OnFocusedAnnotationUpdated() which will call SetInFormTextArea().
      // Destroy SelectionChangeInvalidator object before SetInFormTextArea()
      // changes plugin's focus to be in form text area. This way, regular text
      // selection can be cleared when a user clicks into a form text area
      // because the pp::PDF::SetSelectedText() call in
      // ~SelectionChangeInvalidator() still goes to the Mimehandler
      // (not the Renderer).
      selection_invalidator.reset();
    }

    FPDF_PAGE page = pages_[page_index]->GetPage();

    if (event.GetClickCount() == 1) {
      FORM_OnLButtonDown(form(), page, event.GetModifiers(), page_x, page_y);
    } else if (event.GetClickCount() == 2) {
      FORM_OnLButtonDoubleClick(form(), page, event.GetModifiers(), page_x,
                                page_y);
    }
    if (form_type != FPDF_FORMFIELD_UNKNOWN)
      return true;  // Return now before we get into the selection code.
  }
  SetInFormTextArea(false);

  if (area != PDFiumPage::TEXT_AREA)
    return true;  // Return true so WebKit doesn't do its own highlighting.

  if (event.GetClickCount() == 1)
    OnSingleClick(page_index, char_index);
  else if (event.GetClickCount() == 2 || event.GetClickCount() == 3)
    OnMultipleClick(event.GetClickCount(), page_index, char_index);

  return true;
}

bool PDFiumEngine::OnMiddleMouseDown(const MouseInputEvent& event) {
  SetMouseLeftButtonDown(false);
  mouse_middle_button_down_ = true;
  mouse_middle_button_last_position_ = event.GetPosition();

  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();

  int unused_page_index = -1;
  int unused_char_index = -1;
  int unused_form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(event.GetPosition(), &unused_page_index, &unused_char_index,
                   &unused_form_type, &target);
  mouse_down_state_.Set(area, target);

  // Decide whether to open link or not based on user action in mouse up and
  // mouse move events.
  if (IsLinkArea(area))
    return true;

  if (kViewerImplementedPanning) {
    // Switch to hand cursor when panning.
    client_->UpdateCursor(PP_CURSORTYPE_HAND);
  }

  // Prevent middle mouse button from selecting texts.
  return false;
}

bool PDFiumEngine::OnRightMouseDown(const MouseInputEvent& event) {
  DCHECK_EQ(InputEventMouseButtonType::kRight, event.GetButton());

  gfx::Point point = event.GetPosition();
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);
  DCHECK_GE(form_type, FPDF_FORMFIELD_UNKNOWN);

  bool is_form_text_area = IsFormTextArea(area, form_type);

  double page_x = -1;
  double page_y = -1;
  FPDF_PAGE page = nullptr;
  if (is_form_text_area) {
    DCHECK_NE(page_index, -1);

    DeviceToPage(page_index, point, &page_x, &page_y);
    page = pages_[page_index]->GetPage();
  }

  // Handle the case when focus starts inside a form text area.
  if (in_form_text_area_) {
    if (is_form_text_area) {
      FORM_OnFocus(form(), page, 0, page_x, page_y);
    } else {
      // Transition out of a form text area.
      FORM_ForceToKillFocus(form());
      SetInFormTextArea(false);
    }
    return true;
  }

  // Handle the case when focus starts outside a form text area and transitions
  // into a form text area.
  if (is_form_text_area) {
    FORM_OnFocus(form(), page, 0, page_x, page_y);
    return true;
  }

  // Handle the case when focus starts outside a form text area and stays
  // outside.
  if (selection_.empty())
    return false;

  std::vector<gfx::Rect> selection_rect_vector =
      GetAllScreenRectsUnion(selection_, GetVisibleRect().origin());
  for (const auto& rect : selection_rect_vector) {
    if (rect.Contains(point))
      return false;
  }
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();
  return true;
}

bool PDFiumEngine::NavigateToLinkDestination(
    PDFiumPage::Area area,
    const PDFiumPage::LinkTarget& target,
    WindowOpenDisposition disposition) {
  if (area == PDFiumPage::WEBLINK_AREA) {
    client_->NavigateTo(target.url, disposition);
    SetInFormTextArea(false);
    return true;
  }
  if (area == PDFiumPage::DOCLINK_AREA) {
    if (!PageIndexInBounds(target.page))
      return true;

    if (disposition == WindowOpenDisposition::CURRENT_TAB) {
      client_->NavigateToDestination(
          target.page, base::OptionalOrNullptr(target.x_in_pixels),
          base::OptionalOrNullptr(target.y_in_pixels),
          base::OptionalOrNullptr(target.zoom));
    } else {
      std::string parameters = base::StringPrintf("#page=%d", target.page + 1);
      parameters += base::StringPrintf(
          "&zoom=%d,%d,%d", static_cast<int>(target.zoom.value_or(1.0) * 100),
          static_cast<int>(target.x_in_pixels.value_or(0)),
          static_cast<int>(target.y_in_pixels.value_or(0)));

      client_->NavigateTo(parameters, disposition);
    }
    SetInFormTextArea(false);
    return true;
  }
  return false;
}

bool PDFiumEngine::OnMouseUp(const MouseInputEvent& event) {
  if (event.GetButton() != InputEventMouseButtonType::kLeft &&
      event.GetButton() != InputEventMouseButtonType::kMiddle) {
    return false;
  }

  if (event.GetButton() == InputEventMouseButtonType::kLeft)
    SetMouseLeftButtonDown(false);
  else if (event.GetButton() == InputEventMouseButtonType::kMiddle)
    mouse_middle_button_down_ = false;

  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  gfx::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);

  // Open link on mouse up for same link for which mouse down happened earlier.
  if (mouse_down_state_.Matches(area, target)) {
    uint32_t modifiers = event.GetModifiers();
    bool middle_button = !!(modifiers & kInputEventModifierMiddleButtonDown);
    bool alt_key = !!(modifiers & kInputEventModifierAltKey);
    bool ctrl_key = !!(modifiers & kInputEventModifierControlKey);
    bool meta_key = !!(modifiers & kInputEventModifierMetaKey);
    bool shift_key = !!(modifiers & kInputEventModifierShiftKey);

    WindowOpenDisposition disposition = ui::DispositionFromClick(
        middle_button, alt_key, ctrl_key, meta_key, shift_key);

    if (NavigateToLinkDestination(area, target, disposition))
      return true;
  }

  if (event.GetButton() == InputEventMouseButtonType::kMiddle) {
    if (kViewerImplementedPanning) {
      // Update the cursor when panning stops.
      client_->UpdateCursor(DetermineCursorType(area, form_type));
    }

    // Prevent middle mouse button from selecting texts.
    return false;
  }

  if (page_index != -1) {
    double page_x;
    double page_y;
    DeviceToPage(page_index, point, &page_x, &page_y);
    FORM_OnLButtonUp(form(), pages_[page_index]->GetPage(),
                     event.GetModifiers(), page_x, page_y);
  }

  if (!selecting_)
    return false;

  SetSelecting(false);
  return true;
}

bool PDFiumEngine::OnMouseMove(const MouseInputEvent& event) {
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  gfx::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);

  // Clear |mouse_down_state_| if mouse moves away from where the mouse down
  // happened.
  if (!mouse_down_state_.Matches(area, target))
    mouse_down_state_.Reset();

  if (!selecting_) {
    client_->UpdateCursor(DetermineCursorType(area, form_type));

    if (page_index != -1) {
      double page_x;
      double page_y;
      DeviceToPage(page_index, point, &page_x, &page_y);
      FORM_OnMouseMove(form(), pages_[page_index]->GetPage(), 0, page_x,
                       page_y);
    }

    UpdateLinkUnderCursor(GetLinkAtPosition(point));

    // If in form text area while left mouse button is held down, check if form
    // text selection needs to be updated.
    if (mouse_left_button_down_ && area == PDFiumPage::FORM_TEXT_AREA &&
        last_focused_page_ != -1) {
      SetFormSelectedText(form(), pages_[last_focused_page_]->GetPage());
    }

    if (kViewerImplementedPanning && mouse_middle_button_down_) {
      // Subtract (origin - destination) so delta is already the delta for
      // moving the page, rather than the delta the mouse moved.
      // GetMovement() does not work here, as small mouse movements are
      // considered zero.
      gfx::Vector2d page_position_delta =
          mouse_middle_button_last_position_ - event.GetPosition();
      if (page_position_delta.x() != 0 || page_position_delta.y() != 0) {
        client_->ScrollBy(page_position_delta);
        mouse_middle_button_last_position_ = event.GetPosition();
      }
    }

    // No need to swallow the event, since this might interfere with the
    // scrollbars if the user is dragging them.
    return false;
  }

  // We're selecting but right now we're not over text, so don't change the
  // current selection.
  if (area != PDFiumPage::TEXT_AREA && !IsLinkArea(area))
    return false;

  SelectionChangeInvalidator selection_invalidator(this);
  return ExtendSelection(page_index, char_index);
}

PP_CursorType_Dev PDFiumEngine::DetermineCursorType(PDFiumPage::Area area,
                                                    int form_type) const {
  if (kViewerImplementedPanning && mouse_middle_button_down_) {
    return PP_CURSORTYPE_HAND;
  }

  switch (area) {
    case PDFiumPage::TEXT_AREA:
      return PP_CURSORTYPE_IBEAM;
    case PDFiumPage::WEBLINK_AREA:
    case PDFiumPage::DOCLINK_AREA:
      return PP_CURSORTYPE_HAND;
    case PDFiumPage::NONSELECTABLE_AREA:
    case PDFiumPage::FORM_TEXT_AREA:
    default:
      switch (form_type) {
        case FPDF_FORMFIELD_PUSHBUTTON:
        case FPDF_FORMFIELD_CHECKBOX:
        case FPDF_FORMFIELD_RADIOBUTTON:
        case FPDF_FORMFIELD_COMBOBOX:
        case FPDF_FORMFIELD_LISTBOX:
          return PP_CURSORTYPE_HAND;
        case FPDF_FORMFIELD_TEXTFIELD:
          return PP_CURSORTYPE_IBEAM;
#if defined(PDF_ENABLE_XFA)
        case FPDF_FORMFIELD_XFA_CHECKBOX:
        case FPDF_FORMFIELD_XFA_COMBOBOX:
        case FPDF_FORMFIELD_XFA_IMAGEFIELD:
        case FPDF_FORMFIELD_XFA_LISTBOX:
        case FPDF_FORMFIELD_XFA_PUSHBUTTON:
        case FPDF_FORMFIELD_XFA_SIGNATURE:
          return PP_CURSORTYPE_HAND;
        case FPDF_FORMFIELD_XFA_TEXTFIELD:
          return PP_CURSORTYPE_IBEAM;
#endif
        default:
          return PP_CURSORTYPE_POINTER;
      }
  }
}

void PDFiumEngine::OnMouseEnter(const MouseInputEvent& event) {
  if (event.GetModifiers() & kInputEventModifierMiddleButtonDown) {
    if (!mouse_middle_button_down_) {
      mouse_middle_button_down_ = true;
      mouse_middle_button_last_position_ = event.GetPosition();
    }
  } else {
    if (mouse_middle_button_down_) {
      mouse_middle_button_down_ = false;
    }
  }
}

bool PDFiumEngine::ExtendSelection(int page_index, int char_index) {
  // Check if the user has decreased their selection area and we need to remove
  // pages from |selection_|.
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (selection_[i].page_index() == page_index) {
      // There should be no other pages after this.
      selection_.erase(selection_.begin() + i + 1, selection_.end());
      break;
    }
  }
  if (selection_.empty())
    return false;

  const int last_page_index = selection_.back().page_index();
  const int last_char_index = selection_.back().char_index();
  if (last_page_index == page_index) {
    // Selecting within a page.
    int count = char_index - last_char_index;
    if (count >= 0) {
      // Selecting forward.
      ++count;
    } else {
      --count;
    }
    selection_.back().SetCharCount(count);
  } else if (last_page_index < page_index) {
    // Selecting into the next page.

    // Save the current last selection for use below.
    // Warning: Do not use references / pointers into |selection_|, as the code
    // below can modify |selection_| and invalidate those references / pointers.
    const size_t last_selection_index = selection_.size() - 1;

    // First make sure that there are no gaps in selection, i.e. if mousedown on
    // page one but we only get mousemove over page three, we want page two.
    for (int i = last_page_index + 1; i < page_index; ++i) {
      selection_.push_back(
          PDFiumRange(pages_[i].get(), 0, pages_[i]->GetCharCount()));
    }

    int count = pages_[last_page_index]->GetCharCount();
    selection_[last_selection_index].SetCharCount(count - last_char_index);
    selection_.push_back(PDFiumRange(pages_[page_index].get(), 0, char_index));
  } else {
    // Selecting into the previous page.
    // The selection's char_index is 0-based, so the character count is one
    // more than the index. The character count needs to be negative to
    // indicate a backwards selection.
    selection_.back().SetCharCount(-last_char_index - 1);

    // First make sure that there are no gaps in selection, i.e. if mousedown on
    // page three but we only get mousemove over page one, we want page two.
    for (int i = last_page_index - 1; i > page_index; --i) {
      selection_.push_back(
          PDFiumRange(pages_[i].get(), 0, pages_[i]->GetCharCount()));
    }

    int count = pages_[page_index]->GetCharCount();
    selection_.push_back(
        PDFiumRange(pages_[page_index].get(), count, count - char_index));
  }

  return true;
}

bool PDFiumEngine::OnKeyDown(const KeyboardInputEvent& event) {
  // Handle tab events first as we might need to transition focus to an
  // annotation in PDF.
  if (event.GetKeyCode() == FWL_VKEY_Tab)
    return HandleTabEvent(event.GetModifiers());

  if (last_focused_page_ == -1)
    return false;

  bool rv = !!FORM_OnKeyDown(form(), pages_[last_focused_page_]->GetPage(),
                             event.GetKeyCode(), event.GetModifiers());

  if (event.GetKeyCode() == ui::VKEY_BACK ||
      event.GetKeyCode() == ui::VKEY_ESCAPE) {
    // Blink does not send char events for backspace or escape keys, see
    // WebKeyboardEvent::IsCharacterKey() and b/961192 for more information.
    // So just fake one since PDFium uses it.
    std::string str;
    str.push_back(event.GetKeyCode());
    KeyboardInputEvent synthesized(InputEventType::kChar, event.GetTimeStamp(),
                                   event.GetModifiers(), event.GetKeyCode(),
                                   str);
    OnChar(synthesized);
  }

#if !defined(OS_MAC)
  // macOS doesn't have keyboard-triggered context menus.
  // Scroll focused annotation into view when context menu is invoked through
  // keyboard <Shift-F10>.
  if (event.GetKeyCode() == FWL_VKEY_F10 &&
      (event.GetModifiers() & kInputEventModifierShiftKey)) {
    DCHECK(!rv);
    ScrollFocusedAnnotationIntoView();
  }
#endif

  return rv;
}

bool PDFiumEngine::OnKeyUp(const KeyboardInputEvent& event) {
  if (last_focused_page_ == -1)
    return false;

  // Check if form text selection needs to be updated.
  FPDF_PAGE page = pages_[last_focused_page_]->GetPage();
  if (in_form_text_area_)
    SetFormSelectedText(form(), page);

  return !!FORM_OnKeyUp(form(), page, event.GetKeyCode(), event.GetModifiers());
}

bool PDFiumEngine::OnChar(const KeyboardInputEvent& event) {
  if (last_focused_page_ == -1)
    return false;

  base::string16 str = base::UTF8ToUTF16(event.GetKeyChar());
  bool rv = !!FORM_OnChar(form(), pages_[last_focused_page_]->GetPage(), str[0],
                          event.GetModifiers());

  // Scroll editable form text into view on char events. We should not scroll
  // focused annotation on escape char event since escape char is used to
  // dismiss focus from form controls.
  if (rv && editable_form_text_area_ && event.GetKeyCode() != ui::VKEY_ESCAPE) {
    ScrollFocusedAnnotationIntoView();
  }

  return rv;
}

void PDFiumEngine::StartFind(const std::string& text, bool case_sensitive) {
  // If the caller asks StartFind() to search for no text, then this is an
  // error on the part of the caller. The PPAPI Find_Private interface
  // guarantees it is not empty, so this should never happen.
  DCHECK(!text.empty());

  // If StartFind() gets called before we have any page information (i.e.
  // before the first call to LoadDocument has happened). Handle this case.
  if (pages_.empty()) {
    client_->NotifyNumberOfFindResultsChanged(0, true);
    return;
  }

  bool first_search = (current_find_text_ != text);
  int character_to_start_searching_from = 0;
  if (first_search) {
    std::vector<PDFiumRange> old_selection = selection_;
    StopFind();
    current_find_text_ = text;

    if (old_selection.empty()) {
      // Start searching from the beginning of the document.
      next_page_to_search_ = 0;
      last_page_to_search_ = pages_.size() - 1;
      last_character_index_to_search_ = -1;
    } else {
      // There's a current selection, so start from it.
      next_page_to_search_ = old_selection[0].page_index();
      last_character_index_to_search_ = old_selection[0].char_index();
      character_to_start_searching_from = old_selection[0].char_index();
      last_page_to_search_ = next_page_to_search_;
    }
    search_in_progress_ = true;
  }

  int current_page = next_page_to_search_;

  if (pages_[current_page]->available()) {
    base::string16 str = base::UTF8ToUTF16(text);
    // Don't use PDFium to search for now, since it doesn't support unicode
    // text. Leave the code for now to avoid bit-rot, in case it's fixed later.
    // The extra parens suppress a -Wunreachable-code warning.
    if ((0)) {
      SearchUsingPDFium(str, case_sensitive, first_search,
                        character_to_start_searching_from, current_page);
    } else {
      SearchUsingICU(str, case_sensitive, first_search,
                     character_to_start_searching_from, current_page);
    }

    if (!IsPageVisible(current_page))
      pages_[current_page]->Unload();
  }

  if (next_page_to_search_ != last_page_to_search_ ||
      (first_search && last_character_index_to_search_ != -1)) {
    ++next_page_to_search_;
  }

  if (next_page_to_search_ == static_cast<int>(pages_.size()))
    next_page_to_search_ = 0;
  // If there's only one page in the document and we start searching midway,
  // then we'll want to search the page one more time.
  bool end_of_search =
      next_page_to_search_ == last_page_to_search_ &&
      // Only one page but didn't start midway.
      ((pages_.size() == 1 && last_character_index_to_search_ == -1) ||
       // Started midway, but only 1 page and we already looped around.
       (pages_.size() == 1 && !first_search) ||
       // Started midway, and we've just looped around.
       (pages_.size() > 1 && current_page == next_page_to_search_));

  if (end_of_search) {
    search_in_progress_ = false;

    // Send the final notification.
    client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
    return;
  }

  // In unit tests, PPAPI is not initialized, so just call ContinueFind()
  // directly.
  if (doc_loader_set_for_testing_) {
    ContinueFind(case_sensitive ? 1 : 0);
  } else {
    pp::Module::Get()->core()->CallOnMainThread(
        0,
        PPCompletionCallbackFromResultCallback(base::BindOnce(
            &PDFiumEngine::ContinueFind, find_weak_factory_.GetWeakPtr())),
        case_sensitive ? 1 : 0);
  }
}

void PDFiumEngine::SearchUsingPDFium(const base::string16& term,
                                     bool case_sensitive,
                                     bool first_search,
                                     int character_to_start_searching_from,
                                     int current_page) {
  // Find all the matches in the current page.
  unsigned long flags = case_sensitive ? FPDF_MATCHCASE : 0;
  FPDF_SCHHANDLE find =
      FPDFText_FindStart(pages_[current_page]->GetTextPage(),
                         reinterpret_cast<const unsigned short*>(term.c_str()),
                         flags, character_to_start_searching_from);

  // Note: since we search one page at a time, we don't find matches across
  // page boundaries.  We could do this manually ourself, but it seems low
  // priority since Reader itself doesn't do it.
  while (FPDFText_FindNext(find)) {
    PDFiumRange result(pages_[current_page].get(),
                       FPDFText_GetSchResultIndex(find),
                       FPDFText_GetSchCount(find));

    if (!first_search && last_character_index_to_search_ != -1 &&
        result.page_index() == last_page_to_search_ &&
        result.char_index() >= last_character_index_to_search_) {
      break;
    }

    AddFindResult(result);
  }

  FPDFText_FindClose(find);
}

void PDFiumEngine::SearchUsingICU(const base::string16& term,
                                  bool case_sensitive,
                                  bool first_search,
                                  int character_to_start_searching_from,
                                  int current_page) {
  DCHECK(!term.empty());

  // Various types of quotions marks need to be converted to the simple ASCII
  // version for searching to get better matching.
  base::string16 adjusted_term = term;
  for (base::char16& c : adjusted_term)
    c = SimplifyForSearch(c);

  const int original_text_length = pages_[current_page]->GetCharCount();
  int text_length = original_text_length;
  if (character_to_start_searching_from) {
    text_length -= character_to_start_searching_from;
  } else if (!first_search && last_character_index_to_search_ != -1 &&
             current_page == last_page_to_search_) {
    text_length = last_character_index_to_search_;
  }
  if (text_length <= 0)
    return;

  base::string16 page_text;
  PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
      &page_text, text_length, false);
  unsigned short* data =
      reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
  int written =
      FPDFText_GetText(pages_[current_page]->GetTextPage(),
                       character_to_start_searching_from, text_length, data);
  api_string_adapter.Close(written);

  base::string16 adjusted_page_text;
  adjusted_page_text.reserve(page_text.size());
  // Values in |removed_indices| are in the adjusted text index space and
  // indicate a character was removed from the page text before the given
  // index. If multiple characters are removed in a row then there will be
  // multiple entries with the same value.
  std::vector<size_t> removed_indices;
  // When walking through the page text collapse any whitespace regions,
  // including \r and \n, down to a single ' ' character. This code does
  // not use base::CollapseWhitespace(), because that function does not
  // return where the collapsing occurs, but uses the same underlying list of
  // whitespace characters. Calculating where the collapsed regions are after
  // the fact is as complex as collapsing them manually.
  for (size_t i = 0; i < page_text.size(); i++) {
    base::char16 c = page_text[i];
    // Collapse whitespace regions by inserting a ' ' into the
    // adjusted text and recording any removed whitespace indices as preceding
    // it.
    if (base::IsUnicodeWhitespace(c)) {
      size_t whitespace_region_begin = i;
      while (i < page_text.size() && base::IsUnicodeWhitespace(page_text[i]))
        ++i;

      size_t count = i - whitespace_region_begin - 1;
      removed_indices.insert(removed_indices.end(), count,
                             adjusted_page_text.size());
      adjusted_page_text.push_back(' ');
      if (i >= page_text.size())
        break;
      c = page_text[i];
    }

    if (IsIgnorableCharacter(c))
      removed_indices.push_back(adjusted_page_text.size());
    else
      adjusted_page_text.push_back(SimplifyForSearch(c));
  }

  std::vector<PDFEngine::Client::SearchStringResult> results =
      client_->SearchString(adjusted_page_text.c_str(), adjusted_term.c_str(),
                            case_sensitive);
  for (const auto& result : results) {
    // Need to convert from adjusted page text start to page text start, by
    // incrementing for all the characters adjusted before it in the string.
    auto removed_indices_begin = std::upper_bound(
        removed_indices.begin(), removed_indices.end(), result.start_index);
    size_t page_text_result_start_index =
        result.start_index +
        std::distance(removed_indices.begin(), removed_indices_begin);

    // Need to convert the adjusted page length into a page text length, since
    // the matching range may have adjusted characters within it. This
    // conversion only cares about skipped characters in the result interval.
    auto removed_indices_end =
        std::upper_bound(removed_indices_begin, removed_indices.end(),
                         result.start_index + result.length);
    int term_removed_count =
        std::distance(removed_indices_begin, removed_indices_end);
    int page_text_result_length = result.length + term_removed_count;

    // Need to map the indexes from the page text, which may have generated
    // characters like space etc, to character indices from the page.
    int text_to_start_searching_from = FPDFText_GetTextIndexFromCharIndex(
        pages_[current_page]->GetTextPage(), character_to_start_searching_from);
    int temp_start =
        page_text_result_start_index + text_to_start_searching_from;
    int start = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(), temp_start);
    int end = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(),
        temp_start + page_text_result_length);

    // If |term| occurs at the end of a page, then |end| will be -1 due to the
    // index being out of bounds. Compensate for this case so the range
    // character count calculation below works out.
    if (temp_start + page_text_result_length == original_text_length) {
      DCHECK_EQ(-1, end);
      end = original_text_length;
    }
    DCHECK_LT(start, end);
    DCHECK_EQ(term.size() + term_removed_count,
              static_cast<size_t>(end - start));
    AddFindResult(PDFiumRange(pages_[current_page].get(), start, end - start));
  }
}

void PDFiumEngine::AddFindResult(const PDFiumRange& result) {
  bool first_result = find_results_.empty();
  // Figure out where to insert the new location, since we could have
  // started searching midway and now we wrapped.
  size_t result_index;
  int page_index = result.page_index();
  int char_index = result.char_index();
  for (result_index = 0; result_index < find_results_.size(); ++result_index) {
    if (find_results_[result_index].page_index() > page_index ||
        (find_results_[result_index].page_index() == page_index &&
         find_results_[result_index].char_index() > char_index)) {
      break;
    }
  }
  find_results_.insert(find_results_.begin() + result_index, result);
  UpdateTickMarks();
  client_->NotifyNumberOfFindResultsChanged(find_results_.size(), false);
  if (first_result) {
    DCHECK(!resume_find_index_);
    DCHECK(!current_find_index_);
    SelectFindResult(/*forward=*/true);
  }
}

bool PDFiumEngine::SelectFindResult(bool forward) {
  if (find_results_.empty())
    return false;

  SelectionChangeInvalidator selection_invalidator(this);

  // Move back/forward through the search locations we previously found.
  size_t new_index;
  const size_t last_index = find_results_.size() - 1;

  if (resume_find_index_) {
    new_index = resume_find_index_.value();
    resume_find_index_.reset();
  } else if (current_find_index_) {
    size_t current_index = current_find_index_.value();
    if ((forward && current_index >= last_index) ||
        (!forward && current_index == 0)) {
      current_find_index_.reset();
      client_->NotifySelectedFindResultChanged(-1);
      client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
      return true;
    }
    int increment = forward ? 1 : -1;
    new_index = current_index + increment;
  } else {
    new_index = forward ? 0 : last_index;
  }
  current_find_index_ = new_index;

  // Update the selection before telling the client to scroll, since it could
  // paint then.
  selection_.clear();
  selection_.push_back(find_results_[current_find_index_.value()]);

  // If the result is not in view, scroll to it.
  gfx::Rect bounding_rect;
  gfx::Rect visible_rect = GetVisibleRect();

  // TODO(crbug.com/1108574): Remove after fixing the issue.
  size_t find_results_size = find_results_.size();
  base::debug::Alias(&find_results_size);
  size_t current_find_index_value = current_find_index_.value();
  base::debug::Alias(&current_find_index_value);

  // Use zoom of 1.0 since |visible_rect| is without zoom.
  const std::vector<gfx::Rect>& rects =
      find_results_[current_find_index_.value()].GetScreenRects(
          gfx::Point(), 1.0, layout_.options().default_page_orientation());
  for (const auto& rect : rects)
    bounding_rect.Union(rect);
  if (!visible_rect.Contains(bounding_rect)) {
    gfx::Point center = bounding_rect.CenterPoint();
    // Make the page centered.
    int new_y = CalculateCenterForZoom(center.y(), visible_rect.height(),
                                       current_zoom_);
    client_->ScrollToY(new_y, /*compensate_for_toolbar=*/false);

    // Only move horizontally if it's not visible.
    if (center.x() < visible_rect.x() || center.x() > visible_rect.right()) {
      int new_x = CalculateCenterForZoom(center.x(), visible_rect.width(),
                                         current_zoom_);
      client_->ScrollToX(new_x);
    }
  }

  client_->NotifySelectedFindResultChanged(current_find_index_.value());
  if (!search_in_progress_)
    client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
  return true;
}

void PDFiumEngine::StopFind() {
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();
  selecting_ = false;

  find_results_.clear();
  search_in_progress_ = false;
  next_page_to_search_ = -1;
  last_page_to_search_ = -1;
  last_character_index_to_search_ = -1;
  current_find_index_.reset();
  current_find_text_.clear();

  UpdateTickMarks();
  find_weak_factory_.InvalidateWeakPtrs();
}

std::vector<gfx::Rect> PDFiumEngine::GetAllScreenRectsUnion(
    const std::vector<PDFiumRange>& rect_range,
    const gfx::Point& point) const {
  std::vector<gfx::Rect> rect_vector;
  rect_vector.reserve(rect_range.size());
  for (const auto& range : rect_range) {
    gfx::Rect result_rect;
    const std::vector<gfx::Rect>& rects = range.GetScreenRects(
        point, current_zoom_, layout_.options().default_page_orientation());
    for (const auto& rect : rects)
      result_rect.Union(rect);
    rect_vector.push_back(result_rect);
  }
  return rect_vector;
}

void PDFiumEngine::UpdateTickMarks() {
  std::vector<gfx::Rect> tickmarks =
      GetAllScreenRectsUnion(find_results_, gfx::Point());
  client_->UpdateTickMarks(tickmarks);
}

void PDFiumEngine::ZoomUpdated(double new_zoom_level) {
  CancelPaints();

  current_zoom_ = new_zoom_level;

  CalculateVisiblePages();
  UpdateTickMarks();
}

void PDFiumEngine::RotateClockwise() {
  desired_layout_options_.RotatePagesClockwise();
  ProposeNextDocumentLayout();
}

void PDFiumEngine::RotateCounterclockwise() {
  desired_layout_options_.RotatePagesCounterclockwise();
  ProposeNextDocumentLayout();
}

void PDFiumEngine::SetTwoUpView(bool enable) {
  desired_layout_options_.set_two_up_view_enabled(enable);
  ProposeNextDocumentLayout();
}

void PDFiumEngine::DisplayAnnotations(bool display) {
  if (render_annots_ == display)
    return;

  render_annots_ = display;
  InvalidateAllPages();
}

void PDFiumEngine::InvalidateAllPages() {
  CancelPaints();
  StopFind();
  DCHECK(document_loaded_);
  RefreshCurrentDocumentLayout();
  client_->Invalidate(gfx::Rect(plugin_size_));
}

std::string PDFiumEngine::GetSelectedText() {
  if (!HasPermission(PDFEngine::PERMISSION_COPY))
    return std::string();

  base::string16 result;
  for (size_t i = 0; i < selection_.size(); ++i) {
    static constexpr base::char16 kNewLineChar = L'\n';
    base::string16 current_selection_text = selection_[i].GetText();
    if (i != 0) {
      if (selection_[i - 1].page_index() > selection_[i].page_index())
        std::swap(current_selection_text, result);
      result.push_back(kNewLineChar);
    }
    result.append(current_selection_text);
  }

  FormatStringWithHyphens(&result);
  FormatStringForOS(&result);
  return base::UTF16ToUTF8(result);
}

bool PDFiumEngine::CanEditText() {
  return editable_form_text_area_;
}

bool PDFiumEngine::HasEditableText() {
  DCHECK(CanEditText());
  if (last_focused_page_ == -1)
    return false;
  FPDF_PAGE page = pages_[last_focused_page_]->GetPage();
  // If the return value is 2, that corresponds to "\0\0".
  return FORM_GetFocusedText(form(), page, nullptr, 0) > 2;
}

void PDFiumEngine::ReplaceSelection(const std::string& text) {
  DCHECK(CanEditText());
  if (last_focused_page_ != -1) {
    base::string16 text_wide = base::UTF8ToUTF16(text);
    FPDF_WIDESTRING text_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(text_wide.c_str());

    FORM_ReplaceSelection(form(), pages_[last_focused_page_]->GetPage(),
                          text_pdf_wide);
  }
}

bool PDFiumEngine::CanUndo() {
  if (last_focused_page_ == -1)
    return false;
  return !!FORM_CanUndo(form(), pages_[last_focused_page_]->GetPage());
}

bool PDFiumEngine::CanRedo() {
  if (last_focused_page_ == -1)
    return false;
  return !!FORM_CanRedo(form(), pages_[last_focused_page_]->GetPage());
}

void PDFiumEngine::Undo() {
  if (last_focused_page_ != -1)
    FORM_Undo(form(), pages_[last_focused_page_]->GetPage());
}

void PDFiumEngine::Redo() {
  if (last_focused_page_ != -1)
    FORM_Redo(form(), pages_[last_focused_page_]->GetPage());
}

void PDFiumEngine::HandleAccessibilityAction(
    const PP_PdfAccessibilityActionData& action_data) {
  switch (action_data.action) {
    case PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_MAKE_VISIBLE: {
      ScrollBasedOnScrollAlignment(RectFromPPRect(action_data.target_rect),
                                   action_data.horizontal_scroll_alignment,
                                   action_data.vertical_scroll_alignment);
      break;
    }
    case PP_PdfAccessibilityAction::PP_PDF_DO_DEFAULT_ACTION: {
      if (PageIndexInBounds(action_data.page_index)) {
        if (action_data.annotation_type ==
            PP_PdfAccessibilityAnnotationType::PP_PDF_LINK) {
          PDFiumPage::LinkTarget target;
          PDFiumPage::Area area =
              pages_[action_data.page_index]->GetLinkTargetAtIndex(
                  action_data.annotation_index, &target);
          NavigateToLinkDestination(area, target,
                                    WindowOpenDisposition::CURRENT_TAB);
        }
      }
      break;
    }
    case PP_PdfAccessibilityAction::PP_PDF_SCROLL_TO_GLOBAL_POINT: {
      ScrollToGlobalPoint(RectFromPPRect(action_data.target_rect),
                          PointFromPPPoint(action_data.target_point));
      break;
    }
    case PP_PdfAccessibilityAction::PP_PDF_SET_SELECTION: {
      if (IsPageCharacterIndexInBounds(action_data.selection_start_index) &&
          IsPageCharacterIndexInBounds(action_data.selection_end_index)) {
        SetSelection(action_data.selection_start_index,
                     action_data.selection_end_index);
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

std::string PDFiumEngine::GetLinkAtPosition(const gfx::Point& point) {
  std::string url;
  int temp;
  int page_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &temp, &form_type, &target);
  if (area == PDFiumPage::WEBLINK_AREA)
    url = target.url;
  return url;
}

bool PDFiumEngine::HasPermission(DocumentPermission permission) const {
  // No |permissions_| means no restrictions.
  if (!permissions_)
    return true;
  return permissions_->HasPermission(permission);
}

void PDFiumEngine::SelectAll() {
  if (in_form_text_area_)
    return;

  SelectionChangeInvalidator selection_invalidator(this);

  selection_.clear();
  for (const auto& page : pages_) {
    if (page->available())
      selection_.push_back(PDFiumRange(page.get(), 0, page->GetCharCount()));
  }
}

const std::vector<DocumentAttachmentInfo>&
PDFiumEngine::GetDocumentAttachmentInfoList() const {
  DCHECK(document_loaded_);
  return doc_attachment_info_list_;
}

std::vector<uint8_t> PDFiumEngine::GetAttachmentData(size_t index) {
  DCHECK_LT(index, doc_attachment_info_list_.size());
  DCHECK(doc_attachment_info_list_[index].is_readable);
  unsigned long length_bytes = doc_attachment_info_list_[index].size_bytes;
  DCHECK_NE(length_bytes, 0u);

  FPDF_ATTACHMENT attachment = FPDFDoc_GetAttachment(doc(), index);
  std::vector<uint8_t> content_buf(length_bytes);
  unsigned long data_size_bytes;
  bool is_attachment_readable = FPDFAttachment_GetFile(
      attachment, content_buf.data(), length_bytes, &data_size_bytes);
  if (!is_attachment_readable || length_bytes != data_size_bytes) {
    NOTREACHED();
    return std::vector<uint8_t>();
  }

  return content_buf;
}

const DocumentMetadata& PDFiumEngine::GetDocumentMetadata() const {
  DCHECK(document_loaded_);
  return doc_metadata_;
}

int PDFiumEngine::GetNumberOfPages() const {
  return pages_.size();
}

pp::VarArray PDFiumEngine::GetBookmarks() {
  pp::VarDictionary dict = TraverseBookmarks(nullptr, 0);
  // The root bookmark contains no useful information.
  return pp::VarArray(dict.Get(pp::Var("children")));
}

pp::VarDictionary PDFiumEngine::TraverseBookmarks(FPDF_BOOKMARK bookmark,
                                                  unsigned int depth) {
  pp::VarDictionary dict;
  base::string16 title = CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDFBookmark_GetTitle, bookmark),
      /*check_expected_size=*/true);
  dict.Set(pp::Var("title"), pp::Var(base::UTF16ToUTF8(title)));

  FPDF_DEST dest = FPDFBookmark_GetDest(doc(), bookmark);
  // Some bookmarks don't have a page to select.
  if (dest) {
    int page_index = FPDFDest_GetDestPageIndex(doc(), dest);
    if (PageIndexInBounds(page_index)) {
      dict.Set(pp::Var("page"), pp::Var(page_index));

      base::Optional<gfx::PointF> xy;
      base::Optional<float> zoom;
      pages_[page_index]->GetPageDestinationTarget(dest, &xy, &zoom);
      if (xy) {
        dict.Set(pp::Var("x"), pp::Var(static_cast<int>(xy.value().x())));
        dict.Set(pp::Var("y"), pp::Var(static_cast<int>(xy.value().y())));
      }
      if (zoom) {
        dict.Set(pp::Var("zoom"), pp::Var(zoom.value()));
      }
    }
  } else {
    // Extract URI for bookmarks linking to an external page.
    FPDF_ACTION action = FPDFBookmark_GetAction(bookmark);
    std::string uri = CallPDFiumStringBufferApi(
        base::BindRepeating(&FPDFAction_GetURIPath, doc(), action),
        /*check_expected_size=*/true);
    if (!uri.empty())
      dict.Set(pp::Var("uri"), pp::Var(uri));
  }

  pp::VarArray children;

  // Don't trust PDFium to handle circular bookmarks.
  constexpr unsigned int kMaxDepth = 128;
  if (depth < kMaxDepth) {
    int child_index = 0;
    std::set<FPDF_BOOKMARK> seen_bookmarks;
    for (FPDF_BOOKMARK child_bookmark =
             FPDFBookmark_GetFirstChild(doc(), bookmark);
         child_bookmark;
         child_bookmark = FPDFBookmark_GetNextSibling(doc(), child_bookmark)) {
      if (base::Contains(seen_bookmarks, child_bookmark))
        break;

      seen_bookmarks.insert(child_bookmark);
      children.Set(child_index, TraverseBookmarks(child_bookmark, depth + 1));
      child_index++;
    }
  }
  dict.Set(pp::Var("children"), children);
  return dict;
}

void PDFiumEngine::ScrollBasedOnScrollAlignment(
    const gfx::Rect& scroll_rect,
    const PP_PdfAccessibilityScrollAlignment& horizontal_scroll_alignment,
    const PP_PdfAccessibilityScrollAlignment& vertical_scroll_alignment) {
  gfx::Vector2d scroll_offset = GetScreenRect(scroll_rect).OffsetFromOrigin();
  switch (horizontal_scroll_alignment) {
    case PP_PdfAccessibilityScrollAlignment::PP_PDF_SCROLL_ALIGNMENT_RIGHT:
      scroll_offset.set_x(scroll_offset.x() - plugin_size_.width());
      break;
    case PP_PDF_SCROLL_ALIGNMENT_CENTER:
      scroll_offset.set_x(scroll_offset.x() - (plugin_size_.width() / 2));
      break;
    case PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE: {
      scroll_offset.set_x((std::abs(scroll_offset.x()) <=
                           std::abs(scroll_offset.x() - plugin_size_.width()))
                              ? scroll_offset.x()
                              : scroll_offset.x() - plugin_size_.width());
      break;
    }
    case PP_PDF_SCROLL_NONE:
      scroll_offset.set_x(0);
      break;
    case PP_PDF_SCROLL_ALIGNMENT_LEFT:
    case PP_PDF_SCROLL_ALIGNMENT_TOP:
    case PP_PDF_SCROLL_ALIGNMENT_BOTTOM:
    default:
      break;
  }

  switch (vertical_scroll_alignment) {
    case PP_PDF_SCROLL_ALIGNMENT_BOTTOM:
      scroll_offset.set_y(scroll_offset.y() - plugin_size_.height());
      break;
    case PP_PDF_SCROLL_ALIGNMENT_CENTER:
      scroll_offset.set_y(scroll_offset.y() - (plugin_size_.height() / 2));
      break;
    case PP_PDF_SCROLL_ALIGNMENT_CLOSEST_EDGE: {
      scroll_offset.set_y((std::abs(scroll_offset.y()) <=
                           std::abs(scroll_offset.y() - plugin_size_.height()))
                              ? scroll_offset.y()
                              : scroll_offset.y() - plugin_size_.height());
      break;
    }
    case PP_PDF_SCROLL_NONE:
      scroll_offset.set_y(0);
      break;
    case PP_PDF_SCROLL_ALIGNMENT_TOP:
    case PP_PDF_SCROLL_ALIGNMENT_LEFT:
    case PP_PDF_SCROLL_ALIGNMENT_RIGHT:
    default:
      break;
  }

  client_->ScrollBy(scroll_offset);
}

void PDFiumEngine::ScrollToGlobalPoint(const gfx::Rect& target_rect,
                                       const gfx::Point& global_point) {
  gfx::Point scroll_offset = GetScreenRect(target_rect).origin();
  client_->ScrollBy(scroll_offset - global_point);
}

base::Optional<PDFEngine::NamedDestination> PDFiumEngine::GetNamedDestination(
    const std::string& destination) {
  // Look for the destination.
  FPDF_DEST dest = FPDF_GetNamedDestByName(doc(), destination.c_str());
  if (!dest) {
    // Look for a bookmark with the same name.
    base::string16 destination_wide = base::UTF8ToUTF16(destination);
    FPDF_WIDESTRING destination_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(destination_wide.c_str());
    FPDF_BOOKMARK bookmark = FPDFBookmark_Find(doc(), destination_pdf_wide);
    if (bookmark)
      dest = FPDFBookmark_GetDest(doc(), bookmark);
  }

  if (!dest)
    return {};

  int page = FPDFDest_GetDestPageIndex(doc(), dest);
  if (page < 0)
    return {};

  PDFEngine::NamedDestination result;
  result.page = page;
  unsigned long view_int =
      FPDFDest_GetView(dest, &result.num_params, result.params);
  result.view = ConvertViewIntToViewString(view_int);
  return result;
}

int PDFiumEngine::GetMostVisiblePage() {
  if (in_flight_visible_page_)
    return *in_flight_visible_page_;

  // We can call GetMostVisiblePage through a callback from PDFium. We have
  // to defer the page deletion otherwise we could potentially delete the page
  // that originated the calling JS request and destroy the objects that are
  // currently being used.
  base::AutoReset<bool> defer_page_unload_guard(&defer_page_unload_, true);
  CalculateVisiblePages();
  return most_visible_page_;
}

gfx::Rect PDFiumEngine::GetPageBoundsRect(int index) {
  return pages_[index]->rect();
}

gfx::Rect PDFiumEngine::GetPageContentsRect(int index) {
  return GetScreenRect(pages_[index]->rect());
}

int PDFiumEngine::GetVerticalScrollbarYPosition() {
  return position_.y();
}

void PDFiumEngine::SetGrayscale(bool grayscale) {
  render_grayscale_ = grayscale;
}

void PDFiumEngine::HandleLongPress(const TouchInputEvent& event) {
  base::AutoReset<bool> handling_long_press_guard(&handling_long_press_, true);
  gfx::Point point = gfx::ToRoundedPoint(event.GetTargetTouchPoint());

  // Send a fake mouse down to trigger the multi-click selection code.
  MouseInputEvent mouse_event(
      InputEventType::kMouseDown, event.GetTimeStamp(), event.GetModifiers(),
      InputEventMouseButtonType::kLeft, point, 2, point);

  OnMouseDown(mouse_event);
}

int PDFiumEngine::GetCharCount(int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharCount();
}

gfx::RectF PDFiumEngine::GetCharBounds(int page_index, int char_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharBounds(char_index);
}

uint32_t PDFiumEngine::GetCharUnicode(int page_index, int char_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharUnicode(char_index);
}

base::Optional<pp::PDF::PrivateAccessibilityTextRunInfo>
PDFiumEngine::GetTextRunInfo(int page_index, int start_char_index) {
  DCHECK(PageIndexInBounds(page_index));
  auto info = pages_[page_index]->GetTextRunInfo(start_char_index);
  if (!client_->IsPrintPreview() && start_char_index >= 0)
    pages_[page_index]->LogOverlappingAnnotations();
  return info;
}

std::vector<PDFEngine::AccessibilityLinkInfo> PDFiumEngine::GetLinkInfo(
    int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetLinkInfo();
}

std::vector<PDFEngine::AccessibilityImageInfo> PDFiumEngine::GetImageInfo(
    int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetImageInfo();
}

std::vector<PDFEngine::AccessibilityHighlightInfo>
PDFiumEngine::GetHighlightInfo(int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetHighlightInfo();
}

std::vector<PDFEngine::AccessibilityTextFieldInfo>
PDFiumEngine::GetTextFieldInfo(int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetTextFieldInfo();
}

bool PDFiumEngine::GetPrintScaling() {
  return !!FPDF_VIEWERREF_GetPrintScaling(doc());
}

int PDFiumEngine::GetCopiesToPrint() {
  return FPDF_VIEWERREF_GetNumCopies(doc());
}

int PDFiumEngine::GetDuplexType() {
  return static_cast<int>(FPDF_VIEWERREF_GetDuplex(doc()));
}

bool PDFiumEngine::GetPageSizeAndUniformity(gfx::Size* size) {
  if (pages_.empty())
    return false;

  gfx::Size page_size = GetPageSize(0);
  for (size_t i = 1; i < pages_.size(); ++i) {
    if (page_size != GetPageSize(i))
      return false;
  }

  // Convert |page_size| back to points.
  size->set_width(
      ConvertUnit(page_size.width(), kPixelsPerInch, kPointsPerInch));
  size->set_height(
      ConvertUnit(page_size.height(), kPixelsPerInch, kPointsPerInch));
  return true;
}

void PDFiumEngine::AppendBlankPages(size_t num_pages) {
  DCHECK_GT(num_pages, 0U);

  if (!doc())
    return;

  selection_.clear();
  pending_pages_.clear();

  // Delete all pages except the first one.
  while (pages_.size() > 1) {
    pages_.pop_back();
    FPDFPage_Delete(doc(), pages_.size());
  }

  // Create blank pages with the same size as the first page.
  gfx::Size page_0_size = GetPageSize(0);
  double page_0_width_in_points =
      ConvertUnitDouble(page_0_size.width(), kPixelsPerInch, kPointsPerInch);
  double page_0_height_in_points =
      ConvertUnitDouble(page_0_size.height(), kPixelsPerInch, kPointsPerInch);

  for (size_t i = 1; i < num_pages; ++i) {
    {
      // Add a new page to the document, but delete the FPDF_PAGE object.
      ScopedFPDFPage temp_page(FPDFPage_New(doc(), i, page_0_width_in_points,
                                            page_0_height_in_points));
    }

    auto page = std::make_unique<PDFiumPage>(this, i);
    page->MarkAvailable();
    pages_.push_back(std::move(page));
  }

  DCHECK(document_loaded_);
  LoadPageInfo();
}

void PDFiumEngine::LoadDocument() {
  // Check if the document is ready for loading. If it isn't just bail for now,
  // we will call LoadDocument() again later.
  if (!doc() && !doc_loader_->IsDocumentComplete()) {
    FX_DOWNLOADHINTS& download_hints = document_->download_hints();
    if (!FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints))
      return;
  }

  // If we're in the middle of getting a password, just return. We will retry
  // loading the document after we get the password anyway.
  if (getting_password_)
    return;

  bool needs_password = false;
  if (TryLoadingDoc(std::string(), &needs_password)) {
    ContinueLoadingDocument(std::string());
    return;
  }
  if (needs_password)
    GetPasswordAndLoad();
  else
    client_->DocumentLoadFailed();
}

bool PDFiumEngine::TryLoadingDoc(const std::string& password,
                                 bool* needs_password) {
  *needs_password = false;
  if (doc()) {
    // This is probably not necessary, because it should have already been
    // called below in the |doc_| initialization path. However, the previous
    // call may have failed, so call it again for good measure.
    FX_DOWNLOADHINTS& download_hints = document_->download_hints();
    FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints);
    return true;
  }

  if (!password.empty())
    password_tries_remaining_--;

  {
    ScopedUnsupportedFeature scoped_unsupported_feature(this);
    document_->LoadDocument(password);
  }

  if (!doc()) {
    if (FPDF_GetLastError() == FPDF_ERR_PASSWORD)
      *needs_password = true;
    return false;
  }

  // Always call FPDFAvail_IsDocAvail() so PDFium initializes internal data
  // structures.
  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints);
  return true;
}

void PDFiumEngine::GetPasswordAndLoad() {
  getting_password_ = true;
  DCHECK(!doc());
  DCHECK_EQ(static_cast<unsigned long>(FPDF_ERR_PASSWORD), FPDF_GetLastError());
  client_->GetDocumentPassword(base::BindOnce(
      &PDFiumEngine::OnGetPasswordComplete, weak_factory_.GetWeakPtr()));
}

void PDFiumEngine::OnGetPasswordComplete(const std::string& password) {
  getting_password_ = false;
  ContinueLoadingDocument(password);
}

void PDFiumEngine::ContinueLoadingDocument(const std::string& password) {
  bool needs_password = false;
  bool loaded = TryLoadingDoc(password, &needs_password);
  bool password_incorrect = !loaded && needs_password && !password.empty();
  if (password_incorrect && password_tries_remaining_ > 0) {
    GetPasswordAndLoad();
    return;
  }

  if (!doc()) {
    client_->DocumentLoadFailed();
    return;
  }

  if (FPDFDoc_GetPageMode(doc()) == PAGEMODE_USEOUTLINES)
    client_->DocumentHasUnsupportedFeature("Bookmarks");

  permissions_ = std::make_unique<PDFiumPermissions>(doc());

  LoadBody();

  if (doc_loader_->IsDocumentComplete())
    FinishLoadingDocument();
}

void PDFiumEngine::LoadPageInfo() {
  RefreshCurrentDocumentLayout();

  // TODO(crbug.com/1013800): RefreshCurrentDocumentLayout() should send some
  // sort of "current layout changed" notification, instead of proposing a new
  // layout. Proposals are never rejected currently, so this is OK for now.
  ProposeNextDocumentLayout();
}

void PDFiumEngine::RefreshCurrentDocumentLayout() {
  UpdateDocumentLayout(&layout_);
  if (!layout_.dirty())
    return;

  DCHECK_EQ(pages_.size(), layout_.page_count());
  for (size_t i = 0; i < layout_.page_count(); ++i) {
    // TODO(kmoon): This should be the only place that sets |PDFiumPage::rect_|.
    pages_[i]->set_rect(layout_.page_bounds_rect(i));
  }

  layout_.clear_dirty();

  CalculateVisiblePages();
}

void PDFiumEngine::ProposeNextDocumentLayout() {
  DocumentLayout next_layout;
  next_layout.SetOptions(desired_layout_options_);
  UpdateDocumentLayout(&next_layout);

  // The time windows between proposal and application may overlap, so we must
  // always propose a new layout, regardless of the current layout state.
  client_->ProposeDocumentLayout(next_layout);
}

void PDFiumEngine::UpdateDocumentLayout(DocumentLayout* layout) {
  std::vector<gfx::Size> page_sizes = LoadPageSizes(layout->options());
  if (page_sizes.empty())
    return;

  if (layout->options().two_up_view_enabled()) {
    layout->ComputeTwoUpViewLayout(page_sizes);
  } else {
    layout->ComputeSingleViewLayout(page_sizes);
  }
}

std::vector<gfx::Size> PDFiumEngine::LoadPageSizes(
    const DocumentLayout::Options& layout_options) {
  std::vector<gfx::Size> page_sizes;
  if (!doc_loader_)
    return page_sizes;
  if (pages_.empty() && document_loaded_)
    return page_sizes;

  pending_pages_.clear();
  size_t new_page_count = FPDF_GetPageCount(doc());

  bool doc_complete = doc_loader_->IsDocumentComplete();
  bool is_linear =
      FPDFAvail_IsLinearized(fpdf_availability()) == PDF_LINEARIZED;
  for (size_t i = 0; i < new_page_count; ++i) {
    // Get page availability. If |document_loaded_| == true and the page is not
    // new, then the page has been constructed already. Get page availability
    // flag from already existing PDFiumPage object. If |document_loaded_| ==
    // false or the page is new, then the page may not be fully loaded yet.
    bool page_available;
    if (document_loaded_ && i < pages_.size()) {
      page_available = pages_[i]->available();
    } else if (is_linear) {
      FX_DOWNLOADHINTS& download_hints = document_->download_hints();
      int linear_page_avail =
          FPDFAvail_IsPageAvail(fpdf_availability(), i, &download_hints);
      page_available = linear_page_avail == PDF_DATA_AVAIL;
    } else {
      page_available = doc_complete;
    }

    // TODO(crbug.com/1013800): It'd be better if page size were independent of
    // layout options, and handled in the layout code.
    gfx::Size size = page_available ? GetPageSizeForLayout(i, layout_options)
                                    : default_page_size_;
    EnlargePage(layout_options, i, new_page_count, &size);
    page_sizes.push_back(size);
  }

  // Add new pages. If |document_loaded_| == false, do not mark page as
  // available even if |doc_complete| is true because FPDFAvail_IsPageAvail()
  // still has to be called for this page, which will be done in
  // FinishLoadingDocument().
  for (size_t i = pages_.size(); i < new_page_count; ++i) {
    auto page = std::make_unique<PDFiumPage>(this, i);
    if (document_loaded_ &&
        FPDFAvail_IsPageAvail(fpdf_availability(), i, nullptr))
      page->MarkAvailable();
    pages_.push_back(std::move(page));
  }

  // Remove pages that do not exist anymore.
  if (pages_.size() > new_page_count) {
    for (size_t i = new_page_count; i < pages_.size(); ++i)
      pages_[i]->Unload();

    pages_.resize(new_page_count);
  }

  return page_sizes;
}

void PDFiumEngine::LoadBody() {
  DCHECK(doc());
  DCHECK(fpdf_availability());
  if (doc_loader_->IsDocumentComplete()) {
    LoadForm();
  } else if (FPDFAvail_IsLinearized(fpdf_availability()) == PDF_LINEARIZED &&
             FPDF_GetPageCount(doc()) == 1) {
    // If we have only one page we should load form first, bacause it is may be
    // XFA document. And after loading form the page count and its contents may
    // be changed.
    LoadForm();
    if (document_->form_status() == PDF_FORM_NOTAVAIL)
      return;
  }
  LoadPages();
}

void PDFiumEngine::LoadPages() {
  if (pages_.empty()) {
    if (!doc_loader_->IsDocumentComplete()) {
      // Check if the first page is available.  In a linearized PDF, that is not
      // always page 0.  Doing this gives us the default page size, since when
      // the document is available, the first page is available as well.
      CheckPageAvailable(FPDFAvail_GetFirstPageNum(doc()), &pending_pages_);
    }
    DCHECK(!document_loaded_);
    LoadPageInfo();
  }
}

void PDFiumEngine::LoadForm() {
  if (form())
    return;

  DCHECK(doc());
  document_->SetFormStatus();
  if (document_->form_status() != PDF_FORM_NOTAVAIL ||
      doc_loader_->IsDocumentComplete()) {
    {
      ScopedUnsupportedFeature scoped_unsupported_feature(this);
      document_->InitializeForm(&form_filler_);
    }

    if (form_filler_.script_option() ==
        PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA) {
      FPDF_LoadXFA(doc());
    }
    FPDF_SetFormFieldHighlightColor(form(), FPDF_FORMFIELD_UNKNOWN,
                                    kFormHighlightColor);
    FPDF_SetFormFieldHighlightAlpha(form(), kFormHighlightAlpha);

    if (base::FeatureList::IsEnabled(features::kTabAcrossPDFAnnotations) &&
        !client_->IsPrintPreview()) {
      static constexpr FPDF_ANNOTATION_SUBTYPE kFocusableAnnotSubtypes[] = {
          FPDF_ANNOT_LINK, FPDF_ANNOT_HIGHLIGHT, FPDF_ANNOT_WIDGET};
      FPDF_BOOL ret = FPDFAnnot_SetFocusableSubtypes(
          form(), kFocusableAnnotSubtypes, base::size(kFocusableAnnotSubtypes));
      DCHECK(ret);
    }
  }
}

void PDFiumEngine::CalculateVisiblePages() {
  // Early return if the PDF isn't being loaded or if we don't have the document
  // info yet. The latter is important because otherwise as the PDF is being
  // initialized by the renderer there could be races that call this method
  // before we get the initial network responses. The document loader depends on
  // the list of pending requests to be valid for progressive loading to
  // function.
  if (!doc_loader_ || pages_.empty())
    return;
  // Clear pending requests queue, since it may contain requests to the pages
  // that are already invisible (after scrolling for example).
  pending_pages_.clear();
  doc_loader_->ClearPendingRequests();

  visible_pages_.clear();
  gfx::Rect visible_rect(plugin_size_);
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    // Check an entire PageScreenRect, since we might need to repaint side
    // borders and shadows even if the page itself is not visible.
    // For example, when user use pdf with different page sizes and zoomed in
    // outside page area.
    if (visible_rect.Intersects(GetPageScreenRect(i))) {
      visible_pages_.push_back(i);
      CheckPageAvailable(i, &pending_pages_);
    } else {
      // Need to unload pages when we're not using them, since some PDFs use a
      // lot of memory.  See http://crbug.com/48791
      if (defer_page_unload_) {
        deferred_page_unloads_.push_back(i);
      } else {
        pages_[i]->Unload();
      }
    }
  }

  // Any pending highlighting of form fields will be invalid since these are in
  // screen coordinates.
  form_highlights_.clear();

  std::vector<draw_utils::IndexedPage> visible_pages_rects;
  visible_pages_rects.reserve(visible_pages_.size());
  for (int visible_page_index : visible_pages_) {
    visible_pages_rects.emplace_back(visible_page_index,
                                     pages_[visible_page_index]->rect());
  }

  int most_visible_page =
      draw_utils::GetMostVisiblePage(visible_pages_rects, GetVisibleRect());
  SetCurrentPage(most_visible_page);
}

bool PDFiumEngine::IsPageVisible(int index) const {
  return base::Contains(visible_pages_, index);
}

void PDFiumEngine::ScrollToPage(int page) {
  if (!PageIndexInBounds(page))
    return;

  in_flight_visible_page_ = page;
  client_->ScrollToPage(page);
}

bool PDFiumEngine::CheckPageAvailable(int index, std::vector<int>* pending) {
  if (!doc())
    return false;

  const int num_pages = static_cast<int>(pages_.size());
  if (index < num_pages && pages_[index]->available())
    return true;

  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  if (!FPDFAvail_IsPageAvail(fpdf_availability(), index, &download_hints)) {
    if (!base::Contains(*pending, index))
      pending->push_back(index);
    return false;
  }

  if (index < num_pages)
    pages_[index]->MarkAvailable();
  if (default_page_size_.IsEmpty())
    default_page_size_ = GetPageSize(index);
  return true;
}

gfx::Size PDFiumEngine::GetPageSize(int index) {
  return GetPageSizeForLayout(index, layout_.options());
}

gfx::Size PDFiumEngine::GetPageSizeForLayout(
    int index,
    const DocumentLayout::Options& layout_options) {
  FS_SIZEF size_in_points;
  if (!FPDF_GetPageSizeByIndexF(doc(), index, &size_in_points))
    return gfx::Size();

  int width_in_pixels = static_cast<int>(
      ConvertUnitDouble(size_in_points.width, kPointsPerInch, kPixelsPerInch));
  int height_in_pixels = static_cast<int>(
      ConvertUnitDouble(size_in_points.height, kPointsPerInch, kPixelsPerInch));

  switch (layout_options.default_page_orientation()) {
    case PageOrientation::kOriginal:
    case PageOrientation::kClockwise180:
      // No axis swap needed.
      break;
    case PageOrientation::kClockwise90:
    case PageOrientation::kClockwise270:
      // Rotated 90 degrees: swap axes.
      std::swap(width_in_pixels, height_in_pixels);
      break;
  }

  return gfx::Size(width_in_pixels, height_in_pixels);
}

draw_utils::PageInsetSizes PDFiumEngine::GetInsetSizes(
    const DocumentLayout::Options& layout_options,
    size_t page_index,
    size_t num_of_pages) const {
  DCHECK_LT(page_index, num_of_pages);

  if (layout_options.two_up_view_enabled()) {
    return draw_utils::GetPageInsetsForTwoUpView(
        page_index, num_of_pages, DocumentLayout::kSingleViewInsets,
        DocumentLayout::kHorizontalSeparator);
  }

  return DocumentLayout::kSingleViewInsets;
}

void PDFiumEngine::EnlargePage(const DocumentLayout::Options& layout_options,
                               size_t page_index,
                               size_t num_of_pages,
                               gfx::Size* page_size) const {
  draw_utils::PageInsetSizes inset_sizes =
      GetInsetSizes(layout_options, page_index, num_of_pages);
  page_size->Enlarge(inset_sizes.left + inset_sizes.right,
                     inset_sizes.top + inset_sizes.bottom);
}

void PDFiumEngine::InsetPage(const DocumentLayout::Options& layout_options,
                             size_t page_index,
                             size_t num_of_pages,
                             double multiplier,
                             gfx::Rect& rect) const {
  draw_utils::PageInsetSizes inset_sizes =
      GetInsetSizes(layout_options, page_index, num_of_pages);
  rect.Inset(static_cast<int>(ceil(inset_sizes.left * multiplier)),
             static_cast<int>(ceil(inset_sizes.top * multiplier)),
             static_cast<int>(ceil(inset_sizes.right * multiplier)),
             static_cast<int>(ceil(inset_sizes.bottom * multiplier)));
}

base::Optional<size_t> PDFiumEngine::GetAdjacentPageIndexForTwoUpView(
    size_t page_index,
    size_t num_of_pages) const {
  DCHECK_LT(page_index, num_of_pages);

  if (!layout_.options().two_up_view_enabled())
    return base::nullopt;

  int adjacent_page_offset = page_index % 2 ? -1 : 1;
  size_t adjacent_page_index = page_index + adjacent_page_offset;
  if (adjacent_page_index >= num_of_pages)
    return base::nullopt;

  return adjacent_page_index;
}

int PDFiumEngine::StartPaint(int page_index, const gfx::Rect& dirty) {
  // For the first time we hit paint, do nothing and just record the paint for
  // the next callback.  This keeps the UI responsive in case the user is doing
  // a lot of scrolling.
  progressive_paints_.emplace_back(page_index, dirty);
  return progressive_paints_.size() - 1;
}

bool PDFiumEngine::ContinuePaint(int progressive_index, SkBitmap& image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  last_progressive_start_time_ = base::Time::Now();
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetLastInstance(client_->GetPluginInstance());
#endif

  int page_index = progressive_paints_[progressive_index].page_index();
  DCHECK(PageIndexInBounds(page_index));

  int rv;
  FPDF_PAGE page = pages_[page_index]->GetPage();
  if (progressive_paints_[progressive_index].bitmap()) {
    rv = FPDF_RenderPage_Continue(page, this);
  } else {
    int start_x;
    int start_y;
    int size_x;
    int size_y;
    gfx::Rect dirty = progressive_paints_[progressive_index].rect();
    GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);

    ScopedFPDFBitmap new_bitmap = CreateBitmap(dirty, image_data);
    FPDFBitmap_FillRect(new_bitmap.get(), start_x, start_y, size_x, size_y,
                        0xFFFFFFFF);
    rv = FPDF_RenderPageBitmap_Start(
        new_bitmap.get(), page, start_x, start_y, size_x, size_y,
        ToPDFiumRotation(layout_.options().default_page_orientation()),
        GetRenderingFlags(), this);
    progressive_paints_[progressive_index].SetBitmapAndImageData(
        std::move(new_bitmap), image_data);
  }
  return rv != FPDF_RENDER_TOBECONTINUED;
}

void PDFiumEngine::FinishPaint(int progressive_index, SkBitmap& image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index();
  gfx::Rect dirty_in_screen = progressive_paints_[progressive_index].rect();

  int start_x;
  int start_y;
  int size_x;
  int size_y;
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap();
  GetPDFiumRect(page_index, dirty_in_screen, &start_x, &start_y, &size_x,
                &size_y);

  // Draw the forms.
  FPDF_FFLDraw(form(), bitmap, pages_[page_index]->GetPage(), start_x, start_y,
               size_x, size_y,
               ToPDFiumRotation(layout_.options().default_page_orientation()),
               GetRenderingFlags());

  FillPageSides(progressive_index);

  // Paint the page shadows.
  PaintPageShadow(progressive_index, image_data);

  DrawSelections(progressive_index, image_data);
  form_highlights_.clear();

  FPDF_RenderPage_Close(pages_[page_index]->GetPage());
  progressive_paints_.erase(progressive_paints_.begin() + progressive_index);
}

void PDFiumEngine::CancelPaints() {
  for (const auto& paint : progressive_paints_)
    FPDF_RenderPage_Close(pages_[paint.page_index()]->GetPage());

  progressive_paints_.clear();
}

void PDFiumEngine::FillPageSides(int progressive_index) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index();
  gfx::Rect dirty_in_screen = progressive_paints_[progressive_index].rect();
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap();
  draw_utils::PageInsetSizes inset_sizes =
      GetInsetSizes(layout_.options(), page_index, pages_.size());

  gfx::Rect page_rect = pages_[page_index]->rect();
  const bool is_two_up_view = layout_.options().two_up_view_enabled();
  if (page_rect.x() > 0 && (!is_two_up_view || page_index % 2 == 0)) {
    // If in two-up view, only need to draw the left empty space for left pages
    // since the gap between the left and right page will be drawn by the left
    // page.
    gfx::Rect left_in_screen = GetScreenRect(draw_utils::GetLeftFillRect(
        page_rect, inset_sizes, DocumentLayout::kBottomSeparator));
    left_in_screen.Intersect(dirty_in_screen);

    FPDFBitmap_FillRect(bitmap, left_in_screen.x() - dirty_in_screen.x(),
                        left_in_screen.y() - dirty_in_screen.y(),
                        left_in_screen.width(), left_in_screen.height(),
                        client_->GetBackgroundColor());
  }

  if (page_rect.right() < layout_.size().width()) {
    gfx::Rect right_in_screen = GetScreenRect(draw_utils::GetRightFillRect(
        page_rect, inset_sizes, layout_.size().width(),
        DocumentLayout::kBottomSeparator));
    right_in_screen.Intersect(dirty_in_screen);

    FPDFBitmap_FillRect(bitmap, right_in_screen.x() - dirty_in_screen.x(),
                        right_in_screen.y() - dirty_in_screen.y(),
                        right_in_screen.width(), right_in_screen.height(),
                        client_->GetBackgroundColor());
  }

  gfx::Rect bottom_in_screen;
  if (is_two_up_view) {
    gfx::Rect page_in_screen = GetScreenRect(page_rect);
    bottom_in_screen = draw_utils::GetBottomGapBetweenRects(
        page_in_screen.bottom(), dirty_in_screen);

    if (page_index % 2 == 1) {
      draw_utils::AdjustBottomGapForRightSidePage(page_in_screen.x(),
                                                  &bottom_in_screen);
    }

    bottom_in_screen.Intersect(dirty_in_screen);
  } else {
    bottom_in_screen = GetScreenRect(draw_utils::GetBottomFillRect(
        page_rect, inset_sizes, DocumentLayout::kBottomSeparator));
    bottom_in_screen.Intersect(dirty_in_screen);
  }

  FPDFBitmap_FillRect(bitmap, bottom_in_screen.x() - dirty_in_screen.x(),
                      bottom_in_screen.y() - dirty_in_screen.y(),
                      bottom_in_screen.width(), bottom_in_screen.height(),
                      client_->GetBackgroundColor());
}

void PDFiumEngine::PaintPageShadow(int progressive_index,
                                   SkBitmap& image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index();
  gfx::Rect dirty_in_screen = progressive_paints_[progressive_index].rect();
  gfx::Rect page_rect = pages_[page_index]->rect();
  gfx::Rect shadow_rect(page_rect);
  InsetPage(layout_.options(), page_index, pages_.size(), /*multiplier=*/-1,
            shadow_rect);

  // Due to the rounding errors of the GetScreenRect it is possible to get
  // different size shadows on the left and right sides even they are defined
  // the same. To fix this issue let's calculate shadow rect and then shrink
  // it by the size of the shadows.
  shadow_rect = GetScreenRect(shadow_rect);
  page_rect = shadow_rect;
  InsetPage(layout_.options(), page_index, pages_.size(),
            /*multiplier=*/current_zoom_, page_rect);

  DrawPageShadow(page_rect, shadow_rect, dirty_in_screen, image_data);
}

void PDFiumEngine::DrawSelections(int progressive_index,
                                  SkBitmap& image_data) const {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index();
  gfx::Rect dirty_in_screen = progressive_paints_[progressive_index].rect();

  void* region = nullptr;
  int stride;
  GetRegion(dirty_in_screen.origin(), image_data, region, stride);

  std::vector<gfx::Rect> highlighted_rects;
  gfx::Rect visible_rect = GetVisibleRect();
  for (const auto& range : selection_) {
    if (range.page_index() != page_index)
      continue;

    const std::vector<gfx::Rect>& rects =
        range.GetScreenRects(visible_rect.origin(), current_zoom_,
                             layout_.options().default_page_orientation());
    for (const auto& rect : rects) {
      gfx::Rect visible_selection = gfx::IntersectRects(rect, dirty_in_screen);
      if (visible_selection.IsEmpty())
        continue;

      visible_selection.Offset(-dirty_in_screen.OffsetFromOrigin());
      Highlight(region, stride, visible_selection, kHighlightColorR,
                kHighlightColorG, kHighlightColorB, highlighted_rects);
    }
  }

  for (const auto& highlight : form_highlights_) {
    gfx::Rect visible_selection =
        gfx::IntersectRects(highlight, dirty_in_screen);
    if (visible_selection.IsEmpty())
      continue;

    visible_selection.Offset(-dirty_in_screen.OffsetFromOrigin());
    Highlight(region, stride, visible_selection, kHighlightColorR,
              kHighlightColorG, kHighlightColorB, highlighted_rects);
  }
}

void PDFiumEngine::PaintUnavailablePage(int page_index,
                                        const gfx::Rect& dirty,
                                        SkBitmap& image_data) {
  int start_x;
  int start_y;
  int size_x;
  int size_y;
  GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);
  ScopedFPDFBitmap bitmap(CreateBitmap(dirty, image_data));
  FPDFBitmap_FillRect(bitmap.get(), start_x, start_y, size_x, size_y,
                      kPendingPageColor);

  gfx::Rect loading_text_in_screen(
      pages_[page_index]->rect().width() / 2,
      pages_[page_index]->rect().y() + kLoadingTextVerticalOffset, 0, 0);
  loading_text_in_screen = GetScreenRect(loading_text_in_screen);
}

int PDFiumEngine::GetProgressiveIndex(int page_index) const {
  for (size_t i = 0; i < progressive_paints_.size(); ++i) {
    if (progressive_paints_[i].page_index() == page_index)
      return i;
  }
  return -1;
}

ScopedFPDFBitmap PDFiumEngine::CreateBitmap(const gfx::Rect& rect,
                                            SkBitmap& image_data) const {
  void* region;
  int stride;
  GetRegion(rect.origin(), image_data, region, stride);
  if (!region)
    return nullptr;
  return ScopedFPDFBitmap(FPDFBitmap_CreateEx(rect.width(), rect.height(),
                                              FPDFBitmap_BGRx, region, stride));
}

void PDFiumEngine::GetPDFiumRect(int page_index,
                                 const gfx::Rect& rect,
                                 int* start_x,
                                 int* start_y,
                                 int* size_x,
                                 int* size_y) const {
  gfx::Rect page_rect = GetScreenRect(pages_[page_index]->rect());
  page_rect.Offset(-rect.x(), -rect.y());

  *start_x = page_rect.x();
  *start_y = page_rect.y();
  *size_x = page_rect.width();
  *size_y = page_rect.height();
}

int PDFiumEngine::GetRenderingFlags() const {
  int flags = FPDF_LCD_TEXT;
  if (render_grayscale_)
    flags |= FPDF_GRAYSCALE;
  if (client_->IsPrintPreview())
    flags |= FPDF_PRINTING;
  if (render_annots_)
    flags |= FPDF_ANNOT;
  return flags;
}

gfx::Rect PDFiumEngine::GetVisibleRect() const {
  gfx::Rect rv;
  rv.set_x(static_cast<int>(position_.x() / current_zoom_));
  rv.set_y(static_cast<int>(position_.y() / current_zoom_));
  rv.set_width(static_cast<int>(ceil(plugin_size_.width() / current_zoom_)));
  rv.set_height(static_cast<int>(ceil(plugin_size_.height() / current_zoom_)));
  return rv;
}

gfx::Rect PDFiumEngine::GetPageScreenRect(int page_index) const {
  gfx::Rect page_rect = pages_[page_index]->rect();
  draw_utils::PageInsetSizes inset_sizes =
      GetInsetSizes(layout_.options(), page_index, pages_.size());

  int max_page_height = page_rect.height();
  base::Optional<size_t> adjacent_page_index =
      GetAdjacentPageIndexForTwoUpView(page_index, pages_.size());
  if (adjacent_page_index.has_value()) {
    max_page_height = std::max(
        max_page_height, pages_[adjacent_page_index.value()]->rect().height());
  }

  return GetScreenRect(draw_utils::GetSurroundingRect(
      page_rect.y(), max_page_height, inset_sizes, layout_.size().width(),
      DocumentLayout::kBottomSeparator));
}

gfx::Rect PDFiumEngine::GetScreenRect(const gfx::Rect& rect) const {
  return draw_utils::GetScreenRect(rect, position_, current_zoom_);
}

void PDFiumEngine::Highlight(void* buffer,
                             int stride,
                             const gfx::Rect& rect,
                             int color_red,
                             int color_green,
                             int color_blue,
                             std::vector<gfx::Rect>& highlighted_rects) const {
  if (!buffer)
    return;

  gfx::Rect new_rect = rect;
  for (const auto& highlighted : highlighted_rects)
    new_rect.Subtract(highlighted);
  if (new_rect.IsEmpty())
    return;

  std::vector<size_t> overlapping_rect_indices;
  for (size_t i = 0; i < highlighted_rects.size(); ++i) {
    if (new_rect.Intersects((highlighted_rects)[i]))
      overlapping_rect_indices.push_back(i);
  }

  highlighted_rects.push_back(new_rect);
  int l = new_rect.x();
  int t = new_rect.y();
  int w = new_rect.width();
  int h = new_rect.height();

  for (int y = t; y < t + h; ++y) {
    for (int x = l; x < l + w; ++x) {
      bool overlaps = false;
      for (size_t i : overlapping_rect_indices) {
        const auto& highlighted = (highlighted_rects)[i];
        if (highlighted.Contains(x, y)) {
          overlaps = true;
          break;
        }
      }
      if (overlaps)
        continue;

      uint8_t* pixel = static_cast<uint8_t*>(buffer) + y * stride + x * 4;
      pixel[0] = static_cast<uint8_t>(pixel[0] * (color_blue / 255.0));
      pixel[1] = static_cast<uint8_t>(pixel[1] * (color_green / 255.0));
      pixel[2] = static_cast<uint8_t>(pixel[2] * (color_red / 255.0));
    }
  }
}

PDFiumEngine::SelectionChangeInvalidator::SelectionChangeInvalidator(
    PDFiumEngine* engine)
    : engine_(engine),
      previous_origin_(engine_->GetVisibleRect().origin()),
      old_selections_(GetVisibleSelections()) {}

PDFiumEngine::SelectionChangeInvalidator::~SelectionChangeInvalidator() {
  // Offset the old selections if the document scrolled since we recorded them.
  gfx::Vector2d offset = previous_origin_ - engine_->GetVisibleRect().origin();
  for (auto& old_selection : old_selections_)
    old_selection.Offset(offset);

  std::vector<gfx::Rect> new_selections = GetVisibleSelections();
  for (auto& new_selection : new_selections) {
    for (auto& old_selection : old_selections_) {
      if (!old_selection.IsEmpty() && new_selection == old_selection) {
        // Rectangle was selected before and after, so no need to invalidate it.
        // Mark the rectangles by setting them to empty.
        new_selection = old_selection = gfx::Rect();
        break;
      }
    }
  }

  bool selection_changed = false;
  for (const auto& old_selection : old_selections_) {
    if (!old_selection.IsEmpty()) {
      Invalidate(old_selection);
      selection_changed = true;
    }
  }
  for (const auto& new_selection : new_selections) {
    if (!new_selection.IsEmpty()) {
      Invalidate(new_selection);
      selection_changed = true;
    }
  }

  if (selection_changed) {
    engine_->OnSelectionTextChanged();
    engine_->OnSelectionPositionChanged();
  }
}

std::vector<gfx::Rect>
PDFiumEngine::SelectionChangeInvalidator::GetVisibleSelections() const {
  std::vector<gfx::Rect> rects;
  gfx::Point visible_point = engine_->GetVisibleRect().origin();
  for (const auto& range : engine_->selection_) {
    // Exclude selections on pages that's not currently visible.
    if (!engine_->IsPageVisible(range.page_index()))
      continue;

    const std::vector<gfx::Rect>& selection_rects = range.GetScreenRects(
        visible_point, engine_->current_zoom_,
        engine_->layout_.options().default_page_orientation());
    rects.insert(rects.end(), selection_rects.begin(), selection_rects.end());
  }
  return rects;
}

void PDFiumEngine::SelectionChangeInvalidator::Invalidate(
    const gfx::Rect& selection) {
  gfx::Rect expanded_selection = selection;
  expanded_selection.Inset(-1, -1);
  engine_->client_->Invalidate(expanded_selection);
}

PDFiumEngine::MouseDownState::MouseDownState(
    const PDFiumPage::Area& area,
    const PDFiumPage::LinkTarget& target)
    : area_(area), target_(target) {}

PDFiumEngine::MouseDownState::~MouseDownState() = default;

void PDFiumEngine::MouseDownState::Set(const PDFiumPage::Area& area,
                                       const PDFiumPage::LinkTarget& target) {
  area_ = area;
  target_ = target;
}

void PDFiumEngine::MouseDownState::Reset() {
  area_ = PDFiumPage::NONSELECTABLE_AREA;
  target_ = PDFiumPage::LinkTarget();
}

bool PDFiumEngine::MouseDownState::Matches(
    const PDFiumPage::Area& area,
    const PDFiumPage::LinkTarget& target) const {
  if (area_ != area)
    return false;

  if (area == PDFiumPage::WEBLINK_AREA)
    return target_.url == target.url;

  if (area == PDFiumPage::DOCLINK_AREA)
    return target_.page == target.page;

  return true;
}

void PDFiumEngine::DeviceToPage(int page_index,
                                const gfx::Point& device_point,
                                double* page_x,
                                double* page_y) {
  *page_x = *page_y = 0;
  float device_x = device_point.x();
  float device_y = device_point.y();
  int temp_x = static_cast<int>((device_x + position_.x()) / current_zoom_ -
                                pages_[page_index]->rect().x());
  int temp_y = static_cast<int>((device_y + position_.y()) / current_zoom_ -
                                pages_[page_index]->rect().y());
  FPDF_BOOL ret = FPDF_DeviceToPage(
      pages_[page_index]->GetPage(), 0, 0, pages_[page_index]->rect().width(),
      pages_[page_index]->rect().height(),
      ToPDFiumRotation(layout_.options().default_page_orientation()), temp_x,
      temp_y, page_x, page_y);
  DCHECK(ret);
}

int PDFiumEngine::GetVisiblePageIndex(FPDF_PAGE page) {
  // Copy |visible_pages_| since it can change as a result of loading the page
  // in GetPage(). See https://crbug.com/822091.
  std::vector<int> visible_pages_copy(visible_pages_);
  for (int page_index : visible_pages_copy) {
    if (pages_[page_index]->GetPage() == page)
      return page_index;
  }
  return -1;
}

void PDFiumEngine::SetCurrentPage(int index) {
  in_flight_visible_page_.reset();

  if (index == most_visible_page_ || !form())
    return;

  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE old_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(old_page, form(), FPDFPAGE_AACTION_CLOSE);
  }
  most_visible_page_ = index;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  SetLastInstance(client_->GetPluginInstance());
#endif
  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form(), FPDFPAGE_AACTION_OPEN);
  }
}

void PDFiumEngine::DrawPageShadow(const gfx::Rect& page_rc,
                                  const gfx::Rect& shadow_rc,
                                  const gfx::Rect& clip_rc,
                                  SkBitmap& image_data) {
  gfx::Rect page_rect(page_rc);
  page_rect.Offset(page_offset_);

  gfx::Rect shadow_rect(shadow_rc);
  shadow_rect.Offset(page_offset_);

  gfx::Rect clip_rect(clip_rc);
  clip_rect.Offset(page_offset_);

  // Page drop shadow parameters.
  constexpr double factor = 0.5;
  uint32_t depth = std::max({page_rect.x() - shadow_rect.x(),
                             page_rect.y() - shadow_rect.y(),
                             shadow_rect.right() - page_rect.right(),
                             shadow_rect.bottom() - page_rect.bottom()});
  depth = static_cast<uint32_t>(depth * 1.5) + 1;

  // We need to check depth only to verify our copy of shadow matrix is correct.
  if (!page_shadow_ || page_shadow_->depth() != depth) {
    page_shadow_ = std::make_unique<draw_utils::ShadowMatrix>(
        depth, factor, client_->GetBackgroundColor());
  }

  DCHECK(!image_data.isNull());
  DrawShadow(image_data, shadow_rect, page_rect, clip_rect, *page_shadow_);
}

void PDFiumEngine::GetRegion(const gfx::Point& location,
                             SkBitmap& image_data,
                             void*& region,
                             int& stride) const {
  if (image_data.isNull()) {
    DCHECK(plugin_size_.IsEmpty());
    stride = 0;
    region = nullptr;
    return;
  }
  char* buffer = static_cast<char*>(image_data.getPixels());
  stride = image_data.rowBytes();

  gfx::Point offset_location = location + page_offset_;
  // TODO: update this when we support BIDI and scrollbars can be on the left.
  if (!buffer ||
      !gfx::Rect(gfx::PointAtOffsetFromOrigin(page_offset_), plugin_size_)
           .Contains(offset_location)) {
    region = nullptr;
    return;
  }

  buffer += location.y() * stride;
  buffer += (location.x() + page_offset_.x()) * 4;
  region = buffer;
}

void PDFiumEngine::OnSelectionTextChanged() {
  DCHECK(!in_form_text_area_);
  pp::PDF::SetSelectedText(GetPluginInstance(), GetSelectedText().c_str());
}

void PDFiumEngine::OnSelectionPositionChanged() {
  // We need to determine the top-left and bottom-right points of the selection
  // in order to report those to the embedder. This code assumes that the
  // selection list is out of order.
  gfx::Rect left(std::numeric_limits<int32_t>::max(),
                 std::numeric_limits<int32_t>::max(), 0, 0);
  gfx::Rect right;
  for (const auto& sel : selection_) {
    const std::vector<gfx::Rect>& screen_rects =
        sel.GetScreenRects(GetVisibleRect().origin(), current_zoom_,
                           layout_.options().default_page_orientation());
    for (const auto& rect : screen_rects) {
      if (IsAboveOrDirectlyLeftOf(rect, left))
        left = rect;
      if (IsAboveOrDirectlyLeftOf(right, rect))
        right = rect;
    }
  }
  right.set_x(right.x() + right.width());
  if (left.IsEmpty()) {
    left.set_x(0);
    left.set_y(0);
  }
  client_->SelectionChanged(left, right);
}

gfx::Size PDFiumEngine::ApplyDocumentLayout(
    const DocumentLayout::Options& options) {
  // We need to return early if the layout would not change, otherwise calling
  // client_->ScrollToPage() would send another "viewport" message, triggering
  // an infinite loop.
  //
  // TODO(crbug.com/1013800): The current implementation computes layout twice
  // (here, and in InvalidateAllPages()). This shouldn't be too expensive at
  // realistic page counts, but could be avoided.
  layout_.SetOptions(options);
  UpdateDocumentLayout(&layout_);
  if (!layout_.dirty())
    return layout_.size();

  // Store the current find index so that we can resume finding at that
  // particular index after we have recomputed the find results.
  std::string current_find_text = current_find_text_;
  resume_find_index_ = current_find_index_;

  // Save the current page.
  int most_visible_page = most_visible_page_;

  InvalidateAllPages();

  // Restore find results.
  if (!current_find_text.empty()) {
    // Clear the UI.
    client_->NotifyNumberOfFindResultsChanged(0, false);
    StartFind(current_find_text, false);
  }

  // Restore current page. After a rotation, the page heights have changed but
  // the scroll position has not. Re-adjust.
  // TODO(thestig): It would be better to also restore the position on the page.
  client_->ScrollToPage(most_visible_page);

  return layout_.size();
}

void PDFiumEngine::SetSelecting(bool selecting) {
  bool was_selecting = selecting_;
  selecting_ = selecting;
  if (selecting_ != was_selecting)
    client_->IsSelectingChanged(selecting);
}

void PDFiumEngine::EnteredEditMode() {
  if (edit_mode_)
    return;

  edit_mode_ = true;
  client_->EnteredEditMode();
}

void PDFiumEngine::SetInFormTextArea(bool in_form_text_area) {
  // If focus was previously in form text area, clear form text selection.
  // Clearing needs to be done before changing focus to ensure the correct
  // observer is notified of the change in selection. When |in_form_text_area_|
  // is true, this is the Renderer. After it flips, the MimeHandler is notified.
  if (in_form_text_area_) {
    SetSelectedTextFunction set_selected_text_func =
        g_set_selected_text_func_for_testing
            ? g_set_selected_text_func_for_testing
            : &SetSelectedText;
    set_selected_text_func(GetPluginInstance(), "");
  }

  client_->FormTextFieldFocusChange(in_form_text_area);
  in_form_text_area_ = in_form_text_area;

  // Clear |editable_form_text_area_| when focus no longer in form text area.
  if (!in_form_text_area_)
    editable_form_text_area_ = false;
}

void PDFiumEngine::SetMouseLeftButtonDown(bool is_mouse_left_button_down) {
  mouse_left_button_down_ = is_mouse_left_button_down;
}

bool PDFiumEngine::IsAnnotationAnEditableFormTextArea(FPDF_ANNOTATION annot,
                                                      int form_type) const {
#if defined(PDF_ENABLE_XFA)
  if (IS_XFA_FORMFIELD(form_type)) {
    return form_type == FPDF_FORMFIELD_XFA_TEXTFIELD ||
           form_type == FPDF_FORMFIELD_XFA_COMBOBOX;
  }
#endif  // defined(PDF_ENABLE_XFA)

  if (!annot)
    return false;

  int flags = FPDFAnnot_GetFormFieldFlags(form(), annot);
  return CheckIfEditableFormTextArea(flags, form_type);
}

void PDFiumEngine::ScheduleTouchTimer(const TouchInputEvent& evt) {
  touch_timer_.Start(FROM_HERE, kTouchLongPressTimeout,
                     base::BindOnce(&PDFiumEngine::HandleLongPress,
                                    base::Unretained(this), evt));
}

void PDFiumEngine::KillTouchTimer() {
  touch_timer_.Stop();
}

bool PDFiumEngine::PageIndexInBounds(int index) const {
  return index >= 0 && index < static_cast<int>(pages_.size());
}

bool PDFiumEngine::IsPageCharacterIndexInBounds(
    const PP_PdfPageCharacterIndex& index) const {
  return PageIndexInBounds(index.page_index) &&
         pages_[index.page_index]->IsCharIndexInBounds(index.char_index);
}

float PDFiumEngine::GetToolbarHeightInScreenCoords() {
  return client_->GetToolbarHeightInScreenCoords();
}

FPDF_BOOL PDFiumEngine::Pause_NeedToPauseNow(IFSDK_PAUSE* param) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  return base::Time::Now() - engine->last_progressive_start_time_ >
         engine->progressive_paint_timeout_;
}

void PDFiumEngine::SetSelection(
    const PP_PdfPageCharacterIndex& selection_start_index,
    const PP_PdfPageCharacterIndex& selection_end_index) {
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();

  PP_PdfPageCharacterIndex sel_start_index = selection_start_index;
  PP_PdfPageCharacterIndex sel_end_index = selection_end_index;
  if (sel_end_index.page_index < sel_start_index.page_index) {
    std::swap(sel_end_index.page_index, sel_start_index.page_index);
    std::swap(sel_end_index.char_index, sel_start_index.char_index);
  }

  if (sel_end_index.page_index == sel_start_index.page_index &&
      sel_end_index.char_index < sel_start_index.char_index) {
    std::swap(sel_end_index.char_index, sel_start_index.char_index);
  }

  for (uint32_t i = sel_start_index.page_index; i <= sel_end_index.page_index;
       ++i) {
    int32_t char_count = pages_[i]->GetCharCount();
    if (char_count <= 0)
      continue;
    int32_t start_char_index = 0;
    int32_t end_char_index = char_count;
    if (i == sel_start_index.page_index)
      start_char_index = sel_start_index.char_index;
    if (i == sel_end_index.page_index)
      end_char_index = sel_end_index.char_index;
    selection_.push_back(PDFiumRange(pages_[i].get(), start_char_index,
                                     end_char_index - start_char_index));
  }
}

void PDFiumEngine::ScrollFocusedAnnotationIntoView() {
  FPDF_ANNOTATION annot;
  int page_index;
  if (!FORM_GetFocusedAnnot(form(), &page_index, &annot))
    return;

  ScrollAnnotationIntoView(annot, page_index);
  FPDFPage_CloseAnnot(annot);
}

void PDFiumEngine::ScrollAnnotationIntoView(FPDF_ANNOTATION annot,
                                            int page_index) {
  if (!PageIndexInBounds(page_index))
    return;

  FS_RECTF annot_rect;
  if (!FPDFAnnot_GetRect(annot, &annot_rect))
    return;

  gfx::Rect rect = pages_[page_index]->PageToScreen(
      gfx::Point(), /*zoom=*/1.0, annot_rect.left, annot_rect.top,
      annot_rect.right, annot_rect.bottom,
      layout_.options().default_page_orientation());

  gfx::Rect visible_rect = GetVisibleRect();
  if (visible_rect.Contains(rect))
    return;
  // Since the focus rect is not already in the visible area, scrolling
  // horizontally and/or vertically is required.
  if (rect.y() < visible_rect.y() || rect.bottom() > visible_rect.bottom()) {
    // Scroll the viewport vertically to align the top of focus rect to
    // centre.
    client_->ScrollToY(rect.y() * current_zoom_ - plugin_size_.height() / 2,
                       /*compensate_for_toolbar=*/false);
  }
  if (rect.x() < visible_rect.x() || rect.right() > visible_rect.right()) {
    // Scroll the viewport horizontally to align the left of focus rect to
    // centre.
    client_->ScrollToX(rect.x() * current_zoom_ - plugin_size_.width() / 2);
  }
}

void PDFiumEngine::OnFocusedAnnotationUpdated(FPDF_ANNOTATION annot,
                                              int page_index) {
  SetLinkUnderCursorForAnnotation(annot, page_index);
  int form_type = FPDFAnnot_GetFormFieldType(form(), annot);
  if (form_type <= FPDF_FORMFIELD_UNKNOWN) {
    SetInFormTextArea(false);
    return;
  }
  bool is_form_text_area =
      PDFiumPage::FormTypeToArea(form_type) == PDFiumPage::FORM_TEXT_AREA;
  if (is_form_text_area) {
    SelectionChangeInvalidator selection_invalidator(this);
    selection_.clear();
  }
  SetInFormTextArea(is_form_text_area);
  editable_form_text_area_ =
      is_form_text_area && IsAnnotationAnEditableFormTextArea(annot, form_type);
}

void PDFiumEngine::SetCaretPosition(const gfx::Point& position) {
  // TODO(dsinclair): Handle caret position ...
}

void PDFiumEngine::MoveRangeSelectionExtent(const gfx::Point& extent) {
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  GetCharIndex(extent, &page_index, &char_index, &form_type, &target);
  if (page_index < 0 || char_index < 0)
    return;

  SelectionChangeInvalidator selection_invalidator(this);
  if (range_selection_direction_ == RangeSelectionDirection::Right) {
    ExtendSelection(page_index, char_index);
    return;
  }

  // For a left selection we clear the current selection and set a new starting
  // point based on the new left position. We then extend that selection out to
  // the previously provided base location.
  selection_.clear();
  selection_.push_back(PDFiumRange(pages_[page_index].get(), char_index, 0));

  // This should always succeeed because the range selection base should have
  // already been selected.
  GetCharIndex(range_selection_base_, &page_index, &char_index, &form_type,
               &target);
  ExtendSelection(page_index, char_index);
}

void PDFiumEngine::SetSelectionBounds(const gfx::Point& base,
                                      const gfx::Point& extent) {
  range_selection_base_ = base;
  range_selection_direction_ = IsAboveOrDirectlyLeftOf(base, extent)
                                   ? RangeSelectionDirection::Left
                                   : RangeSelectionDirection::Right;
}

void PDFiumEngine::GetSelection(uint32_t* selection_start_page_index,
                                uint32_t* selection_start_char_index,
                                uint32_t* selection_end_page_index,
                                uint32_t* selection_end_char_index) {
  size_t len = selection_.size();
  if (len == 0) {
    *selection_start_page_index = 0;
    *selection_start_char_index = 0;
    *selection_end_page_index = 0;
    *selection_end_char_index = 0;
    return;
  }

  *selection_start_page_index = selection_[0].page_index();
  *selection_start_char_index = selection_[0].char_index();
  *selection_end_page_index = selection_[len - 1].page_index();

  // If the selection is all within one page, the end index is the
  // start index plus the char count. But if the selection spans
  // multiple pages, the selection starts at the beginning of the
  // last page in |selection_| and goes to the char count.
  if (len == 1) {
    *selection_end_char_index =
        selection_[0].char_index() + selection_[0].char_count();
  } else {
    *selection_end_char_index = selection_[len - 1].char_count();
  }
}

void PDFiumEngine::LoadDocumentAttachmentInfoList() {
  DCHECK(document_loaded_);

  int attachment_count = FPDFDoc_GetAttachmentCount(doc());
  if (attachment_count <= 0)
    return;

  doc_attachment_info_list_.resize(attachment_count);
  for (int i = 0; i < attachment_count; ++i) {
    FPDF_ATTACHMENT attachment = FPDFDoc_GetAttachment(doc(), i);
    DCHECK(attachment);

    doc_attachment_info_list_[i].name = GetAttachmentName(attachment);
    doc_attachment_info_list_[i].creation_date =
        GetAttachmentAttribute(attachment, "CreationDate");
    doc_attachment_info_list_[i].modified_date =
        GetAttachmentAttribute(attachment, "ModDate");

    unsigned long actual_length_bytes;
    doc_attachment_info_list_[i].is_readable =
        FPDFAttachment_GetFile(attachment, /*buffer=*/nullptr,
                               /*buflen=*/0, &actual_length_bytes);
    if (doc_attachment_info_list_[i].is_readable)
      doc_attachment_info_list_[i].size_bytes = actual_length_bytes;
  }
}

void PDFiumEngine::LoadDocumentMetadata() {
  DCHECK(document_loaded_);

  // Document information dictionary entries
  doc_metadata_.version = GetDocumentVersion();
  doc_metadata_.title = GetMetadataByField("Title");
  doc_metadata_.author = GetMetadataByField("Author");
  doc_metadata_.subject = GetMetadataByField("Subject");
  doc_metadata_.creator = GetMetadataByField("Creator");
  doc_metadata_.producer = GetMetadataByField("Producer");
}

std::string PDFiumEngine::GetMetadataByField(FPDF_BYTESTRING field) const {
  DCHECK(doc());

  return base::UTF16ToUTF8(CallPDFiumWideStringBufferApi(
      base::BindRepeating(&FPDF_GetMetaText, doc(), field),
      /*check_expected_size=*/false));
}

PdfVersion PDFiumEngine::GetDocumentVersion() const {
  DCHECK(doc());

  int version;
  if (!FPDF_GetFileVersion(doc(), &version))
    return PdfVersion::kUnknown;

  switch (version) {
    case 10:
      return PdfVersion::k1_0;
    case 11:
      return PdfVersion::k1_1;
    case 12:
      return PdfVersion::k1_2;
    case 13:
      return PdfVersion::k1_3;
    case 14:
      return PdfVersion::k1_4;
    case 15:
      return PdfVersion::k1_5;
    case 16:
      return PdfVersion::k1_6;
    case 17:
      return PdfVersion::k1_7;
    case 18:
      return PdfVersion::k1_8;
    case 20:
      return PdfVersion::k2_0;
    default:
      return PdfVersion::kUnknown;
  }
}

bool PDFiumEngine::HandleTabEvent(uint32_t modifiers) {
  bool alt_key = !!(modifiers & kInputEventModifierAltKey);
  bool ctrl_key = !!(modifiers & kInputEventModifierControlKey);
  if (alt_key || ctrl_key)
    return HandleTabEventWithModifiers(modifiers);

  return modifiers & kInputEventModifierShiftKey ? HandleTabBackward(modifiers)
                                                 : HandleTabForward(modifiers);
}

bool PDFiumEngine::HandleTabEventWithModifiers(uint32_t modifiers) {
  // Only handle cases when a page is focused, else return false.
  switch (focus_item_type_) {
    case FocusElementType::kNone:
    case FocusElementType::kDocument:
      return false;
    case FocusElementType::kPage:
      if (last_focused_page_ == -1)
        return false;
      return !!FORM_OnKeyDown(form(), pages_[last_focused_page_]->GetPage(),
                              FWL_VKEY_Tab, modifiers);
    default:
      NOTREACHED();
      return false;
  }
}

bool PDFiumEngine::HandleTabForward(uint32_t modifiers) {
  if (focus_item_type_ == FocusElementType::kNone) {
    UpdateFocusItemType(FocusElementType::kDocument);
    return true;
  }

  int page_index = last_focused_page_;
  if (page_index == -1)
    page_index = 0;

  bool did_tab_forward = false;
  while (!did_tab_forward && PageIndexInBounds(page_index)) {
    did_tab_forward = !!FORM_OnKeyDown(form(), pages_[page_index]->GetPage(),
                                       FWL_VKEY_Tab, modifiers);
    if (!did_tab_forward)
      ++page_index;
  }

  if (did_tab_forward) {
    last_focused_page_ = page_index;
    UpdateFocusItemType(FocusElementType::kPage);
  } else {
    last_focused_page_ = -1;
    UpdateFocusItemType(FocusElementType::kNone);
  }
  return did_tab_forward;
}

bool PDFiumEngine::HandleTabBackward(uint32_t modifiers) {
  if (focus_item_type_ == FocusElementType::kDocument) {
    UpdateFocusItemType(FocusElementType::kNone);
    return false;
  }

  int page_index = last_focused_page_;
  if (page_index == -1)
    page_index = GetNumberOfPages() - 1;

  bool did_tab_backward = false;
  while (!did_tab_backward && PageIndexInBounds(page_index)) {
    did_tab_backward = !!FORM_OnKeyDown(form(), pages_[page_index]->GetPage(),
                                        FWL_VKEY_Tab, modifiers);
    if (!did_tab_backward)
      --page_index;
  }

  if (did_tab_backward) {
    last_focused_page_ = page_index;
    UpdateFocusItemType(FocusElementType::kPage);
  } else {
    // No focusable annotation found in pages. Possible scenarios:
    // Case 1: |focus_item_type_| is None. Since no object in any page can take
    // the focus, the document should take focus.
    // Case 2: |focus_item_type_| is Page. Since there aren't any objects that
    // could take focus, the document should take focus.
    // Case 3: |focus_item_type_| is Document. Move focus_item_type_ to None.
    switch (focus_item_type_) {
      case FocusElementType::kPage:
      case FocusElementType::kNone:
        did_tab_backward = true;
        last_focused_page_ = -1;
        UpdateFocusItemType(FocusElementType::kDocument);
        KillFormFocus();
        break;
      case FocusElementType::kDocument:
        UpdateFocusItemType(FocusElementType::kNone);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return did_tab_backward;
}

void PDFiumEngine::UpdateFocusItemType(FocusElementType focus_item_type) {
  if (focus_item_type_ == focus_item_type)
    return;
  if (focus_item_type_ == FocusElementType::kDocument)
    client_->DocumentFocusChanged(false);
  focus_item_type_ = focus_item_type;
  if (focus_item_type_ == FocusElementType::kDocument)
    client_->DocumentFocusChanged(true);
}

#if defined(PDF_ENABLE_XFA)
void PDFiumEngine::UpdatePageCount() {
  InvalidateAllPages();
}
#endif  // defined(PDF_ENABLE_XFA)

void PDFiumEngine::UpdateLinkUnderCursor(const std::string& target_url) {
  if (link_under_cursor_ == target_url)
    return;

  link_under_cursor_ = target_url;

  SetLinkUnderCursorFunction set_link_under_cursor_func =
      g_set_link_under_cursor_func_for_testing
          ? g_set_link_under_cursor_func_for_testing
          : &SetLinkUnderCursor;
  set_link_under_cursor_func(GetPluginInstance(), link_under_cursor_);
}

void PDFiumEngine::SetLinkUnderCursorForAnnotation(FPDF_ANNOTATION annot,
                                                   int page_index) {
  if (!PageIndexInBounds(page_index)) {
    UpdateLinkUnderCursor("");
    return;
  }

  PDFiumPage::LinkTarget target;
  pages_[page_index]->GetLinkTarget(FPDFAnnot_GetLink(annot), &target);
  UpdateLinkUnderCursor(target.url);
}

void PDFiumEngine::RequestThumbnail(int page_index,
                                    float device_pixel_ratio,
                                    SendThumbnailCallback send_callback) {
  DCHECK(PageIndexInBounds(page_index));
  pages_[page_index]->RequestThumbnail(device_pixel_ratio,
                                       std::move(send_callback));
}

PDFiumEngine::ProgressivePaint::ProgressivePaint(int index,
                                                 const gfx::Rect& rect)
    : page_index_(index), rect_(rect) {}

PDFiumEngine::ProgressivePaint::ProgressivePaint(ProgressivePaint&& that) =
    default;

PDFiumEngine::ProgressivePaint::~ProgressivePaint() = default;

PDFiumEngine::ProgressivePaint& PDFiumEngine::ProgressivePaint::operator=(
    ProgressivePaint&& that) = default;

void PDFiumEngine::ProgressivePaint::SetBitmapAndImageData(
    ScopedFPDFBitmap bitmap,
    SkBitmap image_data) {
  bitmap_ = std::move(bitmap);
  image_data_ = std::move(image_data);
}

}  // namespace chrome_pdf
