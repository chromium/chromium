// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_CROSSCALL_CLIENT_H_
#define SANDBOX_SRC_CROSSCALL_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/sandbox.h"

// This header defines the CrossCall(..) family of templated functions
// Their purpose is to simulate the syntax of regular call but to generate
// and IPC from the client-side.
//
// The basic pattern is to
//   1) use template argument deduction to compute the size of each
//      parameter and the appropriate copy method
//   2) pack the parameters in the appropriate ActualCallParams< > object
//   3) call the IPC interface IPCProvider::DoCall( )
//
// The general interface of CrossCall is:
//  ResultCode CrossCall(IPCProvider& ipc_provider,
//                       uint32_t tag,
//                       const Par1& p1, const Par2& p2,...pn
//                       CrossCallReturn* answer)
//
//  where:
//    ipc_provider: is a specific implementation of the ipc transport see
//                  sharedmem_ipc_server.h for an example.
//    tag : is the unique id for this IPC call. Is used to route the call to
//          the appropriate service.
//    p1, p2,.. pn : The input parameters of the IPC. Use only simple types
//                   and wide strings (can add support for others).
//    answer : If the IPC was successful. The server-side answer is here. The
//             interpretation of the answer is private to client and server.
//
// The return value is ALL_OK if the IPC was delivered to the server, other
// return codes indicate that the IPC transport failed to deliver it.
namespace sandbox {

enum class IpcTag;

// this is the assumed channel size. This can be overridden in a given
// IPC implementation.
const uint32_t kIPCChannelSize = 1024;

// The copy helper uses templates to deduce the appropriate copy function to
// copy the input parameters in the buffer that is going to be send across the
// IPC. These template facility can be made more sophisticated as need arises.

// The default copy helper. It catches the general case where no other
// specialized template matches better. We set the type to UINT32_TYPE, so this
// only works with objects whose size is 32 bits.
template <typename T>
class CopyHelper {
 public:
  CopyHelper(const T& t) : t_(t) {}

  // Returns the pointer to the start of the input.
  const void* GetStart() const { return &t_; }

  // Update the stored value with the value in the buffer. This is not
  // supported for this type.
  bool Update(void* buffer) {
    // Not supported;
    return true;
  }

  // Returns the size of the input in bytes.
  uint32_t GetSize() const { return sizeof(T); }

  // Returns true if the current type is used as an In or InOut parameter.
  bool IsInOut() { return false; }

  // Returns this object's type.
  ArgType GetType() {
    static_assert(sizeof(T) == sizeof(uint32_t), "specialization needed");
    return UINT32_TYPE;
  }

 private:
  const T& t_;
};

// This copy helper template specialization if for the void pointer
// case both 32 and 64 bit.
template <>
class CopyHelper<void*> {
 public:
  CopyHelper(void* t) : t_(t) {}

  // Returns the pointer to the start of the input.
  const void* GetStart() const { return &t_; }

  // Update the stored value with the value in the buffer. This is not
  // supported for this type.
  bool Update(void* buffer) {
    // Not supported;
    return true;
  }

  // Returns the size of the input in bytes.
  uint32_t GetSize() const { return sizeof(t_); }

  // Returns true if the current type is used as an In or InOut parameter.
  bool IsInOut() { return false; }

  // Returns this object's type.
  ArgType GetType() { return VOIDPTR_TYPE; }

 private:
  const void* t_;
};

// This copy helper template specialization catches the cases where the
// parameter is a pointer to a string.
template <>
class CopyHelper<const wchar_t*> {
 public:
  CopyHelper(const wchar_t* t) : t_(t) {}

  // Returns the pointer to the start of the string.
  const void* GetStart() const { return t_; }

  // Update the stored value with the value in the buffer. This is not
  // supported for this type.
  bool Update(void* buffer) {
    // Not supported;
    return true;
  }

  // Returns the size of the string in bytes. We define a nullptr string to
  // be of zero length.
  uint32_t GetSize() const {
    __try {
      return (!t_) ? 0
                   : static_cast<uint32_t>(StringLength(t_) * sizeof(t_[0]));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return UINT32_MAX;
    }
  }

  // Returns true if the current type is used as an In or InOut parameter.
  bool IsInOut() { return false; }

  ArgType GetType() { return WCHAR_TYPE; }

 private:
  // We provide our not very optimized version of wcslen(), since we don't
  // want to risk having the linker use the version in the CRT since the CRT
  // might not be present when we do an early IPC call.
  static size_t CDECL StringLength(const wchar_t* wcs) {
    const wchar_t* eos = wcs;
    while (*eos++)
      ;
    return static_cast<size_t>(eos - wcs - 1);
  }

  const wchar_t* t_;
};

// Specialization for non-const strings. We just reuse the implementation of the
// const string specialization.
template <>
class CopyHelper<wchar_t*> : public CopyHelper<const wchar_t*> {
 public:
  typedef CopyHelper<const wchar_t*> Base;
  CopyHelper(wchar_t* t) : Base(t) {}

  const void* GetStart() const { return Base::GetStart(); }

  bool Update(void* buffer) { return Base::Update(buffer); }

  uint32_t GetSize() const { return Base::GetSize(); }

  bool IsInOut() { return Base::IsInOut(); }

  ArgType GetType() { return Base::GetType(); }
};

// Specialization for wchar_t arrays strings. We just reuse the implementation
// of the const string specialization.
template <size_t n>
class CopyHelper<const wchar_t[n]> : public CopyHelper<const wchar_t*> {
 public:
  typedef const wchar_t array[n];
  typedef CopyHelper<const wchar_t*> Base;
  CopyHelper(array t) : Base(t) {}

  const void* GetStart() const { return Base::GetStart(); }

  bool Update(void* buffer) { return Base::Update(buffer); }

  uint32_t GetSize() const { return Base::GetSize(); }

  bool IsInOut() { return Base::IsInOut(); }

  ArgType GetType() { return Base::GetType(); }
};

// Generic encapsulation class containing a pointer to a buffer and the
// size of the buffer. It is used by the IPC to be able to pass in/out
// parameters.
class InOutCountedBuffer : public CountedBuffer {
 public:
  InOutCountedBuffer(void* buffer, uint32_t size)
      : CountedBuffer(buffer, size) {}
};

// This copy helper template specialization catches the cases where the
// parameter is a an input/output buffer.
template <>
class CopyHelper<InOutCountedBuffer> {
 public:
  CopyHelper(const InOutCountedBuffer t) : t_(t) {}

  // Returns the pointer to the start of the string.
  const void* GetStart() const { return t_.Buffer(); }

  // Updates the buffer with the value from the new buffer in parameter.
  bool Update(void* buffer) {
    // We are touching user memory, this has to be done from inside a try
    // except.
    __try {
      memcpy_wrapper(t_.Buffer(), buffer, t_.Size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return false;
    }
    return true;
  }

  // Returns the size of the string in bytes. We define a nullptr string to
  // be of zero length.
  uint32_t GetSize() const { return t_.Size(); }

  // Returns true if the current type is used as an In or InOut parameter.
  bool IsInOut() { return true; }

  ArgType GetType() { return INOUTPTR_TYPE; }

 private:
  const InOutCountedBuffer t_;
};

// The following two macros make it less error prone the generation
// of CrossCall functions with ever more input parameters.

#define XCALL_GEN_PARAMS_OBJ(num, params)                      \
  typedef ActualCallParams<num, kIPCChannelSize> ActualParams; \
  void* raw_mem = ipc_provider.GetBuffer();                    \
  if (!raw_mem)                                                \
    return SBOX_ERROR_NO_SPACE;                                \
  ActualParams* params = new (raw_mem) ActualParams(tag);

#define XCALL_GEN_COPY_PARAM(num, params)                                  \
  static_assert(kMaxIpcParams >= num, "too many parameters");              \
  CopyHelper<Par##num> ch##num(p##num);                                    \
  if (!params->CopyParamIn(num - 1, ch##num.GetStart(), ch##num.GetSize(), \
                           ch##num.IsInOut(), ch##num.GetType()))          \
    return SBOX_ERROR_NO_SPACE;

#define XCALL_GEN_UPDATE_PARAM(num, params)            \
  if (!ch##num.Update(params->GetParamPtr(num - 1))) { \
    ipc_provider.FreeBuffer(raw_mem);                  \
    return SBOX_ERROR_BAD_PARAMS;                      \
  }

#define XCALL_GEN_FREE_CHANNEL() ipc_provider.FreeBuffer(raw_mem);

// CrossCall template with one input parameter
template <typename IPCProvider, typename Par1>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(1, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }

  return result;
}

// CrossCall template with two input parameters.
template <typename IPCProvider, typename Par1, typename Par2>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(2, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}

// CrossCall template with three input parameters.
template <typename IPCProvider, typename Par1, typename Par2, typename Par3>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     const Par3& p3,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(3, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);
  XCALL_GEN_COPY_PARAM(3, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_UPDATE_PARAM(3, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}

// CrossCall template with four input parameters.
template <typename IPCProvider,
          typename Par1,
          typename Par2,
          typename Par3,
          typename Par4>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     const Par3& p3,
                     const Par4& p4,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(4, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);
  XCALL_GEN_COPY_PARAM(3, call_params);
  XCALL_GEN_COPY_PARAM(4, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_UPDATE_PARAM(3, call_params);
    XCALL_GEN_UPDATE_PARAM(4, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}

// CrossCall template with five input parameters.
template <typename IPCProvider,
          typename Par1,
          typename Par2,
          typename Par3,
          typename Par4,
          typename Par5>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     const Par3& p3,
                     const Par4& p4,
                     const Par5& p5,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(5, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);
  XCALL_GEN_COPY_PARAM(3, call_params);
  XCALL_GEN_COPY_PARAM(4, call_params);
  XCALL_GEN_COPY_PARAM(5, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_UPDATE_PARAM(3, call_params);
    XCALL_GEN_UPDATE_PARAM(4, call_params);
    XCALL_GEN_UPDATE_PARAM(5, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}

// CrossCall template with six input parameters.
template <typename IPCProvider,
          typename Par1,
          typename Par2,
          typename Par3,
          typename Par4,
          typename Par5,
          typename Par6>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     const Par3& p3,
                     const Par4& p4,
                     const Par5& p5,
                     const Par6& p6,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(6, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);
  XCALL_GEN_COPY_PARAM(3, call_params);
  XCALL_GEN_COPY_PARAM(4, call_params);
  XCALL_GEN_COPY_PARAM(5, call_params);
  XCALL_GEN_COPY_PARAM(6, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_UPDATE_PARAM(3, call_params);
    XCALL_GEN_UPDATE_PARAM(4, call_params);
    XCALL_GEN_UPDATE_PARAM(5, call_params);
    XCALL_GEN_UPDATE_PARAM(6, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}

// CrossCall template with seven input parameters.
template <typename IPCProvider,
          typename Par1,
          typename Par2,
          typename Par3,
          typename Par4,
          typename Par5,
          typename Par6,
          typename Par7>
ResultCode CrossCall(IPCProvider& ipc_provider,
                     IpcTag tag,
                     const Par1& p1,
                     const Par2& p2,
                     const Par3& p3,
                     const Par4& p4,
                     const Par5& p5,
                     const Par6& p6,
                     const Par7& p7,
                     CrossCallReturn* answer) {
  XCALL_GEN_PARAMS_OBJ(7, call_params);
  XCALL_GEN_COPY_PARAM(1, call_params);
  XCALL_GEN_COPY_PARAM(2, call_params);
  XCALL_GEN_COPY_PARAM(3, call_params);
  XCALL_GEN_COPY_PARAM(4, call_params);
  XCALL_GEN_COPY_PARAM(5, call_params);
  XCALL_GEN_COPY_PARAM(6, call_params);
  XCALL_GEN_COPY_PARAM(7, call_params);

  ResultCode result = ipc_provider.DoCall(call_params, answer);

  if (SBOX_ERROR_CHANNEL_ERROR != result) {
    XCALL_GEN_UPDATE_PARAM(1, call_params);
    XCALL_GEN_UPDATE_PARAM(2, call_params);
    XCALL_GEN_UPDATE_PARAM(3, call_params);
    XCALL_GEN_UPDATE_PARAM(4, call_params);
    XCALL_GEN_UPDATE_PARAM(5, call_params);
    XCALL_GEN_UPDATE_PARAM(6, call_params);
    XCALL_GEN_UPDATE_PARAM(7, call_params);
    XCALL_GEN_FREE_CHANNEL();
  }
  return result;
}
}  // namespace sandbox

#endif  // SANDBOX_SRC_CROSSCALL_CLIENT_H__
