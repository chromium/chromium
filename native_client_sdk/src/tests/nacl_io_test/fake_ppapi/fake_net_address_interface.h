// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NET_ADDRESS_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NET_ADDRESS_INTERFACE_H_

#include <ppapi/c/ppb_host_resolver.h>

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakePepperInterface;

class FakeNetAddressInterface : public nacl_io::NetAddressInterface {
 public:
  explicit FakeNetAddressInterface(FakePepperInterface* ppapi);

  FakeNetAddressInterface(const FakeNetAddressInterface&) = delete;
  FakeNetAddressInterface& operator=(const FakeNetAddressInterface&) = delete;

  virtual PP_Resource CreateFromIPv4Address(PP_Instance, PP_NetAddress_IPv4*);
  virtual PP_Resource CreateFromIPv6Address(PP_Instance, PP_NetAddress_IPv6*);
  virtual PP_Bool IsNetAddress(PP_Resource);
  virtual PP_NetAddress_Family GetFamily(PP_Resource);
  virtual PP_Bool DescribeAsIPv4Address(PP_Resource, PP_NetAddress_IPv4*);
  virtual PP_Bool DescribeAsIPv6Address(PP_Resource, PP_NetAddress_IPv6*);
  virtual PP_Var DescribeAsString(PP_Resource, PP_Bool);

 private:
  FakePepperInterface* ppapi_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_NET_ADDRESS_INTERFACE_H_
