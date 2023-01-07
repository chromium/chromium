// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_list.h"
#include "base/hash/hash.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/ppapi_proxy_test.h"

namespace ppapi {
namespace proxy {

class InterfaceListTest : public PluginProxyTest {
 public:
  // Wrapper function so we can use the private InterfaceList::AddPPB.
  void AddPPB(InterfaceList* list,
              const char* iface_name, void* iface_addr, Permission perm) {
    list->AddPPB(iface_name, iface_addr, perm);
  }

  // Wrapper function so we can use the private
  // InterfaceList::HashInterfaceName.
  int HashInterfaceName(const std::string& name) {
    return InterfaceList::HashInterfaceName(name);
  }
};

// Tests looking up a stable interface.
TEST_F(InterfaceListTest, Stable) {
  InterfaceList list;
  ASSERT_TRUE(list.GetInterfaceForPPB(PPB_CORE_INTERFACE_1_0) != NULL);
  ASSERT_TRUE(list.GetInterfaceForPPB("FakeUnknownInterface") == NULL);
}

// Tests that dev channel restrictions work properly.
TEST_F(InterfaceListTest, DevChannel) {
  InterfaceList list;
  // "Dev channel" interface.
  static const char* dev_channel_iface_name = "TestDevChannelInterface";
  void* dev_channel_iface_addr = (void*)0xdeadbeef;
  // "Dev" interface
  static const char* dev_iface_name = "TestDevInterface";
  void* dev_iface_addr = (void*)0xcafefade;

  AddPPB(&list, dev_channel_iface_name, dev_channel_iface_addr,
         PERMISSION_DEV_CHANNEL);
  AddPPB(&list, dev_iface_name, dev_iface_addr, PERMISSION_DEV);

  InterfaceList::SetProcessGlobalPermissions(
      PpapiPermissions(PERMISSION_NONE));
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_channel_iface_name) == NULL);
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_iface_name) == NULL);

  InterfaceList::SetProcessGlobalPermissions(
      PpapiPermissions(PERMISSION_DEV_CHANNEL));
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_channel_iface_name) ==
              dev_channel_iface_addr);
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_iface_name) == NULL);

  InterfaceList::SetProcessGlobalPermissions(
      PpapiPermissions(PERMISSION_DEV));
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_channel_iface_name) == NULL);
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_iface_name) == dev_iface_addr);

  InterfaceList::SetProcessGlobalPermissions(
      PpapiPermissions(PERMISSION_DEV | PERMISSION_DEV_CHANNEL));
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_channel_iface_name) ==
              dev_channel_iface_addr);
  ASSERT_TRUE(list.GetInterfaceForPPB(dev_iface_name) == dev_iface_addr);
}

// Test that the hash function provided by base::Hash is unchanged. This is so
// that we will generate correct values when logging interface use to UMA.
TEST_F(InterfaceListTest, InterfaceHash) {
  EXPECT_EQ(612625164, HashInterfaceName("PPB_InputEvent;1.0"));
  EXPECT_EQ(79708274, HashInterfaceName("PPB_TCPSocket;1.1"));
}

}  // namespace proxy
}  // namespace ppapi
