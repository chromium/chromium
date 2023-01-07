// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PEPPER_INTERFACE_DELEGATE_H_
#define LIBRARIES_NACL_IO_PEPPER_INTERFACE_DELEGATE_H_

#include "nacl_io/pepper_interface.h"

// This class allows you to delegate Interface requests to different
// PepperInterface-derived classes.
//
// For example:
//  class FooPepperInterface : public PepperInterface {
//    ...
//    CoreInterface* GetCoreInterface() { ... };
//    ...
//  };
//
//  class BarPepperInterface : public PepperInterface {
//    ...
//    VarInterface* GetVarInterface() { ... };
//    ...
//  };
//
//  void SomeFunction() {
//    FooPepperInterface foo;
//    BarPepperInterface bar;
//    PepperInterfaceDelegate delegate(pp_instance);
//    delegate.SetCoreInterface(foo.GetCoreInterface());
//    delegate.SetVarInterface(bar.GetVarInterface());
//    ...
//  }

namespace nacl_io {

class PepperInterfaceDelegate : public PepperInterface {
 public:
  explicit PepperInterfaceDelegate(PP_Instance instance);
  virtual ~PepperInterfaceDelegate();
  virtual PP_Instance GetInstance();

// Interface getters.
//
// These declarations look like:
//
//   CoreInterface* GetCoreInterface();
//
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  virtual BaseClass* Get##BaseClass();
#include "nacl_io/pepper/all_interfaces.h"

// Interface delegate setters.
//
// These declarations look like:
//
//   void SetCoreInterface(CoreInterface* delegate);
//
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  void Set##BaseClass##Delegate(BaseClass* delegate);
#include "nacl_io/pepper/all_interfaces.h"

 private:
  PP_Instance instance_;
// Interface delegate pointers.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  BaseClass* BaseClass##delegate_;
#include "nacl_io/pepper/all_interfaces.h"
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PEPPER_INTERFACE_DELEGATE_H_
