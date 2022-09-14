// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_

#include <ppapi/c/pp_bool.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_file_io.h>
#include <ppapi/c/ppb_file_ref.h>
#include <ppapi/c/ppb_file_system.h>
#include <ppapi/c/ppb_host_resolver.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_net_address.h>
#include <ppapi/c/ppb_tcp_socket.h>
#include <ppapi/c/ppb_url_loader.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/ppb_url_response_info.h>
#include <ppapi/c/ppb_udp_socket.h>
#include <ppapi/c/ppb_var.h>
#include <ppapi/c/ppb_var_array.h>
#include <ppapi/c/ppb_var_array_buffer.h>
#include <ppapi/c/ppb_var_dictionary.h>

#include <sdk_util/macros.h>

namespace nacl_io {

// This class is the base interface for Pepper used by nacl_io.
//
// We use #include and macro magic to simplify adding new interfaces. The
// resulting PepperInterface basically looks like this:
//
// class PepperInterface {
//  public:
//   virtual ~PepperInterface() {}
//   virtual PP_Instance GetInstance() = 0;
//   ...
//
//   // Interface getters.
//   ConsoleInterface* GetConsoleInterface() = 0;
//   CoreInterface* GetCoreInterface() = 0;
//   FileIoInterface* GetFileIoInterface() = 0;
//   ... etc.
// };
//
//
// Note: To add a new interface:
//
// 1. Using one of the other interfaces as a template, add your interface to
//    all_interfaces.h.
// 2. Add the necessary pepper header to the top of this file.
// 3. Compile and cross your fingers!

// Forward declare interface classes.
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  class BaseClass;
#include "nacl_io/pepper/all_interfaces.h"

int PPErrorToErrno(int32_t err);

// Helper function to log real pepper error at call site.
#if defined(NDEBUG)

#define PPERROR_TO_ERRNO(err) \
  PPErrorToErrno(err)

#else

int PPErrorToErrnoLog(int32_t err, const char* file, int line);

#define PPERROR_TO_ERRNO(err) \
  PPErrorToErrnoLog(err, __FILE__, __LINE__)

#endif

class PepperInterface {
 public:
  virtual ~PepperInterface() {}
  virtual PP_Instance GetInstance() = 0;

  // Convenience functions. These forward to
  // GetCoreInterface()->{AddRef,Release}Resource.
  void AddRefResource(PP_Resource resource);
  void ReleaseResource(PP_Resource resource);

// Interface getters.
//
// These macros expand to definitions like:
//
//   CoreInterface* GetCoreInterface() = 0;
//
#include "nacl_io/pepper/undef_macros.h"
#include "nacl_io/pepper/define_empty_macros.h"
#undef BEGIN_INTERFACE
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
  virtual BaseClass* Get##BaseClass() = 0;
#include "nacl_io/pepper/all_interfaces.h"
};

// Interface class definitions.
//
// Each class will be defined with all pure virtual methods, e.g:
//
//   class CoreInterface {
//    public:
//     virtual ~CoreInterface() {}
//     virtual void AddRefResource() = 0;
//     virtual void ReleaseResource() = 0;
//     virtual PP_Bool IsMainThread() = 0;
//   };
//
#include "nacl_io/pepper/undef_macros.h"
#define BEGIN_INTERFACE(BaseClass, PPInterface, InterfaceString) \
    class BaseClass { \
     public: \
      virtual ~BaseClass() {}
#define END_INTERFACE(BaseClass, PPInterface) \
    };
#define METHOD0(Class, ReturnType, MethodName) \
    virtual ReturnType MethodName() = 0;
#define METHOD1(Class, ReturnType, MethodName, Type0) \
    virtual ReturnType MethodName(Type0) = 0;
#define METHOD2(Class, ReturnType, MethodName, Type0, Type1) \
    virtual ReturnType MethodName(Type0, Type1) = 0;
#define METHOD3(Class, ReturnType, MethodName, Type0, Type1, Type2) \
    virtual ReturnType MethodName(Type0, Type1, Type2) = 0;
#define METHOD4(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3) = 0;
#define METHOD5(Class, ReturnType, MethodName, Type0, Type1, Type2, Type3, \
                Type4) \
    virtual ReturnType MethodName(Type0, Type1, Type2, Type3, Type4) = 0;
#include "nacl_io/pepper/all_interfaces.h"


class ScopedResource {
 public:
  // Does not AddRef.
  explicit ScopedResource(PepperInterface* ppapi);
  ScopedResource(PepperInterface* ppapi, PP_Resource resource);

  ScopedResource(const ScopedResource&) = delete;
  ScopedResource& operator=(const ScopedResource&) = delete;

  ~ScopedResource();

  PP_Resource pp_resource() const { return resource_; }

  // Set a new resource, releasing the old one. Does not AddRef the new
  // resource.
  void Reset(PP_Resource resource);

  // Return the resource without decrementing its refcount.
  PP_Resource Release();

 private:
  PepperInterface* ppapi_;
  PP_Resource resource_;
};

class ScopedVar {
 public:
  // Does not AddRef.
  explicit ScopedVar(PepperInterface* ppapi);
  ScopedVar(PepperInterface* ppapi, PP_Var var);

  ScopedVar(const ScopedVar&) = delete;
  ScopedVar& operator=(const ScopedVar&) = delete;

  ~ScopedVar();

  PP_Var pp_var() const { return var_; }

  // Set a new var, releasing the old one. Does not AddRef the new
  // resource.
  void Reset(PP_Var resource);

  // Return the var without decrementing its refcount.
  PP_Var Release();

 private:
  PepperInterface* ppapi_;
  PP_Var var_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PEPPER_INTERFACE_H_
