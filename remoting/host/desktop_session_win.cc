// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_win.h"

#include <objbase.h>

#include <sddl.h>
#include <wrl/client.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/sas_injector.h"
// MIDL-generated declarations and definitions.
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/host_service.h"
#include "remoting/host/win/trust_util.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_session_process_delegate.h"
#include "remoting/host/win/wts_terminal_monitor.h"
#include "remoting/host/win/wts_terminal_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;

namespace remoting {

namespace {

// The security descriptor of the daemon IPC endpoint. It gives full access
// to SYSTEM and denies access by anyone else.
const wchar_t kDaemonIpcSecurityDescriptor[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":("
        SDDL_ACCESS_ALLOWED L";;" SDDL_GENERIC_ALL L";;;" SDDL_LOCAL_SYSTEM
    L")";

// The command line parameters that should be copied from the service's command
// line to the desktop process.
const char* const kCopiedSwitchNames[] = {switches::kV, switches::kVModule};

// The default screen dimensions for an RDP session.
const int kDefaultRdpScreenWidth = 1280;
const int kDefaultRdpScreenHeight = 768;

// The minimum effective screen dimensions supported by Windows are 800x600.
const int kMinRdpScreenWidth = 800;
const int kMinRdpScreenHeight = 600;

// Windows supports dimensions up to 8192x8192.
const int kMaxRdpScreenWidth = 8192;
const int kMaxRdpScreenHeight = 8192;

// Default dots per inch used by RDP is 96 DPI.
const int kDefaultRdpDpi = 96;

// The session attach notification should arrive within 30 seconds.
const int kSessionAttachTimeoutSeconds = 30;

// The default port number used for establishing an RDP session.
const int kDefaultRdpPort = 3389;

// Used for validating the required RDP registry values.
const int kRdpConnectionsDisabled = 1;
const int kNetworkLevelAuthEnabled = 1;
const int kSecurityLayerTlsRequired = 2;

// The values used to establish RDP connections are stored in the registry.
const wchar_t kRdpSettingsKeyName[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server";
const wchar_t kRdpTcpSettingsKeyName[] =
    L"SYSTEM\\CurrentControlSet\\"
    L"Control\\Terminal Server\\WinStations\\RDP-Tcp";
const wchar_t kRdpPortValueName[] = L"PortNumber";
const wchar_t kDenyTsConnectionsValueName[] = L"fDenyTSConnections";
const wchar_t kNetworkLevelAuthValueName[] = L"UserAuthentication";
const wchar_t kSecurityLayerValueName[] = L"SecurityLayer";

webrtc::DesktopSize GetBoundedRdpDesktopSize(int width, int height) {
  return webrtc::DesktopSize(
      std::clamp(width, kMinRdpScreenWidth, kMaxRdpScreenWidth),
      std::clamp(height, kMinRdpScreenHeight, kMaxRdpScreenHeight));
}

// DesktopSession implementation which attaches to the host's physical console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserver|.
class ConsoleSession : public DesktopSessionWin {
 public:
  // Same as DesktopSessionWin().
  ConsoleSession(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                 scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                 DaemonProcess* daemon_process,
                 int id,
                 WtsTerminalMonitor* monitor);

  ConsoleSession(const ConsoleSession&) = delete;
  ConsoleSession& operator=(const ConsoleSession&) = delete;

  ~ConsoleSession() override;

 protected:
  // DesktopSession overrides.
  void SetScreenResolution(const ScreenResolution& resolution) override;

  // DesktopSessionWin overrides.
  void InjectSas() override;

 private:
  std::unique_ptr<SasInjector> sas_injector_;
};

// DesktopSession implementation which attaches to virtual RDP console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserver|.
class RdpSession : public DesktopSessionWin {
 public:
  // Same as DesktopSessionWin().
  RdpSession(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
             scoped_refptr<AutoThreadTaskRunner> io_task_runner,
             DaemonProcess* daemon_process,
             int id,
             WtsTerminalMonitor* monitor);

  RdpSession(const RdpSession&) = delete;
  RdpSession& operator=(const RdpSession&) = delete;

  ~RdpSession() override;

  // Performs the part of initialization that can fail.
  bool Initialize(const ScreenResolution& resolution);

  // Mirrors IRdpDesktopSessionEventHandler.
  void OnRdpConnected();
  void OnRdpClosed();

 protected:
  // DesktopSession overrides.
  void SetScreenResolution(const ScreenResolution& resolution) override;

  // DesktopSessionWin overrides.
  void InjectSas() override;

 private:
  // An implementation of IRdpDesktopSessionEventHandler interface that forwards
  // notifications to the owning desktop session.
  class EventHandler : public IRdpDesktopSessionEventHandler {
   public:
    explicit EventHandler(base::WeakPtr<RdpSession> desktop_session);

    EventHandler(const EventHandler&) = delete;
    EventHandler& operator=(const EventHandler&) = delete;

    virtual ~EventHandler();

    // IUnknown interface.
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;

    // IRdpDesktopSessionEventHandler interface.
    STDMETHOD(OnRdpConnected)() override;
    STDMETHOD(OnRdpClosed)() override;

   private:
    ULONG ref_count_;

    // Points to the desktop session object receiving OnRdpXxx() notifications.
    base::WeakPtr<RdpSession> desktop_session_;

    // This class must be used on a single thread.
    base::ThreadChecker thread_checker_;
  };

  // Examines the system settings required to establish an RDP session.
  // This method returns false if the values are retrieved and any of them would
  // prevent us from creating an RDP connection.
  bool VerifyRdpSettings();

  // Retrieves a DWORD value from the registry.  Returns true on success.
  bool RetrieveDwordRegistryValue(const wchar_t* key_name,
                                  const wchar_t* value_name,
                                  DWORD* value);

  // Used to create an RDP desktop session.
  Microsoft::WRL::ComPtr<IRdpDesktopSession> rdp_desktop_session_;

  // Used to match |rdp_desktop_session_| with the session it is attached to.
  std::string terminal_id_;

  base::WeakPtrFactory<RdpSession> weak_factory_{this};
};

ConsoleSession::ConsoleSession(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor)
    : DesktopSessionWin(caller_task_runner,
                        io_task_runner,
                        daemon_process,
                        id,
                        monitor) {
  StartMonitoring(WtsTerminalMonitor::kConsole);
}

ConsoleSession::~ConsoleSession() {}

void ConsoleSession::SetScreenResolution(const ScreenResolution& resolution) {
  // Do nothing. The screen resolution of the console session is controlled by
  // the DesktopSessionAgent instance running in that session.
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

void ConsoleSession::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!sas_injector_) {
    sas_injector_ = SasInjector::Create();
  }
  if (!sas_injector_->InjectSas()) {
    LOG(ERROR) << "Failed to inject Secure Attention Sequence.";
  }
}

RdpSession::RdpSession(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                       scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                       DaemonProcess* daemon_process,
                       int id,
                       WtsTerminalMonitor* monitor)
    : DesktopSessionWin(caller_task_runner,
                        io_task_runner,
                        daemon_process,
                        id,
                        monitor) {}

RdpSession::~RdpSession() {}

bool RdpSession::Initialize(const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!VerifyRdpSettings()) {
    LOG(ERROR) << "Could not create an RDP session due to invalid settings.";
    return false;
  }

  // Create the RDP wrapper object.
  HRESULT result =
      ::CoCreateInstance(__uuidof(RdpDesktopSession), nullptr, CLSCTX_ALL,
                         IID_PPV_ARGS(&rdp_desktop_session_));
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to create RdpSession object, 0x" << std::hex << result
               << std::dec << ".";
    return false;
  }

  ScreenResolution local_resolution = resolution;

  // If the screen resolution is not specified, use the default screen
  // resolution.
  if (local_resolution.IsEmpty()) {
    local_resolution = ScreenResolution(
        webrtc::DesktopSize(kDefaultRdpScreenWidth, kDefaultRdpScreenHeight),
        webrtc::DesktopVector(kDefaultRdpDpi, kDefaultRdpDpi));
  }

  // Get the screen dimensions using the default DPI for the RDP client window.
  webrtc::DesktopSize host_size = local_resolution.ScaleDimensionsToDpi(
      webrtc::DesktopVector(kDefaultRdpDpi, kDefaultRdpDpi));

  // Make sure that the host resolution is within the limits supported by RDP.
  host_size = GetBoundedRdpDesktopSize(host_size.width(), host_size.height());

  // Read the port number used by RDP.
  DWORD server_port = kDefaultRdpPort;
  if (RetrieveDwordRegistryValue(kRdpTcpSettingsKeyName, kRdpPortValueName,
                                 &server_port) &&
      server_port > 65535) {
    LOG(ERROR) << "Invalid RDP port specified: " << server_port;
    return false;
  }

  // Create an RDP session.
  Microsoft::WRL::ComPtr<IRdpDesktopSessionEventHandler> event_handler(
      new EventHandler(weak_factory_.GetWeakPtr()));
  terminal_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::win::ScopedBstr terminal_id(base::UTF8ToWide(terminal_id_));
  result = rdp_desktop_session_->Connect(
      host_size.width(), host_size.height(), kDefaultRdpDpi, kDefaultRdpDpi,
      terminal_id.Get(), server_port, event_handler.Get());
  if (FAILED(result)) {
    LOG(ERROR) << "RdpSession::Create() failed, 0x" << std::hex << result
               << std::dec << ".";
    return false;
  }

  return true;
}

void RdpSession::OnRdpConnected() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  StopMonitoring();
  StartMonitoring(terminal_id_);
}

void RdpSession::OnRdpClosed() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  TerminateSession();
}

void RdpSession::SetScreenResolution(const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!resolution.IsEmpty());

  webrtc::DesktopSize bounded_size = GetBoundedRdpDesktopSize(
      resolution.dimensions().width(), resolution.dimensions().height());

  rdp_desktop_session_->ChangeResolution(
      bounded_size.width(), bounded_size.height(), resolution.dpi().x(),
      resolution.dpi().y());
}

void RdpSession::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  rdp_desktop_session_->InjectSas();
}

bool RdpSession::VerifyRdpSettings() {
  // Verify RDP connections are enabled.
  DWORD deny_ts_connections_flag = 0;
  if (RetrieveDwordRegistryValue(kRdpSettingsKeyName,
                                 kDenyTsConnectionsValueName,
                                 &deny_ts_connections_flag) &&
      deny_ts_connections_flag == kRdpConnectionsDisabled) {
    LOG(ERROR) << "RDP Connections must be enabled.";
    return false;
  }

  // Verify Network Level Authentication is disabled.
  DWORD network_level_auth_flag = 0;
  if (RetrieveDwordRegistryValue(kRdpTcpSettingsKeyName,
                                 kNetworkLevelAuthValueName,
                                 &network_level_auth_flag) &&
      network_level_auth_flag == kNetworkLevelAuthEnabled) {
    LOG(ERROR) << "Network Level Authentication for RDP must be disabled.";
    return false;
  }

  // Verify Security Layer is not set to TLS.  It can be either of the other two
  // values, but forcing TLS will prevent us from establishing a connection.
  DWORD security_layer_flag = 0;
  if (RetrieveDwordRegistryValue(kRdpTcpSettingsKeyName,
                                 kSecurityLayerValueName,
                                 &security_layer_flag) &&
      security_layer_flag == kSecurityLayerTlsRequired) {
    LOG(ERROR) << "RDP SecurityLayer must not be set to TLS.";
    return false;
  }

  return true;
}

bool RdpSession::RetrieveDwordRegistryValue(const wchar_t* key_name,
                                            const wchar_t* value_name,
                                            DWORD* value) {
  DCHECK(key_name);
  DCHECK(value_name);
  DCHECK(value);

  base::win::RegKey key(HKEY_LOCAL_MACHINE, key_name, KEY_READ);
  if (!key.Valid()) {
    LOG(WARNING) << "Failed to open key: " << key_name;
    return false;
  }

  if (key.ReadValueDW(value_name, value) != ERROR_SUCCESS) {
    LOG(WARNING) << "Failed to read registry value: " << value_name;
    return false;
  }

  return true;
}

RdpSession::EventHandler::EventHandler(
    base::WeakPtr<RdpSession> desktop_session)
    : ref_count_(0), desktop_session_(desktop_session) {}

RdpSession::EventHandler::~EventHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (desktop_session_) {
    desktop_session_->OnRdpClosed();
  }
}

ULONG STDMETHODCALLTYPE RdpSession::EventHandler::AddRef() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return ++ref_count_;
}

ULONG STDMETHODCALLTYPE RdpSession::EventHandler::Release() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (--ref_count_ == 0) {
    delete this;
    return 0;
  }

  return ref_count_;
}

STDMETHODIMP
RdpSession::EventHandler::QueryInterface(REFIID riid, void** ppv) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (riid == IID_IUnknown || riid == IID_IRdpDesktopSessionEventHandler) {
    *ppv = static_cast<IRdpDesktopSessionEventHandler*>(this);
    AddRef();
    return S_OK;
  }

  *ppv = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP RdpSession::EventHandler::OnRdpConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (desktop_session_) {
    desktop_session_->OnRdpConnected();
  }

  return S_OK;
}

STDMETHODIMP RdpSession::EventHandler::OnRdpClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!desktop_session_) {
    return S_OK;
  }

  base::WeakPtr<RdpSession> desktop_session = desktop_session_;
  desktop_session_.reset();
  desktop_session->OnRdpClosed();
  return S_OK;
}

}  // namespace

// static
std::unique_ptr<DesktopSession> DesktopSessionWin::CreateForConsole(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    const ScreenResolution& resolution) {
  return std::make_unique<ConsoleSession>(caller_task_runner, io_task_runner,
                                          daemon_process, id,
                                          HostService::GetInstance());
}

// static
std::unique_ptr<DesktopSession> DesktopSessionWin::CreateForVirtualTerminal(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    const ScreenResolution& resolution) {
  std::unique_ptr<RdpSession> session(
      new RdpSession(caller_task_runner, io_task_runner, daemon_process, id,
                     HostService::GetInstance()));
  if (!session->Initialize(resolution)) {
    return nullptr;
  }

  return std::move(session);
}

DesktopSessionWin::DesktopSessionWin(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor)
    : DesktopSession(daemon_process, id),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      monitor_(monitor),
      monitoring_notifications_(false) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ReportElapsedTime("created");
}

DesktopSessionWin::~DesktopSessionWin() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();
}

void DesktopSessionWin::OnSessionAttachTimeout() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(ERROR) << "Session attach notification didn't arrived within "
             << kSessionAttachTimeoutSeconds << " seconds.";
  TerminateSession();
}

void DesktopSessionWin::StartMonitoring(const std::string& terminal_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!monitoring_notifications_);
  DCHECK(!session_attach_timer_.IsRunning());

  ReportElapsedTime("started monitoring");

  session_attach_timer_.Start(FROM_HERE,
                              base::Seconds(kSessionAttachTimeoutSeconds), this,
                              &DesktopSessionWin::OnSessionAttachTimeout);

  monitoring_notifications_ = true;
  monitor_->AddWtsTerminalObserver(terminal_id, this);
}

void DesktopSessionWin::StopMonitoring() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (monitoring_notifications_) {
    ReportElapsedTime("stopped monitoring");

    monitoring_notifications_ = false;
    monitor_->RemoveWtsTerminalObserver(this);
  }

  session_attach_timer_.Stop();
  OnSessionDetached();
}

void DesktopSessionWin::TerminateSession() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();

  // This call will delete |this| so it should be at the very end of the method.
  daemon_process()->CloseDesktopSession(id());
}

void DesktopSessionWin::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ReportElapsedTime("channel connected");

  VLOG(1) << "IPC: daemon <- desktop (" << peer_pid << ")";
}

void DesktopSessionWin::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  TerminateSession();
}

void DesktopSessionWin::OnWorkerProcessStopped() {}

void DesktopSessionWin::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::DesktopSessionRequestHandler::Name_) {
    if (desktop_session_request_handler_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::DesktopSessionRequestHandler::Name_;
      CrashDesktopProcess(FROM_HERE);
    }

    mojo::PendingAssociatedReceiver<mojom::DesktopSessionRequestHandler>
        pending_receiver(std::move(handle));
    desktop_session_request_handler_.Bind(std::move(pending_receiver));

    // Reset the receiver on disconnect so |desktop_session_request_handler_|
    // can be re-bound if |launcher_| spawns a new desktop process.
    desktop_session_request_handler_.reset_on_disconnect();
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing the desktop process";
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionWin::OnSessionAttached(uint32_t session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!launcher_);
  DCHECK(monitoring_notifications_);

  ReportElapsedTime("attached");

  // Get the name of the executable to run. `kDesktopBinaryName` specifies
  // uiAccess="true" in its manifest.  Prefer kDesktopBinaryName but fall back
  // to kHostBinaryName if there is a problem loading it.
  base::FilePath desktop_binary;
  bool result = GetInstalledBinaryPath(kDesktopBinaryName, &desktop_binary);

  if (!result || !IsBinaryTrusted(desktop_binary)) {
    result = GetInstalledBinaryPath(kHostBinaryName, &desktop_binary);
  }

  if (!result) {
    TerminateSession();
    return;
  }

  session_attach_timer_.Stop();

  std::unique_ptr<base::CommandLine> target(
      new base::CommandLine(desktop_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeDesktop);
  // Copy the command line switches enabling verbose logging.
  target->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames);

  // Create a delegate capable of launching a process in a different session.
  // Launch elevated to enable injection of Alt+Tab and Ctrl+Alt+Del.
  std::unique_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(
          io_task_runner_, std::move(target), /*launch_elevated=*/true,
          base::WideToUTF8(kDaemonIpcSecurityDescriptor)));
  if (!delegate->Initialize(session_id)) {
    TerminateSession();
    return;
  }

  // Create a launcher for the desktop process, using the per-session delegate.
  launcher_ =
      std::make_unique<WorkerProcessLauncher>(std::move(delegate), this);
  session_id_ = session_id;
}

void DesktopSessionWin::OnSessionDetached() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_.reset();
  desktop_session_request_handler_.reset();
  session_id_ = UINT32_MAX;

  if (monitoring_notifications_) {
    ReportElapsedTime("detached");

    session_attach_timer_.Start(
        FROM_HERE, base::Seconds(kSessionAttachTimeoutSeconds), this,
        &DesktopSessionWin::OnSessionAttachTimeout);
  }
}

void DesktopSessionWin::ConnectDesktopChannel(
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!daemon_process()->OnDesktopSessionAgentAttached(
          id(), session_id_, std::move(desktop_pipe))) {
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionWin::InjectSecureAttentionSequence() {
  InjectSas();
}

void DesktopSessionWin::CrashNetworkProcess() {
  daemon_process()->CrashNetworkProcess(FROM_HERE);
}

void DesktopSessionWin::CrashDesktopProcess(const base::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_->Crash(location);
}

void DesktopSessionWin::ReportElapsedTime(const std::string& event) {
  base::Time now = base::Time::Now();

  std::string passed;
  if (!last_timestamp_.is_null()) {
    passed = base::StringPrintf(", %.2fs passed",
                                (now - last_timestamp_).InSecondsF());
  }

  VLOG(1) << base::StringPrintf(
      "session(%d): %s at %s%s", id(), event.c_str(),
      base::UnlocalizedTimeFormatWithPattern(now, "HH:mm:ss.SSS").c_str(),
      passed.c_str());

  last_timestamp_ = now;
}

}  // namespace remoting
