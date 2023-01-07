// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/real_pepper_interface.h"

#include <assert.h>
#include <stdio.h>
#include <ppapi/c/pp_errors.h>

#include "nacl_io/log.h"

namespace nacl_io {

#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class Real##BaseClass : public BaseClass { \
     public: \
      explicit Real##BaseClass(const PPInterface* interface);
#define END_INTERFACE(BaseClass, PPInterface) \
     private: \
      const PPInterface* interface_; \
    };
#define METHOD0(Class, ReturnType, MethodName) \
    virtual ReturnType MethodName();
#define METHOD1(Class, ReturnType, MethodName, Type0) \
    virtual ReturnType MethodName(Type0);
#define METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    virtual ReturnType MethodName(Type0, Type1);
#define METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    virtual ReturnType MethodName(Type0, Type1, Type2);
#define METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3);
#define METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3, Type4);
#include "nacl_io/pepper/all_interfaces.h"


#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    Real##BaseClass::Real##BaseClass(const PPInterface* interface) \
        : interface_(interface) {}

#define END_INTERFACE(BaseClass, PPInterface)

#define METHOD0(BaseClass, ReturnType, MethodName) \
    ReturnType Real##BaseClass::MethodName() { \
      return interface_->MethodName(); \
    }
#define METHOD1(BaseClass, ReturnType, MethodName, Type0) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0) { \
      return interface_->MethodName(arg0); \
    }
#define METHOD2(BaseClass, ReturnType, MethodName, Type0, Type1) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1) { \
      return interface_->MethodName(arg0, arg1); \
    }
#define METHOD3(BaseClass, ReturnType, MethodName, Type0, Type1, Type2) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, \
                                           Type2 arg2) { \
      return interface_->MethodName(arg0, arg1, arg2); \
    }
#define METHOD4(BaseClass, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                           Type3 arg3) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3); \
    }
#define METHOD5(BaseClass, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    ReturnType Real##BaseClass::MethodName(Type0 arg0, Type1 arg1, Type2 arg2, \
                                           Type3 arg3, Type4 arg4) { \
      return interface_->MethodName(arg0, arg1, arg2, arg3, arg4); \
    }
#include "nacl_io/pepper/all_interfaces.h"


RealPepperInterface::RealPepperInterface(PP_Instance instance,
                                         PPB_GetInterface get_browser_interface)
    : instance_(instance) {
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) { \
    const PPInterface* iface = static_cast<const PPInterface*>( \
        get_browser_interface(InterfaceString)); \
    BaseClass##interface_ = NULL; \
    if (iface) \
      BaseClass##interface_ = new Real##BaseClass(iface); \
    else \
      LOG_ERROR("interface missing: %s\n", InterfaceString); \
  }
#include "nacl_io/pepper/all_interfaces.h"
}

PP_Instance RealPepperInterface::GetInstance() {
  return instance_;
}

// Define getter function.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  BaseClass* RealPepperInterface::Get##BaseClass() {             \
    return BaseClass##interface_;                                \
  }
#include "nacl_io/pepper/all_interfaces.h"

}  // namespace nacl_io
