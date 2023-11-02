// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_REAL_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_IO_REAL_PEPPER_INTERFACE_H_

#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_message_loop.h>
#include "pepper_interface.h"

namespace nacl_io {

// Forward declare interface classes.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  class Real##BaseClass;
#include "nacl_io/pepper/all_interfaces.h"

class RealPepperInterface : public PepperInterface {
 public:
  RealPepperInterface(PP_Instance instance,
                      PPB_GetInterface get_browser_interface);

  virtual PP_Instance GetInstance();

// Interface getters.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  virtual BaseClass* Get##BaseClass();
#include "nacl_io/pepper/all_interfaces.h"

 private:
  PP_Instance instance_;

// Interface pointers.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  Real##BaseClass* BaseClass##interface_;
#include "nacl_io/pepper/all_interfaces.h"
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_REAL_PEPPER_INTERFACE_H_
