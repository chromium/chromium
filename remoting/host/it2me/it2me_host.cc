// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "components/webrtc/thread_wrapper.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/corp_session_authz_service_client_factory.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/corp_host_status_logger.h"
#include "remoting/host/create_desktop_interaction_strategy_factory.h"
#include "remoting/host/ftl_signaling_connector.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_event_reporter.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"
#include "remoting/host/it2me/it2me_helpers.h"
#include "remoting/host/it2me/reconnect_params.h"
#include "remoting/host/it2me_desktop_environment.h"
#include "remoting/host/passthrough_register_support_host_request.h"
#include "remoting/host/session_policies_from_dict.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_config_fetcher_default.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/validating_authenticator.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/signaling_id_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#include "remoting/host/chromeos/features.h"
#endif

namespace remoting {

using protocol::ErrorCode;

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";
const int kMaxLoginAttempts = 5;

using protocol::ValidatingAuthenticator;
typedef ValidatingAuthenticator::Result ValidationResult;
typedef ValidatingAuthenticator::ValidationCallback ValidationCallback;
typedef ValidatingAuthenticator::ResultCallback ValidationResultCallback;

// The amount of time to wait before destroying objects that send messages over
// the network, such as the signal strategy. This delay ensures there is time
// for messages (such as session-terminate) to be sent.
constexpr base::TimeDelta kDestroyMessagingObjectDelay = base::Seconds(2);

#if BUILDFLAG(IS_CHROMEOS)
// Enabled value for ClassManagementEnabled when host belongs to a student and
// their screen can be viewed by a teacher.
constexpr char kClassManagementStudent[] = "student";
// Enabled value for ClassManagementEnabled when host belongs to a teacher and
// they would like to access their host via another device.
constexpr char kClassManagementTeacher[] = "teacher";
#endif  // BUILDFLAG(IS_CHROMEOS)

// STL containers do not have a defined destruction orders for their elements.
// Post(Delayed)Task relies on these containers so the destruction order is also
// undefined, causing problems when there are dependencies between objects to be
// delayed destructed. This class takes ownership of the objects passed to the
// constructor, and destroys them in their order in the parameter list when
// the destructor is called.
template <typename... T>
class OrderedDestruction {
 public:
  explicit OrderedDestruction(std::unique_ptr<T>... objects)
      : objects_{std::move(objects)...} {}

  OrderedDestruction(OrderedDestruction&&) = default;

  ~OrderedDestruction() {
    // Reset the to-be-destroyed pointers in passed order.
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (get<I>(objects_).reset(), ...);
    }(std::index_sequence_for<T...>());
  }

 private:
  std::tuple<std::unique_ptr<T>...> objects_;
};

}  // namespace

It2MeHost::DeferredConnectContext::DeferredConnectContext() = default;

It2MeHost::DeferredConnectContext::~DeferredConnectContext() = default;

It2MeHost::It2MeHost() {
#if BUILDFLAG(IS_CHROMEOS)
  host_event_reporter_factory_ =
      base::BindRepeating(&HostEventReporter::Create);
#endif
}

It2MeHost::~It2MeHost() {
  // Check that resources that need to be torn down on the UI thread are gone.
  DCHECK(!desktop_environment_factory_.get());
}

void It2MeHost::set_chrome_os_enterprise_params(
    ChromeOsEnterpriseParams params) {
#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  CHECK_NE(params.request_origin, ChromeOsEnterpriseRequestOrigin::kUnknown);
  chrome_os_enterprise_params_ = std::move(params);
#else
  NOTREACHED() << "It2MeHost::set_chrome_os_enterprise_params is only "
                  "supported on ChromeOS";
#endif
}

void It2MeHost::set_authorized_helper(const std::string& authorized_helper) {
  authorized_helper_ = authorized_helper;
}

void It2MeHost::set_reconnect_params(ReconnectParams reconnect_params) {
#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  reconnect_params_.emplace(std::move(reconnect_params));
#else
  NOTREACHED() << "It2MeHost::set_reconnect_params is only supported on CrOS";
#endif
}

bool It2MeHost::SessionSupportsReconnections() const {
#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  return is_enterprise_session() &&
         chrome_os_enterprise_params_->allow_reconnections;
#else
  return false;
#endif
}

std::optional<ReconnectParams> It2MeHost::CreateReconnectParams() const {
  std::optional<ReconnectParams> reconnect_params;
#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  if (!SessionSupportsReconnections()) {
    return reconnect_params;
  }
  // This function is meant to be queried just after the remote client connects,
  // otherwise the required fields will not be set.
  CHECK_EQ(state_, It2MeHostState::kConnected);

  reconnect_params.emplace();
  reconnect_params->support_id = support_id_;
  reconnect_params->host_secret = host_secret_;
  reconnect_params->private_key = host_key_pair_->ToString();
  reconnect_params->ftl_device_id = ftl_device_id_;
  reconnect_params->client_ftl_address = connecting_jid_;
#endif

  return reconnect_params;
}

void It2MeHost::SendReconnectSessionMessage() const {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != It2MeHostState::kReceivedAccessCode) {
    // If the host state has changed since the task was posted, just bail early.
    return;
  }

  ftl::ChromotingMessage crd_message;
  crd_message.mutable_reconnect()->set_support_id(
      reconnect_params_->support_id);
  SignalingAddress signaling_address(reconnect_params_->client_ftl_address);

  signal_strategy_->SendMessage(signaling_address, crd_message);
}

void It2MeHost::Connect(
    std::unique_ptr<ChromotingHostContext> host_context,
    base::Value::Dict policies,
    std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
    base::WeakPtr<It2MeHost::Observer> observer,
    CreateDeferredConnectContext create_context,
    const std::string& username,
    const protocol::IceConfig& ice_config) {
  DCHECK(host_context->ui_task_runner()->BelongsToCurrentThread());

  host_context_ = std::move(host_context);
  observer_ = std::move(observer);
  confirmation_dialog_factory_ = std::move(dialog_factory);
  local_session_policies_provider_ =
      std::make_unique<LocalSessionPoliciesProvider>();

  if (is_enterprise_session()) {
    // Don't notify on local policy changes for Admin sessions as the policies
    // can change as they log into different sessions and this should not cause
    // them to be disconnected: See crbug.com/380421478.
    local_session_policies_provider_->send_policy_change_notifications(false);
  }

  OnPolicyUpdate(std::move(policies));

  desktop_environment_factory_ =
      std::make_unique<It2MeDesktopEnvironmentFactory>(
          host_context_->network_task_runner(), host_context_->ui_task_runner(),
          CreateDesktopInteractionStrategyFactory(
              host_context_->network_task_runner(),
              host_context_->ui_task_runner(),
              host_context_->video_capture_task_runner(),
              host_context_->input_task_runner()));

  // Switch to the network thread to start the actual connection.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeHost::ConnectOnNetworkThread, this, username,
                     ice_config, std::move(create_context)));
}

void It2MeHost::Disconnect() {
  DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::DisconnectOnNetworkThread, this,
                                protocol::ErrorCode::OK));
}

void It2MeHost::ConnectOnNetworkThread(
    const std::string& username,
    const protocol::IceConfig& ice_config,
    CreateDeferredConnectContext create_context) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(It2MeHostState::kDisconnected, state_);
  // This thread is used as a network thread in WebRTC.
  webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

  if (!remote_support_connections_allowed_) {
    SetState(It2MeHostState::kError, ErrorCode::DISALLOWED_BY_POLICY);
    return;
  }

  SetState(It2MeHostState::kStarting, ErrorCode::OK);

  auto connection_context = std::move(create_context).Run(host_context_.get());
  log_to_server_ = std::move(connection_context->log_to_server);
  signal_strategy_ = std::move(connection_context->signal_strategy);
  api_token_getter_ = std::move(connection_context->api_token_getter);
  DCHECK(log_to_server_);
  DCHECK(signal_strategy_);

  if (connection_context->use_ftl_signaling) {
    // If the host owns the signaling channel then we want to make sure that it
    // will reconnect that channel if a transient network error occurs.
    // FtlSignalingConnector takes a callback which will indicate whether an
    // auth error has occurred (e.g. token expired). For our purposes, there
    // isn't anything we need to do in this case since a new token will be
    // generated for the next connection.
    ftl_signaling_connector_ = std::make_unique<FtlSignalingConnector>(
        signal_strategy_.get(), base::DoNothing());
    ftl_signaling_connector_->Start();
    ftl_device_id_ = connection_context->ftl_device_id;
  }

  // Check the host domain policy.
  // Skip this check for enterprise sessions, as they use the device specific
  // robot account as host, and we should not expect the customers to add this
  // internal account to their host domain list.
  if (!required_host_domain_list_.empty() && !is_enterprise_session()) {
    bool matched = false;
    for (const auto& domain : required_host_domain_list_) {
      if (base::EndsWith(username, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      SetState(It2MeHostState::kInvalidDomainError, ErrorCode::OK);
      return;
    }
  }

  if (connection_context->is_corp_user ||
      connection_context->use_corp_session_authz) {
    use_corp_session_authz_ = true;
  }

  if (!reconnect_params_.has_value()) {
    // Generate a key pair for the Host to use.
    host_key_pair_ = RsaKeyPair::Generate();

    // Shared secret auth is not supported when SessionAuthz is in use.
    if (!use_corp_session_authz_) {
      // Generate a new host secret for this instance.
      host_secret_ = GenerateSupportHostSecret();
    }

    // Register this host instance in the backend service.
    register_request_ = std::move(connection_context->register_request);
  } else {
    // Reconnections are only allowed for Chrome OS enterprise sessions.
    CHECK(SessionSupportsReconnections());

    // Reconnections are not allowed when SessionAuthz is in use.
    CHECK(!use_corp_session_authz_);

    // Regenerate the key pair from the private key.
    host_key_pair_ = RsaKeyPair::FromString(reconnect_params_->private_key);

    // Restore the host_secret from the previous connection.
    host_secret_ = reconnect_params_->host_secret;

    // Skip the registration service call as the entry will be retrievable by
    // the `authorized_helper` for ~24 hours when 'allow_reconnections' is set.
    register_request_ = std::make_unique<PassthroughRegisterSupportHostRequest>(
        reconnect_params_->support_id);
  }
  register_request_->StartRequest(
      signal_strategy_.get(), host_context_->CreateClientCertStore(),
      host_key_pair_, authorized_helper_,
      std::move(chrome_os_enterprise_params_),
      base::BindOnce(&It2MeHost::OnReceivedSupportID,
                     weak_factory_.GetWeakPtr()));

  auto ice_config_fetcher = std::make_unique<protocol::IceConfigFetcherDefault>(
      host_context_->url_loader_factory(), api_token_getter_.get());
  auto transport_context = base::MakeRefCounted<protocol::TransportContext>(
      std::make_unique<protocol::ChromiumPortAllocatorFactory>(),
      webrtc::ThreadWrapper::current()->SocketServer(),
      std::move(ice_config_fetcher), protocol::TransportRole::SERVER);
  if (!ice_config.is_null()) {
    transport_context->set_turn_ice_config(ice_config);
  }

  std::unique_ptr<protocol::SessionManager> session_manager(
      new protocol::JingleSessionManager(signal_strategy_.get()));

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  // Disable audio by default.
  // TODO(sergeyu): Add UI to enable it.
  protocol_config->DisableAudioChannel();
  protocol_config->set_webrtc_supported(true);
  session_manager->set_protocol_config(std::move(protocol_config));

  if (use_corp_session_authz_) {
    corp_host_status_logger_ = CorpHostStatusLogger::CreateForRemoteSupport(
        host_context_->url_loader_factory(),
        host_context_->CreateClientCertStore(),
        local_session_policies_provider_.get(),
        api_token_getter_->GetWeakPtr());
    corp_host_status_logger_->StartObserving(*session_manager);
  }

  // Set up the desktop environment options.
  DesktopEnvironmentOptions options(DesktopEnvironmentOptions::CreateDefault());

#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  if (is_enterprise_session()) {
    options.set_enable_user_interface(
        !chrome_os_enterprise_params_->suppress_user_dialogs);
    options.set_enable_notifications(
        !chrome_os_enterprise_params_->suppress_notifications);
    options.set_terminate_upon_input(
        chrome_os_enterprise_params_->terminate_upon_input);
    options.set_maximum_session_duration(
        chrome_os_enterprise_params_->maximum_session_duration);
  }
#endif

  // Create the host.
  host_ = std::make_unique<ChromotingHost>(
      desktop_environment_factory_.get(), std::move(session_manager),
      transport_context, host_context_->audio_task_runner(),
      host_context_->video_encode_task_runner(), options,
      base::BindRepeating(&It2MeHost::OnEffectiveSessionPoliciesReceived,
                          base::Unretained(this)),
      local_session_policies_provider_.get());
  host_->status_monitor()->AddStatusObserver(this);
  host_status_logger_ = std::make_unique<HostStatusLogger>(
      host_->status_monitor(), log_to_server_.get());

  // Create event logger.
  host_event_logger_ =
      HostEventLogger::Create(host_->status_monitor(), kApplicationName);
#if BUILDFLAG(IS_CHROMEOS)
  host_event_reporter_ =
      host_event_reporter_factory_.Run(host_->status_monitor());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Connect signaling and start the host.
  signal_strategy_->Connect();
  host_->Start(username);

  SetState(It2MeHostState::kRequestedAccessCode, ErrorCode::OK);
  return;
}

void It2MeHost::OnClientAccessDenied(const std::string& signaling_id) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    DisconnectOnNetworkThread();
  } else if (connecting_jid_ == NormalizeSignalingId(signaling_id)) {
    DCHECK_EQ(state_, It2MeHostState::kConnecting);
    connecting_jid_.clear();
    confirmation_dialog_proxy_.reset();
    SetState(It2MeHostState::kReceivedAccessCode, ErrorCode::OK);
  }
}

void It2MeHost::OnClientConnected(const std::string& signaling_id) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // ChromotingHost doesn't allow concurrent connections and the host is
  // destroyed in OnClientDisconnected() after the first connection.
  CHECK_NE(state_, It2MeHostState::kConnected);

  std::string client_username;
  if (!SplitSignalingIdResource(signaling_id, &client_username,
                                /*resource=*/nullptr)) {
    LOG(WARNING) << "Incorrectly formatted signaling ID received: "
                 << signaling_id;
    client_username = signaling_id;
  }

  HOST_LOG << "Client " << client_username << " connected.";

  // Pass the client user name to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnClientAuthenticated,
                                observer_, client_username));

  SetState(It2MeHostState::kConnected, ErrorCode::OK);
}

void It2MeHost::OnClientDisconnected(const std::string& signaling_id) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // Handling HostStatusObserver events should not cause the destruction of the
  // ChromotingHost instance, however that is exactly what happens inside of
  // DisconnectOnNetworkThread() so we post a task to disconnect asynchronously
  // which will allow any other HostStatusObservers to handle the event as well
  // before everything is torn down.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::DisconnectOnNetworkThread, this,
                                protocol::ErrorCode::OK));
}

ValidationCallback It2MeHost::GetValidationCallbackForTesting() {
  return base::BindRepeating(&It2MeHost::ValidateConnectionDetails,
                             base::Unretained(this));
}

#if BUILDFLAG(IS_CHROMEOS)
void It2MeHost::SetHostEventReporterFactoryForTesting(
    HostEventReporterFactory factory) {
  host_event_reporter_factory_ = factory;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void It2MeHost::OnPolicyUpdate(base::Value::Dict policies) {
  // The policy watcher runs on the |ui_task_runner|.
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&It2MeHost::OnPolicyUpdate, this, std::move(policies)));
    return;
  }

  if (is_enterprise_session() && IsRunning()) {
    // Don't notify on policy changes for Admin sessions as the policies can
    // change as they log into different sessions and this should not cause
    // them to be disconnected: See crbug.com/380421478.
    HOST_LOG << "Dropping policy update during enterprise connection.";
    return;
  }

  // Retrieve the policy value on whether to allow connections but don't apply
  // it until after we've finished reading the rest of the policies and started
  // the connection process.
  remote_support_connections_allowed_ =
      RemoteSupportConnectionsAllowed(policies);

  const base::Value::List* host_domain_list =
      policies.FindList(policy::key::kRemoteAccessHostDomainList);
  if (host_domain_list) {
    std::vector<std::string> host_domain_list_vector;
    for (const auto& value : *host_domain_list) {
      host_domain_list_vector.push_back(value.GetString());
    }
    UpdateHostDomainListPolicy(std::move(host_domain_list_vector));
  }

  const base::Value::List* client_domain_list =
      policies.FindList(policy::key::kRemoteAccessHostClientDomainList);
  if (client_domain_list) {
    std::vector<std::string> client_domain_list_vector;
    for (const auto& value : *client_domain_list) {
      client_domain_list_vector.push_back(value.GetString());
    }
    UpdateClientDomainListPolicy(std::move(client_domain_list_vector));
  }

  UpdateLocalSessionPolicies(policies);

  // If |session_policies_finalized_| is true, then local policy changes will
  // either disconnect the session, or have no effects if the effective policies
  // come from the authenticator. In either case, we do not need to report the
  // local NAT policies.
  if (local_session_policies_provider_ && !session_policies_finalized_) {
    ReportNatPolicies(local_session_policies_provider_->get_local_policies());
  }
}

std::optional<ErrorCode> It2MeHost::OnEffectiveSessionPoliciesReceived(
    const SessionPolicies& session_policies) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  session_policies_finalized_ = true;
  ReportNatPolicies(session_policies);
  return std::nullopt;
}

void It2MeHost::ReportNatPolicies(const SessionPolicies& session_policies) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  bool nat_policy_value =
      session_policies.allow_stun_connections.value_or(true);
  VLOG(2) << "UpdateNatPolicies: nat_policy_value: " << nat_policy_value;
  bool nat_traversal_value_changed =
      !last_reported_nat_traversal_enabled_.has_value() ||
      *last_reported_nat_traversal_enabled_ != nat_policy_value;
  last_reported_nat_traversal_enabled_ = nat_policy_value;

  bool relay_policy_value =
      session_policies.allow_relayed_connections.value_or(true);
  VLOG(2) << "UpdateNatPolicies: relay_policy_value: " << relay_policy_value;
  bool relay_value_changed =
      !last_reported_relay_connections_allowed_.has_value() ||
      *last_reported_relay_connections_allowed_ != relay_policy_value;
  last_reported_relay_connections_allowed_ = relay_policy_value;

  // Notify listeners of the policy setting change.
  if (nat_traversal_value_changed || relay_value_changed) {
    host_context_->ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&It2MeHost::Observer::OnNatPoliciesChanged, observer_,
                       nat_policy_value, relay_policy_value));
  }
}

void It2MeHost::UpdateHostDomainListPolicy(
    std::vector<std::string> host_domain_list) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainListPolicy: "
          << base::JoinString(host_domain_list, ", ");

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain_list.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_host_domain_list_ = std::move(host_domain_list);
}

void It2MeHost::UpdateClientDomainListPolicy(
    std::vector<std::string> client_domain_list) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateClientDomainPolicy: "
          << base::JoinString(client_domain_list, ", ");

  // When setting a client domain policy, disconnect any existing session.
  if (!client_domain_list.empty() && IsRunning()) {
    DisconnectOnNetworkThread();
  }

  required_client_domain_list_ = std::move(client_domain_list);
}

void It2MeHost::UpdateLocalSessionPolicies(
    const base::Value::Dict& platform_policies) {
  // |local_session_policies_provider_| is null if there is no active
  // connection. Connect() calls OnPolicyUpdate() with the platform policies, so
  // we don't need to track session policies when there is no active connection.
  if (!local_session_policies_provider_) {
    return;
  }

  std::optional<SessionPolicies> local_session_policies =
      SessionPoliciesFromDict(platform_policies);
  if (!local_session_policies.has_value()) {
    LOG(FATAL) << "Failed to parse local session policies.";
  }

  // These are currently disallowed for IT2ME connections by default.
  // TODO: yuweih - Figure out what should be done when we add SessionAuthz
  // policies support for IT2ME. Given the current logic, these features can be
  // enabled by SessionAuthz policies, which is not possible by local Chrome
  // policies.
  local_session_policies->allow_file_transfer = false;
  local_session_policies->allow_uri_forwarding = false;

  local_session_policies->allow_remote_input = true;

#if BUILDFLAG(IS_CHROMEOS) || !defined(NDEBUG)
  if (is_enterprise_session()) {
    local_session_policies->curtain_required =
        chrome_os_enterprise_params_->curtain_local_user_session;

    local_session_policies->allow_remote_input =
        chrome_os_enterprise_params_->allow_remote_input;

    if (!chrome_os_enterprise_params_->allow_clipboard_sync) {
      local_session_policies->clipboard_size_bytes = 0;
    }

    if (!chrome_os_enterprise_params_->maximum_session_duration.is_zero()) {
      local_session_policies->maximum_session_duration =
          chrome_os_enterprise_params_->maximum_session_duration;
    }

#if BUILDFLAG(IS_CHROMEOS)
    bool enterprise_file_transfer_allowed =
        platform_policies
            .FindBool(policy::key::kRemoteAccessHostAllowEnterpriseFileTransfer)
            .value_or(false);
#else
    bool enterprise_file_transfer_allowed = false;
#endif
    local_session_policies->allow_file_transfer =
        chrome_os_enterprise_params_->allow_file_transfer &&
        enterprise_file_transfer_allowed;
  }
#endif

  local_session_policies_provider_->set_local_policies(*local_session_policies);
}

void It2MeHost::SetState(It2MeHostState state, ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (state_) {
    case It2MeHostState::kDisconnected:
      DCHECK(state == It2MeHostState::kStarting ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kStarting:
      DCHECK(state == It2MeHostState::kRequestedAccessCode ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError ||
             state == It2MeHostState::kInvalidDomainError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kRequestedAccessCode:
      DCHECK(state == It2MeHostState::kReceivedAccessCode ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kReceivedAccessCode:
      DCHECK(state == It2MeHostState::kConnecting ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kConnecting:
      DCHECK(state == It2MeHostState::kConnected ||
             state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kConnected:
      DCHECK(state == It2MeHostState::kDisconnected ||
             state == It2MeHostState::kError)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kError:
      DCHECK(state == It2MeHostState::kDisconnected)
          << It2MeHostStateToString(state);
      break;
    case It2MeHostState::kInvalidDomainError:
      DCHECK(state == It2MeHostState::kDisconnected)
          << It2MeHostStateToString(state);
      break;
  }

  state_ = state;

  // Post a state-change notification to the web-app.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnStateChanged, observer_,
                                state, error_code));
}

bool It2MeHost::IsRunning() const {
  return state_ == It2MeHostState::kRequestedAccessCode ||
         state_ == It2MeHostState::kReceivedAccessCode ||
         state_ == It2MeHostState::kConnected ||
         state_ == It2MeHostState::kConnecting;
}

void It2MeHost::OnReceivedSupportID(const std::string& support_id,
                                    const base::TimeDelta& lifetime,
                                    const ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (error_code != ErrorCode::OK) {
    SetState(It2MeHostState::kError, error_code);
    DisconnectOnNetworkThread();
    return;
  }

  support_id_ = support_id;
  std::string access_code = support_id_ + host_secret_;

  std::string local_certificate = host_key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    LOG(ERROR) << "Failed to generate host certificate.";
    SetState(It2MeHostState::kError, ErrorCode::HOST_CERTIFICATE_ERROR);
    DisconnectOnNetworkThread();
    return;
  }

  auto factory = std::make_unique<protocol::It2MeHostAuthenticatorFactory>(
      local_certificate, host_key_pair_,
      base::BindRepeating(&It2MeHost::ValidateConnectionDetails,
                          base::Unretained(this)));
  if (use_corp_session_authz_) {
    factory->AddSessionAuthzAuth(
        base::MakeRefCounted<CorpSessionAuthzServiceClientFactory>(
            host_context_->url_loader_factory(),
            host_context_->create_client_cert_store_callback(),
            api_token_getter_->GetWeakPtr(), support_id_));
  } else {
    CHECK(!host_secret_.empty());
    std::string access_code_hash =
        protocol::GetSharedSecretHash(support_id_, access_code);
    factory->AddSharedSecretAuth(access_code_hash);
  }
  host_->SetAuthenticatorFactory(std::move(factory));

  // Pass the Access Code to the script object before changing state.
  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnStoreAccessCode,
                                observer_, access_code, lifetime));

  SetState(It2MeHostState::kReceivedAccessCode, ErrorCode::OK);

  // If this host instance was started using |reconnect_params_| then send a
  // signaling message to the client address from the previous connection to let
  // it know that it needs to reconnect. The client address is regenerated for
  // every connection (and reconnection) which is important because this message
  // will only be delivered if the client hasn't already restarted the
  // connection process.
  if (reconnect_params_.has_value()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&It2MeHost::SendReconnectSessionMessage,
                                  weak_factory_.GetWeakPtr()));
  }
}

void It2MeHost::DisconnectOnNetworkThread(protocol::ErrorCode error_code) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // Disconnect() may be called even after the host has already been stopped.
  // Ignore repeated calls.
  if (state_ == It2MeHostState::kDisconnected) {
    return;
  }

  confirmation_dialog_proxy_.reset();

  if (host_) {
    host_->status_monitor()->RemoveStatusObserver(this);
    host_ = nullptr;
  }

  register_request_ = nullptr;
  host_status_logger_ = nullptr;
  log_to_server_ = nullptr;
  ftl_signaling_connector_ = nullptr;
  reconnect_params_.reset();

  // Delay destruction of the objects that send messages over the network by a
  // few seconds to give them a chance to send any outgoing messages (e.g.
  // session-terminate) so the other end of the connection can display and log
  // an accurate disconnect reason.
  OrderedDestruction ordered_destruction{
      std::move(signal_strategy_), std::move(corp_host_status_logger_),
      // Needed by |corp_host_status_logger_|.
      std::move(local_session_policies_provider_)};
  host_context_->network_task_runner()->PostDelayedTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(ordered_destruction)),
      kDestroyMessagingObjectDelay);

#if BUILDFLAG(IS_CHROMEOS)
  host_event_reporter_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)
  host_event_logger_ = nullptr;

  // Post tasks to delete UI objects on the UI thread.
  host_context_->ui_task_runner()->DeleteSoon(
      FROM_HERE, desktop_environment_factory_.release());

  SetState(It2MeHostState::kDisconnected, error_code);
}

void It2MeHost::ValidateConnectionDetails(
    const std::string& original_remote_jid,
    ValidationResultCallback result_callback) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  // First ensure the JID we received is valid.
  std::string client_username;
  if (!SplitSignalingIdResource(original_remote_jid, &client_username,
                                /*resource=*/nullptr)) {
    LOG(ERROR) << "Rejecting incoming connection from " << original_remote_jid
               << ": Invalid JID.";
    std::move(result_callback)
        .Run(protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }
  std::string remote_jid = NormalizeSignalingId(original_remote_jid);

  if (client_username.empty()) {
    LOG(ERROR) << "Invalid user name passed in: " << remote_jid;
    std::move(result_callback)
        .Run(protocol::ValidatingAuthenticator::Result::ERROR_INVALID_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }

  // Check the client domain policy.
  if (!required_client_domain_list_.empty()) {
    bool matched = false;
    for (const auto& domain : required_client_domain_list_) {
      if (base::EndsWith(client_username, std::string("@") + domain,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
                 << ": Domain not allowed.";
      std::move(result_callback).Run(ValidationResult::ERROR_INVALID_ACCOUNT);
      DisconnectOnNetworkThread();
      return;
    }
  }

  if (!authorized_helper_.empty() &&
      !gaia::AreEmailsSame(authorized_helper_, client_username)) {
    LOG(ERROR) << "Rejecting connection request from (" << client_username
               << ") as it does not match the authorized_helper ("
               << authorized_helper_ << ")";
    std::move(result_callback)
        .Run(ValidationResult::ERROR_UNAUTHORIZED_ACCOUNT);
    DisconnectOnNetworkThread();
    return;
  }

  // If we receive valid connection details multiple times, then we don't know
  // which remote user (if either) is valid so disconnect everyone.
  if (state_ != It2MeHostState::kReceivedAccessCode) {
    DCHECK_EQ(It2MeHostState::kConnecting, state_);
    LOG(ERROR) << "Received too many connection requests.";
    std::move(result_callback)
        .Run(ValidationResult::ERROR_TOO_MANY_CONNECTIONS);
    DisconnectOnNetworkThread();
    return;
  }

  HOST_LOG << "Client " << client_username << " connecting.";
  connecting_jid_ = remote_jid;
  SetState(It2MeHostState::kConnecting, ErrorCode::OK);

  // Show a confirmation dialog to the user to allow them to confirm/reject it.
  // If dialogs are suppressed, just call the callback directly.
  if (is_enterprise_session() &&
      chrome_os_enterprise_params_->suppress_user_dialogs) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), ValidationResult::SUCCESS));
  } else {
    confirmation_dialog_proxy_ = std::make_unique<It2MeConfirmationDialogProxy>(
        host_context_->ui_task_runner(),
        confirmation_dialog_factory_->Create());
    confirmation_dialog_proxy_->Show(
        client_username,
        base::BindOnce(&It2MeHost::OnConfirmationResult, base::Unretained(this),
                       std::move(result_callback)));
  }
}

void It2MeHost::OnConfirmationResult(ValidationResultCallback result_callback,
                                     It2MeConfirmationDialog::Result result) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  connecting_jid_.clear();
  switch (result) {
    case It2MeConfirmationDialog::Result::OK:
      std::move(result_callback).Run(ValidationResult::SUCCESS);
      break;

    case It2MeConfirmationDialog::Result::CANCEL:
      std::move(result_callback).Run(ValidationResult::ERROR_REJECTED_BY_USER);
      DisconnectOnNetworkThread(ErrorCode::SESSION_REJECTED);
      break;
  }
}

bool It2MeHost::RemoteSupportConnectionsAllowed(
    const base::Value::Dict& policies) {
#if BUILDFLAG(IS_CHROMEOS)
  // The policy to disallow remote support connections
  // (RemoteAccessHostAllowRemoteSupportConnections) does not apply to support
  // sessions initiated by the enterprise admin via a RemoteCommand or by Class
  // tools. These two cases are handled specifically by the policy to disallow
  // enterprise remote support connections
  // (RemoteAccessHostAllowEnterpriseRemoteSupportConnections) and the policy
  // to disallow teachers from viewing student screens
  // (ClassManagementEnabled).
  if (is_enterprise_session()) {
    switch (chrome_os_enterprise_params_->request_origin) {
      case remoting::ChromeOsEnterpriseRequestOrigin::kClassManagement:
        if (const std::string* class_management_enabled_value =
                policies.FindString(policy::key::kClassManagementEnabled)) {
          return *class_management_enabled_value == kClassManagementStudent ||
                 *class_management_enabled_value == kClassManagementTeacher;
        }
        return false;
      case remoting::ChromeOsEnterpriseRequestOrigin::kEnterpriseAdmin:
        return policies
            .FindBool(
                policy::key::
                    kRemoteAccessHostAllowEnterpriseRemoteSupportConnections)
            .value_or(true);
      case remoting::ChromeOsEnterpriseRequestOrigin::kUnknown:
        NOTREACHED() << "RequestOrigin is validated to be known when "
                        "enterprise parameters are set";
    }
  }
#endif
  return policies
      .FindBool(policy::key::kRemoteAccessHostAllowRemoteSupportConnections)
      .value_or(true);
}

It2MeHostFactory::It2MeHostFactory() = default;
It2MeHostFactory::~It2MeHostFactory() = default;

std::unique_ptr<It2MeHostFactory> It2MeHostFactory::Clone() const {
  return std::make_unique<It2MeHostFactory>();
}

scoped_refptr<It2MeHost> It2MeHostFactory::CreateIt2MeHost() {
  return new It2MeHost();
}

}  // namespace remoting
