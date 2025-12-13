// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/policy/policy_constants.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/oauth_token_getter_proxy.h"
#include "remoting/base/passthrough_oauth_token_getter.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_event_reporter.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/linux_util.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace remoting {

using protocol::ErrorCode;

namespace {

// Shortening some type names for readability.
typedef protocol::ValidatingAuthenticator::Result ValidationResult;
typedef It2MeConfirmationDialog::Result DialogResult;

constexpr char kTestSupportId[] = "1234567";
constexpr char kTestHostUsername[] = "helpee@gmail.com";
const char kTestClientUsername[] = "ficticious_user@gmail.com";
const char kTestClientJid[] = "ficticious_user@gmail.com/jid_resource";
const char kTestClientJid2[] = "ficticious_user_2@gmail.com/jid_resource";
const char kTestClientUsernameNoJid[] = "completely_ficticious_user@gmail.com";
const char kTestClientJidWithSlash[] = "fake/user@gmail.com/jid_resource";
const char kResourceOnly[] = "/jid_resource";
const char kMatchingDomain[] = "gmail.com";
const char kMismatchedDomain1[] = "similar_to_gmail.com";
const char kMismatchedDomain2[] = "gmail_at_the_beginning.com";
const char kMismatchedDomain3[] = "not_even_close.com";
// Note that this is intentionally different from the default port range.
const char kPortRange[] = "12401-12408";

const char kTestStunServer[] = "test_relay_server.com";

class HostEventReporterStub : public HostEventReporter {
 public:
  HostEventReporterStub() = default;
  HostEventReporterStub(const HostEventReporterStub&) = delete;
  HostEventReporterStub& operator=(const HostEventReporterStub&) = delete;
  ~HostEventReporterStub() override = default;
};

class FakeRegisterSupportHostRequest : public RegisterSupportHostRequest {
 public:
  void StartRequest(SignalStrategy* signal_strategy,
                    std::unique_ptr<net::ClientCertStore> client_cert_store,
                    scoped_refptr<RsaKeyPair> key_pair,
                    const std::string& authorized_helper,
                    std::optional<ChromeOsEnterpriseParams> params,
                    RegisterCallback callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kTestSupportId,
                                  base::TimeDelta(), ErrorCode::OK));
  }
};

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<HostEventReporter> CreateHostEventReporterStub(
    scoped_refptr<HostStatusMonitor>) {
  return std::make_unique<HostEventReporterStub>();
}

ChromeOsEnterpriseParams GetDefaultEnterpriseParamsForEnterpriseAdmin() {
  ChromeOsEnterpriseParams params;
  params.request_origin = ChromeOsEnterpriseRequestOrigin::kEnterpriseAdmin;
  params.audio_playback = ChromeOsEnterpriseAudioPlayback::kLocalOnly;
  return params;
}

ChromeOsEnterpriseParams GetDefaultEnterpriseParamsForClassManagement() {
  ChromeOsEnterpriseParams params;
  params.request_origin = ChromeOsEnterpriseRequestOrigin::kClassManagement;
  params.audio_playback = ChromeOsEnterpriseAudioPlayback::kRemoteOnly;
  return params;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

// This is invoked automatically by the gtest framework, and improves the error
// messages when a test fails (by properly formatting the host state instead
// of printing their byte value).
void PrintTo(It2MeHostState state, std::ostream* os) {
#define CASE(_state)           \
  case It2MeHostState::_state: \
    *os << #_state;            \
    return;

  switch (state) {
    CASE(kDisconnected);
    CASE(kStarting);
    CASE(kRequestedAccessCode);
    CASE(kReceivedAccessCode);
    CASE(kConnecting);
    CASE(kConnected);
    CASE(kError);
    CASE(kInvalidDomainError);
  }
  NOTREACHED();
}

class FakeIt2MeConfirmationDialog : public It2MeConfirmationDialog {
 public:
  FakeIt2MeConfirmationDialog(const std::string& remote_user_email,
                              DialogResult dialog_result);

  FakeIt2MeConfirmationDialog(const FakeIt2MeConfirmationDialog&) = delete;
  FakeIt2MeConfirmationDialog& operator=(const FakeIt2MeConfirmationDialog&) =
      delete;

  ~FakeIt2MeConfirmationDialog() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  FakeIt2MeConfirmationDialog();

  std::string remote_user_email_;
  DialogResult dialog_result_ = DialogResult::OK;
};

FakeIt2MeConfirmationDialog::FakeIt2MeConfirmationDialog() = default;

FakeIt2MeConfirmationDialog::FakeIt2MeConfirmationDialog(
    const std::string& remote_user_email,
    DialogResult dialog_result)
    : remote_user_email_(remote_user_email), dialog_result_(dialog_result) {}

FakeIt2MeConfirmationDialog::~FakeIt2MeConfirmationDialog() = default;

void FakeIt2MeConfirmationDialog::Show(const std::string& remote_user_email,
                                       ResultCallback callback) {
  EXPECT_STREQ(remote_user_email_.c_str(), remote_user_email.c_str());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dialog_result_));
}

class FakeIt2MeDialogFactory : public It2MeConfirmationDialogFactory {
 public:
  FakeIt2MeDialogFactory();

  FakeIt2MeDialogFactory(const FakeIt2MeDialogFactory&) = delete;
  FakeIt2MeDialogFactory& operator=(const FakeIt2MeDialogFactory&) = delete;

  ~FakeIt2MeDialogFactory() override;

  std::unique_ptr<It2MeConfirmationDialog> Create() override;

  void set_dialog_result(DialogResult dialog_result) {
    dialog_result_ = dialog_result;
  }

  void set_remote_user_email(const std::string& remote_user_email) {
    remote_user_email_ = remote_user_email;
  }

  bool dialog_created() const { return dialog_created_; }

 private:
  std::string remote_user_email_;
  DialogResult dialog_result_ = DialogResult::OK;
  bool dialog_created_ = false;
};

FakeIt2MeDialogFactory::FakeIt2MeDialogFactory()
    : It2MeConfirmationDialogFactory(
          It2MeConfirmationDialog::DialogStyle::kConsumer),
      remote_user_email_(kTestClientUsername) {}

FakeIt2MeDialogFactory::~FakeIt2MeDialogFactory() = default;

std::unique_ptr<It2MeConfirmationDialog> FakeIt2MeDialogFactory::Create() {
  EXPECT_FALSE(remote_user_email_.empty());
  dialog_created_ = true;
  return std::make_unique<FakeIt2MeConfirmationDialog>(remote_user_email_,
                                                       dialog_result_);
}

class It2MeHostTest : public testing::Test, public It2MeHost::Observer {
 public:
  It2MeHostTest();

  It2MeHostTest(const It2MeHostTest&) = delete;
  It2MeHostTest& operator=(const It2MeHostTest&) = delete;

  ~It2MeHostTest() override;

  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  void OnValidationComplete(base::OnceClosure resume_callback,
                            ValidationResult validation_result);

 protected:
  // It2MeHost::Observer interface.
  void OnClientAuthenticated(const std::string& client_username) override;
  void OnStoreAccessCode(const std::string& access_code,
                         base::TimeDelta access_code_lifetime) override;
  void OnNatPoliciesChanged(bool nat_traversal_enabled,
                            bool relay_connections_allowed) override;
  void OnStateChanged(It2MeHostState state, ErrorCode error_code) override;

  void SetPolicies(
      std::initializer_list<std::pair<std::string_view, const base::Value&>>
          policies);

  void RunUntilStateChanged(It2MeHostState expected_state);

  // Posts a task to the network thread, which posts a task back to the current
  // thread to unblock. Useful when waiting for a side effect in the network
  // thread to take place.
  void RunNetworkThreadPendingTasks();

  void RunValidationCallback(const std::string& remote_jid);

  void StartHost();
  void StartHost(std::optional<ChromeOsEnterpriseParams> enterprise_params);
  void ShutdownHost();
  void SimulateEffectiveSessionPoliciesReceived();

  static base::Value MakeList(std::initializer_list<std::string_view> values);

  ChromotingHost* GetHost() { return it2me_host_->host_.get(); }

  const SessionPolicies& get_local_session_policies() const {
    return it2me_host_->local_session_policies_provider_->get_local_policies();
  }

  bool is_using_corp_session_authz() const {
    return it2me_host_->use_corp_session_authz_;
  }

  bool has_corp_host_status_logger() const {
    return it2me_host_->corp_host_status_logger_.get() != nullptr;
  }

  // Configuration values used by StartHost();
  std::optional<ChromeOsEnterpriseParams> enterprise_params_;
  std::optional<std::string> authorized_helper_;

  // Stores the last nat traversal policy value received.
  bool last_nat_traversal_enabled_value_ = false;

  // Stores the last relay enabled policy value received.
  bool last_relay_connections_allowed_value_ = false;

  ValidationResult validation_result_ = ValidationResult::SUCCESS;

  base::OnceClosure state_change_callback_;

  It2MeHostState last_host_state_ = It2MeHostState::kDisconnected;
  ErrorCode last_error_code_ = ErrorCode::OK;

  // Used to set ConfirmationDialog behavior.
  raw_ptr<FakeIt2MeDialogFactory, AcrossTasksDanglingUntriaged>
      dialog_factory_ = nullptr;

  std::optional<base::Value::Dict> policies_;

  scoped_refptr<It2MeHost> it2me_host_;

  PassthroughOAuthTokenGetter token_getter_;

  bool is_corp_user_ = false;

  std::string stored_access_code_;

 private:
  void StartupHostStateHelper(const base::RepeatingClosure& quit_closure);

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  std::unique_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  scoped_refptr<network::TestSharedURLLoaderFactory> test_url_loader_factory_;

  base::WeakPtrFactory<It2MeHostTest> weak_factory_{this};
};

It2MeHostTest::It2MeHostTest() = default;
It2MeHostTest::~It2MeHostTest() = default;

void It2MeHostTest::SetUp() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif

#if BUILDFLAG(IS_CHROMEOS)
  test_url_loader_factory_ = new network::TestSharedURLLoaderFactory();
#endif

  run_loop_ = std::make_unique<base::RunLoop>();

  network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();

  host_context_ = ChromotingHostContext::CreateForTesting(
      new AutoThreadTaskRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          run_loop_->QuitClosure()),
      test_url_loader_factory_);
  network_task_runner_ = host_context_->network_task_runner();
  ui_task_runner_ = host_context_->ui_task_runner();
}

void It2MeHostTest::TearDown() {
  // Shutdown the host if it hasn't been already. Without this, the call to
  // run_loop_->Run() may never return.
  it2me_host_->Disconnect();
  network_task_runner_ = nullptr;
  ui_task_runner_ = nullptr;
  host_context_.reset();
  it2me_host_ = nullptr;
  run_loop_->Run();
}

void It2MeHostTest::OnValidationComplete(base::OnceClosure resume_callback,
                                         ValidationResult validation_result) {
  validation_result_ = validation_result;

  ui_task_runner_->PostTask(FROM_HERE, std::move(resume_callback));
}

void It2MeHostTest::SetPolicies(
    std::initializer_list<std::pair<std::string_view, const base::Value&>>
        policies) {
  policies_.emplace();
  for (const auto& policy : policies) {
    policies_->Set(policy.first, policy.second.Clone());
  }
  if (it2me_host_) {
    it2me_host_->OnPolicyUpdate(std::move(*policies_));
  }
}

void It2MeHostTest::StartupHostStateHelper(
    const base::RepeatingClosure& quit_closure) {
  if (last_host_state_ != It2MeHostState::kStarting &&
      last_host_state_ != It2MeHostState::kRequestedAccessCode) {
    quit_closure.Run();
    return;
  }
  state_change_callback_ =
      base::BindOnce(&It2MeHostTest::StartupHostStateHelper,
                     base::Unretained(this), quit_closure);
}

void It2MeHostTest::StartHost() {
  if (!policies_) {
    policies_ = PolicyWatcher::GetDefaultPolicies();
  }

  std::unique_ptr<FakeIt2MeDialogFactory> dialog_factory(
      new FakeIt2MeDialogFactory());
  dialog_factory_ = dialog_factory.get();

  protocol::IceConfig ice_config;
  ice_config.stun_servers.push_back(
      webrtc::SocketAddress(kTestStunServer, 100));
  ice_config.expiration_time = base::Time::Now() + base::Hours(2);

  auto fake_signal_strategy =
      std::make_unique<FakeSignalStrategy>(SignalingAddress("fake_local_jid"));

  it2me_host_ = new It2MeHost();
  if (enterprise_params_.has_value()) {
    // Only ChromeOS supports this method, so tests setting enterprise params
    // should only be run on ChromeOS.
    it2me_host_->set_chrome_os_enterprise_params(*enterprise_params_);
  }
  if (authorized_helper_.has_value()) {
    it2me_host_->set_authorized_helper(*authorized_helper_);
  }

#if BUILDFLAG(IS_CHROMEOS)
  it2me_host_->SetHostEventReporterFactoryForTesting(
      base::BindRepeating(CreateHostEventReporterStub));
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto create_connection_context = base::BindOnce(
      [](std::unique_ptr<SignalStrategy> signal_strategy,
         base::WeakPtr<OAuthTokenGetter> token_getter, bool is_corp_user,
         ChromotingHostContext* host_context) {
        auto context = std::make_unique<It2MeHost::DeferredConnectContext>();
        context->is_corp_user = is_corp_user;
        context->register_request =
            std::make_unique<FakeRegisterSupportHostRequest>();
        context->signaling_token_getter =
            std::make_unique<OAuthTokenGetterProxy>(token_getter);
        context->api_token_getter =
            std::make_unique<OAuthTokenGetterProxy>(token_getter);
        context->signal_strategy = std::move(signal_strategy);
        return context;
      },
      std::move(fake_signal_strategy), token_getter_.GetWeakPtr(),
      is_corp_user_);
  it2me_host_->Connect(host_context_->Copy(), policies_->Clone(),
                       std::move(dialog_factory), weak_factory_.GetWeakPtr(),
                       std::move(create_connection_context), kTestHostUsername,
                       ice_config);

  base::RunLoop run_loop;
  state_change_callback_ =
      base::BindOnce(&It2MeHostTest::StartupHostStateHelper,
                     base::Unretained(this), run_loop.QuitClosure());
  run_loop.Run();
}

void It2MeHostTest::StartHost(
    std::optional<ChromeOsEnterpriseParams> enterprise_params) {
  enterprise_params_ = enterprise_params;
  StartHost();
}

void It2MeHostTest::RunUntilStateChanged(It2MeHostState expected_state) {
  if (last_host_state_ == expected_state) {
    // Bail out early if the state is already correct.
    return;
  }

  base::RunLoop run_loop;
  state_change_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}
void It2MeHostTest::RunNetworkThreadPendingTasks() {
  base::RunLoop run_loop;
  network_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         run_loop.QuitClosure());
  run_loop.Run();
}

void It2MeHostTest::RunValidationCallback(const std::string& remote_jid) {
  base::RunLoop run_loop;

  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          it2me_host_->GetValidationCallbackForTesting(), remote_jid,
          base::BindOnce(&It2MeHostTest::OnValidationComplete,
                         base::Unretained(this), run_loop.QuitClosure())));

  run_loop.Run();
}

void It2MeHostTest::OnClientAuthenticated(const std::string& client_username) {}

void It2MeHostTest::OnStoreAccessCode(const std::string& access_code,
                                      base::TimeDelta access_code_lifetime) {
  stored_access_code_ = access_code;
}

void It2MeHostTest::OnNatPoliciesChanged(bool nat_traversal_enabled,
                                         bool relay_connections_allowed) {
  last_nat_traversal_enabled_value_ = nat_traversal_enabled;
  last_relay_connections_allowed_value_ = relay_connections_allowed;
}

void It2MeHostTest::OnStateChanged(It2MeHostState state, ErrorCode error_code) {
  last_host_state_ = state;
  last_error_code_ = error_code;

  if (state_change_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(state_change_callback_));
  }
}

void It2MeHostTest::ShutdownHost() {
  if (it2me_host_) {
    it2me_host_->Disconnect();
    RunUntilStateChanged(It2MeHostState::kDisconnected);
  }
}

void It2MeHostTest::SimulateEffectiveSessionPoliciesReceived() {
  base::RunLoop run_loop;
  network_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&It2MeHost::OnEffectiveSessionPoliciesReceived),
          it2me_host_, SessionPolicies{}),
      run_loop.QuitClosure());
  run_loop.Run();
}

base::Value It2MeHostTest::MakeList(
    std::initializer_list<std::string_view> values) {
  base::Value::List result;
  for (const auto& value : values) {
    result.Append(value);
  }
  return base::Value(std::move(result));
}

// Callback to receive IceConfig from TransportContext
void ReceiveIceConfig(protocol::IceConfig* ice_config,
                      const protocol::IceConfig& received_ice_config) {
  *ice_config = received_ice_config;
}

TEST_F(It2MeHostTest, StartAndStop) {
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  // The first 7 digits of the access code are the support ID.
  ASSERT_TRUE(stored_access_code_.starts_with(kTestSupportId));

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

// Verify that IceConfig is passed to the TransportContext.
TEST_F(It2MeHostTest, IceConfig) {
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  protocol::IceConfig ice_config;
  GetHost()->transport_context_for_tests()->GetIceConfig(
      base::BindOnce(&ReceiveIceConfig, &ice_config));
  EXPECT_EQ(ice_config.stun_servers[0].hostname(), kTestStunServer);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, LocalNatTraversalPolicyEnabled) {
  SetPolicies(
      {{policy::key::kRemoteAccessHostFirewallTraversal, base::Value(true)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_TRUE(last_nat_traversal_enabled_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, LocalNatTraversalPolicyDisabled) {
  SetPolicies(
      {{policy::key::kRemoteAccessHostFirewallTraversal, base::Value(false)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_FALSE(last_nat_traversal_enabled_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, LocalRelayPolicyEnabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRelayedConnection,
                base::Value(true)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_TRUE(last_relay_connections_allowed_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, LocalRelayPolicyDisabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRelayedConnection,
                base::Value(false)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_FALSE(last_relay_connections_allowed_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(
    It2MeHostTest,
    LocalNatPoliciesChangedBeforeEffectivePoliciesAreReceived_ReportedToObserver) {
  SetPolicies({
      {policy::key::kRemoteAccessHostFirewallTraversal, base::Value(true)},
      {policy::key::kRemoteAccessHostAllowRelayedConnection, base::Value(true)},
  });

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_TRUE(last_nat_traversal_enabled_value_);
  EXPECT_TRUE(last_relay_connections_allowed_value_);

  SetPolicies({
      {policy::key::kRemoteAccessHostFirewallTraversal, base::Value(false)},
      {policy::key::kRemoteAccessHostAllowRelayedConnection,
       base::Value(false)},
  });
  RunNetworkThreadPendingTasks();

  EXPECT_FALSE(last_nat_traversal_enabled_value_);
  EXPECT_FALSE(last_relay_connections_allowed_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(
    It2MeHostTest,
    LocalNatPoliciesChangedAfterEffectivePoliciesAreReceived_NotReportedToObserver) {
  SetPolicies({
      {policy::key::kRemoteAccessHostFirewallTraversal, base::Value(true)},
      {policy::key::kRemoteAccessHostAllowRelayedConnection, base::Value(true)},
  });

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  EXPECT_TRUE(last_nat_traversal_enabled_value_);
  EXPECT_TRUE(last_relay_connections_allowed_value_);

  SimulateEffectiveSessionPoliciesReceived();
  SetPolicies({
      {policy::key::kRemoteAccessHostFirewallTraversal, base::Value(false)},
      {policy::key::kRemoteAccessHostAllowRelayedConnection,
       base::Value(false)},
  });
  RunNetworkThreadPendingTasks();

  EXPECT_TRUE(last_nat_traversal_enabled_value_);
  EXPECT_TRUE(last_relay_connections_allowed_value_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyMatchingDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyMatchStart) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain2})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyMatchEnd) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyMatchFirst) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMatchingDomain, kMismatchedDomain1})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyMatchSecond) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1, kMatchingDomain})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidationHostDomainListPolicyNoMatch) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1, kMismatchedDomain2,
                          kMismatchedDomain3})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationNoClientDomainListPolicyValidJid) {
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationNoClientDomainListPolicyInvalidJid) {
  StartHost();
  RunValidationCallback(kTestClientUsernameNoJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidationNoClientDomainListPolicyInvalidUsername) {
  StartHost();
  dialog_factory_->set_remote_user_email("fake");
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidationNoClientDomainListPolicyResourceOnly) {
  StartHost();
  RunValidationCallback(kResourceOnly);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidationClientDomainListPolicyMatchingDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidationClientDomainListPolicyInvalidUserName) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationClientDomainListPolicyNoJid) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientUsernameNoJid);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidationWrongClientDomainMatchStart) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain2})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationWrongClientDomainMatchEnd) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationClientDomainListPolicyMatchFirst) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain, kMismatchedDomain1})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationClientDomainListPolicyMatchSecond) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1, kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidationClientDomainListPolicyNoMatch) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1, kMismatchedDomain2,
                          kMismatchedDomain3})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, AuthorizedHelperCanConnect) {
  authorized_helper_ = kTestClientUsername;
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, UnauthorizedHelperIsRejected) {
  authorized_helper_ = kTestClientUsername;
  StartHost();
  RunValidationCallback(kTestClientJid2);
  ASSERT_EQ(ValidationResult::ERROR_UNAUTHORIZED_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostUdpPortRangePolicyValidRange) {
  PortRange port_range_actual;
  ASSERT_TRUE(PortRange::Parse(kPortRange, &port_range_actual));
  SetPolicies(
      {{policy::key::kRemoteAccessHostUdpPortRange, base::Value(kPortRange)}});
  StartHost();
  PortRange port_range = get_local_session_policies().host_udp_port_range;
  ASSERT_EQ(port_range_actual.min_port, port_range.min_port);
  ASSERT_EQ(port_range_actual.max_port, port_range.max_port);
}

TEST_F(It2MeHostTest, HostUdpPortRangePolicyNoRange) {
  StartHost();
  PortRange port_range = get_local_session_policies().host_udp_port_range;
  ASSERT_TRUE(port_range.is_null());
}

TEST_F(It2MeHostTest, ConnectionValidationConfirmationDialogAccept) {
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

TEST_F(It2MeHostTest, ConnectionValidationConfirmationDialogReject) {
  StartHost();
  dialog_factory_->set_dialog_result(DialogResult::CANCEL);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_REJECTED_BY_USER, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::SESSION_REJECTED, last_error_code_);
}

TEST_F(It2MeHostTest, MultipleConnectionsTriggerDisconnect) {
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);

  RunValidationCallback(kTestClientJid2);
  ASSERT_EQ(ValidationResult::ERROR_TOO_MANY_CONNECTIONS, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, AllowSupportHostConnectionsPolicyEnabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRemoteSupportConnections,
                base::Value(true)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, AllowSupportHostConnectionsPolicyDisabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRemoteSupportConnections,
                base::Value(false)}});

  StartHost();
  ASSERT_EQ(It2MeHostState::kError, last_host_state_);
  ASSERT_EQ(ErrorCode::DISALLOWED_BY_POLICY, last_error_code_);
}

TEST_F(It2MeHostTest, FileTransferDisallowedByDefault) {
  StartHost();

  EXPECT_FALSE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest, UriForwardingDisallowedByDefault) {
  StartHost();

  EXPECT_FALSE(*get_local_session_policies().allow_uri_forwarding);
}

TEST_F(It2MeHostTest, StartHost_CorpUser_UseCorpSessionAuthz) {
  is_corp_user_ = true;
  StartHost();
  ASSERT_EQ(last_host_state_, It2MeHostState::kReceivedAccessCode);
  // No shared secret after the support ID.
  ASSERT_EQ(stored_access_code_, kTestSupportId);
  ASSERT_TRUE(is_using_corp_session_authz());
  ASSERT_TRUE(has_corp_host_status_logger());
}

TEST_F(It2MeHostTest, StartHost_NonCorpUser_DoesNotUseCorpSessionAuthz) {
  is_corp_user_ = false;
  StartHost();
  ASSERT_EQ(last_host_state_, It2MeHostState::kReceivedAccessCode);
  // The access code includes the shared secret so it is longer than the support
  // ID.
  ASSERT_GT(stored_access_code_.length(), strlen(kTestSupportId));
  ASSERT_FALSE(is_using_corp_session_authz());
  ASSERT_FALSE(has_corp_host_status_logger());
}

TEST_F(It2MeHostTest, AllowRemoteInputSessionPolicyEnabledByDefault) {
  StartHost();

  EXPECT_TRUE(*get_local_session_policies().allow_remote_input);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(It2MeHostTest, ConnectRespectsSuppressDialogsParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.suppress_user_dialogs = true;
  StartHost(std::move(params));

  EXPECT_FALSE(dialog_factory_->dialog_created());
  EXPECT_FALSE(
      GetHost()->desktop_environment_options().enable_user_interface());
}

TEST_F(It2MeHostTest, ConnectRespectsSuppressNotificationsParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.suppress_notifications = true;
  StartHost(std::move(params));

  EXPECT_FALSE(dialog_factory_->dialog_created());
  EXPECT_FALSE(GetHost()->desktop_environment_options().enable_notifications());
}

TEST_F(It2MeHostTest, ConnectRespectsTerminateUponInputParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.terminate_upon_input = true;
  StartHost(std::move(params));

  EXPECT_TRUE(GetHost()->desktop_environment_options().terminate_upon_input());
}

TEST_F(It2MeHostTest, TerminateUponInputDefaultsToFalse) {
  StartHost(/*enterprise_params=*/std::nullopt);

  EXPECT_FALSE(GetHost()->desktop_environment_options().terminate_upon_input());
}

TEST_F(It2MeHostTest, ConnectRespectsMaximumSessionDurationParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.maximum_session_duration = base::Hours(8);
  StartHost(std::move(params));

  EXPECT_EQ(GetHost()->desktop_environment_options().maximum_session_duration(),
            base::Hours(8));
}

TEST_F(It2MeHostTest, ConnectRespectsEnableCurtainingParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.curtain_local_user_session = true;
  StartHost(std::move(params));

  EXPECT_TRUE(*get_local_session_policies().curtain_required);
}

TEST_F(It2MeHostTest, ConnectRespectsAllowRemoteInputParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.allow_remote_input = false;
  StartHost(std::move(params));

  EXPECT_FALSE(*get_local_session_policies().allow_remote_input);
}

TEST_F(It2MeHostTest, ConnectRespectsAllowClipboardSyncParameter) {
  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.allow_clipboard_sync = false;
  StartHost(std::move(params));

  EXPECT_EQ(*get_local_session_policies().clipboard_size_bytes, 0U);
}

TEST_F(It2MeHostTest, EnableCurtainingDefaultsToFalse) {
  StartHost(/*enterprise_params=*/std::nullopt);

  EXPECT_FALSE(get_local_session_policies().curtain_required.has_value());
}

TEST_F(It2MeHostTest, AllowEnterpriseFileTransferWithPolicyEnabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowEnterpriseFileTransfer,
                base::Value(true)}});

  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.allow_file_transfer = true;
  StartHost(std::move(params));

  EXPECT_TRUE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest, AllowEnterpriseFileTransferWithPolicyDisabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowEnterpriseFileTransfer,
                base::Value(false)}});

  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.allow_file_transfer = true;
  StartHost(std::move(params));

  EXPECT_FALSE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest,
       AllowEnterpriseFileTransferWithPolicyEnabledForNonEnterpriseSession) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowEnterpriseFileTransfer,
                base::Value(true)}});

  StartHost(/*enterprise_params=*/std::nullopt);

  EXPECT_FALSE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest, AllowEnterpriseFileTransferWithPolicyNotSet) {
  SetPolicies({});

  ChromeOsEnterpriseParams params(
      GetDefaultEnterpriseParamsForEnterpriseAdmin());
  params.allow_file_transfer = true;
  StartHost(std::move(params));

  EXPECT_FALSE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest, EnableFileTransferDefaultsToFalse) {
  StartHost(/*enterprise_params=*/std::nullopt);

  EXPECT_FALSE(*get_local_session_policies().allow_file_transfer);
}

TEST_F(It2MeHostTest,
       EnterpriseSessionsSucceedWhenRemoteSupportConnectionsPolicyDisabled) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRemoteSupportConnections,
                base::Value(false)}});

  StartHost(GetDefaultEnterpriseParamsForEnterpriseAdmin());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

TEST_F(It2MeHostTest, EnterpriseSessionsShouldNotCheckHostDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({"other-domain.com"})}});

  StartHost(GetDefaultEnterpriseParamsForEnterpriseAdmin());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

TEST_F(
    It2MeHostTest,
    EnterpriseSessionsFailWhenEnterpriseRemoteSupportConnectionsPolicyDisabled) {
  SetPolicies(
      {{policy::key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
        base::Value(false)}});

  StartHost(GetDefaultEnterpriseParamsForEnterpriseAdmin());
  ASSERT_EQ(It2MeHostState::kError, last_host_state_);
  ASSERT_EQ(ErrorCode::DISALLOWED_BY_POLICY, last_error_code_);
}

TEST_F(
    It2MeHostTest,
    RemoteSupportSessionsSucceedWhenEnterpriseRemoteSupportConnectionsPolicyDisabled) {
  SetPolicies(
      {{policy::key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
        base::Value(false)}});

  StartHost(/*enterprise_params=*/std::nullopt);
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
}

TEST_F(It2MeHostTest, EnterpriseSessionsShouldNotDisconnectOnPolicyChange) {
  StartHost(GetDefaultEnterpriseParamsForEnterpriseAdmin());
  const It2MeHostState initial_state = last_host_state_;
  ASSERT_EQ(initial_state, It2MeHostState::kReceivedAccessCode);

  SetPolicies({{policy::key::kRemoteAccessHostFirewallTraversal,
                base::Value(!last_nat_traversal_enabled_value_)}});

  // Using RunUntilIdle is frowned upon, but there is no other way to check a
  // change does *not* happen.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(last_host_state_, initial_state);
}

TEST_F(It2MeHostTest, EnterpriseClassManagementSessionsSucceedAsAStudent) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRemoteSupportConnections,
                base::Value(false)},
               {policy::key::kClassManagementEnabled, base::Value("student")}});

  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
}

TEST_F(It2MeHostTest, EnterpriseClassManagementSessionsSucceedAsATeacher) {
  SetPolicies({{policy::key::kRemoteAccessHostAllowRemoteSupportConnections,
                base::Value(false)},
               {policy::key::kClassManagementEnabled, base::Value("teacher")}});

  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
}

TEST_F(
    It2MeHostTest,
    EnterpriseClassManagementSessionsFailsWhenClassManagementPolicyDisabled) {
  SetPolicies(
      {{policy::key::kClassManagementEnabled, base::Value("disabled")}});

  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kError, last_host_state_);
  ASSERT_EQ(ErrorCode::DISALLOWED_BY_POLICY, last_error_code_);
}

TEST_F(It2MeHostTest,
       EnterpriseClassManagementSessionsFailsWhenClassManagementPolicyUnset) {
  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kError, last_host_state_);
  ASSERT_EQ(ErrorCode::DISALLOWED_BY_POLICY, last_error_code_);
}

TEST_F(It2MeHostTest,
       EnterpriseClassManagementSessionsShouldNotCheckHostDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({"other-domain.com"})},
               {policy::key::kClassManagementEnabled, base::Value("student")}});

  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

TEST_F(It2MeHostTest,
       EnterpriseClassManagementSessionsShouldNotCheckClientDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({"other-domain.com"})},
               {policy::key::kClassManagementEnabled, base::Value("student")}});

  authorized_helper_ = kTestClientUsername;
  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ErrorCode::OK, last_error_code_);
}

TEST_F(It2MeHostTest,
       EnterpriseClassManagementSessionsFailWithoutAuthorizedUser) {
  SetPolicies({{policy::key::kClassManagementEnabled, base::Value("student")}});

  StartHost(GetDefaultEnterpriseParamsForClassManagement());
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_UNAUTHORIZED_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}
#endif

}  // namespace remoting
