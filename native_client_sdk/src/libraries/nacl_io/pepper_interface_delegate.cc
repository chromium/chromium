// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pepper_interface_delegate.h"

namespace nacl_io {

PepperInterfaceDelegate::PepperInterfaceDelegate(PP_Instance instance)
    : instance_(instance) {
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  BaseClass##delegate_ = NULL;
#include "nacl_io/pepper/all_interfaces.h"
}

PepperInterfaceDelegate::~PepperInterfaceDelegate() {
}

PP_Instance PepperInterfaceDelegate::GetInstance() {
  return instance_;
}

// Interface getters.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
BaseClass* PepperInterfaceDelegate::Get##BaseClass() { \
  return BaseClass##delegate_; \
}
#include "nacl_io/pepper/all_interfaces.h"

// Interface delegate setters.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
void PepperInterfaceDelegate::Set##BaseClass##Delegate( \
    BaseClass* delegate) { \
  BaseClass##delegate_ = delegate; \
}
#include "nacl_io/pepper/all_interfaces.h"

}  // namespace nacl_io
