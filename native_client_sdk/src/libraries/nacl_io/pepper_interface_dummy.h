// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PEPPER_INTERFACE_DUMMY_H_
#define LIBRARIES_NACL_IO_PEPPER_INTERFACE_DUMMY_H_

#include "nacl_io/pepper_interface.h"

// This class simplifies implementing a PepperInterface-derived class where you
// don't care about certain interfaces. All interface-getters return NULL by
// default.
//
// For example:
//
// class FooPepperInterface : public PepperInterfaceDummy {
//  public:
//   CoreInterface* GetCoreInterface() { ... };
// };
//
// // FooPepperInterface is not abstract -- all pure virtual functions have
// been defined to return NULL.

namespace nacl_io {

class PepperInterfaceDummy : public PepperInterface {
 public:
  PepperInterfaceDummy() {}
  virtual ~PepperInterfaceDummy() {}
  virtual PP_Instance GetInstance() { return 0; }

// Interface getters.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  virtual BaseClass* Get##BaseClass() { return NULL; }
#include "nacl_io/pepper/all_interfaces.h"
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PEPPER_INTERFACE_DUMMY_H_
