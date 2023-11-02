// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pepper_interface_mock.h"

PepperInterfaceMock::PepperInterfaceMock(PP_Instance instance)
    : instance_(instance) {
    // Initialize interfaces.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##interface_ = new BaseClass##Mock;
#include "nacl_io/pepper/all_interfaces.h"
}

PepperInterfaceMock::~PepperInterfaceMock() {
  // Delete interfaces.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    delete BaseClass##interface_;
#include "nacl_io/pepper/all_interfaces.h"

}

PP_Instance PepperInterfaceMock::GetInstance() {
  return instance_;
}

// Define Getter functions, constructors, destructors.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    BaseClass##Mock* PepperInterfaceMock::Get##BaseClass() { \
      return BaseClass##interface_; \
    } \
    BaseClass##Mock::BaseClass##Mock() { \
    } \
    BaseClass##Mock::~BaseClass##Mock() { \
    }
#include "nacl_io/pepper/all_interfaces.h"

