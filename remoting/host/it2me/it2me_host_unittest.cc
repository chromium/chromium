// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/policy_constants.h"
#include "net/base/network_change_notifier.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/xmpp_register_support_host_request.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/xmpp_log_to_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif  // defined(OS_LINUX)

namespace remoting {

using protocol::ErrorCode;

namespace {

// Shortening some type names for readability.
typedef protocol::ValidatingAuthenticator::Result ValidationResult;
typedef It2MeConfirmationDialog::Result DialogResult;

const char kTestUserName[] = "ficticious_user@gmail.com";
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

}  // namespace

class FakeIt2MeConfirmationDialog : public It2MeConfirmationDialog {
 public:
  FakeIt2MeConfirmationDialog(const std::string& remote_user_email,
                              DialogResult dialog_result);
  ~FakeIt2MeConfirmationDialog() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            const ResultCallback& callback) override;

 private:
  FakeIt2MeConfirmationDialog();

  std::string remote_user_email_;
  DialogResult dialog_result_ = DialogResult::OK;

  DISALLOW_COPY_AND_ASSIGN(FakeIt2MeConfirmationDialog);
};

FakeIt2MeConfirmationDialog::FakeIt2MeConfirmationDialog() = default;

FakeIt2MeConfirmationDialog::FakeIt2MeConfirmationDialog(
    const std::string& remote_user_email,
    DialogResult dialog_result)
    : remote_user_email_(remote_user_email), dialog_result_(dialog_result) {}

FakeIt2MeConfirmationDialog::~FakeIt2MeConfirmationDialog() = default;

void FakeIt2MeConfirmationDialog::Show(const std::string& remote_user_email,
                                       const ResultCallback& callback) {
  EXPECT_STREQ(remote_user_email_.c_str(), remote_user_email.c_str());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, dialog_result_));
}

class FakeIt2MeDialogFactory : public It2MeConfirmationDialogFactory {
 public:
  FakeIt2MeDialogFactory();
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

  DISALLOW_COPY_AND_ASSIGN(FakeIt2MeDialogFactory);
};

FakeIt2MeDialogFactory::FakeIt2MeDialogFactory()
    : remote_user_email_(kTestUserName) {}

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
  ~It2MeHostTest() override;

  // testing::Test interface.
  void SetUp() override;
  void TearDown() override;

  void OnValidationComplete(const base::Closure& resume_callback,
                            ValidationResult validation_result);

 protected:
  // It2MeHost::Observer interface.
  void OnClientAuthenticated(const std::string& client_username) override;
  void OnStoreAccessCode(const std::string& access_code,
                         base::TimeDelta access_code_lifetime) override;
  void OnNatPolicyChanged(bool nat_traversal_enabled) override;
  void OnStateChanged(It2MeHostState state, ErrorCode error_code) override;

  void SetPolicies(
      std::initializer_list<std::pair<base::StringPiece, const base::Value&>>
          policies);

  void RunUntilStateChanged(It2MeHostState expected_state);

  void RunValidationCallback(const std::string& remote_jid);

  void StartHost(bool enable_dialogs = true);
  void ShutdownHost();

  static base::ListValue MakeList(
      std::initializer_list<base::StringPiece> values);

  ChromotingHost* GetHost() { return it2me_host_->host_.get(); }

  ValidationResult validation_result_ = ValidationResult::SUCCESS;

  base::Closure state_change_callback_;

  It2MeHostState last_host_state_ = It2MeHostState::kDisconnected;

  // Used to set ConfirmationDialog behavior.
  FakeIt2MeDialogFactory* dialog_factory_ = nullptr;

  std::unique_ptr<base::DictionaryValue> policies_;

  scoped_refptr<It2MeHost> it2me_host_;

 private:
  void StartupHostStateHelper(const base::Closure& quit_closure);

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<FakeSignalStrategy> fake_bot_signal_strategy_;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  std::unique_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<It2MeHostTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(It2MeHostTest);
};

It2MeHostTest::It2MeHostTest() {}
It2MeHostTest::~It2MeHostTest() = default;

void It2MeHostTest::SetUp() {
#if defined(OS_LINUX)
  // Need to prime the host OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();
#endif
  run_loop_.reset(new base::RunLoop());

  network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();

  host_context_ = ChromotingHostContext::Create(new AutoThreadTaskRunner(
      base::ThreadTaskRunnerHandle::Get(), run_loop_->QuitClosure()));
  network_task_runner_ = host_context_->network_task_runner();
  ui_task_runner_ = host_context_->ui_task_runner();
  fake_bot_signal_strategy_.reset(
      new FakeSignalStrategy(SignalingAddress("fake_bot_jid")));
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

void It2MeHostTest::OnValidationComplete(const base::Closure& resume_callback,
                                         ValidationResult validation_result) {
  validation_result_ = validation_result;

  ui_task_runner_->PostTask(FROM_HERE, resume_callback);
}

void It2MeHostTest::SetPolicies(
    std::initializer_list<std::pair<base::StringPiece, const base::Value&>>
        policies) {
  policies_ = std::make_unique<base::DictionaryValue>();
  for (const auto& policy : policies) {
    policies_->Set(policy.first, policy.second.CreateDeepCopy());
  }
  if (it2me_host_) {
    it2me_host_->OnPolicyUpdate(std::move(policies_));
  }
}

void It2MeHostTest::StartupHostStateHelper(const base::Closure& quit_closure) {
  if (last_host_state_ == It2MeHostState::kRequestedAccessCode) {
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&It2MeHost::SetStateForTesting, it2me_host_.get(),
                       It2MeHostState::kReceivedAccessCode, ErrorCode::OK));
  } else if (last_host_state_ != It2MeHostState::kStarting) {
    quit_closure.Run();
    return;
  }
  state_change_callback_ = base::Bind(&It2MeHostTest::StartupHostStateHelper,
                                      base::Unretained(this), quit_closure);
}

void It2MeHostTest::StartHost(bool enable_dialogs) {
  if (!policies_) {
    policies_ = PolicyWatcher::GetDefaultPolicies();
  }

  std::unique_ptr<FakeIt2MeDialogFactory> dialog_factory(
      new FakeIt2MeDialogFactory());
  dialog_factory_ = dialog_factory.get();

  protocol::IceConfig ice_config;
  ice_config.stun_servers.push_back(rtc::SocketAddress(kTestStunServer, 100));
  ice_config.expiration_time =
      base::Time::Now() + base::TimeDelta::FromHours(2);

  auto fake_signal_strategy =
      std::make_unique<FakeSignalStrategy>(SignalingAddress("fake_local_jid"));
  fake_bot_signal_strategy_->ConnectTo(fake_signal_strategy.get());

  it2me_host_ = new It2MeHost();
  if (!enable_dialogs) {
    // Only ChromeOS supports this method, so tests setting enable_dialogs to
    // false should only be run on ChromeOS.
    it2me_host_->set_enable_dialogs(enable_dialogs);
  }
  auto register_host_request =
      std::make_unique<XmppRegisterSupportHostRequest>("fake_bot_jid");
  auto log_to_server = std::make_unique<XmppLogToServer>(
      ServerLogEntry::IT2ME, fake_signal_strategy.get(), "fake_bot_jid",
      host_context_->network_task_runner());
  it2me_host_->Connect(
      host_context_->Copy(), policies_->CreateDeepCopy(),
      std::move(dialog_factory), std::move(register_host_request),
      std::move(log_to_server), weak_factory_.GetWeakPtr(),
      std::move(fake_signal_strategy), kTestUserName, ice_config);

  base::RunLoop run_loop;
  state_change_callback_ =
      base::Bind(&It2MeHostTest::StartupHostStateHelper, base::Unretained(this),
                 run_loop.QuitClosure());
  run_loop.Run();
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

void It2MeHostTest::RunValidationCallback(const std::string& remote_jid) {
  base::RunLoop run_loop;

  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          it2me_host_->GetValidationCallbackForTesting(), remote_jid,
          base::Bind(&It2MeHostTest::OnValidationComplete,
                     base::Unretained(this), run_loop.QuitClosure())));

  run_loop.Run();
}

void It2MeHostTest::OnClientAuthenticated(const std::string& client_username) {}

void It2MeHostTest::OnStoreAccessCode(const std::string& access_code,
                                      base::TimeDelta access_code_lifetime) {}

void It2MeHostTest::OnNatPolicyChanged(bool nat_traversal_enabled) {}

void It2MeHostTest::OnStateChanged(It2MeHostState state, ErrorCode error_code) {
  last_host_state_ = state;

  if (state_change_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(state_change_callback_));
  }
}

void It2MeHostTest::ShutdownHost() {
  if (it2me_host_) {
    it2me_host_->Disconnect();
    RunUntilStateChanged(It2MeHostState::kDisconnected);
  }
}

base::ListValue It2MeHostTest::MakeList(
    std::initializer_list<base::StringPiece> values) {
  base::ListValue result;
  for (const auto& value : values) {
    result.AppendString(value);
  }
  return result;
}

// Callback to receive IceConfig from TransportContext
void ReceiveIceConfig(protocol::IceConfig* ice_config,
                      const protocol::IceConfig& received_ice_config) {
  *ice_config = received_ice_config;
}

TEST_F(It2MeHostTest, StartAndStop) {
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

// Verify that IceConfig is passed to the TransportContext.
TEST_F(It2MeHostTest, IceConfig) {
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);

  protocol::IceConfig ice_config;
  GetHost()->transport_context_for_tests()->set_relay_mode(
      protocol::TransportContext::TURN);
  GetHost()->transport_context_for_tests()->GetIceConfig(
      base::Bind(&ReceiveIceConfig, &ice_config));
  EXPECT_EQ(ice_config.stun_servers[0].hostname(), kTestStunServer);

  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_MatchingDomain) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_MatchStart) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain2})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_MatchEnd) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_MatchFirst) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMatchingDomain, kMismatchedDomain1})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_MatchSecond) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1, kMatchingDomain})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kReceivedAccessCode, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostValidation_HostDomainListPolicy_NoMatch) {
  SetPolicies({{policy::key::kRemoteAccessHostDomainList,
                MakeList({kMismatchedDomain1, kMismatchedDomain2,
                          kMismatchedDomain3})}});
  StartHost();
  ASSERT_EQ(It2MeHostState::kInvalidDomainError, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_NoClientDomainListPolicy_ValidJid) {
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_NoClientDomainListPolicy_InvalidJid) {
  StartHost();
  RunValidationCallback(kTestClientUsernameNoJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_NoClientDomainListPolicy_InvalidUsername) {
  StartHost();
  dialog_factory_->set_remote_user_email("fake");
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_NoClientDomainListPolicy_ResourceOnly) {
  StartHost();
  RunValidationCallback(kResourceOnly);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest,
       ConnectionValidation_ClientDomainListPolicy_MatchingDomain) {
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
       ConnectionValidation_ClientDomainListPolicy_InvalidUserName) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientJidWithSlash);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainListPolicy_NoJid) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientUsernameNoJid);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
}

TEST_F(It2MeHostTest, ConnectionValidation_WrongClientDomain_MatchStart) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain2})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_WrongClientDomain_MatchEnd) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainListPolicy_MatchFirst) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMatchingDomain, kMismatchedDomain1})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainListPolicy_MatchSecond) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1, kMatchingDomain})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ClientDomainListPolicy_NoMatch) {
  SetPolicies({{policy::key::kRemoteAccessHostClientDomainList,
                MakeList({kMismatchedDomain1, kMismatchedDomain2,
                          kMismatchedDomain3})}});
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_INVALID_ACCOUNT, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, HostUdpPortRangePolicy_ValidRange) {
  PortRange port_range_actual;
  ASSERT_TRUE(PortRange::Parse(kPortRange, &port_range_actual));
  SetPolicies(
      {{policy::key::kRemoteAccessHostUdpPortRange, base::Value(kPortRange)}});
  StartHost();
  PortRange port_range =
      GetHost()->transport_context_for_tests()->network_settings().port_range;
  ASSERT_EQ(port_range_actual.min_port, port_range.min_port);
  ASSERT_EQ(port_range_actual.max_port, port_range.max_port);
}

TEST_F(It2MeHostTest, HostUdpPortRangePolicy_NoRange) {
  StartHost();
  PortRange port_range =
      GetHost()->transport_context_for_tests()->network_settings().port_range;
  ASSERT_TRUE(port_range.is_null());
}

TEST_F(It2MeHostTest, ConnectionValidation_ConfirmationDialog_Accept) {
  StartHost();
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::SUCCESS, validation_result_);
  ASSERT_EQ(It2MeHostState::kConnecting, last_host_state_);
  ShutdownHost();
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
}

TEST_F(It2MeHostTest, ConnectionValidation_ConfirmationDialog_Reject) {
  StartHost();
  dialog_factory_->set_dialog_result(DialogResult::CANCEL);
  RunValidationCallback(kTestClientJid);
  ASSERT_EQ(ValidationResult::ERROR_REJECTED_BY_USER, validation_result_);
  RunUntilStateChanged(It2MeHostState::kDisconnected);
  ASSERT_EQ(It2MeHostState::kDisconnected, last_host_state_);
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

#if defined(OS_CHROMEOS)
TEST_F(It2MeHostTest, ConnectRespectsNoDialogsParameter) {
  StartHost(false);
  EXPECT_FALSE(dialog_factory_->dialog_created());
  EXPECT_FALSE(
      GetHost()->desktop_environment_options().enable_user_interface());
}
#endif

}  // namespace remoting
