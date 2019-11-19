// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_CROSSCALL_PARAMS_H__
#define SANDBOX_SRC_CROSSCALL_PARAMS_H__

#if !defined(SANDBOX_FUZZ_TARGET)
#include <windows.h>

#include <lmaccess.h>
#else
#include "sandbox/win/fuzzer/fuzzer_types.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "sandbox/win/src/internal_types.h"
#if !defined(SANDBOX_FUZZ_TARGET)
#include "sandbox/win/src/sandbox_nt_types.h"
#endif
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox_types.h"

// This header is part of CrossCall: the sandbox inter-process communication.
// This header defines the basic types used both in the client IPC and in the
// server IPC code. CrossCallParams and ActualCallParams model the input
// parameters of an IPC call and CrossCallReturn models the output params and
// the return value.
//
// An IPC call is defined by its 'tag' which is a (uint32_t) unique identifier
// that is used to route the IPC call to the proper server. Every tag implies
// a complete call signature including the order and type of each parameter.
//
// Like most IPC systems. CrossCall is designed to take as inputs 'simple'
// types such as integers and strings. Classes, generic arrays or pointers to
// them are not supported.
//
// Another limitation of CrossCall is that the return value and output
// parameters can only be uint32_t integers. Returning complex structures or
// strings is not supported.

namespace sandbox {

// This is the list of all imported symbols from ntdll.dll.
SANDBOX_INTERCEPT NtExports g_nt;

namespace {

// Increases |value| until there is no need for padding given an int64_t
// alignment. Returns the increased value.
inline uint32_t Align(uint32_t value) {
  uint32_t alignment = sizeof(int64_t);
  return ((value + alignment - 1) / alignment) * alignment;
}

inline void* memcpy_wrapper(void* dest, const void* src, size_t count) {
  if (g_nt.memcpy)
    return g_nt.memcpy(dest, src, count);
  return memcpy(dest, src, count);
}

}  // namespace

// max number of extended return parameters. See CrossCallReturn
const size_t kExtendedReturnCount = 8;

// Union of multiple types to be used as extended results
// in the CrossCallReturn.
union MultiType {
  uint32_t unsigned_int;
  void* pointer;
  HANDLE handle;
  ULONG_PTR ulong_ptr;
};

// Maximum number of IPC parameters currently supported.
// To increase this value, we have to:
//  - Add another Callback typedef to Dispatcher.
//  - Add another case to the switch on SharedMemIPCServer::InvokeCallback.
//  - Add another case to the switch in GetActualAndMaxBufferSize
const int kMaxIpcParams = 9;

// Contains the information about a parameter in the ipc buffer.
struct ParamInfo {
  ArgType type_;
  uint32_t offset_;
  uint32_t size_;
};

// Models the return value and the return parameters of an IPC call
// currently limited to one status code and eight generic return values
// which cannot be pointers to other data. For x64 ports this structure
// might have to use other integer types.
struct CrossCallReturn {
  // the IPC tag. It should match the original IPC tag.
  uint32_t tag;
  // The result of the IPC operation itself.
  ResultCode call_outcome;
  // the result of the IPC call as executed in the server. The interpretation
  // of this value depends on the specific service.
  union {
    NTSTATUS nt_status;
    DWORD win32_result;
  };
  // Number of extended return values.
  uint32_t extended_count;
  // for calls that should return a windows handle. It is found here.
  HANDLE handle;
  // The array of extended values.
  MultiType extended[kExtendedReturnCount];
};

// CrossCallParams base class that models the input params all packed in a
// single compact memory blob. The representation can vary but in general a
// given child of this class is meant to represent all input parameters
// necessary to make a IPC call.
//
// This class cannot have virtual members because its assumed the IPC
// parameters start from the 'this' pointer to the end, which is defined by
// one of the subclasses
//
// Objects of this class cannot be constructed directly. Only derived
// classes have the proper knowledge to construct it.
class CrossCallParams {
 public:
  // Returns the tag (ipc unique id) associated with this IPC.
  IpcTag GetTag() const { return tag_; }

  // Returns the beggining of the buffer where the IPC params can be stored.
  // prior to an IPC call
  const void* GetBuffer() const { return this; }

  // Returns how many parameter this IPC call should have.
  uint32_t GetParamsCount() const { return params_count_; }

  // Returns a pointer to the CrossCallReturn structure.
  CrossCallReturn* GetCallReturn() { return &call_return; }

  // Returns true if this call contains InOut parameters.
  bool IsInOut() const { return (1 == is_in_out_); }

  // Tells the CrossCall object if it contains InOut parameters.
  void SetIsInOut(bool value) {
    if (value)
      is_in_out_ = 1;
    else
      is_in_out_ = 0;
  }

 protected:
  // constructs the IPC call params. Called only from the derived classes
  CrossCallParams(IpcTag tag, uint32_t params_count)
      : tag_(tag), is_in_out_(0), params_count_(params_count) {}

 private:
  IpcTag tag_;
  uint32_t is_in_out_;
  CrossCallReturn call_return;
  const uint32_t params_count_;
  DISALLOW_COPY_AND_ASSIGN(CrossCallParams);
};

// ActualCallParams models an specific IPC call parameters with respect to the
// storage allocation that the packed parameters should need.
// NUMBER_PARAMS: the number of parameters, valid from 1 to N
// BLOCK_SIZE: the total storage that the NUMBER_PARAMS parameters can take,
// typically the block size is defined by the channel size of the underlying
// ipc mechanism.
// In practice this class is used to levergage C++ capacity to properly
// calculate sizes and displacements given the possibility of the packed params
// blob to be complex.
//
// As is, this class assumes that the layout of the blob is as follows. Assume
// that NUMBER_PARAMS = 2 and a 32-bit build:
//
// [ tag                4 bytes]
// [ IsOnOut            4 bytes]
// [ call return       52 bytes]
// [ params count       4 bytes]
// [ parameter 0 type   4 bytes]
// [ parameter 0 offset 4 bytes] ---delta to ---\
// [ parameter 0 size   4 bytes]                |
// [ parameter 1 type   4 bytes]                |
// [ parameter 1 offset 4 bytes] ---------------|--\
// [ parameter 1 size   4 bytes]                |  |
// [ parameter 2 type   4 bytes]                |  |
// [ parameter 2 offset 4 bytes] ----------------------\
// [ parameter 2 size   4 bytes]                |  |   |
// |---------------------------|                |  |   |
// | value 0     (x bytes)     | <--------------/  |   |
// | value 1     (y bytes)     | <-----------------/   |
// |                           |                       |
// | end of buffer             | <---------------------/
// |---------------------------|
//
// Note that the actual number of params is NUMBER_PARAMS + 1
// so that the size of each actual param can be computed from the difference
// between one parameter and the next down. The offset of the last param
// points to the end of the buffer and the type and size are undefined.
//
template <size_t NUMBER_PARAMS, size_t BLOCK_SIZE>
class ActualCallParams : public CrossCallParams {
 public:
  // constructor. Pass the ipc unique tag as input
  explicit ActualCallParams(IpcTag tag) : CrossCallParams(tag, NUMBER_PARAMS) {
    param_info_[0].offset_ =
        static_cast<uint32_t>(parameters_ - reinterpret_cast<char*>(this));
  }

  // Testing-only constructor. Allows setting the |number_params| to a
  // wrong value.
  ActualCallParams(IpcTag tag, uint32_t number_params)
      : CrossCallParams(tag, number_params) {
    param_info_[0].offset_ =
        static_cast<uint32_t>(parameters_ - reinterpret_cast<char*>(this));
  }

  // Testing-only method. Allows setting the apparent size to a wrong value.
  // returns the previous size.
  uint32_t OverrideSize(uint32_t new_size) {
    uint32_t previous_size = param_info_[NUMBER_PARAMS].offset_;
    param_info_[NUMBER_PARAMS].offset_ = new_size;
    return previous_size;
  }

  // Copies each paramter into the internal buffer. For each you must supply:
  // index: 0 for the first param, 1 for the next an so on
  bool CopyParamIn(uint32_t index,
                   const void* parameter_address,
                   uint32_t size,
                   bool is_in_out,
                   ArgType type) {
    if (index >= NUMBER_PARAMS) {
      return false;
    }

    if (UINT32_MAX == size) {
      // Memory error while getting the size.
      return false;
    }

    if (size && !parameter_address) {
      return false;
    }

    if ((size > sizeof(*this)) ||
        (param_info_[index].offset_ > (sizeof(*this) - size))) {
      // It does not fit, abort copy.
      return false;
    }

    char* dest = reinterpret_cast<char*>(this) + param_info_[index].offset_;

    // We might be touching user memory, this has to be done from inside a try
    // except.
    __try {
      memcpy_wrapper(dest, parameter_address, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return false;
    }

    // Set the flag to tell the broker to update the buffer once the call is
    // made.
    if (is_in_out)
      SetIsInOut(true);

    param_info_[index + 1].offset_ = Align(param_info_[index].offset_ + size);
    param_info_[index].size_ = size;
    param_info_[index].type_ = type;
    return true;
  }

  // Returns a pointer to a parameter in the memory section.
  void* GetParamPtr(size_t index) {
    return reinterpret_cast<char*>(this) + param_info_[index].offset_;
  }

  // Returns the total size of the buffer. Only valid once all the paramters
  // have been copied in with CopyParamIn.
  uint32_t GetSize() const { return param_info_[NUMBER_PARAMS].offset_; }

 protected:
  ActualCallParams() : CrossCallParams(IpcTag::UNUSED, NUMBER_PARAMS) {}

 private:
  ParamInfo param_info_[NUMBER_PARAMS + 1];
  char parameters_[BLOCK_SIZE - sizeof(CrossCallParams) -
                   sizeof(ParamInfo) * (NUMBER_PARAMS + 1)];
  DISALLOW_COPY_AND_ASSIGN(ActualCallParams);
};

static_assert(sizeof(ActualCallParams<1, 1024>) == 1024, "bad size buffer");
static_assert(sizeof(ActualCallParams<2, 1024>) == 1024, "bad size buffer");
static_assert(sizeof(ActualCallParams<3, 1024>) == 1024, "bad size buffer");

}  // namespace sandbox

#endif  // SANDBOX_SRC_CROSSCALL_PARAMS_H__
