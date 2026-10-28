// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/disconnect_window_base.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/host_window_win_util.h"

namespace remoting {

namespace {

// Registers the window class used by per-monitor privacy overlay backdrop
// windows if it is not already registered in the current module.
bool RegisterPrivacyOverlayClass(WNDPROC wnd_proc) {
  WNDCLASSEXW existing_wc = {};
  existing_wc.cbSize = sizeof(existing_wc);
  if (GetClassInfoExW(CURRENT_MODULE(), kPrivacyOverlayClassName,
                      &existing_wc)) {
    return true;
  }

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = CURRENT_MODULE();
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kPrivacyOverlayClassName;

  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    PLOG(ERROR) << "Failed to register Privacy Overlay window class";
    return false;
  }

  return true;
}

// Implements the Windows "Light Curtain" Privacy Overlay (`HostWindow`).
//
// Creates one topmost, unactivated popup window per active monitor. Each
// backdrop is permanently click-through (`WS_EX_LAYERED | WS_EX_TRANSPARENT`)
// and excluded from desktop capture (`WDA_EXCLUDEFROMCAPTURE`). The local
// physical monitors display the branded Google Blue privacy screen (localized
// product name + localized sharing message), while WebRTC/DXGI desktop
// capture sees right through the overlay to the underlying desktop
// applications, and remote mouse/touch inputs pass straight through without
// obstruction.
class PrivacyOverlayWin : public DisconnectWindowBase {
 public:
  explicit PrivacyOverlayWin(
      std::unique_ptr<LocalInputMonitor> local_input_monitor);

  PrivacyOverlayWin(const PrivacyOverlayWin&) = delete;
  PrivacyOverlayWin& operator=(const PrivacyOverlayWin&) = delete;

  ~PrivacyOverlayWin() override;

  // `HostWindow` implementation.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  // Holds DPI-scaled GDI resources for a specific monitor DPI so multi-monitor
  // setups with mixed DPIs do not thrash or recreate icons/fonts on every
  // `WM_PAINT`.
  struct BackdropDpiResources {
    BackdropDpiResources();
    BackdropDpiResources(BackdropDpiResources&&);
    BackdropDpiResources& operator=(BackdropDpiResources&&);
    ~BackdropDpiResources();

    // Note: `base::win::ScopedGDIObject<HICON>` is specialized in
    // `base/win/scoped_gdi_object.cc` to call `::DestroyIcon()` rather than
    // `::DeleteObject()`.
    base::win::ScopedGDIObject<HICON> crd_icon;
    base::win::ScopedGDIObject<HFONT> title_font;
    base::win::ScopedGDIObject<HFONT> body_font;
  };

  // Initializes the localized strings and per-monitor privacy overlay backdrop
  // windows.
  bool BeginOverlay();

  // Destroys all backdrop windows and releases cached GDI resources.
  void EndOverlay();

  // Returns the cached backdrop fonts and icon for `dpi`, creating and
  // inserting them into `backdrop_dpi_cache_` if not yet present.
  const BackdropDpiResources& GetBackdropResourcesForDpi(UINT dpi);

  // Recreates or resizes per-monitor privacy overlay backdrop windows across
  // all active physical displays.
  bool UpdateBackdrops();

  // Renders the privacy overlay backdrop content (icon, localized product
  // title, and localized sharing message) into `hdc` using an offscreen double
  // buffer for the central content region.
  void PaintBackdrop(HWND hwnd, HDC hdc);

  // Window procedure for per-monitor backdrop windows (`backdrop_hwnds_`).
  static LRESULT CALLBACK BackdropWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam);

  // Retained for subsequent CLs that hook local input monitoring.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Re-entrancy guard preventing `EndOverlay()` from running more than once.
  bool is_ending_ = false;

  // True if the localized strings or process UI language use a right-to-left
  // (RTL) reading layout.
  bool is_rtl_ = false;

  // Topmost click-through backdrop popup windows, one per active display.
  std::vector<HWND> backdrop_hwnds_;

  // Cached solid brush for the branded Google Blue background.
  base::win::ScopedGDIObject<HBRUSH> google_blue_brush_;

  // Per-DPI cache of backdrop GDI resources (`HICON` and `HFONT`s), preventing
  // GDI object thrashing on mixed-DPI multi-monitor hosts.
  base::flat_map<UINT, BackdropDpiResources> backdrop_dpi_cache_;

  // Localized strings loaded from the embedded Windows resource table.
  std::wstring product_name_text_;
  std::wstring sharing_with_text_;
};

PrivacyOverlayWin::BackdropDpiResources::BackdropDpiResources() = default;
PrivacyOverlayWin::BackdropDpiResources::BackdropDpiResources(
    BackdropDpiResources&&) = default;
PrivacyOverlayWin::BackdropDpiResources&
PrivacyOverlayWin::BackdropDpiResources::operator=(BackdropDpiResources&&) =
    default;
PrivacyOverlayWin::BackdropDpiResources::~BackdropDpiResources() = default;

PrivacyOverlayWin::PrivacyOverlayWin(
    std::unique_ptr<LocalInputMonitor> local_input_monitor)
    : local_input_monitor_(std::move(local_input_monitor)),
      google_blue_brush_(CreateSolidBrush(kGoogleBlueColor)) {}

PrivacyOverlayWin::~PrivacyOverlayWin() {
  EndOverlay();
}

void PrivacyOverlayWin::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_session_control);

  DisconnectWindowBase::Start(client_session_control);

  if (!BeginOverlay()) {
    EndOverlay();
    DisconnectSession("Failed to initialize Privacy Overlay.");
  }
}

bool PrivacyOverlayWin::BeginOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RegisterPrivacyOverlayClass(&BackdropWndProc)) {
    return false;
  }

  product_name_text_ = LoadModuleString(IDS_PRODUCT_NAME);
  if (product_name_text_.empty()) {
    LOG(ERROR) << "Failed to load IDS_PRODUCT_NAME for Privacy Overlay";
    return false;
  }

  is_rtl_ =
      base::i18n::IsRTL() || base::i18n::StringContainsStrongRTLChars(
                                 base::AsStringPiece16(product_name_text_));

  std::u16string email_for_display = formatted_email();
  if (is_rtl_) {
    base::i18n::WrapStringWithLTRFormatting(&email_for_display);
  }
  sharing_with_text_ = base::AsWString(email_for_display);

  return UpdateBackdrops();
}

void PrivacyOverlayWin::EndOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_ending_) {
    return;
  }
  is_ending_ = true;

  std::vector<HWND> backdrop_hwnds = std::exchange(backdrop_hwnds_, {});
  for (HWND backdrop_hwnd : backdrop_hwnds) {
    if (backdrop_hwnd && IsWindow(backdrop_hwnd)) {
      SetWindowLongPtr(backdrop_hwnd, GWLP_USERDATA, 0);
      DestroyWindow(backdrop_hwnd);
    }
  }

  backdrop_dpi_cache_.clear();
}

const PrivacyOverlayWin::BackdropDpiResources&
PrivacyOverlayWin::GetBackdropResourcesForDpi(UINT dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dpi == 0) {
    dpi = USER_DEFAULT_SCREEN_DPI;
  }

  auto it = backdrop_dpi_cache_.find(dpi);
  if (it != backdrop_dpi_cache_.end()) {
    return it->second;
  }

  BackdropDpiResources resources;
  int icon_size = ScaleForDpi(kBaseIconSize, dpi);
  int title_height = ScaleForDpi(kBaseTitleFontSize, dpi);
  int body_height = ScaleForDpi(kBaseBodyFontSize, dpi);

  resources.crd_icon.reset(static_cast<HICON>(
      LoadImageW(CURRENT_MODULE(), MAKEINTRESOURCEW(IDI_CHROME_REMOTE_DESKTOP),
                 IMAGE_ICON, icon_size, icon_size, LR_DEFAULTCOLOR)));
  resources.title_font = CreateOverlayFont(title_height, FW_SEMIBOLD);
  resources.body_font = CreateOverlayFont(body_height, FW_NORMAL);

  auto [inserted_it, _] =
      backdrop_dpi_cache_.emplace(dpi, std::move(resources));
  return inserted_it->second;
}

bool PrivacyOverlayWin::UpdateBackdrops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<RECT> monitor_rects = GetActiveMonitorRects();
  if (monitor_rects.empty()) {
    return false;
  }

  // Destroy any surplus backdrop windows if monitors were disconnected.
  while (backdrop_hwnds_.size() > monitor_rects.size()) {
    HWND surplus = backdrop_hwnds_.back();
    backdrop_hwnds_.pop_back();
    if (surplus && IsWindow(surplus)) {
      SetWindowLongPtr(surplus, GWLP_USERDATA, 0);
      DestroyWindow(surplus);
    }
  }

  DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                   WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
  if (is_rtl_) {
    ex_style |= WS_EX_RTLREADING;
  }

  const std::wstring& backdrop_title =
      !sharing_with_text_.empty() ? sharing_with_text_ : product_name_text_;

  for (size_t i = 0; i < monitor_rects.size(); ++i) {
    const RECT& rect = monitor_rects[i];
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HWND backdrop_hwnd = nullptr;
    if (i < backdrop_hwnds_.size() && IsWindow(backdrop_hwnds_[i])) {
      backdrop_hwnd = backdrop_hwnds_[i];
      SetWindowTextW(backdrop_hwnd, backdrop_title.c_str());
      SetWindowPos(backdrop_hwnd, HWND_TOPMOST, rect.left, rect.top, width,
                   height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
      InvalidateRect(backdrop_hwnd, nullptr, FALSE);
    } else {
      backdrop_hwnd = CreateWindowExW(ex_style, kPrivacyOverlayClassName,
                                      backdrop_title.c_str(), WS_POPUP,
                                      rect.left, rect.top, width, height,
                                      nullptr, nullptr, CURRENT_MODULE(), this);
      if (!backdrop_hwnd) {
        PLOG(ERROR) << "Failed to create Privacy Overlay backdrop window";
        return false;
      }

      // Configure layered opacity and exclude the backdrop from screen capture
      // before showing so remote viewers never see a flash of the overlay.
      SetLayeredWindowAttributes(backdrop_hwnd, 0, 255, LWA_ALPHA);
      if (!ApplyCaptureExclusion(backdrop_hwnd)) {
        SetWindowLongPtr(backdrop_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(backdrop_hwnd);
        return false;
      }
      ShowWindow(backdrop_hwnd, SW_SHOWNOACTIVATE);
      UpdateWindow(backdrop_hwnd);
      NotifyWinEvent(EVENT_OBJECT_SHOW, backdrop_hwnd, OBJID_WINDOW,
                     CHILDID_SELF);

      if (i < backdrop_hwnds_.size()) {
        backdrop_hwnds_[i] = backdrop_hwnd;
      } else {
        backdrop_hwnds_.push_back(backdrop_hwnd);
      }
    }
  }

  return true;
}

void PrivacyOverlayWin::PaintBackdrop(HWND hwnd, HDC hdc) {
  RECT client_rect;
  if (!GetClientRect(hwnd, &client_rect)) {
    return;
  }

  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  if (client_width <= 0 || client_height <= 0) {
    return;
  }

  const bool high_contrast = IsHighContrastEnabled();
  HBRUSH bg_brush =
      high_contrast ? GetSysColorBrush(COLOR_WINDOW) : google_blue_brush_.get();
  COLORREF text_color =
      high_contrast ? GetSysColor(COLOR_WINDOWTEXT) : kWhiteColor;

  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  UINT dpi = GetMonitorDpi(monitor, hwnd);
  const BackdropDpiResources& resources = GetBackdropResourcesForDpi(dpi);

  int center_x = client_width / 2;
  int center_y = client_height / 2;

  int icon_size = ScaleForDpi(kBaseIconSize, dpi);
  int spacing = ScaleForDpi(kBaseVerticalSpacing, dpi);
  int max_text_width = std::max(
      1, (client_width * kMaxTextWidthNumerator) / kMaxTextWidthDenominator);
  int text_left = center_x - max_text_width / 2;
  int text_right = text_left + max_text_width;

  UINT draw_text_flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL |
                         DT_EXTERNALLEADING | DT_NOPREFIX;
  if (is_rtl_) {
    draw_text_flags |= DT_RTLREADING;
  }

  // Measure the title and body text bounding boxes first so we only need to
  // allocate a compact offscreen double-buffer bitmap for the central content
  // region (rather than a ~33 MB full-screen 4K bitmap) while filling the
  // surrounding solid background bands directly without flicker.
  int title_box_height = 0;
  if (!product_name_text_.empty() && resources.title_font.is_valid()) {
    title_box_height = ScaleForDpi(kBaseTitleFontSize, dpi);
    base::win::ScopedSelectObject select_font(hdc, resources.title_font.get());
    RECT calc_rect = {text_left, 0, text_right, 0};
    if (DrawTextW(hdc, product_name_text_.c_str(), -1, &calc_rect,
                  draw_text_flags | DT_CALCRECT)) {
      title_box_height = std::max(
          title_box_height, static_cast<int>(calc_rect.bottom - calc_rect.top));
    }
  }

  int body_box_height = 0;
  if (!sharing_with_text_.empty() && resources.body_font.is_valid()) {
    body_box_height = ScaleForDpi(kBaseBodyFontSize, dpi) + spacing;
    base::win::ScopedSelectObject select_font(hdc, resources.body_font.get());
    RECT calc_rect = {text_left, 0, text_right, 0};
    if (DrawTextW(hdc, sharing_with_text_.c_str(), -1, &calc_rect,
                  draw_text_flags | DT_CALCRECT)) {
      body_box_height = std::max(
          body_box_height, static_cast<int>(calc_rect.bottom - calc_rect.top));
    }
  }

  int title_bottom = center_y - spacing;
  int icon_top = title_bottom - title_box_height - spacing - icon_size;
  if (icon_top < spacing) {
    int shift_down = spacing - icon_top;
    icon_top = spacing;
    title_bottom += shift_down;
    center_y += shift_down;
  }
  int icon_left = center_x - icon_size / 2;

  int content_left = std::max(0, std::min(text_left, icon_left));
  int content_top = std::max(0, icon_top);
  int content_right =
      std::min(client_width, std::max(text_right, icon_left + icon_size));
  int content_bottom = std::min(client_height, center_y + body_box_height);
  int content_width = std::max(0, content_right - content_left);
  int content_height = std::max(0, content_bottom - content_top);

  if (content_width <= 0 || content_height <= 0) {
    FillRect(hdc, &client_rect, bg_brush);
    return;
  }

  // Fill the four exterior background bands around the central content box so
  // the solid background never flickers.
  if (content_top > 0) {
    RECT top_band = {0, 0, client_width, content_top};
    FillRect(hdc, &top_band, bg_brush);
  }
  if (content_bottom < client_height) {
    RECT bottom_band = {0, content_bottom, client_width, client_height};
    FillRect(hdc, &bottom_band, bg_brush);
  }
  if (content_left > 0) {
    RECT left_band = {0, content_top, content_left, content_bottom};
    FillRect(hdc, &left_band, bg_brush);
  }
  if (content_right < client_width) {
    RECT right_band = {content_right, content_top, client_width,
                       content_bottom};
    FillRect(hdc, &right_band, bg_brush);
  }

  // Render the central content box into an offscreen memory DC and `BitBlt` in
  // a single pass. If offscreen allocation fails, fall back to drawing directly
  // into `hdc`.
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(hdc));
  base::win::ScopedGDIObject<HBITMAP> mem_bitmap;
  std::optional<base::win::ScopedSelectObject> select_bitmap;
  HDC target_dc = hdc;
  int offset_x = 0;
  int offset_y = 0;
  if (mem_dc.is_valid()) {
    mem_bitmap.reset(
        CreateCompatibleBitmap(hdc, content_width, content_height));
    if (mem_bitmap.is_valid()) {
      select_bitmap.emplace(mem_dc.get(), mem_bitmap.get());
      target_dc = mem_dc.get();
      offset_x = -content_left;
      offset_y = -content_top;
    }
  }

  RECT target_bg_rect = {content_left + offset_x, content_top + offset_y,
                         content_right + offset_x, content_bottom + offset_y};
  FillRect(target_dc, &target_bg_rect, bg_brush);

  SetBkMode(target_dc, TRANSPARENT);
  SetTextColor(target_dc, text_color);

  if (resources.crd_icon.is_valid()) {
    DrawIconEx(target_dc, icon_left + offset_x, icon_top + offset_y,
               resources.crd_icon.get(), icon_size, icon_size, 0, nullptr,
               DI_NORMAL | DI_NOMIRROR);
  }

  if (title_box_height > 0 && resources.title_font.is_valid()) {
    base::win::ScopedSelectObject select_font(target_dc,
                                              resources.title_font.get());
    int title_top = title_bottom - title_box_height;
    RECT title_rect = {text_left + offset_x, title_top + offset_y,
                       text_right + offset_x, title_bottom + offset_y};
    DrawTextW(target_dc, product_name_text_.c_str(), -1, &title_rect,
              draw_text_flags);
  }

  if (body_box_height > 0 && resources.body_font.is_valid()) {
    base::win::ScopedSelectObject select_font(target_dc,
                                              resources.body_font.get());
    RECT sharing_rect = {text_left + offset_x, center_y + offset_y,
                         text_right + offset_x,
                         center_y + body_box_height + offset_y};
    DrawTextW(target_dc, sharing_with_text_.c_str(), -1, &sharing_rect,
              draw_text_flags);
  }

  if (target_dc != hdc) {
    BitBlt(hdc, content_left, content_top, content_width, content_height,
           target_dc, 0, 0, SRCCOPY);
  }
}

// static
LRESULT CALLBACK PrivacyOverlayWin::BackdropWndProc(HWND hwnd,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam) {
  PrivacyOverlayWin* self = nullptr;
  if (message == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
    self = static_cast<PrivacyOverlayWin*>(create_struct->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<PrivacyOverlayWin*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  switch (message) {
    // Ignore close requests on backdrop windows so the privacy curtain cannot
    // be dismissed without disconnecting the session.
    case WM_CLOSE:
      return 0;

    // Never activate a backdrop window when clicked.
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;

    // Pass all mouse hit-testing through to underlying desktop windows.
    case WM_NCHITTEST:
      return HTTRANSPARENT;

    // Background erasure is handled entirely inside `PaintBackdrop()` to
    // prevent flicker.
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (self) {
        self->PaintBackdrop(hwnd, hdc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_PRINTCLIENT: {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      if (self && hdc) {
        self->PaintBackdrop(hwnd, hdc);
      }
      return 0;
    }

      // TODO(crbug.com/451238243): Handle WM_DISPLAYCHANGE, WM_DPICHANGED,
      // WM_SETTINGCHANGE, and WM_SYSCOLORCHANGE in a follow-up CL so backdrops
      // dynamically update when display topology, DPI, or High Contrast
      // settings change during an active session.
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreatePrivacyOverlay(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  return std::make_unique<PrivacyOverlayWin>(std::move(local_input_monitor));
}

}  // namespace remoting
