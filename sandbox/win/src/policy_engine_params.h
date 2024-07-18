// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SANDBOX_WIN_SRC_POLICY_ENGINE_PARAMS_H_
#define SANDBOX_WIN_SRC_POLICY_ENGINE_PARAMS_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

// This header defines the classes that allow the low level policy to select
// the input parameters. In order to better make sense of this header is
// recommended that you check policy_engine_opcodes.h first.

namespace sandbox {

// Models the set of interesting parameters of an intercepted system call
// normally you don't create objects of this class directly, instead you
// use the POLPARAMS_XXX macros.
// For example, if an intercepted function has the following signature:
//
// NTSTATUS NtOpenFileFunction (PHANDLE FileHandle,
//                              ACCESS_MASK DesiredAccess,
//                              POBJECT_ATTRIBUTES ObjectAttributes,
//                              PIO_STATUS_BLOCK IoStatusBlock,
//                              ULONG ShareAccess,
//                              ULONG OpenOptions);
//
// You could say that the following parameters are of interest to policy:
//
//   POLPARAMS_BEGIN(open_params)
//      POLPARAM(DESIRED_ACCESS)
//      POLPARAM(OBJECT_NAME)
//      POLPARAM(SECURITY_DESCRIPTOR)
//      POLPARAM(IO_STATUS)
//      POLPARAM(OPEN_OPTIONS)
//   POLPARAMS_END;
//
// and the actual code will use this for defining the parameters:
//
//   CountedParameterSet<open_params> p;
//   p[open_params::DESIRED_ACCESS] = ParamPickerMake(DesiredAccess);
//   p[open_params::OBJECT_NAME] =
//       ParamPickerMake(ObjectAttributes->ObjectName);
//   p[open_params::SECURITY_DESCRIPTOR] =
//       ParamPickerMake(ObjectAttributes->SecurityDescriptor);
//   p[open_params::IO_STATUS] = ParamPickerMake(IoStatusBlock);
//   p[open_params::OPEN_OPTIONS] = ParamPickerMake(OpenOptions);
//
//  These will create an stack-allocated array of ParameterSet objects which
//  have each 1) the address of the parameter 2) a numeric id that encodes the
//  original C++ type. This allows the policy to treat any set of supported
//  argument types uniformily and with some type safety.
class ParameterSet {
 public:
  ParameterSet() : real_type_(INVALID_TYPE), address_(nullptr) {}

  // Retrieve the stored parameter. If the type does not match ulong fail.
  bool Get(uint32_t* destination) const {
    if (real_type_ != UINT32_TYPE) {
      return false;
    }
    *destination = Void2TypePointerCopy<uint32_t>();
    return true;
  }

  // Retrieve the stored parameter. If the type does not match void* fail.
  bool Get(const void** destination) const {
    if (real_type_ != VOIDPTR_TYPE) {
      return false;
    }
    *destination = Void2TypePointerCopy<void*>();
    return true;
  }

  // Retrieve the stored parameter. If the type does not match wchar_t* fail.
  bool Get(const wchar_t** destination) const {
    if (real_type_ != WCHAR_TYPE) {
      return false;
    }
    *destination = Void2TypePointerCopy<const wchar_t*>();
    return true;
  }

  // False if the parameter is not properly initialized.
  bool IsValid() const { return real_type_ != INVALID_TYPE; }

 protected:
  // The constructor can only be called by derived types, which should
  // safely provide the real_type and the address of the argument.
  ParameterSet(ArgType real_type, const void* address)
      : real_type_(real_type), address_(address) {}

 private:
  // This template provides the same functionality as bits_cast but
  // it works with pointer while the former works only with references.
  template <typename T>
  T Void2TypePointerCopy() const {
    return *(reinterpret_cast<const T*>(address_.get()));
  }

  // Note - we fuzz this via a fake type in sandbox_policy_rule_fuzzer.cc which
  // should reflect the layout of these members.
  ArgType real_type_;
  raw_ptr<const void> address_;
};

// To safely infer the type, we use a set of template specializations
// in ParameterSetEx with a template function ParamPickerMake to do the
// parameter type deduction.

// Base template class. Fails to compile to force use of implemented wrappers.
template <typename T>
class ParameterSetEx : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address) {
    static_assert(false, "Type not supported.");
  }
};

template <>
class ParameterSetEx<void const*> : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address)
      : ParameterSet(VOIDPTR_TYPE, address) {}
};

template <>
class ParameterSetEx<void*> : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address)
      : ParameterSet(VOIDPTR_TYPE, address) {}
};

template <>
class ParameterSetEx<wchar_t*> : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address)
      : ParameterSet(WCHAR_TYPE, address) {}
};

template <>
class ParameterSetEx<wchar_t const*> : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address)
      : ParameterSet(WCHAR_TYPE, address) {}
};

template <>
class ParameterSetEx<uint32_t> : public ParameterSet {
 public:
  explicit ParameterSetEx(const void* address)
      : ParameterSet(UINT32_TYPE, address) {}
};

template <typename T>
ParameterSet ParamPickerMake(T& parameter) {
  return ParameterSetEx<T>(&parameter);
}

struct CountedParameterSetBase {
  size_t count;
  ParameterSet parameters[1];
};

// This template defines the actual list of policy parameters for a given
// interception.
// Warning: This template stores the address to the actual variables, in
// other words, the values are not copied.
template <typename T>
struct CountedParameterSet {
  CountedParameterSet() : count(T::PolParamLast) {}

  ParameterSet& operator[](typename T::Args n) { return parameters[n]; }

  CountedParameterSetBase* GetBase() {
    return reinterpret_cast<CountedParameterSetBase*>(this);
  }

  size_t count;
  ParameterSet parameters[T::PolParamLast];
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_ENGINE_PARAMS_H_
