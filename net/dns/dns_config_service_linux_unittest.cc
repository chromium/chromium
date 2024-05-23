// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_config_service_linux.h"

#include <arpa/inet.h>
#include <resolv.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sys_byteorder.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "net/base/ip_address.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/nsswitch_reader.h"
#include "net/dns/public/dns_protocol.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// MAXNS is normally 3, but let's test 4 if possible.
const char* const kNameserversIPv4[] = {
    "8.8.8.8",
    "192.168.1.1",
    "63.1.2.4",
    "1.0.0.1",
};

const char* const kNameserversIPv6[] = {
    nullptr,
    "2001:db8::42",
    nullptr,
    "::FFFF:129.144.52.38",
};

const std::vector<NsswitchReader::ServiceSpecification> kBasicNsswitchConfig = {
    NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
    NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)};

void DummyConfigCallback(const DnsConfig& config) {
  // Do nothing
}

// Fills in |res| with sane configuration.
void InitializeResState(res_state res) {
  memset(res, 0, sizeof(*res));
  res->options =
      RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH | RES_ROTATE;
  res->ndots = 2;
  res->retrans = 4;
  res->retry = 7;

  const char kDnsrch[] =
      "chromium.org"
      "\0"
      "example.com";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;
  res->dnsrch[1] = res->defdname + sizeof("chromium.org");

  for (unsigned i = 0; i < std::size(kNameserversIPv4) && i < MAXNS; ++i) {
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = base::HostToNet16(NS_DEFAULTPORT + i);
    inet_pton(AF_INET, kNameserversIPv4[i], &sa.sin_addr);
    res->nsaddr_list[i] = sa;
    ++res->nscount;
  }

  // Install IPv6 addresses, replacing the corresponding IPv4 addresses.
  unsigned nscount6 = 0;
  for (unsigned i = 0; i < std::size(kNameserversIPv6) && i < MAXNS; ++i) {
    if (!kNameserversIPv6[i])
      continue;
    // Must use malloc to mimic res_ninit. Expect to be freed in
    // `TestResolvReader::CloseResState()`.
    struct sockaddr_in6* sa6;
    sa6 = static_cast<sockaddr_in6*>(malloc(sizeof(*sa6)));
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = base::HostToNet16(NS_DEFAULTPORT - i);
    inet_pton(AF_INET6, kNameserversIPv6[i], &sa6->sin6_addr);
    res->_u._ext.nsaddrs[i] = sa6;
    memset(&res->nsaddr_list[i], 0, sizeof res->nsaddr_list[i]);
    ++nscount6;
  }
  res->_u._ext.nscount6 = nscount6;
}

void InitializeExpectedConfig(DnsConfig* config) {
  config->ndots = 2;
  config->fallback_period = kDnsDefaultFallbackPeriod;
  config->attempts = 7;
  config->rotate = true;
  config->append_to_multi_label_name = true;
  config->search.clear();
  config->search.push_back("chromium.org");
  config->search.push_back("example.com");

  config->nameservers.clear();
  for (unsigned i = 0; i < std::size(kNameserversIPv4) && i < MAXNS; ++i) {
    IPAddress ip;
    EXPECT_TRUE(ip.AssignFromIPLiteral(kNameserversIPv4[i]));
    config->nameservers.emplace_back(ip, NS_DEFAULTPORT + i);
  }

  for (unsigned i = 0; i < std::size(kNameserversIPv6) && i < MAXNS; ++i) {
    if (!kNameserversIPv6[i])
      continue;
    IPAddress ip;
    EXPECT_TRUE(ip.AssignFromIPLiteral(kNameserversIPv6[i]));
    config->nameservers[i] = IPEndPoint(ip, NS_DEFAULTPORT - i);
  }
}

class CallbackHelper {
 public:
  std::optional<DnsConfig> WaitForResult() {
    run_loop_.Run();
    return GetResult();
  }

  std::optional<DnsConfig> GetResult() {
    std::optional<DnsConfig> result = std::move(config_);
    return result;
  }

  DnsConfigService::CallbackType GetCallback() {
    return base::BindRepeating(&CallbackHelper::OnComplete,
                               base::Unretained(this));
  }

 private:
  void OnComplete(const DnsConfig& config) {
    config_ = config;
    run_loop_.Quit();
  }

  std::optional<DnsConfig> config_;
  base::RunLoop run_loop_;
};

// Helper to allow blocking on some point in the ThreadPool.
class BlockingHelper {
 public:
  ~BlockingHelper() { EXPECT_EQ(state_, State::kUnblocked); }

  // Called by the test code to wait for the block point to be reached.
  void WaitUntilBlocked() {
    CHECK_EQ(state_, State::kUnblocked);
    state_ = State::kRunningUntilBlock;

    CHECK(!run_loop_ || !run_loop_->running());
    run_loop_.emplace();
    run_loop_->Run();

    CHECK_EQ(state_, State::kBlocked);
  }

  // Called by the ThreadPool code on reaching the block point.
  void WaitUntilUnblocked() {
    block_event_.Reset();
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&BlockingHelper::OnBlockedCallback,
                                          base::Unretained(this)));
    block_event_.Wait();
    blocker_event_.Signal();
  }

  // Called by the test code to unblock the ThreadPool code.
  void Unblock() {
    CHECK_EQ(state_, State::kBlocked);
    CHECK(!block_event_.IsSignaled());

    state_ = State::kUnblocked;

    blocker_event_.Reset();
    block_event_.Signal();
    blocker_event_.Wait();
  }

 private:
  enum class State {
    kRunningUntilBlock,
    kBlocked,
    kUnblocked,
  };

  void OnBlockedCallback() {
    CHECK_EQ(state_, State::kRunningUntilBlock);
    CHECK(run_loop_.has_value());
    CHECK(run_loop_->running());

    state_ = State::kBlocked;
    run_loop_->Quit();
  }

  State state_ = State::kUnblocked;
  std::optional<base::RunLoop> run_loop_;
  base::TestWaitableEvent block_event_;
  base::TestWaitableEvent blocker_event_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
};

class TestScopedResState : public ScopedResState {
 public:
  explicit TestScopedResState(std::unique_ptr<struct __res_state> res)
      : res_(std::move(res)) {}

  ~TestScopedResState() override {
    if (res_) {
      // Assume `res->_u._ext.nsaddrs` memory allocated via malloc, e.g. by
      // `InitializeResState()`.
      for (int i = 0; i < res_->nscount; ++i) {
        if (res_->_u._ext.nsaddrs[i] != nullptr)
          free(res_->_u._ext.nsaddrs[i]);
      }
    }
  }

  const struct __res_state& state() const override {
    EXPECT_TRUE(res_);
    return *res_;
  }

 private:
  std::unique_ptr<struct __res_state> res_;
};

class TestResolvReader : public ResolvReader {
 public:
  ~TestResolvReader() override = default;

  void set_value(std::unique_ptr<struct __res_state> value) {
    CHECK(!value_);
    value_ = std::make_unique<TestScopedResState>(std::move(value));
  }

  bool closed() { return !value_; }

  // ResolvReader:
  std::unique_ptr<ScopedResState> GetResState() override {
    if (blocking_helper_)
      blocking_helper_->WaitUntilUnblocked();

    CHECK(value_);
    return std::move(value_);
  }

  void set_blocking_helper(BlockingHelper* blocking_helper) {
    blocking_helper_ = blocking_helper;
  }

 private:
  std::unique_ptr<TestScopedResState> value_;
  raw_ptr<BlockingHelper> blocking_helper_ = nullptr;
};

class TestNsswitchReader : public NsswitchReader {
 public:
  void set_value(std::vector<ServiceSpecification> value) {
    value_ = std::move(value);
  }

  // NsswitchReader:
  std::vector<ServiceSpecification> ReadAndParseHosts() override {
    return value_;
  }

 private:
  std::vector<ServiceSpecification> value_;
};

class DnsConfigServiceLinuxTest : public ::testing::Test,
                                  public WithTaskEnvironment {
 public:
  DnsConfigServiceLinuxTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto resolv_reader = std::make_unique<TestResolvReader>();
    resolv_reader_ = resolv_reader.get();
    service_.set_resolv_reader_for_testing(std::move(resolv_reader));

    auto nsswitch_reader = std::make_unique<TestNsswitchReader>();
    nsswitch_reader_ = nsswitch_reader.get();
    service_.set_nsswitch_reader_for_testing(std::move(nsswitch_reader));
  }

 protected:
  internal::DnsConfigServiceLinux service_;
  raw_ptr<TestResolvReader> resolv_reader_;
  raw_ptr<TestNsswitchReader> nsswitch_reader_;
};

// Regression test to verify crash does not occur if DnsConfigServiceLinux
// instance is destroyed without calling WatchConfig()
TEST_F(DnsConfigServiceLinuxTest, CreateAndDestroy) {
  auto service = std::make_unique<internal::DnsConfigServiceLinux>();
  service.reset();
  RunUntilIdle();
}

TEST_F(DnsConfigServiceLinuxTest, ConvertResStateToDnsConfig) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());

  DnsConfig expected_config;
  EXPECT_FALSE(expected_config.EqualsIgnoreHosts(config.value()));
  InitializeExpectedConfig(&expected_config);
  EXPECT_TRUE(expected_config.EqualsIgnoreHosts(config.value()));

  EXPECT_TRUE(resolv_reader_->closed());
}

TEST_F(DnsConfigServiceLinuxTest, RejectEmptyNameserver) {
  auto res = std::make_unique<struct __res_state>();
  res->options = RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  const char kDnsrch[] = "chromium.org";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;

  struct sockaddr_in sa = {};
  sa.sin_family = AF_INET;
  sa.sin_port = base::HostToNet16(NS_DEFAULTPORT);
  sa.sin_addr.s_addr = INADDR_ANY;
  res->nsaddr_list[0] = sa;
  sa.sin_addr.s_addr = 0xCAFE1337;
  res->nsaddr_list[1] = sa;
  res->nscount = 2;

  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  RunUntilIdle();
  std::optional<DnsConfig> config = callback_helper.GetResult();

  EXPECT_FALSE(config.has_value());
  EXPECT_TRUE(resolv_reader_->closed());
}

TEST_F(DnsConfigServiceLinuxTest, AcceptNonEmptyNameserver) {
  auto res = std::make_unique<struct __res_state>();
  res->options = RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  const char kDnsrch[] = "chromium.org";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;

  struct sockaddr_in sa = {};
  sa.sin_family = AF_INET;
  sa.sin_port = base::HostToNet16(NS_DEFAULTPORT);
  sa.sin_addr.s_addr = 0xDEADBEEF;
  res->nsaddr_list[0] = sa;
  sa.sin_addr.s_addr = 0xCAFE1337;
  res->nsaddr_list[1] = sa;
  res->nscount = 2;

  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();

  EXPECT_TRUE(config.has_value());
  EXPECT_TRUE(resolv_reader_->closed());
}

// Regression test to verify crash does not occur if DnsConfigServiceLinux
// instance is destroyed while SerialWorker jobs have posted to worker pool.
TEST_F(DnsConfigServiceLinuxTest, DestroyWhileJobsWorking) {
  auto service = std::make_unique<internal::DnsConfigServiceLinux>();
  // Call WatchConfig() which also tests ReadConfig().
  service->WatchConfig(base::BindRepeating(&DummyConfigCallback));
  service.reset();
  FastForwardUntilNoTasksRemain();
}

// Regression test to verify crash does not occur if DnsConfigServiceLinux
// instance is destroyed on another thread.
TEST_F(DnsConfigServiceLinuxTest, DestroyOnDifferentThread) {
  scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  std::unique_ptr<internal::DnsConfigServiceLinux, base::OnTaskRunnerDeleter>
      service(new internal::DnsConfigServiceLinux(),
              base::OnTaskRunnerDeleter(runner));

  runner->PostTask(FROM_HERE,
                   base::BindOnce(&internal::DnsConfigServiceLinux::WatchConfig,
                                  base::Unretained(service.get()),
                                  base::BindRepeating(&DummyConfigCallback)));
  service.reset();
  RunUntilIdle();
}

TEST_F(DnsConfigServiceLinuxTest, AcceptsBasicNsswitchConfig) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest,
       IgnoresBasicNsswitchConfigIfResolvConfigUnhandled) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  res->options |= RES_USE_DNSSEC;  // Expect unhandled.
  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchWithoutFiles) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithExtraFiles) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, IgnoresRedundantActions) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kFiles,
           {{/*negated=*/false, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kReturn},
            {/*negated=*/true, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kContinue}}),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kDns,
           {{/*negated=*/false, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kReturn},
            {/*negated=*/true, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kContinue}})});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsInconsistentActions) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kDns,
           {{/*negated=*/false, NsswitchReader::Status::kUnavailable,
             NsswitchReader::Action::kReturn},
            {/*negated=*/true, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kContinue}})});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadFilesSuccessAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kFiles,
           {{/*negated=*/false, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kContinue}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadFilesNotFoundAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kFiles,
           {{/*negated=*/false, NsswitchReader::Status::kNotFound,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchWithoutDns) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadDnsSuccessAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kDns,
           {{/*negated=*/false, NsswitchReader::Status::kSuccess,
             NsswitchReader::Action::kContinue}})});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchWithMisorderedServices) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, AcceptsIncompatibleNsswitchServicesAfterDns) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchMdns) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchMdns4) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns4),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchMdns6) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns6),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, AcceptsNsswitchMdnsMinimal) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdnsMinimal),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdns4Minimal),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdns6Minimal),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

// mdns*_minimal is often paired with [!UNAVAIL=RETURN] or [NOTFOUND=RETURN]
// actions. Ensure that is accepted.
TEST_F(DnsConfigServiceLinuxTest, AcceptsNsswitchMdnsMinimalWithCommonActions) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdnsMinimal,
           {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdns4Minimal,
           {{/*negated=*/false, NsswitchReader::Status::kNotFound,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdns6Minimal,
           {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadMdnsMinimalUnavailableAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMdnsMinimal,
           {{/*negated=*/false, NsswitchReader::Status::kUnavailable,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, AcceptsNsswitchMyHostname) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMyHostname),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_FALSE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadMyHostnameNotFoundAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kMyHostname,
           {{/*negated=*/false, NsswitchReader::Status::kNotFound,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchResolve) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kResolve),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchNis) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kNis),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsWithBadNisNotFoundAction) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(
           NsswitchReader::Service::kNis,
           {{/*negated=*/false, NsswitchReader::Status::kNotFound,
             NsswitchReader::Action::kReturn}}),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, RejectsNsswitchUnknown) {
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));

  nsswitch_reader_->set_value(
      {NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kUnknown),
       NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)});

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());
  std::optional<DnsConfig> config = callback_helper.WaitForResult();
  EXPECT_TRUE(resolv_reader_->closed());

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());
  EXPECT_TRUE(config->unhandled_options);
}

TEST_F(DnsConfigServiceLinuxTest, FreshReadsAfterAdditionalTriggers) {
  BlockingHelper blocking_helper;
  resolv_reader_->set_blocking_helper(&blocking_helper);

  CallbackHelper callback_helper;
  service_.ReadConfig(callback_helper.GetCallback());

  // Expect work to be blocked.
  blocking_helper.WaitUntilBlocked();
  ASSERT_FALSE(callback_helper.GetResult());

  // Signal config changes (trigger a few times to confirm only one fresh read
  // is performed).
  service_.TriggerOnConfigChangedForTesting(/*succeeded=*/true);
  service_.TriggerOnConfigChangedForTesting(/*succeeded=*/true);
  service_.TriggerOnConfigChangedForTesting(/*succeeded=*/true);

  // Initial results (expect to be replaced with second read)
  auto res = std::make_unique<struct __res_state>();
  InitializeResState(res.get());
  resolv_reader_->set_value(std::move(res));
  nsswitch_reader_->set_value(kBasicNsswitchConfig);

  // Unblock first read (expect no completion because second read should begin
  // immediately)
  blocking_helper.Unblock();
  blocking_helper.WaitUntilBlocked();
  ASSERT_FALSE(callback_helper.GetResult());
  EXPECT_TRUE(resolv_reader_->closed());

  // Setup a new config to confirm a fresh read is performed.
  res = std::make_unique<struct __res_state>();
  res->options = RES_INIT | RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  struct sockaddr_in sa = {};
  sa.sin_family = AF_INET;
  sa.sin_port = base::HostToNet16(1000);
  inet_pton(AF_INET, "1.2.3.4", &sa.sin_addr);
  res->nsaddr_list[0] = sa;
  res->nscount = 1;
  resolv_reader_->set_value(std::move(res));

  // Unblock second read (expect completion)
  blocking_helper.Unblock();
  std::optional<DnsConfig> config = callback_helper.WaitForResult();

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->IsValid());

  IPEndPoint expected(IPAddress(1, 2, 3, 4), 1000);
  EXPECT_THAT(config.value().nameservers, testing::ElementsAre(expected));

  EXPECT_TRUE(resolv_reader_->closed());
}

}  // namespace

}  // namespace net
