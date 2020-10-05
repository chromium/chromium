// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me.

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "jingle/glue/thread_wrapper.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/oauth_token_getter_proxy.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/util.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/config_watcher.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ftl_echo_message_listener.h"
#include "remoting/host/ftl_host_change_notification_listener.h"
#include "remoting/host/ftl_signaling_connector.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/host_power_save_blocker.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/host/ipc_host_event_logger.h"
#include "remoting/host/logging.h"
#include "remoting/host/me2me_desktop_environment.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/security_key/security_key_extension.h"
#include "remoting/host/shutdown_watchdog.h"
#include "remoting/host/switches.h"
#include "remoting/host/test_echo_extension.h"
#include "remoting/host/third_party_auth_config.h"
#include "remoting/host/token_validator_factory_impl.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/username.h"
#include "remoting/host/zombie_host_detector.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/port_range.h"
#include "remoting/protocol/token_validator.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/ftl_host_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/remoting_log_to_server.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/api/scoped_refptr.h"

#if defined(OS_POSIX)
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "base/file_descriptor_posix.h"
#include "remoting/host/pam_authorization_factory_posix.h"
#include "remoting/host/posix/signal_handler.h"
#endif  // defined(OS_POSIX)

#if defined(OS_APPLE)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "remoting/host/desktop_capturer_checker.h"
#include "remoting/host/mac/permission_utils.h"
#endif  // defined(OS_APPLE)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <gtk/gtk.h>

#include "base/linux_util.h"
#include "remoting/host/audio_capturer_linux.h"
#include "remoting/host/linux/certificate_watcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include <commctrl.h>
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#include "remoting/host/win/session_desktop_environment.h"
#endif  // defined(OS_WIN)

using remoting::protocol::PairingRegistry;
using remoting::protocol::NetworkSettings;

#if defined(OS_APPLE)

// The following creates a section that tells Mac OS X that it is OK to let us
// inject input in the login screen. Just the name of the section is important,
// not its contents.
__attribute__((used))
__attribute__((section ("__CGPreLoginApp,__cgpreloginapp")))
static const char magic_section[] = "";

#endif  // defined(OS_APPLE)

namespace {

#if !defined(REMOTING_MULTI_PROCESS)
// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

// Value used for --host-config option to indicate that the path must be read
// from stdin.
const char kStdinConfigPath[] = "-";
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// The command line switch used to pass name of the pipe to capture audio on
// linux.
const char kAudioPipeSwitchName[] = "audio-pipe-name";
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
// The command line switch used to pass name of the unix domain socket used to
// listen for security key requests.
const char kAuthSocknameSwitchName[] = "ssh-auth-sockname";
#endif  // defined(OS_POSIX)

// The command line switch used by the parent to request the host to signal it
// when it is successfully started.
const char kSignalParentSwitchName[] = "signal-parent";

// Command line switch used to enable VP9 encoding.
const char kEnableVp9SwitchName[] = "enable-vp9";

// Command line switch used to enable hardware H264 encoding.
const char kEnableH264SwitchName[] = "enable-h264";

// Command line switch used to send a custom offline reason and exit.
const char kReportOfflineReasonSwitchName[] = "report-offline-reason";

// Maximum time to wait for clean shutdown to occur, before forcing termination
// of the process.
const int kShutdownTimeoutSeconds = 15;

// Maximum time to wait for reporting host-offline-reason to the service,
// before continuing normal process shutdown.
const int kHostOfflineReasonTimeoutSeconds = 10;

// Host offline reasons not associated with shutting down the host process
// and therefore not expressible through HostExitCodes enum.
const char kHostOfflineReasonPolicyReadError[] = "POLICY_READ_ERROR";
const char kHostOfflineReasonPolicyChangeRequiresRestart[] =
    "POLICY_CHANGE_REQUIRES_RESTART";
const char kHostOfflineReasonRemoteRestartHost[] = "REMOTE_RESTART_HOST";
const char kHostOfflineReasonZombieStateDetected[] = "ZOMBIE_STATE_DETECTED";

// The default email domain for Googlers. Used to determine whether the host's
// email address is Google-internal or not.
constexpr char kGooglerEmailDomain[] = "@google.com";

}  // namespace

namespace remoting {

class HostProcess : public ConfigWatcher::Delegate,
                    public FtlHostChangeNotificationListener::Listener,
                    public HeartbeatSender::Delegate,
                    public IPC::Listener,
                    public base::RefCountedThreadSafe<HostProcess> {
 public:
  // |shutdown_watchdog| is armed when shutdown is started, and should be kept
  // alive as long as possible until the process exits (since destroying the
  // watchdog disarms it).
  HostProcess(std::unique_ptr<ChromotingHostContext> context,
              int* exit_code_out,
              ShutdownWatchdog* shutdown_watchdog);

  // ConfigWatcher::Delegate interface.
  void OnConfigUpdated(const std::string& serialized_config) override;
  void OnConfigWatcherError() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  // FtlHostChangeNotificationListener::Listener overrides.
  void OnHostDeleted() override;

  // Handler of the ChromotingDaemonNetworkMsg_InitializePairingRegistry IPC
  // message.
  void OnInitializePairingRegistry(
      IPC::PlatformFileForTransit privileged_key,
      IPC::PlatformFileForTransit unprivileged_key);

 private:
  // See SetState method for a list of allowed state transitions.
  enum HostState {
    // Waiting for valid config and policies to be read from the disk.
    // Either the host process has just been started, or it is trying to start
    // again after temporarily going offline due to policy change or error.
    HOST_STARTING,

    // Host is started and running.
    HOST_STARTED,

    // Host is sending offline reason, before trying to restart.
    HOST_GOING_OFFLINE_TO_RESTART,

    // Host is sending offline reason, before shutting down.
    HOST_GOING_OFFLINE_TO_STOP,

    // Host has been stopped (host process will end soon).
    HOST_STOPPED,
  };

  enum PolicyState {
    // Cannot start the host, because a valid policy has not been read yet.
    POLICY_INITIALIZING,

    // Policy was loaded successfully.
    POLICY_LOADED,

    // Policy error was detected, and we haven't yet sent out a
    // host-offline-reason (i.e. because we haven't yet read the config).
    POLICY_ERROR_REPORT_PENDING,

    // Policy error was detected, and we have sent out a host-offline-reason.
    POLICY_ERROR_REPORTED,
  };

  friend class base::RefCountedThreadSafe<HostProcess>;
  ~HostProcess() override;

  void SetState(HostState target_state);

  void StartOnNetworkThread();

#if defined(OS_POSIX)
  // Callback passed to RegisterSignalHandler() to handle SIGTERM events.
  void SigTermHandler(int signal_number);
#endif

  // Called to initialize resources on the UI thread.
  void StartOnUiThread();

  // Initializes IPC control channel and config file path from |cmd_line|.
  // Called on the UI thread.
  bool InitWithCommandLine(const base::CommandLine* cmd_line);

  // Called on the UI thread to start monitoring the configuration file.
  void StartWatchingConfigChanges();

  // Called on the network thread to set the host's Authenticator factory.
  void CreateAuthenticatorFactory();

  // Tear down resources that run on the UI thread.
  void ShutdownOnUiThread();

  // Applies the host config, returning true if successful.
  bool ApplyConfig(const base::DictionaryValue& config);

  // Handles policy updates, by calling On*PolicyUpdate methods.
  void OnPolicyUpdate(std::unique_ptr<base::DictionaryValue> policies);
  void OnPolicyError();
  void ReportPolicyErrorAndRestartHost();
  void ApplyHostDomainListPolicy();
  void ApplyUsernamePolicy();
  bool OnClientDomainListPolicyUpdate(base::DictionaryValue* policies);
  bool OnHostDomainListPolicyUpdate(base::DictionaryValue* policies);
  bool OnUsernamePolicyUpdate(base::DictionaryValue* policies);
  bool OnNatPolicyUpdate(base::DictionaryValue* policies);
  bool OnRelayPolicyUpdate(base::DictionaryValue* policies);
  bool OnUdpPortPolicyUpdate(base::DictionaryValue* policies);
  bool OnCurtainPolicyUpdate(base::DictionaryValue* policies);
  bool OnHostTokenUrlPolicyUpdate(base::DictionaryValue* policies);
  bool OnPairingPolicyUpdate(base::DictionaryValue* policies);
  bool OnGnubbyAuthPolicyUpdate(base::DictionaryValue* policies);
  bool OnFileTransferPolicyUpdate(base::DictionaryValue* policies);

  void InitializeSignaling();

  void StartHostIfReady();
  void StartHost();

  // HeartbeatSender::Delegate implementations.
  void OnFirstHeartbeatSuccessful() override;
  void OnHostNotFound() override;
  void OnAuthFailed() override;
  void OnRemoteRestartHost() override;

  void OnZombieStateDetected();

  void RestartHost(const std::string& host_offline_reason);
  void ShutdownHost(HostExitCodes exit_code);

  // Helper methods doing the work needed by RestartHost and ShutdownHost.
  void GoOffline(const std::string& host_offline_reason);
  void OnHostOfflineReasonAck(bool success);

#if defined(OS_WIN)
  // Initializes the pairing registry on Windows. This should be invoked on the
  // network thread.
  void InitializePairingRegistry(
      IPC::PlatformFileForTransit privileged_key,
      IPC::PlatformFileForTransit unprivileged_key);
#endif  // defined(OS_WIN)

  // Crashes the process in response to a daemon's request. The daemon passes
  // the location of the code that detected the fatal error resulted in this
  // request.
  void OnCrash(const std::string& function_name,
               const std::string& file_name,
               const int& line_number);

  std::unique_ptr<ChromotingHostContext> context_;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Watch for certificate changes and kill the host when changes occur
  std::unique_ptr<CertificateWatcher> cert_watcher_;
#endif

  // Created on the UI thread but used from the network thread.
  base::FilePath host_config_path_;
  std::string host_config_;
  std::unique_ptr<DesktopEnvironmentFactory> desktop_environment_factory_;

  // Accessed on the network thread.
  HostState state_ = HOST_STARTING;

  std::unique_ptr<ConfigWatcher> config_watcher_;

  std::string host_id_;
  std::string pin_hash_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string oauth_refresh_token_;
  std::string robot_account_username_;
  std::string serialized_config_;
  std::string host_owner_;
  bool enable_vp9_ = false;
  bool enable_h264_ = false;

  std::unique_ptr<PolicyWatcher> policy_watcher_;
  PolicyState policy_state_ = POLICY_INITIALIZING;
  std::vector<std::string> client_domain_list_;
  std::vector<std::string> host_domain_list_;
  bool host_username_match_required_ = false;
  bool allow_nat_traversal_ = true;
  bool allow_relay_ = true;
  PortRange udp_port_range_;
  bool allow_pairing_ = true;

  DesktopEnvironmentOptions desktop_environment_options_;
  ThirdPartyAuthConfig third_party_auth_config_;
  bool security_key_auth_policy_enabled_ = false;
  bool security_key_extension_supported_ = true;

  // Used to specify which window to stream, if enabled.
  webrtc::WindowId window_id_ = 0;

  // Must outlive |signal_strategy_| and |ftl_signaling_connector_|.
  std::unique_ptr<OAuthTokenGetterImpl> oauth_token_getter_;

  // Must outlive |host_status_logger_|.
  std::unique_ptr<LogToServer> log_to_server_;

  // Must outlive |signal_strategy_| and |heartbeat_sender_|.
  std::unique_ptr<ZombieHostDetector> zombie_host_detector_;

  // Signal strategies must outlive |ftl_signaling_connector_|.
  std::unique_ptr<SignalStrategy> signal_strategy_;

  std::unique_ptr<FtlSignalingConnector> ftl_signaling_connector_;
  std::unique_ptr<HeartbeatSender> heartbeat_sender_;
  std::unique_ptr<FtlHostChangeNotificationListener>
      ftl_host_change_notification_listener_;
  std::unique_ptr<FtlEchoMessageListener> ftl_echo_message_listener_;

  std::unique_ptr<HostStatusLogger> host_status_logger_;
  std::unique_ptr<HostEventLogger> host_event_logger_;
  std::unique_ptr<HostPowerSaveBlocker> power_save_blocker_;

  std::unique_ptr<ChromotingHost> host_;

  // Used to keep this HostProcess alive until it is shutdown.
  scoped_refptr<HostProcess> self_;

#if defined(REMOTING_MULTI_PROCESS)
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  // Accessed on the UI thread.
  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  // Owned as |desktop_environment_factory_|.
  DesktopSessionConnector* desktop_session_connector_ = nullptr;
#endif  // defined(REMOTING_MULTI_PROCESS)

  int* exit_code_out_;
  bool signal_parent_ = false;
  std::string report_offline_reason_;

  scoped_refptr<PairingRegistry> pairing_registry_;

  ShutdownWatchdog* shutdown_watchdog_;

#if defined(OS_APPLE)
  // When using the command line option to check the Accessibility or Screen
  // Recording permission, these track the permission state and indicate that
  // the host should exit immediately with the result.
  bool checking_permission_state_ = false;
  bool permission_granted_ = false;
#endif  // defined(OS_APPLE)

  DISALLOW_COPY_AND_ASSIGN(HostProcess);
};

HostProcess::HostProcess(std::unique_ptr<ChromotingHostContext> context,
                         int* exit_code_out,
                         ShutdownWatchdog* shutdown_watchdog)
    : context_(std::move(context)),
      desktop_environment_options_(DesktopEnvironmentOptions::CreateDefault()),
      self_(this),
      exit_code_out_(exit_code_out),
      shutdown_watchdog_(shutdown_watchdog) {
  // TODO(zijiehe):
  // desktop_environment_options_.desktop_capture_options()
  //     ->set_use_update_notifications(true);
  // And remove the same line from me2me_desktop_environment.cc.

  StartOnUiThread();

#if defined(OS_APPLE)
  if (checking_permission_state_) {
    *exit_code_out = (permission_granted_ ? EXIT_SUCCESS : EXIT_FAILURE);
  }
#endif
}

HostProcess::~HostProcess() {
  // Verify that UI components have been torn down.
  DCHECK(!config_watcher_);
  DCHECK(!desktop_environment_factory_);

  // We might be getting deleted on one of the threads the |host_context| owns,
  // so we need to post it back to the caller thread to safely join & delete the
  // threads it contains.  This will go away when we move to AutoThread.
  // |context_release()| will null |context_| before the method is invoked, so
  // we need to pull out the task-runner on which to call DeleteSoon first.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context_->ui_task_runner();
  task_runner->DeleteSoon(FROM_HERE, context_.release());
}

bool HostProcess::InitWithCommandLine(const base::CommandLine* cmd_line) {
#if defined(OS_APPLE)
  if (cmd_line->HasSwitch(kCheckAccessibilityPermissionSwitchName)) {
    checking_permission_state_ = true;
    permission_granted_ = mac::CanInjectInput();
    return false;
  }
  if (cmd_line->HasSwitch(kCheckScreenRecordingPermissionSwitchName)) {
    // Trigger screen-capture, even if CanRecordScreen() returns true. It uses a
    // heuristic that might not be 100% reliable, but it is critically
    // important to add the host bundle to the list of apps under
    // Security & Privacy -> Screen Recording.
    if (base::mac::IsAtLeastOS10_15()) {
      DesktopCapturerChecker().TriggerSingleCapture();
    }
    checking_permission_state_ = true;
    permission_granted_ = mac::CanRecordScreen();
    return false;
  }
#endif  // defined(OS_APPLE)

#if defined(REMOTING_MULTI_PROCESS)
  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      context_->network_task_runner()->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

  auto endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(*cmd_line);
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));

  // Connect to the daemon process.
  daemon_channel_ = IPC::ChannelProxy::Create(
      invitation
          .ExtractMessagePipe(cmd_line->GetSwitchValueASCII(kMojoPipeToken))
          .release(),
      IPC::Channel::MODE_CLIENT, this, context_->network_task_runner(),
      base::ThreadTaskRunnerHandle::Get());

#else  // !defined(REMOTING_MULTI_PROCESS)
  if (cmd_line->HasSwitch(kHostConfigSwitchName)) {
    host_config_path_ = cmd_line->GetSwitchValuePath(kHostConfigSwitchName);

    // Read config from stdin if necessary.
    if (host_config_path_ == base::FilePath(kStdinConfigPath)) {
      const size_t kBufferSize = 4096;
      std::unique_ptr<char[]> buf(new char[kBufferSize]);
      size_t len;
      while ((len = fread(buf.get(), 1, kBufferSize, stdin)) > 0) {
        host_config_.append(buf.get(), len);
      }
    }
  } else {
    base::FilePath default_config_dir = remoting::GetConfigDir();
    host_config_path_ = default_config_dir.Append(kDefaultHostConfigFile);
  }

  if (host_config_path_ != base::FilePath(kStdinConfigPath) &&
      !base::PathExists(host_config_path_)) {
    LOG(ERROR) << "Can't find host config at " << host_config_path_.value();
    return false;
  }
#endif  // !defined(REMOTING_MULTI_PROCESS)

  // Ignore certificate requests - the host currently has no client certificate
  // support, so ignoring certificate requests allows connecting to servers that
  // request, but don't require, a certificate (optional client authentication).
  net::URLFetcher::SetIgnoreCertificateRequests(true);

  signal_parent_ = cmd_line->HasSwitch(kSignalParentSwitchName);

  if (cmd_line->HasSwitch(kReportOfflineReasonSwitchName)) {
    report_offline_reason_ =
        cmd_line->GetSwitchValueASCII(kReportOfflineReasonSwitchName);
    if (report_offline_reason_.empty()) {
      LOG(ERROR) << "--" << kReportOfflineReasonSwitchName
                 << " requires an argument.";
      return false;
    }
  }

  return true;
}

void HostProcess::OnConfigUpdated(
    const std::string& serialized_config) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&HostProcess::OnConfigUpdated, this, serialized_config));
    return;
  }

  // Filter out duplicates.
  if (serialized_config_ == serialized_config)
    return;

  HOST_LOG << "Processing new host configuration.";

  serialized_config_ = serialized_config;
  std::unique_ptr<base::DictionaryValue> config(
      HostConfigFromJson(serialized_config));
  if (!config) {
    LOG(ERROR) << "Invalid configuration.";
    ShutdownHost(kInvalidHostConfigurationExitCode);
    return;
  }

  if (!ApplyConfig(*config)) {
    LOG(ERROR) << "Failed to apply the configuration.";
    ShutdownHost(kInvalidHostConfigurationExitCode);
    return;
  }

  if (state_ == HOST_STARTING) {
    StartHostIfReady();
  } else if (state_ == HOST_STARTED) {
    // Reapply policies that could be affected by a new config.
    DCHECK_EQ(policy_state_, POLICY_LOADED);
    ApplyHostDomainListPolicy();
    ApplyUsernamePolicy();

    // TODO(sergeyu): Here we assume that PIN is the only part of the config
    // that may change while the service is running. Change ApplyConfig() to
    // detect other changes in the config and restart host if necessary here.
    CreateAuthenticatorFactory();
  }
}

void HostProcess::OnConfigWatcherError() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  ShutdownHost(kInvalidHostConfigurationExitCode);
}

// Allowed state transitions (enforced via DCHECKs in SetState method):
//   STARTING->STARTED (once we have valid config + policy)
//   STARTING->GOING_OFFLINE_TO_STOP
//   STARTING->GOING_OFFLINE_TO_RESTART
//   STARTED->GOING_OFFLINE_TO_STOP
//   STARTED->GOING_OFFLINE_TO_RESTART
//   GOING_OFFLINE_TO_RESTART->GOING_OFFLINE_TO_STOP
//   GOING_OFFLINE_TO_RESTART->STARTING (after OnHostOfflineReasonAck)
//   GOING_OFFLINE_TO_STOP->STOPPED (after OnHostOfflineReasonAck)
//
// |host_| must be not-null in STARTED state and nullptr in all other states
// (although this invariant can be temporarily violated when doing
// synchronous processing on the networking thread).
void HostProcess::SetState(HostState target_state) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  // DCHECKs below enforce state allowed transitions listed in HostState.
  switch (state_) {
    case HOST_STARTING:
      DCHECK((target_state == HOST_STARTED) ||
             (target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_GOING_OFFLINE_TO_RESTART))
          << state_ << " -> " << target_state;
      break;
    case HOST_STARTED:
      DCHECK((target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_GOING_OFFLINE_TO_RESTART))
          << state_ << " -> " << target_state;
      break;
    case HOST_GOING_OFFLINE_TO_RESTART:
      DCHECK((target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_STARTING))
          << state_ << " -> " << target_state;
      break;
    case HOST_GOING_OFFLINE_TO_STOP:
      DCHECK_EQ(target_state, HOST_STOPPED);
      break;
    case HOST_STOPPED:  // HOST_STOPPED is a terminal state.
    default:
      NOTREACHED() << state_ << " -> " << target_state;
      break;
  }
  state_ = target_state;
}

void HostProcess::StartOnNetworkThread() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != HOST_STARTING) {
    // Host was shutdown before the task had a chance to run.
    return;
  }

#if !defined(REMOTING_MULTI_PROCESS)
  if (host_config_path_ == base::FilePath(kStdinConfigPath)) {
    // Process config we've read from stdin.
    OnConfigUpdated(host_config_);
  } else {
    // Start watching the host configuration file.
    config_watcher_.reset(new ConfigFileWatcher(context_->network_task_runner(),
                                                context_->file_task_runner(),
                                                host_config_path_));
    config_watcher_->Watch(this);
  }
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if defined(OS_POSIX)
  remoting::RegisterSignalHandler(
      SIGTERM, base::BindRepeating(&HostProcess::SigTermHandler,
                                   base::Unretained(this)));
#endif  // defined(OS_POSIX)
}

#if defined(OS_POSIX)
void HostProcess::SigTermHandler(int signal_number) {
  DCHECK(signal_number == SIGTERM);
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  HOST_LOG << "Caught SIGTERM: Shutting down...";
  ShutdownHost(kSuccessExitCode);
}
#endif  // OS_POSIX

void HostProcess::CreateAuthenticatorFactory() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != HOST_STARTED)
    return;

  std::string local_certificate = key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    ShutdownHost(kInitializationFailed);
    return;
  }

  std::unique_ptr<protocol::AuthenticatorFactory> factory;

  if (third_party_auth_config_.is_null()) {
    scoped_refptr<PairingRegistry> pairing_registry;
    if (allow_pairing_) {
      // On Windows |pairing_registry_| is initialized in
      // InitializePairingRegistry().
#if !defined(OS_WIN)
      if (!pairing_registry_) {
        std::unique_ptr<PairingRegistry::Delegate> delegate =
            CreatePairingRegistryDelegate();

        if (delegate)
          pairing_registry_ = new PairingRegistry(context_->file_task_runner(),
                                                  std::move(delegate));
      }
#endif  // defined(OS_WIN)

      pairing_registry = pairing_registry_;
    }

    factory = protocol::Me2MeHostAuthenticatorFactory::CreateWithPin(
        host_owner_, local_certificate, key_pair_, client_domain_list_,
        pin_hash_, pairing_registry);

    host_->set_pairing_registry(pairing_registry);
  } else {
    // ThirdPartyAuthConfig::Parse() leaves the config in a valid state, so
    // these URLs are both valid.
    DCHECK(third_party_auth_config_.token_url.is_valid());
    DCHECK(third_party_auth_config_.token_validation_url.is_valid());

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    if (!cert_watcher_) {
      cert_watcher_ = std::make_unique<CertificateWatcher>(
          base::BindRepeating(&HostProcess::ShutdownHost, this,
                              kSuccessExitCode),
          context_->file_task_runner());
      cert_watcher_->Start();
    }
    cert_watcher_->SetMonitor(host_->status_monitor());
#endif

    scoped_refptr<protocol::TokenValidatorFactory> token_validator_factory =
        new TokenValidatorFactoryImpl(third_party_auth_config_, key_pair_,
                                      context_->url_request_context_getter());
    factory = protocol::Me2MeHostAuthenticatorFactory::CreateWithThirdPartyAuth(
        host_owner_, local_certificate, key_pair_, client_domain_list_,
        token_validator_factory);
  }

#if defined(OS_POSIX)
  // On Linux and Mac, perform a PAM authorization step after authentication.
  factory.reset(new PamAuthorizationFactory(std::move(factory)));
#endif
  host_->SetAuthenticatorFactory(std::move(factory));
}

// IPC::Listener implementation.
bool HostProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if defined(REMOTING_MULTI_PROCESS)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(HostProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingDaemonMsg_Crash, OnCrash)
    IPC_MESSAGE_HANDLER(ChromotingDaemonNetworkMsg_Configuration,
                        OnConfigUpdated)
    IPC_MESSAGE_HANDLER(ChromotingDaemonNetworkMsg_InitializePairingRegistry,
                        OnInitializePairingRegistry)
    IPC_MESSAGE_FORWARD(
        ChromotingDaemonNetworkMsg_DesktopAttached,
        desktop_session_connector_,
        DesktopSessionConnector::OnDesktopSessionAgentAttached)
    IPC_MESSAGE_FORWARD(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                        desktop_session_connector_,
                        DesktopSessionConnector::OnTerminalDisconnected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;

#else  // !defined(REMOTING_MULTI_PROCESS)
  return false;
#endif  // !defined(REMOTING_MULTI_PROCESS)
}

void HostProcess::OnChannelError() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  // Shutdown the host if the daemon process disconnects the IPC channel.
  context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HostProcess::ShutdownHost, this, kSuccessExitCode));
}

void HostProcess::StartOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  if (!InitWithCommandLine(base::CommandLine::ForCurrentProcess())) {
    // Shutdown the host if the command line is invalid.
    ShutdownOnUiThread();
    return;
  }

  if (!report_offline_reason_.empty()) {
    // Don't need to do any UI initialization.
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::StartOnNetworkThread, this));
    return;
  }

  policy_watcher_ =
      PolicyWatcher::CreateWithTaskRunner(context_->file_task_runner());
  policy_watcher_->StartWatching(
      base::BindRepeating(&HostProcess::OnPolicyUpdate, base::Unretained(this)),
      base::BindRepeating(&HostProcess::OnPolicyError, base::Unretained(this)));

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // If an audio pipe is specific on the command-line then initialize
  // AudioCapturerLinux to capture from it.
  base::FilePath audio_pipe_name = base::CommandLine::ForCurrentProcess()->
      GetSwitchValuePath(kAudioPipeSwitchName);
  if (!audio_pipe_name.empty()) {
    remoting::AudioCapturerLinux::InitializePipeReader(
        context_->audio_task_runner(), audio_pipe_name);
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
  base::FilePath security_key_socket_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kAuthSocknameSwitchName);
  if (!security_key_socket_name.empty()) {
    remoting::SecurityKeyAuthHandler::SetSecurityKeySocketName(
        security_key_socket_name);
  } else {
    security_key_extension_supported_ = false;
  }
#endif  // defined(OS_POSIX)

  // Create a desktop environment factory appropriate to the build type &
  // platform.
#if defined(REMOTING_MULTI_PROCESS)
  IpcDesktopEnvironmentFactory* desktop_environment_factory =
      new IpcDesktopEnvironmentFactory(
          context_->audio_task_runner(), context_->network_task_runner(),
          context_->network_task_runner(), daemon_channel_.get());
  desktop_session_connector_ = desktop_environment_factory;
#else  // !defined(REMOTING_MULTI_PROCESS)
  BasicDesktopEnvironmentFactory* desktop_environment_factory;
  desktop_environment_factory = new Me2MeDesktopEnvironmentFactory(
      context_->network_task_runner(), context_->video_capture_task_runner(),
      context_->input_task_runner(), context_->ui_task_runner());
#endif  // !defined(REMOTING_MULTI_PROCESS)

  desktop_environment_factory_.reset(desktop_environment_factory);

  context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&HostProcess::StartOnNetworkThread, this));
}

void HostProcess::ShutdownOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  // Tear down resources that need to be torn down on the UI thread.
  desktop_environment_factory_.reset();
  policy_watcher_.reset();

#if defined(REMOTING_MULTI_PROCESS)
  daemon_channel_.reset();
#endif  // defined(REMOTING_MULTI_PROCESS)

  // It is now safe for the HostProcess to be deleted.
  self_ = nullptr;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Cause the global AudioPipeReader to be freed, otherwise the audio
  // thread will remain in-use and prevent the process from exiting.
  // TODO(wez): DesktopEnvironmentFactory should own the pipe reader.
  // See crbug.com/161373 and crbug.com/104544.
  AudioCapturerLinux::InitializePipeReader(nullptr, base::FilePath());
#endif
}

void HostProcess::OnHostNotFound() {
  LOG(ERROR) << "Host ID not found.";
  ShutdownHost(kInvalidHostIdExitCode);
}

void HostProcess::OnFirstHeartbeatSuccessful() {
  if (state_ != HOST_STARTED) {
    return;
  }
  HOST_LOG << "Host ready to receive connections.";
#if defined(OS_POSIX)
  if (signal_parent_) {
    kill(getppid(), SIGUSR1);
    signal_parent_ = false;
  }
#endif
}

void HostProcess::OnHostDeleted() {
  LOG(ERROR) << "Host was deleted from the directory.";
  ShutdownHost(kHostDeletedExitCode);
}

void HostProcess::OnInitializePairingRegistry(
    IPC::PlatformFileForTransit privileged_key,
    IPC::PlatformFileForTransit unprivileged_key) {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if defined(OS_WIN)
  context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&HostProcess::InitializePairingRegistry, this,
                                privileged_key, unprivileged_key));
#else  // !defined(OS_WIN)
  NOTREACHED();
#endif  // !defined(OS_WIN)
}

#if defined(OS_WIN)
void HostProcess::InitializePairingRegistry(
    IPC::PlatformFileForTransit privileged_key,
    IPC::PlatformFileForTransit unprivileged_key) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  // |privileged_key| can be nullptr but not |unprivileged_key|.
  DCHECK(unprivileged_key.IsValid());
  // |pairing_registry_| should only be initialized once.
  DCHECK(!pairing_registry_);

  HKEY privileged_hkey = reinterpret_cast<HKEY>(
      IPC::PlatformFileForTransitToPlatformFile(privileged_key));
  HKEY unprivileged_hkey = reinterpret_cast<HKEY>(
      IPC::PlatformFileForTransitToPlatformFile(unprivileged_key));

  std::unique_ptr<PairingRegistryDelegateWin> delegate(
      new PairingRegistryDelegateWin());
  delegate->SetRootKeys(privileged_hkey, unprivileged_hkey);

  pairing_registry_ = new PairingRegistry(context_->file_task_runner(),
                                          std::move(delegate));

  // (Re)Create the authenticator factory now that |pairing_registry_| has been
  // initialized.
  CreateAuthenticatorFactory();
}
#endif  // !defined(OS_WIN)

// Applies the host config, returning true if successful.
bool HostProcess::ApplyConfig(const base::DictionaryValue& config) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!config.GetString(kHostIdConfigPath, &host_id_)) {
    LOG(ERROR) << "Config does not define " << kHostIdConfigPath << ".";
    return false;
  }

  std::string key_base64;
  if (!config.GetString(kPrivateKeyConfigPath, &key_base64)) {
    LOG(ERROR) << "Private key couldn't be read from the config file.";
    return false;
  }

  key_pair_ = RsaKeyPair::FromString(key_base64);
  if (!key_pair_.get()) {
    LOG(ERROR) << "Invalid private key in the config file.";
    return false;
  }

  std::string host_secret_hash_string;
  if (!config.GetString(kHostSecretHashConfigPath, &host_secret_hash_string) ||
      !ParsePinHashFromConfig(host_secret_hash_string, host_id_, &pin_hash_)) {
    LOG(ERROR) << "Cannot parse host_secret_hash configuration value.";
    return false;
  }

  // Retrieve robot account credentials for session signaling.
  if (!config.GetString(kXmppLoginConfigPath, &robot_account_username_) ||
      !config.GetString(kOAuthRefreshTokenConfigPath, &oauth_refresh_token_)) {
    LOG(ERROR) << "Robot account credentials are not defined in the config.";
    return false;
  }

  // Some old host configs have a host_owner field that's set to a JID ending
  // with @id.talk.google.com, and a host_owner_email field that's set to the
  // owner's actual email address. Other host configs only have a host_owner
  // field. We are not generating separate addresses nor using JID any more but
  // we still read host_owner_email first for compatibility reason.
  const std::string* host_owner_ptr =
      config.FindStringPath(kHostOwnerEmailConfigPath);
  if (!host_owner_ptr) {
    host_owner_ptr = config.FindStringPath(kHostOwnerConfigPath);
  }
  if (!host_owner_ptr) {
    LOG(ERROR) << "Host config has no host_owner or host_owner_email fields.";
    return false;
  }
  host_owner_ = *host_owner_ptr;

  // Allow offering of VP9 encoding to be overridden by the command-line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableVp9SwitchName)) {
    enable_vp9_ = true;
  } else {
    config.GetBoolean(kEnableVp9ConfigPath, &enable_vp9_);
  }

  // Allow offering of hardware H264 encoding to be overridden by the command
  // line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableH264SwitchName)) {
    enable_h264_ = true;
  } else {
    config.GetBoolean(kEnableH264ConfigPath, &enable_h264_);
  }

  return true;
}

void HostProcess::OnPolicyUpdate(
    std::unique_ptr<base::DictionaryValue> policies) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::OnPolicyUpdate, this,
                                  std::move(policies)));
    return;
  }

  bool restart_required = false;
  restart_required |= OnClientDomainListPolicyUpdate(policies.get());
  restart_required |= OnHostDomainListPolicyUpdate(policies.get());
  restart_required |= OnCurtainPolicyUpdate(policies.get());
  // Note: UsernamePolicyUpdate must run after OnCurtainPolicyUpdate.
  restart_required |= OnUsernamePolicyUpdate(policies.get());
  restart_required |= OnNatPolicyUpdate(policies.get());
  restart_required |= OnRelayPolicyUpdate(policies.get());
  restart_required |= OnUdpPortPolicyUpdate(policies.get());
  restart_required |= OnHostTokenUrlPolicyUpdate(policies.get());
  restart_required |= OnPairingPolicyUpdate(policies.get());
  restart_required |= OnGnubbyAuthPolicyUpdate(policies.get());
  restart_required |= OnFileTransferPolicyUpdate(policies.get());

  policy_state_ = POLICY_LOADED;

  if (state_ == HOST_STARTING) {
    StartHostIfReady();
  } else if (state_ == HOST_STARTED) {
    if (restart_required)
      RestartHost(kHostOfflineReasonPolicyChangeRequiresRestart);
  }
}

void HostProcess::OnPolicyError() {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::OnPolicyError, this));
    return;
  }

  if (policy_state_ != POLICY_ERROR_REPORTED) {
    policy_state_ = POLICY_ERROR_REPORT_PENDING;
    if ((state_ == HOST_STARTED) ||
        (state_ == HOST_STARTING && !serialized_config_.empty())) {
      ReportPolicyErrorAndRestartHost();
    }
  }
}

void HostProcess::ReportPolicyErrorAndRestartHost() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!serialized_config_.empty());

  DCHECK_EQ(policy_state_, POLICY_ERROR_REPORT_PENDING);
  policy_state_ = POLICY_ERROR_REPORTED;

  HOST_LOG << "Restarting the host due to policy errors.";
  RestartHost(kHostOfflineReasonPolicyReadError);
}

void HostProcess::ApplyHostDomainListPolicy() {
  if (state_ != HOST_STARTED)
    return;

  HOST_LOG << "Policy sets host domains: "
           << base::JoinString(host_domain_list_, ", ");

  if (!host_domain_list_.empty()) {
    bool matched = false;
    for (const std::string& domain : host_domain_list_) {
      if (base::EndsWith(host_owner_, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
      }
    }
    if (!matched) {
      LOG(ERROR) << "The host domain does not match the policy.";
      ShutdownHost(kInvalidHostDomainExitCode);
    }
  }
}

bool HostProcess::OnHostDomainListPolicyUpdate(
    base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  const base::ListValue* list;
  if (!policies->GetList(policy::key::kRemoteAccessHostDomainList, &list)) {
    return false;
  }

  host_domain_list_.clear();
  for (const auto& value : *list) {
    host_domain_list_.push_back(value.GetString());
  }

  ApplyHostDomainListPolicy();
  return false;
}

bool HostProcess::OnClientDomainListPolicyUpdate(
    base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  const base::ListValue* list;
  if (!policies->GetList(policy::key::kRemoteAccessHostClientDomainList,
                         &list)) {
    return false;
  }

  client_domain_list_.clear();
  for (const auto& value : *list) {
    client_domain_list_.push_back(value.GetString());
  }

  return true;
}

void HostProcess::ApplyUsernamePolicy() {
  if (state_ != HOST_STARTED)
    return;

  if (host_username_match_required_) {
    HOST_LOG << "Policy requires host username match.";

    std::string username = GetUsername();
    bool shutdown =
        username.empty() ||
        !base::StartsWith(host_owner_, username + std::string("@"),
                          base::CompareCase::INSENSITIVE_ASCII);

#if defined(OS_APPLE)
    // On Mac, we run as root at the login screen, so the username won't match.
    // However, there's no need to enforce the policy at the login screen, as
    // the client will have to reconnect if a login occurs.
    if (shutdown && getuid() == 0) {
      shutdown = false;
    }
#endif

    // Curtain-mode on Windows presents the standard OS login prompt to the user
    // for each connection, removing the need for an explicit user-name matching
    // check.
#if defined(OS_WIN) && defined(REMOTING_RDP_SESSION)
    if (desktop_environment_options_.enable_curtaining())
      return;
#endif  // defined(OS_WIN) && defined(REMOTING_RDP_SESSION)

    // Shutdown the host if the username does not match.
    if (shutdown) {
      LOG(ERROR) << "The host username does not match.";
      ShutdownHost(kUsernameMismatchExitCode);
    }
  } else {
    HOST_LOG << "Policy does not require host username match.";
  }
}

bool HostProcess::OnUsernamePolicyUpdate(base::DictionaryValue* policies) {
  // Returns false: never restart the host after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!policies->GetBoolean(policy::key::kRemoteAccessHostMatchUsername,
                            &host_username_match_required_)) {
    return false;
  }

  ApplyUsernamePolicy();
  return false;
}

bool HostProcess::OnNatPolicyUpdate(base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!policies->GetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                            &allow_nat_traversal_)) {
    return false;
  }

  if (allow_nat_traversal_) {
    HOST_LOG << "Policy enables NAT traversal.";
  } else {
    HOST_LOG << "Policy disables NAT traversal.";
  }
  return true;
}

bool HostProcess::OnRelayPolicyUpdate(base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!policies->GetBoolean(
          policy::key::kRemoteAccessHostAllowRelayedConnection,
          &allow_relay_)) {
    return false;
  }

  if (allow_relay_) {
    HOST_LOG << "Policy enables use of relay server.";
  } else {
    HOST_LOG << "Policy disables use of relay server.";
  }
  return true;
}

bool HostProcess::OnUdpPortPolicyUpdate(base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  std::string string_value;
  if (!policies->GetString(policy::key::kRemoteAccessHostUdpPortRange,
                           &string_value)) {
    return false;
  }

  if (!PortRange::Parse(string_value, &udp_port_range_)) {
    // PolicyWatcher verifies that the value is formatted correctly.
    LOG(FATAL) << "Invalid port range: " << string_value;
  }
  HOST_LOG << "Policy restricts UDP port range to: " << udp_port_range_;
  return true;
}

bool HostProcess::OnCurtainPolicyUpdate(base::DictionaryValue* policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  bool curtain_required;
  if (!policies->GetBoolean(policy::key::kRemoteAccessHostRequireCurtain,
                            &curtain_required)) {
    return false;
  }
  desktop_environment_options_.set_enable_curtaining(curtain_required);

#if defined(OS_APPLE)
  if (curtain_required) {
    // When curtain mode is in effect on Mac, the host process runs in the
    // user's switched-out session, but launchd will also run an instance at
    // the console login screen.  Even if no user is currently logged-on, we
    // can't support remote-access to the login screen because the current host
    // process model disconnects the client during login, which would leave
    // the logged in session un-curtained on the console until they reconnect.
    //
    // TODO(jamiewalch): Fix this once we have implemented the multi-process
    // daemon architecture (crbug.com/134894)
    if (getuid() == 0) {
      LOG(ERROR) << "Running the host in the console login session is not yet "
                    "supported.";
      ShutdownHost(kLoginScreenNotSupportedExitCode);
      return false;
    }
  }
#endif

  if (curtain_required) {
    HOST_LOG << "Policy requires curtain-mode.";
  } else {
    HOST_LOG << "Policy does not require curtain-mode.";
  }

  return true;
}

bool HostProcess::OnHostTokenUrlPolicyUpdate(base::DictionaryValue* policies) {
  switch (ThirdPartyAuthConfig::Parse(*policies, &third_party_auth_config_)) {
    case ThirdPartyAuthConfig::NoPolicy:
      return false;
    case ThirdPartyAuthConfig::ParsingSuccess:
      HOST_LOG << "Policy sets third-party token URLs: "
               << third_party_auth_config_;
      return true;
    case ThirdPartyAuthConfig::InvalidPolicy:
    default:
      // Unreachable, because PolicyWatcher::OnPolicyUpdated() enforces that
      // the policy is well-formed (including checks specific to
      // ThirdPartyAuthConfig), before notifying of policy updates.
      NOTREACHED();
      return false;
  }
}

bool HostProcess::OnPairingPolicyUpdate(base::DictionaryValue* policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!policies->GetBoolean(policy::key::kRemoteAccessHostAllowClientPairing,
                            &allow_pairing_)) {
    return false;
  }

  if (allow_pairing_) {
    HOST_LOG << "Policy enables client pairing.";
  } else {
    HOST_LOG << "Policy disables client pairing.";
  }
  return true;
}

bool HostProcess::OnGnubbyAuthPolicyUpdate(base::DictionaryValue* policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!policies->GetBoolean(policy::key::kRemoteAccessHostAllowGnubbyAuth,
                            &security_key_auth_policy_enabled_)) {
    return false;
  }

  if (security_key_auth_policy_enabled_) {
    HOST_LOG << "Policy enables security key auth.";
  } else {
    HOST_LOG << "Policy disables security key auth.";
  }

  return true;
}

bool HostProcess::OnFileTransferPolicyUpdate(base::DictionaryValue* policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  bool file_transfer_enabled;
  if (!policies->GetBoolean(policy::key::kRemoteAccessHostAllowFileTransfer,
                            &file_transfer_enabled)) {
    return false;
  }
  desktop_environment_options_.set_enable_file_transfer(file_transfer_enabled);

  if (file_transfer_enabled) {
    HOST_LOG << "Policy enables file transfer.";
  } else {
    HOST_LOG << "Policy disables file transfer.";
  }

  // Restart required.
  return true;
}

void HostProcess::InitializeSignaling() {
  DCHECK(!host_id_.empty());  // ApplyConfig() should already have been run.

  DCHECK(!signal_strategy_);
  DCHECK(!oauth_token_getter_);
  DCHECK(!ftl_signaling_connector_);
  DCHECK(!heartbeat_sender_);

  auto oauth_credentials =
      std::make_unique<OAuthTokenGetter::OAuthAuthorizationCredentials>(
          robot_account_username_, oauth_refresh_token_,
          /* is_service_account */ true);
  // Unretained is sound because we own the OAuthTokenGetterImpl, and the
  // callback will never be invoked once it is destroyed.
  oauth_token_getter_ = std::make_unique<OAuthTokenGetterImpl>(
      std::move(oauth_credentials),
      context_->url_loader_factory(), false);

  log_to_server_ = std::make_unique<RemotingLogToServer>(
      ServerLogEntry::ME2ME,
      std::make_unique<OAuthTokenGetterProxy>(
          oauth_token_getter_->GetWeakPtr()),
      context_->url_loader_factory());
  zombie_host_detector_ = std::make_unique<ZombieHostDetector>(base::BindOnce(
      &HostProcess::OnZombieStateDetected, base::Unretained(this)));
  auto ftl_signal_strategy = std::make_unique<FtlSignalStrategy>(
      std::make_unique<OAuthTokenGetterProxy>(
          oauth_token_getter_->GetWeakPtr()),
      context_->url_loader_factory(),
      std::make_unique<FtlHostDeviceIdProvider>(host_id_),
      zombie_host_detector_.get());
  ftl_signaling_connector_ = std::make_unique<FtlSignalingConnector>(
      ftl_signal_strategy.get(),
      base::BindOnce(&HostProcess::OnAuthFailed, base::Unretained(this)));
  ftl_signaling_connector_->Start();

  // Check if the host owner's email is Google-internal.
  bool is_googler = base::EndsWith(host_owner_, kGooglerEmailDomain,
                                   base::CompareCase::INSENSITIVE_ASCII);

  heartbeat_sender_ = std::make_unique<HeartbeatSender>(
      this, host_id_, ftl_signal_strategy.get(), oauth_token_getter_.get(),
      zombie_host_detector_.get(), context_->url_loader_factory(), is_googler);
  signal_strategy_ = std::move(ftl_signal_strategy);

  zombie_host_detector_->Start();
}

void HostProcess::StartHostIfReady() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, HOST_STARTING);

  // Start the host if both the config and the policies are loaded.
  if (!serialized_config_.empty()) {
    if (!report_offline_reason_.empty()) {
      SetState(HOST_GOING_OFFLINE_TO_STOP);
      GoOffline(report_offline_reason_);
    } else if (policy_state_ == POLICY_LOADED) {
      StartHost();
    } else if (policy_state_ == POLICY_ERROR_REPORT_PENDING) {
      ReportPolicyErrorAndRestartHost();
    }
  }
}

void HostProcess::StartHost() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_);

  SetState(HOST_STARTED);

  InitializeSignaling();

  uint32_t network_flags = 0;
  if (allow_nat_traversal_) {
    network_flags = NetworkSettings::NAT_TRAVERSAL_STUN |
                    NetworkSettings::NAT_TRAVERSAL_OUTGOING;
    if (allow_relay_)
      network_flags |= NetworkSettings::NAT_TRAVERSAL_RELAY;
  }

  NetworkSettings network_settings(network_flags);

  if (!udp_port_range_.is_null()) {
    network_settings.port_range = udp_port_range_;
  } else if (!allow_nat_traversal_) {
    // For legacy reasons we have to restrict the port range to a set of default
    // values when nat traversal is disabled, even if the port range was not
    // set in policy.
    network_settings.port_range.min_port = NetworkSettings::kDefaultMinPort;
    network_settings.port_range.max_port = NetworkSettings::kDefaultMaxPort;
  }

  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          context_->url_loader_factory(), network_settings,
          protocol::TransportRole::SERVER);
  std::unique_ptr<protocol::SessionManager> session_manager(
      new protocol::JingleSessionManager(signal_strategy_.get()));

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  if (!desktop_environment_factory_->SupportsAudioCapture())
    protocol_config->DisableAudioChannel();
  if (enable_vp9_)
    protocol_config->set_vp9_experiment_enabled(true);
  if (enable_h264_)
    protocol_config->set_h264_experiment_enabled(true);
  protocol_config->set_webrtc_supported(true);
  session_manager->set_protocol_config(std::move(protocol_config));

  host_.reset(new ChromotingHost(desktop_environment_factory_.get(),
                                 std::move(session_manager), transport_context,
                                 context_->audio_task_runner(),
                                 context_->video_encode_task_runner(),
                                 desktop_environment_options_));

  if (security_key_auth_policy_enabled_ && security_key_extension_supported_) {
    host_->AddExtension(
        std::make_unique<SecurityKeyExtension>(context_->file_task_runner()));
  }

  host_->AddExtension(std::make_unique<TestEchoExtension>());

  // TODO(simonmorris): Get the maximum session duration from a policy.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  host_->SetMaximumSessionDuration(base::TimeDelta::FromHours(20));
#endif

  host_status_logger_ = std::make_unique<HostStatusLogger>(
      host_->status_monitor(), log_to_server_.get());

  power_save_blocker_.reset(new HostPowerSaveBlocker(
      host_->status_monitor(), context_->ui_task_runner(),
      context_->file_task_runner()));

  ftl_host_change_notification_listener_ =
      std::make_unique<FtlHostChangeNotificationListener>(
          this, signal_strategy_.get());

  ftl_echo_message_listener_ = std::make_unique<FtlEchoMessageListener>(
      host_owner_, signal_strategy_.get());

  // Set up reporting the host status notifications.
#if defined(REMOTING_MULTI_PROCESS)
  host_event_logger_.reset(
      new IpcHostEventLogger(host_->status_monitor(), daemon_channel_.get()));
#else  // !defined(REMOTING_MULTI_PROCESS)
  host_event_logger_ =
      HostEventLogger::Create(host_->status_monitor(), kApplicationName);
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if defined(OS_APPLE)
  // Don't run the permission-checks as root (i.e. at the login screen), as they
  // are not actionable there.
  // Also, the permission-checks are not needed on MacOS 10.15+, as they are
  // always handled by the new permission-wizard (the old shell script is
  // never used on 10.15+).
  if (getuid() != 0U && base::mac::IsAtMostOS10_14()) {
    mac::PromptUserToChangeTrustStateIfNeeded(context_->ui_task_runner());
  }
#endif  // defined(OS_APPLE)

  host_->Start(host_owner_);

  CreateAuthenticatorFactory();

  ApplyHostDomainListPolicy();
  ApplyUsernamePolicy();
}

void HostProcess::OnAuthFailed() {
  ShutdownHost(kInvalidOauthCredentialsExitCode);
}

void HostProcess::OnRemoteRestartHost() {
  RestartHost(kHostOfflineReasonRemoteRestartHost);
}

void HostProcess::OnZombieStateDetected() {
  RestartHost(kHostOfflineReasonZombieStateDetected);
}

void HostProcess::RestartHost(const std::string& host_offline_reason) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_offline_reason.empty());

  SetState(HOST_GOING_OFFLINE_TO_RESTART);
  GoOffline(host_offline_reason);
}

void HostProcess::ShutdownHost(HostExitCodes exit_code) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  *exit_code_out_ = exit_code;

  switch (state_) {
    case HOST_STARTING:
    case HOST_STARTED:
      SetState(HOST_GOING_OFFLINE_TO_STOP);
      GoOffline(ExitCodeToString(exit_code));
      break;

    case HOST_GOING_OFFLINE_TO_RESTART:
      SetState(HOST_GOING_OFFLINE_TO_STOP);
      break;

    case HOST_GOING_OFFLINE_TO_STOP:
    case HOST_STOPPED:
      // Host is already stopped or being stopped. No action is required.
      break;
  }
}

void HostProcess::GoOffline(const std::string& host_offline_reason) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_offline_reason.empty());
  DCHECK((state_ == HOST_GOING_OFFLINE_TO_STOP) ||
         (state_ == HOST_GOING_OFFLINE_TO_RESTART));

  // Shut down everything except the HostSignalingManager.
  host_.reset();
  host_event_logger_.reset();
  host_status_logger_.reset();
  power_save_blocker_.reset();
  ftl_host_change_notification_listener_.reset();

  // Before shutting down HostSignalingManager, send the |host_offline_reason|
  // if possible (i.e. if we have the config).
  if (host_offline_reason == ExitCodeToString(kHostDeletedExitCode)) {
    // Host is deleted. There is no need to report the host offline reason back
    // to directory.
    OnHostOfflineReasonAck(true);
    return;
  } else if (!serialized_config_.empty()) {
    if (!signal_strategy_)
      InitializeSignaling();

    HOST_LOG << "SendHostOfflineReason: sending " << host_offline_reason << ".";
    heartbeat_sender_->SetHostOfflineReason(
        host_offline_reason,
        base::TimeDelta::FromSeconds(kHostOfflineReasonTimeoutSeconds),
        base::BindOnce(&HostProcess::OnHostOfflineReasonAck, this));
    return;  // Shutdown will resume after OnHostOfflineReasonAck.
  }

  // Continue the shutdown without sending the host offline reason.
  HOST_LOG << "Can't send offline reason (" << host_offline_reason << ") "
           << "without a valid host config.";
  OnHostOfflineReasonAck(false);
}

void HostProcess::OnHostOfflineReasonAck(bool success) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_);  // Assert that the host is really offline at this point.

  HOST_LOG << "SendHostOfflineReason " << (success ? "succeeded." : "failed.");
  log_to_server_.reset();
  heartbeat_sender_.reset();
  oauth_token_getter_.reset();
  ftl_signaling_connector_.reset();
  ftl_echo_message_listener_.reset();
  signal_strategy_.reset();
  zombie_host_detector_.reset();

  if (state_ == HOST_GOING_OFFLINE_TO_RESTART) {
    SetState(HOST_STARTING);
    StartHostIfReady();
  } else if (state_ == HOST_GOING_OFFLINE_TO_STOP) {
    SetState(HOST_STOPPED);

    shutdown_watchdog_->SetExitCode(*exit_code_out_);
    shutdown_watchdog_->Arm();

    config_watcher_.reset();

    // Complete the rest of shutdown on the main thread.
    context_->ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::ShutdownOnUiThread, this));
  } else {
    NOTREACHED();
  }
}

void HostProcess::OnCrash(const std::string& function_name,
                          const std::string& file_name,
                          const int& line_number) {
  char message[1024];
  base::snprintf(message, sizeof(message),
                 "Requested by %s at %s, line %d.",
                 function_name.c_str(), file_name.c_str(), line_number);
  base::debug::Alias(message);

  // The daemon requested us to crash the process.
  CHECK(false) << message;
}

int HostProcessMain() {
  HOST_LOG << "Starting host process: version " << STRINGIZE(VERSION);

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  std::unique_ptr<ui::X11EventSource> event_source;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kReportOfflineReasonSwitchName)) {
    // Required in order for us to run multiple X11 threads.
    XInitThreads();

    // Create an X11EventSource so the global X11 connection
    // (x11::Connection::Get()) can dispatch X events.
    event_source = std::make_unique<ui::X11EventSource>(x11::Connection::Get());

    // Required for any calls into GTK functions, such as the Disconnect and
    // Continue windows, though these should not be used for the Me2Me case
    // (crbug.com/104377).
#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_init();
#else
    gtk_init(nullptr, nullptr);
#endif
  }

  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me");

  // Create the main task executor and start helper threads.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::Create(new AutoThreadTaskRunner(
          main_task_executor.task_runner(), run_loop.QuitClosure()));
  if (!context)
    return kInitializationFailed;

  // NetworkChangeNotifier must be initialized after SingleThreadTaskExecutor.
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::CreateIfNeeded());

  // Create & start the HostProcess using these threads.
  // TODO(wez): The HostProcess holds a reference to itself until Shutdown().
  // Remove this hack as part of the multi-process refactoring.
  int exit_code = kSuccessExitCode;
  ShutdownWatchdog shutdown_watchdog(
      base::TimeDelta::FromSeconds(kShutdownTimeoutSeconds));
  new HostProcess(std::move(context), &exit_code, &shutdown_watchdog);

  // Run the main (also UI) task executor until the host no longer needs it.
  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return exit_code;
}

}  // namespace remoting
