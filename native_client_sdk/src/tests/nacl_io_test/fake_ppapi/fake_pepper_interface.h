// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "fake_ppapi/fake_host_resolver_interface.h"
#include "fake_ppapi/fake_messaging_interface.h"
#include "fake_ppapi/fake_net_address_interface.h"
#include "fake_ppapi/fake_resource_manager.h"
#include "fake_ppapi/fake_var_array_buffer_interface.h"
#include "fake_ppapi/fake_var_array_interface.h"
#include "fake_ppapi/fake_var_dictionary_interface.h"
#include "fake_ppapi/fake_var_interface.h"
#include "fake_ppapi/fake_var_manager.h"
#include "nacl_io/pepper_interface_dummy.h"

class FakePepperInterface : public nacl_io::PepperInterfaceDummy {
 public:
  FakePepperInterface();

  FakePepperInterface(const FakePepperInterface&) = delete;
  FakePepperInterface& operator=(const FakePepperInterface&) = delete;

  virtual ~FakePepperInterface();

  virtual nacl_io::CoreInterface* GetCoreInterface();
  virtual nacl_io::MessagingInterface* GetMessagingInterface();
  virtual nacl_io::VarArrayInterface* GetVarArrayInterface();
  virtual nacl_io::VarArrayBufferInterface* GetVarArrayBufferInterface();
  virtual nacl_io::VarDictionaryInterface* GetVarDictionaryInterface();
  virtual nacl_io::VarInterface* GetVarInterface();
  virtual nacl_io::HostResolverInterface* GetHostResolverInterface();
  virtual nacl_io::NetAddressInterface* GetNetAddressInterface();
  virtual PP_Instance GetInstance() { return instance_; }

  FakeResourceManager* resource_manager() { return &resource_manager_; }
  FakeVarManager* var_manager() { return &var_manager_; }

 private:
  PP_Instance instance_;
  FakeVarManager var_manager_;
  FakeResourceManager resource_manager_;

  FakeCoreInterface core_interface_;
  FakeMessagingInterface messaging_interface_;
  FakeVarArrayInterface var_array_interface_;
  FakeVarArrayBufferInterface var_array_buffer_interface_;
  FakeVarInterface var_interface_;
  FakeVarDictionaryInterface var_dictionary_interface_;
  FakeHostResolverInterface resolver_interface_;
  FakeNetAddressInterface net_address_interface_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_PEPPER_INTERFACE_H_
