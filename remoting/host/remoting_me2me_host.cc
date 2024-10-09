// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a standalone host process for Me2Me.

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "components/webrtc/thread_wrapper.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/cloud_session_authz_service_client_factory.h"
#include "remoting/base/constants.h"
#include "remoting/base/corp_session_authz_service_client_factory.h"
#include "remoting/base/cpu_utils.h"
#include "remoting/base/host_settings.h"
#include "remoting/base/is_google_email.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/oauth_token_getter_proxy.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/session_policies.h"
#include "remoting/base/util.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/base/username.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/cloud_heartbeat_service_client.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/config_watcher.h"
#include "remoting/host/corp_heartbeat_service_client.h"
#include "remoting/host/corp_host_status_logger.h"
#include "remoting/host/crash_process.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_session_connector.h"
#include "remoting/host/ftl_echo_message_listener.h"
#include "remoting/host/ftl_host_change_notification_listener.h"
#include "remoting/host/ftl_signaling_connector.h"
#include "remoting/host/heartbeat_sender.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_power_save_blocker.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/ipc_desktop_environment.h"
#include "remoting/host/ipc_host_event_logger.h"
#include "remoting/host/me2me_desktop_environment.h"
#include "remoting/host/me2me_heartbeat_service_client.h"
#include "remoting/host/mojom/agent_process_broker.mojom.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/pairing_registry_delegate.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "remoting/host/security_key/security_key_extension.h"
#include "remoting/host/session_policies_from_dict.h"
#include "remoting/host/shutdown_watchdog.h"
#include "remoting/host/test_echo_extension.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/zombie_host_detector.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/ice_config_fetcher_default.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/ftl_host_device_id_provider.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/remoting_log_to_server.h"
#include "remoting/signaling/signaling_id_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/webrtc/rtc_base/event_tracer.h"

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_descriptor_posix.h"
#include "remoting/host/pam_authorization_factory_posix.h"
#include "remoting/host/posix/signal_handler.h"
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_APPLE)
#include "remoting/host/audio_capturer_mac.h"
#include "remoting/host/desktop_capturer_checker.h"
#include "remoting/host/mac/agent_process_broker_client.h"
#include "remoting/host/mac/permission_utils.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if defined(REMOTING_USE_X11)
#include <gtk/gtk.h>
#endif  // defined(REMOTING_USE_X11)

#if defined(REMOTING_USE_X11)
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/xlib_support.h"
#endif  // defined(REMOTING_USE_X11)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#include "remoting/host/audio_capturer_linux.h"
#include "remoting/host/linux/certificate_watcher.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/host_utmp_logger.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <commctrl.h>

#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#include "remoting/host/win/session_desktop_environment.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/linux/wayland_manager.h"
#include "remoting/host/linux/wayland_utils.h"
#endif  // BUILDFLAG(IS_LINUX)

using remoting::protocol::PairingRegistry;

#if BUILDFLAG(IS_APPLE)

// The following creates a section that tells Mac OS X that it is OK to let us
// inject input in the login screen. Just the name of the section is important,
// not its contents.
__attribute__((used)) __attribute__((section(
    "__CGPreLoginApp,__cgpreloginapp"))) static const char magic_section[] = "";

#endif  // BUILDFLAG(IS_APPLE)

namespace {

#if !defined(REMOTING_MULTI_PROCESS)
// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

// Value used for --host-config option to indicate that the path must be read
// from stdin.
const char kStdinConfigPath[] = "-";
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// The command line switch used to pass name of the pipe to capture audio on
// linux.
const char kAudioPipeSwitchName[] = "audio-pipe-name";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
// The command line switch used to pass name of the unix domain socket used to
// listen for security key requests.
const char kAuthSocknameSwitchName[] = "ssh-auth-sockname";
#endif  // BUILDFLAG(IS_POSIX)

// The command line switch used by the parent to request the host to signal it
// when it is successfully started.
const char kSignalParentSwitchName[] = "signal-parent";

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
const char kHostOfflineReasonZombieStateDetected[] = "ZOMBIE_STATE_DETECTED";
const char kHostOfflineReasonSuspended[] = "SUSPENDED";

// File to write webrtc trace events to. If not specified, webrtc trace events
// will not be enabled.
const char kWebRtcTraceEventFile[] = "webrtc-trace-event-file";

// Helper to check if a string value is in a Policy allowlist.
bool IsInAllowlist(std::string_view value,
                   const std::vector<std::string>& allowlist) {
  return std::find_if(allowlist.begin(), allowlist.end(),
                      [&value](const std::string& allowed_value) {
                        return base::EqualsCaseInsensitiveASCII(value,
                                                                allowed_value);
                      }) != allowlist.end();
}

}  // namespace

namespace remoting {

class HostProcess : public ConfigWatcher::Delegate,
                    public FtlHostChangeNotificationListener::Listener,
                    public HeartbeatSender::Delegate,
                    public IPC::Listener,
                    public base::RefCountedThreadSafe<HostProcess>,
#if BUILDFLAG(IS_MAC)
                    public mojom::AgentProcess,
#endif
                    public mojom::RemotingHostControl,
                    public mojom::WorkerProcessControl {
 public:
  // |shutdown_watchdog| is armed when shutdown is started, and should be kept
  // alive as long as possible until the process exits (since destroying the
  // watchdog disarms it).
  HostProcess(std::unique_ptr<ChromotingHostContext> context,
              int* exit_code_out,
              ShutdownWatchdog* shutdown_watchdog);

  HostProcess(const HostProcess&) = delete;
  HostProcess& operator=(const HostProcess&) = delete;

  // ConfigWatcher::Delegate interface.
  void OnConfigUpdated(const std::string& serialized_config) override;
  void OnConfigWatcherError() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // FtlHostChangeNotificationListener::Listener overrides.
  void OnHostDeleted() override;

#if BUILDFLAG(IS_MAC)
  // mojom::AgentProcess overrides.
  void ResumeProcess() override;
  void SuspendProcess() override;
#endif

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

    // Host has been suspended. Meaning it cannot send heartbeats or connect to
    // signaling. In this state, it may either resume (transition to
    // HOST_STARTING) or shut down (transition to HOST_GOING_OFFLINE_TO_STOP).
    HOST_SUSPENDED,
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

  void ShutdownOnNetworkThread();

#if BUILDFLAG(IS_POSIX)
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

  // Indicates whether |user_email| is allowed to access this machine based on
  // |host_owner_emails_| and the client domain policies that are set.
  // Provided as a Callback to Me2MeHostAuthenticatorFactory and is called for
  // every connection attempt.
  bool CheckAccessPermission(std::string_view user_email);

  // Called on the network thread to set the host's Authenticator factory.
  void CreateAuthenticatorFactory();

  // Tear down resources that run on the UI thread.
  void ShutdownOnUiThread();

  // Determines whether a new config should be applied and handles starting or
  // restarting the host process as necessary.
  void OnConfigParsed(base::Value::Dict config);

  // Applies the host config, returning true if successful.
  bool ApplyConfig(const base::Value::Dict& config);

  // Handles policy updates, by calling On*PolicyUpdate methods.
  void OnPolicyUpdate(base::Value::Dict policies);
  void OnPolicyError();
  void ReportPolicyErrorAndRestartHost();
  void ApplyHostDomainListPolicy();
  void ApplyUsernamePolicy();
  void ApplyAllowRemoteAccessConnections();
  bool OnClientDomainListPolicyUpdate(const base::Value::Dict& policies);
  bool OnHostDomainListPolicyUpdate(const base::Value::Dict& policies);
  bool OnUsernamePolicyUpdate(const base::Value::Dict& policies);
  bool OnPairingPolicyUpdate(const base::Value::Dict& policies);
  bool OnGnubbyAuthPolicyUpdate(const base::Value::Dict& policies);
  bool OnEnableUserInterfacePolicyUpdate(const base::Value::Dict& policies);
  bool OnAllowRemoteAccessConnections(const base::Value::Dict& policies);
  bool OnAllowPinAuthenticationUpdate(const base::Value::Dict& policies);

  void InitializeSignaling();

  void StartHostIfReady();
  void StartHost();

  // HeartbeatSender::Delegate implementation.
  void OnFirstHeartbeatSuccessful() override;
  void OnUpdateHostOwner(const std::string& host_owner) override;
  void OnUpdateRequireSessionAuthorization(bool require_session_auth) override;
  void OnHostNotFound() override;
  void OnAuthFailed() override;

  void OnZombieStateDetected();

  void RestartHost(const std::string& host_offline_reason);
  void ShutdownHost(HostExitCodes exit_code);

  // Helper methods doing the work needed by RestartHost and ShutdownHost.
  void GoOffline(const std::string& host_offline_reason);
  void OnHostOfflineReasonAck(bool success);

  // mojom::WorkerProcessControl implementation.
  void CrashProcess(const std::string& function_name,
                    const std::string& file_name,
                    int line_number) override;

#if BUILDFLAG(IS_WIN)
  // mojom::RemotingHostControl implementation.
  void ApplyHostConfig(base::Value::Dict serialized_config) override;
  void InitializePairingRegistry(
      ::mojo::PlatformHandle privileged_handle,
      ::mojo::PlatformHandle unprivileged_handle) override;
  void BindChromotingHostServices(
      mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
      int peer_pid) override;
#endif

#if BUILDFLAG(IS_MAC)
  void ConnectAgentProcessBroker();
  void OnAgentProcessBrokerDisconnected();
#endif

  std::unique_ptr<ChromotingHostContext> context_;

#if BUILDFLAG(IS_MAC)
  // Created and used on the network thread.
  std::unique_ptr<AgentProcessBrokerClient> agent_process_broker_client_;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Watch for certificate changes and kill the host when changes occur
  std::unique_ptr<CertificateWatcher> cert_watcher_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

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
  std::string service_account_email_;
  base::Value::Dict config_;
  std::set<std::string> host_owner_emails_;

  std::unique_ptr<PolicyWatcher> policy_watcher_;
  PolicyState policy_state_ = POLICY_INITIALIZING;
  std::vector<std::string> client_domain_list_;
  std::vector<std::string> host_domain_list_;
  bool host_username_match_required_ = false;
  bool allow_pairing_ = true;
  bool enable_user_interface_ = true;
  bool allow_remote_access_connections_ = true;
  std::optional<bool> allow_pin_auth_;
  bool is_cloud_host_ = false;
  bool is_corp_host_ = false;
  bool require_session_authorization_ = false;
  LocalSessionPoliciesProvider local_session_policies_provider_;

  DesktopEnvironmentOptions desktop_environment_options_;
  bool security_key_auth_policy_enabled_ = false;
  bool security_key_extension_supported_ = true;

  // Allows us to override field trials which are causing issues for chromoting.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

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
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<HostUTMPLogger> host_utmp_logger_;
#endif
  std::unique_ptr<HostPowerSaveBlocker> power_save_blocker_;

  // Only set if |is_corp_host_| is true.
  std::unique_ptr<CorpHostStatusLogger> corp_host_status_logger_;

  std::unique_ptr<ChromotingHost> host_;

  // Used to keep this HostProcess alive until it is shutdown.
  scoped_refptr<HostProcess> self_;

  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

#if defined(REMOTING_MULTI_PROCESS)

  // Accessed on the UI thread.
  std::unique_ptr<IPC::ChannelProxy> daemon_channel_;

  // Raw interface pointer which refers to the object owned by
  // |desktop_environment_factory_|.
  raw_ptr<DesktopSessionConnector> desktop_session_connector_ = nullptr;
#endif  // defined(REMOTING_MULTI_PROCESS)

  raw_ptr<int> exit_code_out_;
  bool signal_parent_ = false;
  std::string report_offline_reason_;

  scoped_refptr<PairingRegistry> pairing_registry_;

  raw_ptr<ShutdownWatchdog> shutdown_watchdog_;

  mojo::AssociatedReceiver<mojom::RemotingHostControl> remoting_host_control_{
      this};
  mojo::AssociatedReceiver<mojom::WorkerProcessControl> worker_process_control_{
      this};

#if BUILDFLAG(IS_APPLE)
  // When using the command line option to check the Accessibility or Screen
  // Recording permission, these track the permission state and indicate that
  // the host should exit immediately with the result.
  bool checking_permission_state_ = false;
  bool permission_granted_ = false;
#endif  // BUILDFLAG(IS_APPLE)
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

#if BUILDFLAG(IS_APPLE)
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
  // |context_.release()| will null |context_| before the method is invoked, so
  // we need to pull out the task-runner on which to call DeleteSoon first.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context_->ui_task_runner();
  task_runner->DeleteSoon(FROM_HERE, context_.release());
}

bool HostProcess::InitWithCommandLine(const base::CommandLine* cmd_line) {
#if BUILDFLAG(IS_APPLE)
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
    DesktopCapturerChecker().TriggerSingleCapture();
    checking_permission_state_ = true;
    permission_granted_ = mac::CanRecordScreen();
    return false;
  }
  if (cmd_line->HasSwitch(kListAudioDevicesSwitchName)) {
    std::vector<AudioCapturerMac::AudioDeviceInfo> audio_devices =
        AudioCapturerMac::GetAudioDevices();
    printf("Audio devices:\n");
    for (const auto& audio_device : audio_devices) {
      printf("\n");
      printf("  Device name: %s\n", audio_device.device_name.c_str());
      printf("  Device UID: %s\n", audio_device.device_uid.c_str());
    }
    return false;
  }
#endif  // BUILDFLAG(IS_APPLE)

  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      context_->network_task_runner()->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

#if defined(REMOTING_MULTI_PROCESS)
  auto endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(*cmd_line);
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "IPC channel endpoint provided via command line param was "
                  "missing or invalid";
    return false;
  }
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));

  // Connect to the daemon process.
  daemon_channel_ = IPC::ChannelProxy::Create(
      invitation
          .ExtractMessagePipe(cmd_line->GetSwitchValueASCII(kMojoPipeToken))
          .release(),
      IPC::Channel::MODE_CLIENT, this, context_->network_task_runner(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

#else   // !defined(REMOTING_MULTI_PROCESS)
  if (cmd_line->HasSwitch(kHostConfigSwitchName)) {
    host_config_path_ = cmd_line->GetSwitchValuePath(kHostConfigSwitchName);

    // Read config from stdin if necessary.
    if (host_config_path_ == base::FilePath(kStdinConfigPath)) {
      base::ReadStreamToString(stdin, &host_config_);
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

void HostProcess::OnConfigUpdated(const std::string& serialized_config) {
  HOST_LOG << "Parsing new host configuration.";

  std::optional<base::Value::Dict> config(
      HostConfigFromJson(serialized_config));
  if (!config.has_value()) {
    LOG(ERROR) << "Invalid configuration.";
    ShutdownHost(kInvalidHostConfigurationExitCode);
    return;
  }

  OnConfigParsed(std::move(*config));
}

void HostProcess::OnConfigParsed(base::Value::Dict config) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&HostProcess::OnConfigParsed, this, std::move(config)));
    return;
  }

  // Filter out duplicates.
  if (config_ == config) {
    return;
  }

  HOST_LOG << "Applying new host configuration.";

  config_ = std::move(config);
  if (!ApplyConfig(config_)) {
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
    ApplyAllowRemoteAccessConnections();

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
//   STARTING->SUSPENDED (on Mac where host processes need to be brokered)
//   STARTING->GOING_OFFLINE_TO_STOP
//   STARTING->GOING_OFFLINE_TO_RESTART
//   STARTED->GOING_OFFLINE_TO_STOP
//   STARTED->GOING_OFFLINE_TO_RESTART
//   STARTED->SUSPENDED (informed by broker process to give way to process with
//                       higher priority)
//   SUSPENDED->STARTING (resumed by broker process)
//   SUSPENDED->GOING_OFFLINE_TO_STOP
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
             (target_state == HOST_GOING_OFFLINE_TO_RESTART) ||
             (target_state == HOST_SUSPENDED))
          << state_ << " -> " << target_state;
      break;
    case HOST_STARTED:
      DCHECK((target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_GOING_OFFLINE_TO_RESTART) ||
             (target_state == HOST_SUSPENDED))
          << state_ << " -> " << target_state;
      break;
    case HOST_GOING_OFFLINE_TO_RESTART:
      DCHECK((target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_STARTING) ||
             (target_state == HOST_SUSPENDED))
          << state_ << " -> " << target_state;
      break;
    case HOST_GOING_OFFLINE_TO_STOP:
      DCHECK_EQ(target_state, HOST_STOPPED);
      break;
    case HOST_SUSPENDED:
      DCHECK((target_state == HOST_GOING_OFFLINE_TO_STOP) ||
             (target_state == HOST_STARTING))
          << state_ << " -> " << target_state;
      break;
    case HOST_STOPPED:  // HOST_STOPPED is a terminal state.
    default:
      NOTREACHED() << state_ << " -> " << target_state;
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
    config_watcher_ = std::make_unique<ConfigFileWatcher>(
        context_->network_task_runner(), context_->file_task_runner(),
        host_config_path_);
    config_watcher_->Watch(this);
  }
#endif  // !defined(REMOTING_MULTI_PROCESS)

#if BUILDFLAG(IS_POSIX)
  remoting::RegisterSignalHandler(
      SIGTERM, base::BindRepeating(&HostProcess::SigTermHandler,
                                   base::Unretained(this)));
#endif  // BUILDFLAG(IS_POSIX)
}

void HostProcess::ShutdownOnNetworkThread() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  config_watcher_.reset();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  cert_watcher_.reset();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_POSIX)
void HostProcess::SigTermHandler(int signal_number) {
  DCHECK(signal_number == SIGTERM);
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  HOST_LOG << "Caught SIGTERM: Shutting down...";
  ShutdownHost(kSuccessExitCode);
}
#endif  // BUILDFLAG(IS_POSIX)

bool HostProcess::CheckAccessPermission(std::string_view user_email_view) {
  // |user_email_view| may already be in a canonical form but we transform it
  // just in case so that it matches the format we use in |host_owner_emails_|.
  // TODO: joedow - Add an overload for GetCanonicalEmail() which takes a
  // std::string_view.
  auto canonical_email = GetCanonicalEmail(std::string(user_email_view));
  auto email_parts = base::SplitStringOnce(canonical_email, '@');
  if (!email_parts) {
    LOG(ERROR) << "Unexpected email address format: " << user_email_view;
    return false;
  }

  if (!host_owner_emails_.contains(canonical_email)) {
    LOG(ERROR) << canonical_email << " does not have access to this machine.";
    return false;
  }

  // Verify the remote user is not disallowed based on the client domain policy.
  if (client_domain_list_.empty()) {
    return true;
  }

  auto [_, domain] = *email_parts;
  bool allowed_by_policy = IsInAllowlist(domain, client_domain_list_);
  LOG_IF(ERROR, !allowed_by_policy) << canonical_email << " has a domain which "
                                    << "is not in the client domain allowlist.";
  return allowed_by_policy;
}

void HostProcess::CreateAuthenticatorFactory() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != HOST_STARTED) {
    return;
  }

  std::string local_certificate = key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    ShutdownHost(kInitializationFailed);
    return;
  }

  auto auth_config = std::make_unique<protocol::HostAuthenticationConfig>(
      local_certificate, key_pair_);
  if (is_cloud_host_) {
    CHECK(require_session_authorization_);
    auth_config->AddSessionAuthzAuth(
        base::MakeRefCounted<CloudSessionAuthzServiceClientFactory>(
            oauth_token_getter_.get(), context_->url_loader_factory()));
  } else if (require_session_authorization_ ||
             (is_corp_host_ && !allow_pin_auth_.value_or(false))) {
    auth_config->AddSessionAuthzAuth(
        base::MakeRefCounted<CorpSessionAuthzServiceClientFactory>(
            context_->url_loader_factory(), service_account_email_,
            oauth_refresh_token_));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    if (!cert_watcher_) {
      cert_watcher_ = std::make_unique<CertificateWatcher>(
          base::BindRepeating(&HostProcess::ShutdownHost,
                              base::Unretained(this), kSuccessExitCode),
          context_->file_task_runner());
      cert_watcher_->Start();
    }
    cert_watcher_->SetMonitor(host_->status_monitor());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  } else {
    scoped_refptr<PairingRegistry> pairing_registry;
    if (allow_pairing_) {
      // On Windows |pairing_registry_| is initialized in
      // InitializePairingRegistry().
#if !BUILDFLAG(IS_WIN)
      if (!pairing_registry_) {
        std::unique_ptr<PairingRegistry::Delegate> delegate =
            CreatePairingRegistryDelegate();

        if (delegate) {
          pairing_registry_ = new PairingRegistry(context_->file_task_runner(),
                                                  std::move(delegate));
        }
      }
#endif  // BUILDFLAG(IS_WIN)

      pairing_registry = pairing_registry_;
    }

    auth_config->AddPairingAuth(pairing_registry);
    auth_config->AddSharedSecretAuth(pin_hash_);
    host_->set_pairing_registry(pairing_registry);
  }
  HOST_LOG << "Host's supported authentication methods: ";
  for (const auto& method : auth_config->GetSupportedMethods()) {
    HOST_LOG << "  " << AuthenticationMethodToString(method);
  }
  std::unique_ptr<protocol::AuthenticatorFactory> factory =
      std::make_unique<protocol::Me2MeHostAuthenticatorFactory>(
          base::BindRepeating(&HostProcess::CheckAccessPermission, this),
          std::move(auth_config));

#if BUILDFLAG(IS_POSIX)
  // On Linux and Mac, perform a PAM authorization step after authentication.
  factory = std::make_unique<PamAuthorizationFactory>(std::move(factory));
#endif  // BUILDFLAG(IS_POSIX)
  host_->SetAuthenticatorFactory(std::move(factory));
}

// IPC::Listener implementation.
bool HostProcess::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED() << "Received unexpected IPC type: " << message.type();
}

void HostProcess::OnChannelError() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  // Shutdown the host if the daemon process disconnects the IPC channel.
  context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&HostProcess::ShutdownHost, this, kSuccessExitCode));
}

void HostProcess::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

#if defined(REMOTING_MULTI_PROCESS)
  if (interface_name == mojom::RemotingHostControl::Name_) {
    if (remoting_host_control_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::RemotingHostControl::Name_;
      CrashProcess(__FUNCTION__, __FILE__, __LINE__);
    }

    mojo::PendingAssociatedReceiver<mojom::RemotingHostControl>
        pending_receiver(std::move(handle));
    remoting_host_control_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::WorkerProcessControl::Name_) {
    if (worker_process_control_.is_bound()) {
      LOG(ERROR) << "Receiver already bound for associated interface: "
                 << mojom::WorkerProcessControl::Name_;
      CrashProcess(__FUNCTION__, __FILE__, __LINE__);
    }

    mojo::PendingAssociatedReceiver<mojom::WorkerProcessControl>
        pending_receiver(std::move(handle));
    worker_process_control_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::DesktopSessionConnectionEvents::Name_) {
    if (!desktop_session_connector_->BindConnectionEventsReceiver(
            std::move(handle))) {
      LOG(ERROR) << "Failed to bind Receiver for associated interface: "
                 << mojom::DesktopSessionConnectionEvents::Name_;
      CrashProcess(__FUNCTION__, __FILE__, __LINE__);
    }
  } else {
    LOG(ERROR) << "Unknown associated interface requested: " << interface_name
               << ", crashing the network process";
    CrashProcess(__FUNCTION__, __FILE__, __LINE__);
  }
#else   // !defined(REMOTING_MULTI_PROCESS)
  LOG(ERROR) << "Unexpected call requesting an associated interface: "
             << interface_name << ", crashing the network process";
  CrashProcess(__FUNCTION__, __FILE__, __LINE__);
#endif  // !defined(REMOTING_MULTI_PROCESS)
}

void HostProcess::StartOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  if (!InitWithCommandLine(base::CommandLine::ForCurrentProcess())) {
    // Shutdown the host if the command line is invalid.
    ShutdownOnUiThread();
    return;
  }

  // Determine if the CPU this host is running on meets a set of minimum
  // requirements. Note that this isn't a perfect solution as it is possible
  // that the host will have crashed prior to reaching this point in the code,
  // however this is the earliest time we can log an offline reason to the
  // directory if it is unsupported.
  if (!IsCpuSupported()) {
    report_offline_reason_ = ExitCodeToString(kCpuNotSupported);
  }

  if (!report_offline_reason_.empty()) {
    // Don't need to do any UI initialization.
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::StartOnNetworkThread, this));
    return;
  }

  HostSettings::Initialize();

  policy_watcher_ = PolicyWatcher::CreateWithTaskRunner(
      context_->file_task_runner(), context_->management_service());
  policy_watcher_->StartWatching(
      base::BindRepeating(&HostProcess::OnPolicyUpdate, base::Unretained(this)),
      base::BindRepeating(&HostProcess::OnPolicyError, base::Unretained(this)));

#if BUILDFLAG(IS_LINUX)
  if (IsRunningWayland()) {
    WaylandManager::Get()->Init(context_->ui_task_runner());
  }
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // If an audio pipe is specific on the command-line then initialize
  // AudioCapturerLinux to capture from it.
  base::FilePath audio_pipe_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kAudioPipeSwitchName);
  if (!audio_pipe_name.empty()) {
    remoting::AudioCapturerLinux::InitializePipeReader(
        context_->audio_task_runner(), audio_pipe_name);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
  base::FilePath security_key_socket_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kAuthSocknameSwitchName);
  if (!security_key_socket_name.empty()) {
    remoting::SecurityKeyAuthHandler::SetSecurityKeySocketName(
        security_key_socket_name);
  } else {
    security_key_extension_supported_ = false;
  }
#endif  // BUILDFLAG(IS_POSIX)

  // Create a desktop environment factory appropriate to the build type &
  // platform.
#if defined(REMOTING_MULTI_PROCESS)
  // Set up the AssociatedRemote used to send requests to the Daemon process.
  // We need to do a little dance here using a pending associated receiver so
  // that the remote is associated with the proper task_runner since it will be
  // invoked on the network thread.
  mojo::AssociatedRemote<mojom::DesktopSessionManager> remote;
  mojo::GenericPendingAssociatedReceiver pending_receiver =
      remote.BindNewEndpointAndPassReceiver(context_->network_task_runner());
  daemon_channel_->GetRemoteAssociatedInterface(std::move(pending_receiver));

  IpcDesktopEnvironmentFactory* desktop_environment_factory =
      new IpcDesktopEnvironmentFactory(
          context_->audio_task_runner(), context_->network_task_runner(),
          context_->network_task_runner(), std::move(remote));
  desktop_session_connector_ = desktop_environment_factory;
#else   // !defined(REMOTING_MULTI_PROCESS)
  BasicDesktopEnvironmentFactory* desktop_environment_factory =
      new Me2MeDesktopEnvironmentFactory(context_->network_task_runner(),
                                         context_->video_capture_task_runner(),
                                         context_->input_task_runner(),
                                         context_->ui_task_runner());
#endif  // !defined(REMOTING_MULTI_PROCESS)

  desktop_environment_factory_.reset(desktop_environment_factory);

  context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&HostProcess::StartOnNetworkThread, this));
}

void HostProcess::ShutdownOnUiThread() {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());

  context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&HostProcess::ShutdownOnNetworkThread, this));

  // Tear down resources that need to be torn down on the UI thread.
  desktop_environment_factory_.reset();
  policy_watcher_.reset();

#if defined(REMOTING_MULTI_PROCESS)
  daemon_channel_.reset();
  desktop_session_connector_ = nullptr;
#endif  // defined(REMOTING_MULTI_PROCESS)

  // Release the remotes after the daemon channel has been closed.
  remoting_host_control_.reset();
  worker_process_control_.reset();

  // It is now safe for the HostProcess to be deleted.
  self_ = nullptr;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Cause the global AudioPipeReader to be freed, otherwise the audio
  // thread will remain in-use and prevent the process from exiting.
  // TODO(wez): DesktopEnvironmentFactory should own the pipe reader.
  // See crbug.com/161373 and crbug.com/104544.
  AudioCapturerLinux::InitializePipeReader(nullptr, base::FilePath());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(REMOTING_USE_X11)
  context_->input_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce([]() { delete ui::X11EventSource::GetInstance(); }));
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // defined(REMOTING_USE_X11)
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
#if BUILDFLAG(IS_POSIX)
  if (signal_parent_) {
    kill(getppid(), SIGUSR1);
    signal_parent_ = false;
  }
#endif
}

void HostProcess::OnUpdateHostOwner(const std::string& owner_email) {
  DCHECK(!owner_email.empty());

  // Use a canonical email form here for martching against FTL signaling IDs.
  auto new_owner_email = GetCanonicalEmail(owner_email);
  if (host_owner_emails_.contains(new_owner_email)) {
    return;
  }

  LOG(INFO) << "Adding '" << new_owner_email << "' to host owner emails.";
  host_owner_emails_.emplace(std::move(new_owner_email));

  ApplyHostDomainListPolicy();
  ApplyUsernamePolicy();
}

void HostProcess::OnUpdateRequireSessionAuthorization(bool require) {
  if (require == require_session_authorization_) {
    return;
  }

  LOG(INFO) << "Updating require_session_authorization from "
            << require_session_authorization_ << " to " << require;
  require_session_authorization_ = require;
}

void HostProcess::OnHostDeleted() {
  LOG(ERROR) << "Host was deleted from the directory.";
  ShutdownHost(kHostDeletedExitCode);
}

#if BUILDFLAG(IS_MAC)

void HostProcess::ResumeProcess() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  if (state_ == HOST_GOING_OFFLINE_TO_STOP) {
    return;
  }
  HOST_LOG << "Resuming process";
  SetState(HOST_STARTING);
  StartHostIfReady();
}

void HostProcess::SuspendProcess() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  if (state_ == HOST_SUSPENDED || state_ == HOST_GOING_OFFLINE_TO_STOP) {
    return;
  }
  HOST_LOG << "Suspending process";
  SetState(HOST_SUSPENDED);
  GoOffline(kHostOfflineReasonSuspended);
}

#endif

#if BUILDFLAG(IS_WIN)
void HostProcess::ApplyHostConfig(base::Value::Dict config) {
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread());
  OnConfigParsed(std::move(config));
}

void HostProcess::InitializePairingRegistry(
    ::mojo::PlatformHandle privileged_handle,
    ::mojo::PlatformHandle unprivileged_handle) {
  // This IPC is handled on the UI thread and bounced over to the network thread
  // so being called on any other thread is unexpected.
  DCHECK(context_->ui_task_runner()->BelongsToCurrentThread() ||
         context_->network_task_runner()->BelongsToCurrentThread());

  if (context_->ui_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::InitializePairingRegistry, this,
                                  std::move(privileged_handle),
                                  std::move(unprivileged_handle)));
    return;
  }
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  // |pairing_registry_| must only be initialized once.
  DCHECK(!pairing_registry_) << "Received multiple calls to initialize the "
                             << "pairing registry";

  std::unique_ptr<PairingRegistryDelegateWin> delegate(
      new PairingRegistryDelegateWin());
  delegate->SetRootKeys(static_cast<HKEY>(privileged_handle.ReleaseHandle()),
                        static_cast<HKEY>(unprivileged_handle.ReleaseHandle()));

  pairing_registry_ =
      new PairingRegistry(context_->file_task_runner(), std::move(delegate));

  // (Re)Create the authenticator factory now that |pairing_registry_| has been
  // initialized.
  CreateAuthenticatorFactory();
}

void HostProcess::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    int peer_pid) {
  if (context_->ui_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::BindChromotingHostServices,
                                  this, std::move(receiver), peer_pid));
    return;
  }
  // This IPC is handled on the UI thread and bounced over to the network thread
  // so being called on any other thread is unexpected.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  if (!host_) {
    LOG(ERROR) << "Binding rejected. Host has not started.";
    return;
  }
  host_->BindChromotingHostServices(std::move(receiver), peer_pid);
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)

void HostProcess::ConnectAgentProcessBroker() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  agent_process_broker_client_ = std::make_unique<AgentProcessBrokerClient>(
      base::BindOnce(&HostProcess::OnAgentProcessBrokerDisconnected,
                     base::Unretained(this)));
  if (!agent_process_broker_client_->ConnectToServer()) {
    LOG(ERROR) << "Failed to connect to agent process broker.";
    ShutdownHost(kInitializationFailed);
    return;
  }
  agent_process_broker_client_->OnAgentProcessLaunched(this);
}

void HostProcess::OnAgentProcessBrokerDisconnected() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  HOST_LOG << "Host terminated by agent process broker.";
  ShutdownHost(kTerminatedByAgentProcessBroker);
}

#endif  // BUILDFLAG(IS_MAC)

// Applies the host config, returning true if successful.
bool HostProcess::ApplyConfig(const base::Value::Dict& config) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  const std::string* host_id = config.FindString(kHostIdConfigPath);
  if (!host_id) {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kHostIdConfigPath << "`";
    return false;
  }
  host_id_ = *host_id;

  const std::string* key_base64 = config.FindString(kPrivateKeyConfigPath);
  if (!key_base64) {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kPrivateKeyConfigPath << "`";
    return false;
  }

  key_pair_ = RsaKeyPair::FromString(*key_base64);
  if (!key_pair_.get()) {
    LOG(ERROR) << "Host config has an invalid value for path: `"
               << kPrivateKeyConfigPath << "`";
    return false;
  }

  // Retrieve the service account used for signaling and backend requests.
  const std::string* service_account_email =
      config.FindString(kServiceAccountConfigPath);
  if (!service_account_email) {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kServiceAccountConfigPath << "`";
    return false;
  }
  service_account_email_ = *service_account_email;

  // Retrieve robot account credentials for session signaling.
  const std::string* oauth_refresh_token =
      config.FindString(kOAuthRefreshTokenConfigPath);
  if (!oauth_refresh_token) {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kOAuthRefreshTokenConfigPath << "`";
    return false;
  }
  oauth_refresh_token_ = *oauth_refresh_token;

  // Retrieve the host_owner field value.
  const std::string* host_owner = config.FindString(kHostOwnerConfigPath);
  if (!host_owner) {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kHostOwnerConfigPath << "`";
    return false;
  }
  // TODO: joedow - Remove the email check once all Corp hosts have a hint set.
  bool has_google_email = IsGoogleEmail(*host_owner);
  OnUpdateHostOwner(*host_owner);

  auto* host_type_hint = config.FindString(kHostTypeHintPath);
  is_cloud_host_ = (host_type_hint && *host_type_hint == kCloudHostTypeHint);
  // TODO: joedow - Remove the !is_cloud_host override here when all Corp hosts
  // have a hint set. This is used to allow Googlers to test with Cloud hosts.
  is_corp_host_ = (host_type_hint && *host_type_hint == kCorpHostTypeHint) ||
                  (has_google_email && !is_cloud_host_);

  require_session_authorization_ =
      config.FindBool(kRequireSessionAuthorizationPath).value_or(false);

  const std::string* host_secret_hash =
      config.FindString(kHostSecretHashConfigPath);
  if (require_session_authorization_) {
    HOST_LOG << "Host config specifies that Session Authorization is required.";
    HOST_LOG << "PIN authentication is disabled.";
  } else if (host_secret_hash) {
    if (!ParsePinHashFromConfig(*host_secret_hash, host_id_, &pin_hash_)) {
      LOG(ERROR) << "Host config has an invalid value for path: `"
                 << kHostSecretHashConfigPath << "`";
      return false;
    }
  } else if (is_corp_host_) {
    // TODO: joedow - Remove this codepath once all Corp host configs include
    // the kRequireSessionAuthorizationPath attribute.
    HOST_LOG << "No value store for: " << kHostSecretHashConfigPath << ". PIN "
             << "authentication is disabled.";
  } else {
    LOG(ERROR) << "Host config is missing a required path: `"
               << kHostSecretHashConfigPath << "`";
    return false;
  }

  return true;
}

void HostProcess::OnPolicyUpdate(base::Value::Dict policies) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::OnPolicyUpdate, this,
                                  std::move(policies)));
    return;
  }

  // Use the platform policies instead of `policies`, since the latter only has
  // incremental changes.
  std::optional<SessionPolicies> local_session_policies =
      SessionPoliciesFromDict(policy_watcher_->GetPlatformPolicies());
  if (!local_session_policies.has_value()) {
    OnPolicyError();
    return;
  }
  local_session_policies_provider_.set_local_policies(*local_session_policies);

  bool restart_required = false;
  restart_required |= OnClientDomainListPolicyUpdate(policies);
  restart_required |= OnHostDomainListPolicyUpdate(policies);
  restart_required |= OnUsernamePolicyUpdate(policies);
  restart_required |= OnPairingPolicyUpdate(policies);
  restart_required |= OnGnubbyAuthPolicyUpdate(policies);
  restart_required |= OnEnableUserInterfacePolicyUpdate(policies);
  restart_required |= OnAllowRemoteAccessConnections(policies);
  restart_required |= OnAllowPinAuthenticationUpdate(policies);

  policy_state_ = POLICY_LOADED;

  if (state_ == HOST_STARTING) {
    DCHECK(!host_);
    StartHostIfReady();
  } else if (state_ == HOST_STARTED) {
    if (restart_required) {
      RestartHost(kHostOfflineReasonPolicyChangeRequiresRestart);
    }
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
        (state_ == HOST_STARTING && !config_.empty())) {
      ReportPolicyErrorAndRestartHost();
    }
  }
}

void HostProcess::ReportPolicyErrorAndRestartHost() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(!config_.empty());

  DCHECK_EQ(policy_state_, POLICY_ERROR_REPORT_PENDING);
  policy_state_ = POLICY_ERROR_REPORTED;

  HOST_LOG << "Restarting the host due to policy errors.";
  RestartHost(kHostOfflineReasonPolicyReadError);
}

void HostProcess::ApplyHostDomainListPolicy() {
  if (state_ != HOST_STARTED) {
    return;
  }

  HOST_LOG << "Policy sets host domains: "
           << base::JoinString(host_domain_list_, ", ");
  if (host_domain_list_.empty()) {
    return;
  }

  std::set<std::string> allowed_emails;
  for (const std::string& owner_email : host_owner_emails_) {
    auto [_, domain] = *base::SplitStringOnce(owner_email, '@');
    bool allowed_by_policy = IsInAllowlist(domain, host_domain_list_);
    if (allowed_by_policy) {
      allowed_emails.emplace(owner_email);
    } else {
      LOG(WARNING) << owner_email << " is not allowed by host domain policy";
    }
  }
  host_owner_emails_.swap(allowed_emails);
  if (host_owner_emails_.empty()) {
    LOG(ERROR) << "No owner emails are allowed based on host domain policy.";
    ShutdownHost(kInvalidHostDomainExitCode);
  }
}

void HostProcess::ApplyAllowRemoteAccessConnections() {
  if (state_ != HOST_STARTED) {
    return;
  }

  HOST_LOG << "Policy allows remote access connections: "
           << allow_remote_access_connections_;

  if (!allow_remote_access_connections_) {
    ShutdownHost(kRemoteAccessDisallowedExitCode);
  }
}

bool HostProcess::OnHostDomainListPolicyUpdate(
    const base::Value::Dict& policies) {
  // Returns false: never restart the host after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  const base::Value::List* list =
      policies.FindList(policy::key::kRemoteAccessHostDomainList);
  if (!list) {
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
    const base::Value::Dict& policies) {
  // Returns true if the host has to be restarted after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  const base::Value::List* list =
      policies.FindList(policy::key::kRemoteAccessHostClientDomainList);
  if (!list) {
    return false;
  }

  client_domain_list_.clear();
  for (const auto& value : *list) {
    client_domain_list_.push_back(value.GetString());
  }

  return true;
}

void HostProcess::ApplyUsernamePolicy() {
  if (state_ != HOST_STARTED) {
    return;
  }

  if (!host_username_match_required_) {
    HOST_LOG << "Policy does not require host username match.";
    return;
  }

#if BUILDFLAG(IS_WIN)
  // The RemoteAccessHostMatchUsername policy does not exist on Windows, so this
  // should be unreached code.
  NOTREACHED();
#endif

  HOST_LOG << "Policy requires host username match.";

#if BUILDFLAG(IS_APPLE)
  // On Mac, we run as root at the login screen, so the username won't match.
  // However, there's no need to enforce the policy at the login screen, as
  // the client will have to reconnect if a login occurs.
  if (getuid() == 0) {
    return;
  }
#endif

  std::string username = GetUsername();
  LOG(INFO) << "Current local username is '" << username << "'";
  std::set<std::string> allowed_emails;
  for (const std::string& owner_email : host_owner_emails_) {
    auto [owner_username, _] = *base::SplitStringOnce(owner_email, '@');
    if (base::EqualsCaseInsensitiveASCII(username, owner_username)) {
      LOG(INFO) << owner_email << " matches the local username";
      allowed_emails.emplace(owner_email);
    } else {
      LOG(WARNING) << owner_email << " does not match the local username";
    }
  }

  host_owner_emails_.swap(allowed_emails);
  if (host_owner_emails_.empty()) {
    LOG(ERROR) << "No owner emails are allowed based on match username policy.";
    ShutdownHost(kUsernameMismatchExitCode);
  }
}

bool HostProcess::OnUsernamePolicyUpdate(const base::Value::Dict& policies) {
  // Returns false: never restart the host after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  std::optional<bool> host_username_match_required =
      policies.FindBool(policy::key::kRemoteAccessHostMatchUsername);
  if (!host_username_match_required.has_value()) {
    return false;
  }

  host_username_match_required_ = *host_username_match_required;
  ApplyUsernamePolicy();
#endif
  return false;
}

bool HostProcess::OnPairingPolicyUpdate(const base::Value::Dict& policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  std::optional<bool> allow_pairing =
      policies.FindBool(policy::key::kRemoteAccessHostAllowClientPairing);
  if (!allow_pairing.has_value()) {
    return false;
  }

  allow_pairing_ = *allow_pairing;
  if (allow_pairing_) {
    HOST_LOG << "Policy enables client pairing.";
  } else {
    HOST_LOG << "Policy disables client pairing.";
  }
  return true;
}

bool HostProcess::OnGnubbyAuthPolicyUpdate(const base::Value::Dict& policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  std::optional<bool> security_key_auth_policy_enabled =
      policies.FindBool(policy::key::kRemoteAccessHostAllowGnubbyAuth);
  if (!security_key_auth_policy_enabled.has_value()) {
    return false;
  }

  security_key_auth_policy_enabled_ = *security_key_auth_policy_enabled;
  if (security_key_auth_policy_enabled_) {
    HOST_LOG << "Policy enables security key auth.";
  } else {
    HOST_LOG << "Policy disables security key auth.";
  }

  return true;
}

bool HostProcess::OnAllowPinAuthenticationUpdate(
    const base::Value::Dict& policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  const base::Value* allow_pin_auth =
      policies.Find(policy::key::kRemoteAccessHostAllowPinAuthentication);
  if (!allow_pin_auth) {
    return false;
  }

  // Save the value until we have parsed the host config since the default
  // behavior depends on whether the user is a googler.
  if (allow_pin_auth->is_none()) {
    // The policy has been unset.
    allow_pin_auth_.reset();
  } else {
    allow_pin_auth_ = allow_pin_auth->GetIfBool();
    DCHECK(allow_pin_auth_.has_value());
    if (*allow_pin_auth_) {
      HOST_LOG << "Policy allows PIN and pairing authentication methods.";
    } else {
      HOST_LOG << "Policy disallows PIN or pairing authentication methods.";
    }
  }

  // Restart required.
  return true;
}

bool HostProcess::OnEnableUserInterfacePolicyUpdate(
    const base::Value::Dict& policies) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  std::optional<bool> enable_user_interface =
      policies.FindBool(policy::key::kRemoteAccessHostEnableUserInterface);
  if (!enable_user_interface) {
    return false;
  }

  // Save the value until we have parsed the host config since we only want the
  // policy to be applied to machines owned by a Googler.
  enable_user_interface_ = *enable_user_interface;
  if (enable_user_interface_) {
    HOST_LOG << "Policy enables user interface for non-curtained sessions.";
  } else {
    HOST_LOG << "Policy disables user interface for non-curtained sessions.";
  }

  // Restart required.
  return true;
}

bool HostProcess::OnAllowRemoteAccessConnections(
    const base::Value::Dict& policies) {
  // Returns false: never restart the host after this policy update.
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  std::optional<bool> allow_remote_access_connections = policies.FindBool(
      policy::key::kRemoteAccessHostAllowRemoteAccessConnections);
  if (!allow_remote_access_connections.has_value()) {
    return false;
  }

  // Update the value if the policy was set and retrieval was successful.
  allow_remote_access_connections_ = *allow_remote_access_connections;
  ApplyAllowRemoteAccessConnections();
  return false;
}

void HostProcess::InitializeSignaling() {
  DCHECK(!host_id_.empty());  // ApplyConfig() should already have been run.
  DCHECK(!signal_strategy_);
  DCHECK(!oauth_token_getter_);
  DCHECK(!ftl_signaling_connector_);
  DCHECK(!heartbeat_sender_);

  auto oauth_credentials =
      std::make_unique<OAuthTokenGetter::OAuthAuthorizationCredentials>(
          service_account_email_, oauth_refresh_token_,
          /* is_service_account */ true);
  // Unretained is sound because we own the OAuthTokenGetterImpl, and the
  // callback will never be invoked once it is destroyed.
  oauth_token_getter_ = std::make_unique<OAuthTokenGetterImpl>(
      std::move(oauth_credentials), context_->url_loader_factory(), false);

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

  // Create the appropriate API service client (corp, cloud, or me2me) for the
  // HeartbeatSender.
  std::unique_ptr<HeartbeatServiceClient> service_client;
  if (is_cloud_host_) {
    service_client = std::make_unique<CloudHeartbeatServiceClient>(
        host_id_, oauth_token_getter_.get(), context_->url_loader_factory());
    // TODO: joedow - Implement CorpHeartbeatServiceClient.
    // } else if (is_corp_host_) {
    //   service_client = std::make_unique<CorpHeartbeatServiceClient>(
    //       host_id_, oauth_token_getter_.get(),
    //       context_->url_loader_factory());
  } else {
    service_client = std::make_unique<Me2MeHeartbeatServiceClient>(
        host_id_, is_corp_host_, oauth_token_getter_.get(),
        context_->url_loader_factory());
  }

  heartbeat_sender_ = std::make_unique<HeartbeatSender>(
      this, host_id_, ftl_signal_strategy.get(), oauth_token_getter_.get(),
      std::move(service_client), zombie_host_detector_.get(),
      context_->url_loader_factory(), is_corp_host_);
  signal_strategy_ = std::move(ftl_signal_strategy);

  zombie_host_detector_->Start();
}

void HostProcess::StartHostIfReady() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, HOST_STARTING);

  // Start the host if both the config and the policies are loaded.
  if (!config_.empty()) {
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

#if BUILDFLAG(IS_MAC)
  if (!agent_process_broker_client_) {
    HOST_LOG << "Suspending process to wait for broker outcome";
    SetState(HOST_SUSPENDED);
    ConnectAgentProcessBroker();
    return;
  }
#endif

  // This thread is used as a network thread in WebRTC.
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

  // Initialize global field trials. In case this code runs a second time,
  // check for any previous instance - see crbug.com/349062464.
  if (!field_trial_list_) {
    field_trial_list_ = std::make_unique<base::FieldTrialList>();

    // Override LossBasedBweV2 trial.
    // TODO(b/266103942): Remove this override once we figure out why the BWE is
    // crashing for some users and have a fix available.
    base::FieldTrialList::CreateTrialsFromString(
        "WebRTC-Bwe-LossBasedBweV2/Enabled:false/");
  }

  SetState(HOST_STARTED);

  InitializeSignaling();

  // TODO: joedow - Create Cloud/Corp/Me2Me fetchers based on host config hint.
  auto ice_config_fetcher = std::make_unique<protocol::IceConfigFetcherDefault>(
      context_->url_loader_factory(), oauth_token_getter_.get());
  scoped_refptr<protocol::TransportContext> transport_context =
      new protocol::TransportContext(
          std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
          webrtc::ThreadWrapper::current()->SocketServer(),
          std::move(ice_config_fetcher), protocol::TransportRole::SERVER);
  std::unique_ptr<protocol::SessionManager> session_manager(
      new protocol::JingleSessionManager(signal_strategy_.get()));

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  if (!desktop_environment_factory_->SupportsAudioCapture()) {
    protocol_config->DisableAudioChannel();
  }
  protocol_config->set_webrtc_supported(true);
  session_manager->set_protocol_config(std::move(protocol_config));

  if (is_corp_host_) {
    // Enabling this policy means that a local user sitting at a host would not
    // see any UI or indication that a remote user was connected.  We do have a
    // few use cases for this internally where we know for a fact that there
    // will not be a local user.  Since that isn't something we can control
    // externally, we don't want to apply this policy for non-Corp machines.
    desktop_environment_options_.set_enable_user_interface(
        enable_user_interface_);
    corp_host_status_logger_ = std::make_unique<CorpHostStatusLogger>(
        context_->url_loader_factory(), &local_session_policies_provider_,
        service_account_email_, oauth_refresh_token_);
    corp_host_status_logger_->StartObserving(*session_manager);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  desktop_environment_options_.set_enable_remote_webauthn(is_corp_host_);
#endif

  host_ = std::make_unique<ChromotingHost>(
      desktop_environment_factory_.get(), std::move(session_manager),
      transport_context, context_->audio_task_runner(),
      context_->video_encode_task_runner(), desktop_environment_options_,
      &local_session_policies_provider_);

  if (security_key_auth_policy_enabled_ && security_key_extension_supported_) {
    host_->AddExtension(
        std::make_unique<SecurityKeyExtension>(context_->file_task_runner()));
  }

  host_->AddExtension(std::make_unique<TestEchoExtension>());

  host_status_logger_ = std::make_unique<HostStatusLogger>(
      host_->status_monitor(), log_to_server_.get());

#if BUILDFLAG(IS_LINUX)
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(kEnableUtempter)) {
    host_utmp_logger_ =
        std::make_unique<HostUTMPLogger>(host_->status_monitor());
  }
#endif

  power_save_blocker_ = std::make_unique<HostPowerSaveBlocker>(
      host_->status_monitor(), context_->ui_task_runner(),
      context_->file_task_runner());

  ftl_host_change_notification_listener_ =
      std::make_unique<FtlHostChangeNotificationListener>(
          this, signal_strategy_.get());

  ftl_echo_message_listener_ = std::make_unique<FtlEchoMessageListener>(
      base::BindRepeating(&HostProcess::CheckAccessPermission, this),
      signal_strategy_.get());

  // Set up reporting the host status notifications.
#if defined(REMOTING_MULTI_PROCESS)
  mojo::AssociatedRemote<mojom::HostStatusObserver> remote;
  daemon_channel_->GetRemoteAssociatedInterface(&remote);
  host_event_logger_ = std::make_unique<IpcHostEventLogger>(
      host_->status_monitor(), std::move(remote));
#else   // !defined(REMOTING_MULTI_PROCESS)
  host_event_logger_ =
      HostEventLogger::Create(host_->status_monitor(), kApplicationName);
#endif  // !defined(REMOTING_MULTI_PROCESS)

  // The email provided here is only used for logging via OnHostStarted().
  // TODO: joedow - Update host observer interface to handle multiple email
  // addresses.
  host_->Start(*host_owner_emails_.begin());

#if BUILDFLAG(IS_LINUX)
  // For Windows, ChromotingHostServices connections are handled by the daemon
  // process, then the message pipe is forwarded to the network process.
  host_->StartChromotingHostServices();
#endif

  CreateAuthenticatorFactory();

  ApplyHostDomainListPolicy();
  ApplyUsernamePolicy();
  ApplyAllowRemoteAccessConnections();
}

void HostProcess::OnAuthFailed() {
  ShutdownHost(kInvalidOAuthCredentialsExitCode);
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
    case HOST_SUSPENDED:
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
         (state_ == HOST_GOING_OFFLINE_TO_RESTART) ||
         (state_ == HOST_SUSPENDED));

  // Shut down everything except the HostSignalingManager.
  host_.reset();
  host_event_logger_.reset();
  host_status_logger_.reset();
  power_save_blocker_.reset();
  corp_host_status_logger_.reset();
  ftl_host_change_notification_listener_.reset();

  // Before shutting down HostSignalingManager, send the |host_offline_reason|
  // if possible (i.e. if we have the config).
  if (
      // Host is deleted. There is no need to report the host offline reason
      // back to directory.
      host_offline_reason == ExitCodeToString(kHostDeletedExitCode) ||
      // kTerminatedByAgentProcessBroker and kHostOfflineReasonSuspended imply
      // that there is another host process heartbeating. Reporting the offline
      // reason will make the host appear to be offline.
      host_offline_reason ==
          ExitCodeToString(kTerminatedByAgentProcessBroker) ||
      host_offline_reason == kHostOfflineReasonSuspended) {
    OnHostOfflineReasonAck(true);
    return;
  } else if (!config_.empty()) {
    if (!signal_strategy_) {
      InitializeSignaling();
    }

    HOST_LOG << "SendHostOfflineReason: sending " << host_offline_reason << ".";
    heartbeat_sender_->SetHostOfflineReason(
        host_offline_reason, base::Seconds(kHostOfflineReasonTimeoutSeconds),
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

#if BUILDFLAG(IS_MAC)
    agent_process_broker_client_.reset();
#endif

    // Complete the rest of shutdown on the main thread.
    context_->ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&HostProcess::ShutdownOnUiThread, this));
  } else if (state_ != HOST_SUSPENDED) {
    NOTREACHED();
  }
}

void HostProcess::CrashProcess(const std::string& function_name,
                               const std::string& file_name,
                               int line_number) {
  // The daemon requested us to crash the process.
  ::remoting::CrashProcess(function_name, file_name, line_number);
}

int HostProcessMain() {
  HOST_LOG << "Starting host process: version " << STRINGIZE(VERSION);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if defined(REMOTING_USE_X11)
  // Initialize Xlib for multi-threaded use, allowing non-Chromium code to
  // use X11 safely (such as the WebRTC capturer, GTK ...)
  x11::InitXlib();
#endif  // defined(REMOTING_USE_X11)

#if defined(REMOTING_USE_X11)
  if (!cmd_line->HasSwitch(kReportOfflineReasonSwitchName)) {
    // Required for any calls into GTK functions, such as the Disconnect and
    // Continue windows, though these should not be used for the Me2Me case
    // (crbug.com/104377).
#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_init();
#else
    gtk_init(nullptr, nullptr);
#endif
  }
#endif  // defined(REMOTING_USE_X11)

  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if (cmd_line->HasSwitch(kWebRtcTraceEventFile)) {
    rtc::tracing::SetupInternalTracer();
    rtc::tracing::StartInternalCapture(
        cmd_line->GetSwitchValuePath(kWebRtcTraceEventFile)
            .AsUTF8Unsafe()
            .c_str());
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Me2Me");

  // Create the main task executor and start helper threads.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::RunLoop run_loop;
  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::Create(base::MakeRefCounted<AutoThreadTaskRunner>(
          main_task_executor.task_runner(), run_loop.QuitClosure()));
  if (!context) {
    return kInitializationFailed;
  }

  // NetworkChangeNotifier must be initialized after SingleThreadTaskExecutor.
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier(
      net::NetworkChangeNotifier::CreateIfNeeded());

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(REMOTING_USE_X11)
  // Create an X11EventSource on all UI threads, so the global X11 connection
  // (x11::Connection::Get()) can dispatch X events.
  auto event_source =
      std::make_unique<ui::X11EventSource>(x11::Connection::Get());
  context->input_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce([]() { new ui::X11EventSource(x11::Connection::Get()); }));
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // defined(REMOTING_USE_X11)

  // Create & start the HostProcess using these threads.
  // TODO(wez): The HostProcess holds a reference to itself until Shutdown().
  // Remove this hack as part of the multi-process refactoring.
  int exit_code = kSuccessExitCode;
  ShutdownWatchdog shutdown_watchdog(base::Seconds(kShutdownTimeoutSeconds));
  new HostProcess(std::move(context), &exit_code, &shutdown_watchdog);

  // Run the main (also UI) task executor until the host no longer needs it.
  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  if (cmd_line->HasSwitch(kWebRtcTraceEventFile)) {
    rtc::tracing::ShutdownInternalTracer();
  }

  return exit_code;
}

}  // namespace remoting
