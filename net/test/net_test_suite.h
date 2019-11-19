// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_NET_TEST_SUITE_H_
#define NET_TEST_NET_TEST_SUITE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "net/dns/mock_host_resolver.h"

namespace net {
class NetworkChangeNotifier;
}

class NetTestSuite : public base::TestSuite {
 public:
  NetTestSuite(int argc, char** argv);
  ~NetTestSuite() override;

  void Initialize() override;

  void Shutdown() override;

 protected:
  // Called from within Initialize(), but separate so that derived classes
  // can initialize the NetTestSuite instance only and not
  // TestSuite::Initialize().  TestSuite::Initialize() performs some global
  // initialization that can only be done once.
  void InitializeTestThread();

  // Same as above, except it does not create a mock
  // NetworkChangeNotifier.  Use this if your test needs to create and
  // manage its own mock NetworkChangeNotifier, or if your test uses
  // the production NetworkChangeNotifier.
  void InitializeTestThreadNoNetworkChangeNotifier();

 private:
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

#endif  // NET_TEST_NET_TEST_SUITE_H_
