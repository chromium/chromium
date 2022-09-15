// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_host_resolver_crash.h"

#include <stddef.h>

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(HostResolverCrash);

TestHostResolverCrash::TestHostResolverCrash(TestingInstance* instance)
    : TestCase(instance) {}

bool TestHostResolverCrash::Init() {
  return pp::HostResolver::IsAvailable();
}

void TestHostResolverCrash::RunTests(const std::string& filter) {
  // No need to run this test with the various callback types since that's
  // orthogonal from the functionality being tested. It would also make the
  // test more complicated because it would have to keep watching the network
  // process restart and telling it to crash again on crash.com.
  RUN_TEST(Basic, filter);
}

std::string TestHostResolverCrash::TestBasic() {
  pp::HostResolver host_resolver(instance_);

  PP_HostResolver_Hint hint;
  hint.family = PP_NETADDRESS_FAMILY_UNSPECIFIED;
  hint.flags = PP_HOSTRESOLVER_FLAG_CANONNAME;

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  std::string host("crash.com");
  callback.WaitForResult(
      host_resolver.Resolve(host.c_str(), 80, hint, callback.GetCallback()));
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());

  PASS();
}
