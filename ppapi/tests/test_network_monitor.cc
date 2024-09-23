// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_network_monitor.h"

#include <stddef.h>
#include <string.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/network_list.h"
#include "ppapi/cpp/network_monitor.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(NetworkMonitor);

namespace {

class MonitorDeletionCallbackDelegate
    : public TestCompletionCallback::Delegate {
 public:
  explicit MonitorDeletionCallbackDelegate(pp::NetworkMonitor* monitor)
      : monitor_(monitor) {
  }

  // TestCompletionCallback::Delegate interface.
  virtual void OnCallback(void* user_data, int32_t result) {
    delete monitor_;
  }

 private:
  pp::NetworkMonitor* monitor_;
};

}  // namespace

TestNetworkMonitor::TestNetworkMonitor(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestNetworkMonitor::Init() {
  if (!pp::NetworkMonitor::IsAvailable())
    return false;

  return CheckTestingInterface();
}

void TestNetworkMonitor::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Basic, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(2Monitors, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(DeleteInCallback, filter);
}

std::string TestNetworkMonitor::VerifyNetworkList(
    const pp::NetworkList& network_list) {
  // Verify that there is at least one network interface.
  size_t count = network_list.GetCount();
  ASSERT_TRUE(count >= 1U);

  // Iterate over all interfaces and verify their properties.
  for (size_t iface = 0; iface < count; ++iface) {
    // Verify that the first interface has at least one address.
    std::vector<pp::NetAddress> addresses;
    network_list.GetIpAddresses(static_cast<uint32_t>(iface), &addresses);
    ASSERT_TRUE(addresses.size() >= 1U);
    // Verify that the addresses are valid.
    for (size_t i = 0; i < addresses.size(); ++i) {
      PP_NetAddress_Family family = addresses[i].GetFamily();

      switch (family) {
        case PP_NETADDRESS_FAMILY_IPV4: {
          PP_NetAddress_IPv4 ipv4;
          ASSERT_TRUE(addresses[i].DescribeAsIPv4Address(&ipv4));

          // Verify that the address is not zero.
          bool all_zeros = true;
          for (size_t j = 0; j < sizeof(ipv4.addr); ++j) {
            if (ipv4.addr[j] != 0) {
              all_zeros = false;
              break;
            }
          }
          ASSERT_TRUE(!all_zeros);

          // Verify that port is set to 0.
          ASSERT_TRUE(ipv4.port == 0);
          break;
        }

        case PP_NETADDRESS_FAMILY_IPV6: {
          PP_NetAddress_IPv6 ipv6;
          ASSERT_TRUE(addresses[i].DescribeAsIPv6Address(&ipv6));

          // Verify that the address is not zero.
          bool all_zeros = true;
          for (size_t j = 0; j < sizeof(ipv6.addr); ++j) {
            if (ipv6.addr[j] != 0) {
              all_zeros = false;
              break;
            }
          }
          ASSERT_TRUE(!all_zeros);

          // Verify that port is set to 0.
          ASSERT_TRUE(ipv6.port == 0);
          break;
        }

        default:
          ASSERT_TRUE(false);
      }
    }

    // Verify that each interface has a unique name and a display name.
    ASSERT_FALSE(network_list.GetName(static_cast<uint32_t>(iface)).empty());
    ASSERT_FALSE(network_list.GetDisplayName(
        static_cast<uint32_t>(iface)).empty());

    PP_NetworkList_Type type =
        network_list.GetType(static_cast<uint32_t>(iface));
    ASSERT_TRUE(type >= PP_NETWORKLIST_TYPE_UNKNOWN);
    ASSERT_TRUE(type <= PP_NETWORKLIST_TYPE_CELLULAR);

    PP_NetworkList_State state =
        network_list.GetState(static_cast<uint32_t>(iface));
    ASSERT_TRUE(state >= PP_NETWORKLIST_STATE_DOWN);
    ASSERT_TRUE(state <= PP_NETWORKLIST_STATE_UP);
  }

  PASS();
}

std::string TestNetworkMonitor::TestBasic() {
  TestCompletionCallbackWithOutput<pp::NetworkList> test_callback(
      instance_->pp_instance());
  pp::NetworkMonitor network_monitor(instance_);
  test_callback.WaitForResult(
      network_monitor.UpdateNetworkList(test_callback.GetCallback()));

  ASSERT_EQ(PP_OK, test_callback.result());
  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(test_callback.output()));

  PASS();
}

std::string TestNetworkMonitor::Test2Monitors() {
  TestCompletionCallbackWithOutput<pp::NetworkList> test_callback(
     instance_->pp_instance());
  pp::NetworkMonitor network_monitor(instance_);
  test_callback.WaitForResult(
      network_monitor.UpdateNetworkList(test_callback.GetCallback()));

  ASSERT_EQ(PP_OK, test_callback.result());
  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(test_callback.output()));

  TestCompletionCallbackWithOutput<pp::NetworkList> test_callback_2(
      instance_->pp_instance());
  pp::NetworkMonitor network_monitor_2(instance_);
  test_callback_2.WaitForResult(
      network_monitor_2.UpdateNetworkList(test_callback_2.GetCallback()));

  ASSERT_EQ(PP_OK, test_callback_2.result());
  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(test_callback_2.output()));

  PASS();
}

std::string TestNetworkMonitor::TestDeleteInCallback() {
  pp::NetworkMonitor* network_monitor =
      new pp::NetworkMonitor(instance_);
  MonitorDeletionCallbackDelegate deletion_delegate(network_monitor);
  TestCompletionCallbackWithOutput<pp::NetworkList> test_callback(
      instance_->pp_instance());
  test_callback.SetDelegate(&deletion_delegate);
  test_callback.WaitForResult(
      network_monitor->UpdateNetworkList(test_callback.GetCallback()));

  ASSERT_EQ(PP_OK, test_callback.result());
  ASSERT_SUBTEST_SUCCESS(VerifyNetworkList(test_callback.output()));

  PASS();
}
