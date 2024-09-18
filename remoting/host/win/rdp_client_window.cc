// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_client_window.h"

#include <wtsdefs.h>

#include <list>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"

namespace remoting {

namespace {

// RDP session disconnect reason codes that should not be interpreted as errors.
constexpr long kDisconnectReasonNoInfo = 0;
constexpr long kDisconnectReasonLocalNotError = 1;
constexpr long kDisconnectReasonRemoteByUser = 2;
constexpr long kDisconnectReasonByServer = 3;

// Maximum length of a window class name including the terminating nullptr.
constexpr int kMaxWindowClassLength = 256;

// Each member of the array returned by GetKeyboardState() contains status data
// for a virtual key. If the high-order bit is 1, the key is down; otherwise, it
// is up.
constexpr BYTE kKeyPressedFlag = 0x80;

constexpr int kKeyboardStateLength = 256;

constexpr base::TimeDelta kReapplyResolutionPeriod = base::Milliseconds(250);

// We want to try to reapply resolution changes for ~5 seconds (20 * 250ms).
constexpr int kMaxResolutionReapplyAttempts = 20;

// The RDP control creates 'IHWindowClass' window to handle keyboard input.
constexpr wchar_t kRdpInputWindowClass[] = L"IHWindowClass";

enum RdpAudioMode {
  // Redirect sounds to the client. This is the default value.
  kRdpAudioModeRedirect = 0,

  // Play sounds at the remote computer. Equivalent to |kRdpAudioModeNone| if
  // the remote computer is running a server SKU.
  kRdpAudioModePlayOnServer = 1,

  // Disable sound redirection; do not play sounds at the remote computer.
  kRdpAudioModeNone = 2
};

// Points to a per-thread instance of the window activation hook handle.
constinit thread_local RdpClientWindow::WindowHook* window_hook = nullptr;

// Finds a child window with the class name matching |class_name|. Unlike
// FindWindowEx() this function walks the tree of windows recursively. The walk
// is done in breadth-first order. The function returns nullptr if the child
// window could not be found.
HWND FindWindowRecursively(HWND parent, const std::wstring& class_name) {
  std::list<HWND> windows;
  windows.push_back(parent);

  while (!windows.empty()) {
    HWND child = FindWindowEx(windows.front(), nullptr, nullptr, nullptr);
    while (child != nullptr) {
      // See if the window class name matches |class_name|.
      WCHAR name[kMaxWindowClassLength];
      int length = GetClassName(child, name, std::size(name));
      if (std::wstring(name, length) == class_name) {
        return child;
      }

      // Remember the window to look through its children.
      windows.push_back(child);

      // Go to the next child.
      child = FindWindowEx(windows.front(), child, nullptr, nullptr);
    }

    windows.pop_front();
  }

  return nullptr;
}

// Returns a scale factor based on the DPI provided.  The client can send
// non-standard DPIs but Windows only supports a specific set of scale factors
// so this function just maps the DPI to a supported scale factor.
// TODO(joedow): Move to //remoting/base if this is needed on other platforms.
ULONG GetScaleFactorFromDpi(UINT dpi) {
  // The set of supported scale factors is listed here:
  // https://learn.microsoft.com/en-us/windows-server/remote/remote-desktop-services/clients/rdp-files
  if (dpi <= 96) {
    return 100;
  } else if (dpi <= 120) {
    return 125;
  } else if (dpi <= 144) {
    return 150;
  } else if (dpi <= 168) {
    return 175;
  } else if (dpi <= 192) {
    return 200;
  } else if (dpi <= 240) {
    return 250;
  } else if (dpi <= 288) {
    return 300;
  } else if (dpi <= 384) {
    return 400;
  }
  return 500;
}

}  // namespace

// Used to close any windows activated on a particular thread. It installs
// a WH_CBT window hook to track window activations and close all activated
// windows. There should be only one instance of |WindowHook| per thread
// at any given moment.
class RdpClientWindow::WindowHook : public base::RefCounted<WindowHook> {
 public:
  static scoped_refptr<WindowHook> Create();

  WindowHook(const WindowHook&) = delete;
  WindowHook& operator=(const WindowHook&) = delete;

 private:
  friend class base::RefCounted<WindowHook>;

  WindowHook();
  virtual ~WindowHook();

  static LRESULT CALLBACK CloseWindowOnActivation(int code,
                                                  WPARAM wparam,
                                                  LPARAM lparam);

  const base::AutoReset<WindowHook*> resetter_;
  HHOOK hook_;
};

RdpClientWindow::RdpClientWindow(const net::IPEndPoint& server_endpoint,
                                 const std::string& terminal_id,
                                 EventHandler* event_handler)
    : event_handler_(event_handler),
      server_endpoint_(server_endpoint),
      terminal_id_(terminal_id) {}

RdpClientWindow::~RdpClientWindow() {
  if (m_hWnd) {
    DestroyWindow();
  }

  DCHECK(!client_.Get());
  DCHECK(!client_settings_.Get());
}

bool RdpClientWindow::Connect(const ScreenResolution& resolution) {
  DCHECK(!m_hWnd);

  display_settings_ = resolution;
  RECT rect = {0, 0, display_settings_.dimensions().width(),
               display_settings_.dimensions().height()};
  bool result = Create(nullptr, rect, nullptr) != nullptr;

  // Hide the window since this class is about establishing a connection, not
  // about showing a UI to the user.
  if (result) {
    ShowWindow(SW_HIDE);
  }

  return result;
}

void RdpClientWindow::Disconnect() {
  if (m_hWnd) {
    SendMessage(WM_CLOSE);
  }
}

void RdpClientWindow::InjectSas() {
  if (!m_hWnd) {
    return;
  }

  // Find the window handling the keyboard input.
  HWND input_window = FindWindowRecursively(m_hWnd, kRdpInputWindowClass);
  if (!input_window) {
    LOG(ERROR) << "Failed to find the window handling the keyboard input.";
    return;
  }

  VLOG(3) << "Injecting Ctrl+Alt+End to emulate SAS.";

  BYTE keyboard_state[kKeyboardStateLength];
  if (!GetKeyboardState(keyboard_state)) {
    PLOG(ERROR) << "Failed to get the keyboard state.";
    return;
  }

  // This code is running in Session 0, so we expect no keys to be pressed.
  DCHECK(!(keyboard_state[VK_CONTROL] & kKeyPressedFlag));
  DCHECK(!(keyboard_state[VK_MENU] & kKeyPressedFlag));
  DCHECK(!(keyboard_state[VK_END] & kKeyPressedFlag));

  // Map virtual key codes to scan codes.
  UINT control = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
  UINT alt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
  UINT end = MapVirtualKey(VK_END, MAPVK_VK_TO_VSC) | KF_EXTENDED;
  UINT up = KF_UP | KF_REPEAT;

  // Press 'Ctrl'.
  keyboard_state[VK_CONTROL] |= kKeyPressedFlag;
  keyboard_state[VK_LCONTROL] |= kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYDOWN, VK_CONTROL, MAKELPARAM(1, control));

  // Press 'Alt'.
  keyboard_state[VK_MENU] |= kKeyPressedFlag;
  keyboard_state[VK_LMENU] |= kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYDOWN, VK_MENU,
              MAKELPARAM(1, alt | KF_ALTDOWN));

  // Press and release 'End'.
  SendMessage(input_window, WM_KEYDOWN, VK_END,
              MAKELPARAM(1, end | KF_ALTDOWN));
  SendMessage(input_window, WM_KEYUP, VK_END,
              MAKELPARAM(1, end | up | KF_ALTDOWN));

  // Release 'Alt'.
  keyboard_state[VK_MENU] &= ~kKeyPressedFlag;
  keyboard_state[VK_LMENU] &= ~kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYUP, VK_MENU, MAKELPARAM(1, alt | up));

  // Release 'Ctrl'.
  keyboard_state[VK_CONTROL] &= ~kKeyPressedFlag;
  keyboard_state[VK_LCONTROL] &= ~kKeyPressedFlag;
  CHECK(SetKeyboardState(keyboard_state));
  SendMessage(input_window, WM_KEYUP, VK_CONTROL, MAKELPARAM(1, control | up));
}

void RdpClientWindow::ChangeResolution(const ScreenResolution& resolution) {
  // Stop any pending resolution changes.
  apply_resolution_timer_.Stop();
  display_settings_ = resolution;
  HRESULT result = UpdateDesktopResolution();
  if (FAILED(result)) {
    LOG(WARNING) << "UpdateSessionDisplaySettings() failed: 0x" << std::hex
                 << result;
  }
}

void RdpClientWindow::OnClose() {
  if (!client_.Get()) {
    NotifyDisconnected();
    return;
  }

  // Request a graceful shutdown.
  mstsc::ControlCloseStatus close_status;
  HRESULT result = client_->RequestClose(&close_status);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to request a graceful shutdown of an RDP connection"
               << ", result=0x" << std::hex << result;
    NotifyDisconnected();
    return;
  }

  if (close_status != mstsc::controlCloseWaitForEvents) {
    NotifyDisconnected();
    return;
  }

  // Expect IMsTscAxEvents::OnConfirmClose() or IMsTscAxEvents::OnDisconnect()
  // to be called if mstsc::controlCloseWaitForEvents was returned.
}

LRESULT RdpClientWindow::OnCreate(CREATESTRUCT* create_struct) {
  CAxWindow2 activex_window;
  Microsoft::WRL::ComPtr<IUnknown> control;
  HRESULT result = E_FAIL;
  Microsoft::WRL::ComPtr<mstsc::IMsTscSecuredSettings> secured_settings;
  Microsoft::WRL::ComPtr<mstsc::IMsRdpClientSecuredSettings> secured_settings2;
  base::win::ScopedBstr server_name(
      base::UTF8ToWide(server_endpoint_.ToStringWithoutPort()));
  base::win::ScopedBstr terminal_id(base::UTF8ToWide(terminal_id_));

  // Create the child window that actually hosts the ActiveX control.
  RECT rect = {0, 0, display_settings_.dimensions().width(),
               display_settings_.dimensions().height()};
  activex_window.Create(m_hWnd, rect, nullptr,
                        WS_CHILD | WS_VISIBLE | WS_BORDER);
  if (activex_window.m_hWnd == nullptr) {
    return LogOnCreateError(HRESULT_FROM_WIN32(GetLastError()));
  }

  // Instantiate the RDP ActiveX control.
  result = activex_window.CreateControlEx(
      OLESTR("MsTscAx.MsTscAx"), nullptr, nullptr, &control,
      __uuidof(mstsc::IMsTscAxEvents),
      reinterpret_cast<IUnknown*>(static_cast<RdpEventsSink*>(this)));
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  result = control.As(&client_);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Use 32-bit color.
  result = client_->put_ColorDepth(32);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Set dimensions of the remote desktop.
  result = client_->put_DesktopWidth(display_settings_.dimensions().width());
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }
  result = client_->put_DesktopHeight(display_settings_.dimensions().height());
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Set the server name to connect to.
  result = client_->put_Server(server_name.Get());
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Fetch IMsRdpClientAdvancedSettings interface for the client.
  result = client_->get_AdvancedSettings2(&client_settings_);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Disable background input mode.
  result = client_settings_->put_allowBackgroundInput(0);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Do not use bitmap cache.
  result = client_settings_->put_BitmapPersistence(0);
  if (SUCCEEDED(result)) {
    result = client_settings_->put_CachePersistenceActive(0);
  }
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Do not use compression.
  result = client_settings_->put_Compress(0);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Enable the Ctrl+Alt+Del screen.
  result = client_settings_->put_DisableCtrlAltDel(0);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Disable printer and clipboard redirection.
  result = client_settings_->put_DisableRdpdr(FALSE);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Do not display the connection bar.
  result = client_settings_->put_DisplayConnectionBar(VARIANT_FALSE);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Do not grab focus on connect.
  result = client_settings_->put_GrabFocusOnConnect(VARIANT_FALSE);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Enable enhanced graphics, font smoothing and desktop composition.
  const LONG kDesiredFlags = WTS_PERF_ENABLE_ENHANCED_GRAPHICS |
                             WTS_PERF_ENABLE_FONT_SMOOTHING |
                             WTS_PERF_ENABLE_DESKTOP_COMPOSITION;
  result = client_settings_->put_PerformanceFlags(kDesiredFlags);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Set the port to connect to.
  result = client_settings_->put_RDPPort(server_endpoint_.port());
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  result = client_->get_SecuredSettings2(&secured_settings2);
  if (SUCCEEDED(result)) {
    result = secured_settings2->put_AudioRedirectionMode(kRdpAudioModeRedirect);
    if (FAILED(result)) {
      return LogOnCreateError(result);
    }
  }

  result = client_->get_SecuredSettings(&secured_settings);
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  // Set the terminal ID as the working directory for the initial program. It is
  // observed that |WorkDir| is used only if an initial program is also
  // specified, but is still passed to the RDP server and can then be read back
  // from the session parameters. This makes it possible to use |WorkDir| to
  // match the RDP connection with the session it is attached to.
  //
  // This code should be in sync with WtsTerminalMonitor::LookupTerminalId().
  result = secured_settings->put_WorkDir(terminal_id.Get());
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  result = client_->Connect();
  if (FAILED(result)) {
    return LogOnCreateError(result);
  }

  return 0;
}

void RdpClientWindow::OnDestroy() {
  client_.Reset();
  client_settings_.Reset();
  apply_resolution_timer_.Stop();
}

STDMETHODIMP
RdpClientWindow::OnAuthenticationWarningDisplayed() {
  LOG(WARNING) << "RDP: authentication warning is about to be shown.";

  // Hook window activation to cancel any modal UI shown by the RDP control.
  // This does not affect creation of other instances of the RDP control on this
  // thread because the RDP control's window is hidden and is not activated.
  window_activate_hook_ = WindowHook::Create();
  return S_OK;
}

STDMETHODIMP
RdpClientWindow::OnAuthenticationWarningDismissed() {
  LOG(WARNING) << "RDP: authentication warning has been dismissed.";

  window_activate_hook_ = nullptr;
  return S_OK;
}

STDMETHODIMP RdpClientWindow::OnConnected() {
  VLOG(1) << "RDP: successfully connected to " << server_endpoint_.ToString();

  NotifyConnected();
  return S_OK;
}

STDMETHODIMP RdpClientWindow::OnLoginComplete() {
  VLOG(1) << "RDP: user successfully logged in.";

  user_logged_in_ = true;

  // Set up a timer to periodically apply pending screen size changes to the
  // desktop.  Attempting to set the resolution now seems to fail consistently,
  // but succeeds after a brief timeout.
  apply_resolution_attempts_ = 0;
  apply_resolution_timer_.Start(
      FROM_HERE, kReapplyResolutionPeriod,
      base::BindRepeating(&RdpClientWindow::ReapplyDesktopResolution,
                          Microsoft::WRL::ComPtr<RdpClientWindow>(this)));

  return S_OK;
}

STDMETHODIMP RdpClientWindow::OnDisconnected(long reason) {
  if (reason == kDisconnectReasonNoInfo ||
      reason == kDisconnectReasonLocalNotError ||
      reason == kDisconnectReasonRemoteByUser ||
      reason == kDisconnectReasonByServer) {
    VLOG(1) << "RDP: disconnected from " << server_endpoint_.ToString()
            << ", reason=" << reason;
    NotifyDisconnected();
    return S_OK;
  }

  // Get the extended disconnect reason code.
  mstsc::ExtendedDisconnectReasonCode extended_code;
  HRESULT result = client_->get_ExtendedDisconnectReason(&extended_code);
  if (FAILED(result)) {
    extended_code = mstsc::exDiscReasonNoInfo;
  }

  // Get the error message as well.
  base::win::ScopedBstr error_message;
  Microsoft::WRL::ComPtr<mstsc::IMsRdpClient5> client5;
  result = client_.As(&client5);
  if (SUCCEEDED(result)) {
    result = client5->GetErrorDescription(reason, extended_code,
                                          error_message.Receive());
    if (FAILED(result)) {
      error_message.Reset();
    }
  }

  LOG(ERROR) << "RDP: disconnected from " << server_endpoint_.ToString() << ": "
             << error_message.Get() << " (reason=" << reason
             << ", extended_code=" << extended_code << ")";

  NotifyDisconnected();
  return S_OK;
}

STDMETHODIMP RdpClientWindow::OnFatalError(long error_code) {
  LOG(ERROR) << "RDP: an error occured: error_code=" << error_code;

  NotifyDisconnected();
  return S_OK;
}

STDMETHODIMP RdpClientWindow::OnConfirmClose(VARIANT_BOOL* allow_close) {
  *allow_close = VARIANT_TRUE;

  NotifyDisconnected();
  return S_OK;
}

int RdpClientWindow::LogOnCreateError(HRESULT error) {
  LOG(ERROR) << "RDP: failed to initiate a connection to "
             << server_endpoint_.ToString() << ": error=" << std::hex << error;
  client_.Reset();
  client_settings_.Reset();
  return -1;
}

void RdpClientWindow::NotifyConnected() {
  if (event_handler_) {
    event_handler_->OnConnected();
  }
}

void RdpClientWindow::NotifyDisconnected() {
  if (event_handler_) {
    EventHandler* event_handler = event_handler_;
    event_handler_ = nullptr;
    event_handler->OnDisconnected();
  }
}

HRESULT RdpClientWindow::UpdateDesktopResolution() {
  if (!user_logged_in_) {
    return S_FALSE;
  }

  DCHECK_EQ(display_settings_.dpi().x(), display_settings_.dpi().y());
  UINT dpi = display_settings_.dpi().x();
  // We choose to scale the desktop rather than scale the device pixels as it
  // makes the math easier to keep one scale factor constant.
  const ULONG device_scale_factor = 100;
  ULONG desktop_scale_factor = GetScaleFactorFromDpi(dpi);

  if (session_display_settings_.dimensions().equals(
          display_settings_.dimensions()) &&
      GetScaleFactorFromDpi(session_display_settings_.dpi().x()) ==
          desktop_scale_factor) {
    // Don't call UpdateSessionDisplaySettings() if nothing has changed as it
    // can cause DXGI to stop producing frames. Technically calling this API
    // always has a chance to cause a hang but we detect the resolution/dpi
    // change in WebRTC and restart the capturer there. If no settings have
    // changed then the WebRTC logic won't kick in and the video stream could
    // stop until the user makes another resolution or dpi change.
    VLOG(0) << "No changes detected, skipping display settings update.";
    return S_OK;
  }

  ULONG width = display_settings_.dimensions().width();
  ULONG height = display_settings_.dimensions().height();
  VLOG(0) << "Setting desktop resolution to " << width << "x" << height << " @ "
          << desktop_scale_factor << "% scale (" << dpi << " dpi)";

  // UpdateSessionDisplaySettings() is poorly documented in MSDN and has a few
  // quirks that should be noted.
  // 1.) This method will only work when the user is logged into their session.
  // 2.) The method may return E_UNEXPECTED until some amount of time (seconds)
  //     have elapsed after logging in to the user's session.
  // 3.) Calling this method will probably cause DXGI-based capturers to fail.
  //     The WebRTC desktop capturer looks for resolution and DPI changes but if
  //     the method is called with the same params then the video stream can
  //     stall.
  HRESULT hr = client_->UpdateSessionDisplaySettings(
      /*ulDesktopWidth=*/width,
      /*ulDesktopHeight*/ height,
      /*ulPhysicalWidth=*/width,
      /*ulPhysicalHeight=*/height,
      /*ulOrientation=*/0,
      /*ulDesktopScaleFactor=*/desktop_scale_factor,
      /*ulDeviceScaleFactor=*/device_scale_factor);

  if (SUCCEEDED(hr)) {
    session_display_settings_ = display_settings_;
  }

  return hr;
}

void RdpClientWindow::ReapplyDesktopResolution() {
  DCHECK_LT(apply_resolution_attempts_, kMaxResolutionReapplyAttempts);

  HRESULT result = UpdateDesktopResolution();
  apply_resolution_attempts_++;

  if (SUCCEEDED(result)) {
    // Successfully applied the new resolution so stop the retry timer.
    apply_resolution_timer_.Stop();
  } else if (apply_resolution_attempts_ == kMaxResolutionReapplyAttempts) {
    // Only log an error on the last attempt to reduce log spam since a few
    // errors can be expected and don't signal an actual failure.
    LOG(WARNING) << "All UpdateSessionDisplaySettings() retries failed: 0x"
                 << std::hex << result;
    apply_resolution_timer_.Stop();
  }
}

scoped_refptr<RdpClientWindow::WindowHook>
RdpClientWindow::WindowHook::Create() {
  if (!window_hook) {
    new WindowHook();  // Sets `window_hook`.
  }
  return window_hook;
}

RdpClientWindow::WindowHook::WindowHook()
    : resetter_(&window_hook, this, nullptr), hook_(nullptr) {
  // Install a window hook to be called on window activation.
  hook_ = SetWindowsHookEx(WH_CBT, &WindowHook::CloseWindowOnActivation,
                           nullptr, GetCurrentThreadId());
  // Without the hook installed, RdpClientWindow will not be able to cancel
  // modal UI windows. This will block the UI message loop so it is better to
  // terminate the process now.
  CHECK(hook_);
}

RdpClientWindow::WindowHook::~WindowHook() {
  DCHECK_EQ(window_hook, this);

  BOOL result = UnhookWindowsHookEx(hook_);
  DCHECK(result);
}

// static
LRESULT CALLBACK
RdpClientWindow::WindowHook::CloseWindowOnActivation(int code,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  // Get the hook handle.
  HHOOK hook = window_hook->hook_;

  if (code != HCBT_ACTIVATE) {
    return CallNextHookEx(hook, code, wparam, lparam);
  }

  // Close the window once all pending window messages are processed.
  HWND window = reinterpret_cast<HWND>(wparam);
  LOG(WARNING) << "RDP: closing a window: " << std::hex << window;
  ::PostMessage(window, WM_CLOSE, 0, 0);
  return 0;
}

}  // namespace remoting
