// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_NETWORK_MONITOR_H_
#define PPAPI_TESTS_TEST_NETWORK_MONITOR_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class NetworkList;
}  // namespace pp

class TestNetworkMonitor : public TestCase {
 public:
  explicit TestNetworkMonitor(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasic();
  std::string Test2Monitors();
  std::string TestDeleteInCallback();

  std::string VerifyNetworkListResource(PP_Resource network_resource);
  std::string VerifyNetworkList(const pp::NetworkList& network_list);

};

#endif  // PPAPI_TESTS_TEST_NETWORK_MONITOR_H_
