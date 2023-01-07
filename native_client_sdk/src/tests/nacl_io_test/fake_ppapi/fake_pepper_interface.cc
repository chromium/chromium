// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_pepper_interface.h"

#include "fake_ppapi/fake_resource_manager.h"

using namespace nacl_io;

namespace {

class FakeInstanceResource : public FakeResource {
 public:
  FakeInstanceResource() {}
  static const char* classname() { return "FakeInstanceResource"; }
};

}

FakePepperInterface::FakePepperInterface()
   : core_interface_(&resource_manager_),
     messaging_interface_(&var_manager_, &var_interface_),
     var_array_interface_(&var_manager_),
     var_array_buffer_interface_(&var_manager_),
     var_interface_(&var_manager_),
     var_dictionary_interface_(&var_manager_,
                               &var_interface_,
                               &var_array_interface_),
     resolver_interface_(this),
     net_address_interface_(this) {
  FakeInstanceResource* instance_resource = new FakeInstanceResource;
  instance_ = CREATE_RESOURCE(&resource_manager_,
                              FakeInstanceResource,
                              instance_resource);
}

FakePepperInterface::~FakePepperInterface() {
  core_interface_.ReleaseResource(instance_);
}

CoreInterface* FakePepperInterface::GetCoreInterface() {
  return &core_interface_;
}

VarArrayInterface* FakePepperInterface::GetVarArrayInterface() {
  return &var_array_interface_;
}

VarArrayBufferInterface* FakePepperInterface::GetVarArrayBufferInterface() {
  return &var_array_buffer_interface_;
}

VarDictionaryInterface* FakePepperInterface::GetVarDictionaryInterface() {
  return &var_dictionary_interface_;
}

VarInterface* FakePepperInterface::GetVarInterface() {
  return &var_interface_;
}

MessagingInterface* FakePepperInterface::GetMessagingInterface() {
  return &messaging_interface_;
}

HostResolverInterface* FakePepperInterface::GetHostResolverInterface() {
  return &resolver_interface_;
}

NetAddressInterface* FakePepperInterface::GetNetAddressInterface() {
  return &net_address_interface_;
}
