// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_HOST_RESOLVER_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_HOST_RESOLVER_INTERFACE_H_

#include <ppapi/c/ppb_host_resolver.h>

#include <netinet/in.h>
#include <string>
#include <vector>

#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

class FakePepperInterface;

class FakeHostResolverInterface : public nacl_io::HostResolverInterface {
 public:
  explicit FakeHostResolverInterface(FakePepperInterface* ppapi);

  FakeHostResolverInterface(const FakeHostResolverInterface&) = delete;
  FakeHostResolverInterface& operator=(const FakeHostResolverInterface&) =
      delete;

  virtual PP_Resource Create(PP_Instance);

  virtual int32_t Resolve(PP_Resource,
                          const char*,
                          uint16_t,
                          const PP_HostResolver_Hint* hints,
                          PP_CompletionCallback);

  virtual PP_Var GetCanonicalName(PP_Resource);
  virtual uint32_t GetNetAddressCount(PP_Resource);
  virtual PP_Resource GetNetAddress(PP_Resource, uint32_t);

  std::string fake_hostname;
  std::vector<struct sockaddr_in> fake_addresses_v4;
  std::vector<struct sockaddr_in6> fake_addresses_v6;

 private:
  FakePepperInterface* ppapi_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_HOST_RESOLVER_INTERFACE_H_
