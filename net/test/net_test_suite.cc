// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/net_test_suite.h"

#include "base/check_op.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_stream_factory.h"
#include "net/quic/platform/impl/quic_test_flags_utils.h"
#include "net/spdy/spdy_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class NetUnitTestEventListener : public testing::EmptyTestEventListener {
 public:
  NetUnitTestEventListener() = default;
  NetUnitTestEventListener(const NetUnitTestEventListener&) = delete;
  NetUnitTestEventListener& operator=(const NetUnitTestEventListener&) = delete;
  ~NetUnitTestEventListener() override = default;

  void OnTestStart(const testing::TestInfo& test_info) override {
    QuicFlagChecker checker;
    DCHECK(!quic_flags_saver_);
    quic_flags_saver_ = std::make_unique<QuicFlagSaverImpl>();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    quic_flags_saver_.reset();
  }

 private:
  std::unique_ptr<QuicFlagSaverImpl> quic_flags_saver_;
};

NetTestSuite* g_current_net_test_suite = nullptr;
}  // namespace

NetTestSuite::NetTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {
  DCHECK(!g_current_net_test_suite);
  g_current_net_test_suite = this;
}

NetTestSuite::~NetTestSuite() {
  DCHECK_EQ(g_current_net_test_suite, this);
  g_current_net_test_suite = nullptr;
}

void NetTestSuite::Initialize() {
  TestSuite::Initialize();
  InitializeTestThread();

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new NetUnitTestEventListener());
}

void NetTestSuite::Shutdown() {
  TestSuite::Shutdown();
}

void NetTestSuite::InitializeTestThread() {
  network_change_notifier_ = net::NetworkChangeNotifier::CreateMockIfNeeded();

  InitializeTestThreadNoNetworkChangeNotifier();
}

void NetTestSuite::InitializeTestThreadNoNetworkChangeNotifier() {
  host_resolver_proc_ =
      base::MakeRefCounted<net::RuleBasedHostResolverProc>(nullptr);
  scoped_host_resolver_proc_.Init(host_resolver_proc_.get());
  // In case any attempts are made to resolve host names, force them all to
  // be mapped to localhost.  This prevents DNS queries from being sent in
  // the process of running these unit tests.
  host_resolver_proc_->AddRule("*", "127.0.0.1");
}
