// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_network_proxy.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/network_proxy.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(NetworkProxy);

TestNetworkProxy::TestNetworkProxy(TestingInstance* instance)
    : TestCase(instance) {
}

void TestNetworkProxy::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestNetworkProxy, GetProxyForURL, filter);
}

std::string TestNetworkProxy::TestGetProxyForURL() {
  TestCompletionCallbackWithOutput<pp::Var> callback(instance_->pp_instance(),
                                                     callback_type());
  callback.WaitForResult(
      pp::NetworkProxy::GetProxyForURL(instance_,
                                       pp::Var("http://127.0.0.1/foobar/"),
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  pp::Var output = callback.output();
  ASSERT_TRUE(output.is_string());
  // Assume no one configures a proxy for localhost.
  ASSERT_EQ("DIRECT", callback.output().AsString());

  callback.WaitForResult(
      pp::NetworkProxy::GetProxyForURL(instance_,
                                       pp::Var("http://www.google.com"),
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  output = callback.output();
  // Don't know what the proxy might be, but it should be a valid result.
  ASSERT_TRUE(output.is_string());

  callback.WaitForResult(
      pp::NetworkProxy::GetProxyForURL(instance_,
                                       pp::Var("file:///tmp"),
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  output = callback.output();
  ASSERT_TRUE(output.is_string());
  // Should get "DIRECT" for file:// URLs.
  ASSERT_EQ("DIRECT", output.AsString());

  callback.WaitForResult(
      pp::NetworkProxy::GetProxyForURL(instance_,
                                       pp::Var("this isn't a url"),
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_TRUE(callback.output().is_undefined());

  callback.WaitForResult(
      pp::NetworkProxy::GetProxyForURL(instance_,
                                       pp::Var(42), // non-string Var
                                       callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  ASSERT_TRUE(callback.output().is_undefined());

  PASS();
}
