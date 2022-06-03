// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/crosscall_server.h"

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "sandbox/win/src/crosscall_client.h"
#include "sandbox/win/src/crosscall_params.h"

// See comment in atomicops.h. This is needed any time windows.h is included
// after atomicops.h.
#undef MemoryBarrier

// This code performs the ipc message validation. Potential security flaws
// on the ipc are likelier to be found in this code than in the rest of
// the ipc code.

namespace {

// The buffer for a message must match the max channel size.
const size_t kMaxBufferSize = sandbox::kIPCChannelSize;

}  // namespace

namespace sandbox {

// Returns the actual size for the parameters in an IPC buffer. Returns
// zero if the |param_count| is zero or too big.
uint32_t GetActualBufferSize(uint32_t param_count, void* buffer_base) {
  // The template types are used to calculate the maximum expected size.
  typedef ActualCallParams<1, kMaxBufferSize> ActualCP1;
  typedef ActualCallParams<2, kMaxBufferSize> ActualCP2;
  typedef ActualCallParams<3, kMaxBufferSize> ActualCP3;
  typedef ActualCallParams<4, kMaxBufferSize> ActualCP4;
  typedef ActualCallParams<5, kMaxBufferSize> ActualCP5;
  typedef ActualCallParams<6, kMaxBufferSize> ActualCP6;
  typedef ActualCallParams<7, kMaxBufferSize> ActualCP7;
  typedef ActualCallParams<8, kMaxBufferSize> ActualCP8;
  typedef ActualCallParams<9, kMaxBufferSize> ActualCP9;

  // Retrieve the actual size and the maximum size of the params buffer.
  switch (param_count) {
    case 0:
      return 0;
    case 1:
      return reinterpret_cast<ActualCP1*>(buffer_base)->GetSize();
    case 2:
      return reinterpret_cast<ActualCP2*>(buffer_base)->GetSize();
    case 3:
      return reinterpret_cast<ActualCP3*>(buffer_base)->GetSize();
    case 4:
      return reinterpret_cast<ActualCP4*>(buffer_base)->GetSize();
    case 5:
      return reinterpret_cast<ActualCP5*>(buffer_base)->GetSize();
    case 6:
      return reinterpret_cast<ActualCP6*>(buffer_base)->GetSize();
    case 7:
      return reinterpret_cast<ActualCP7*>(buffer_base)->GetSize();
    case 8:
      return reinterpret_cast<ActualCP8*>(buffer_base)->GetSize();
    case 9:
      return reinterpret_cast<ActualCP9*>(buffer_base)->GetSize();
    default:
      return 0;
  }
}

// Verifies that the declared sizes of an IPC buffer are within range.
bool IsSizeWithinRange(uint32_t buffer_size,
                       uint32_t min_declared_size,
                       uint32_t declared_size) {
  if ((buffer_size < min_declared_size) ||
      (sizeof(CrossCallParamsEx) > min_declared_size)) {
    // Minimal computed size bigger than existing buffer or param_count
    // integer overflow.
    return false;
  }

  if ((declared_size > buffer_size) || (declared_size < min_declared_size)) {
    // Declared size is bigger than buffer or smaller than computed size
    // or param_count is equal to 0 or bigger than 9.
    return false;
  }

  return true;
}

CrossCallParamsEx::CrossCallParamsEx() : CrossCallParams(IpcTag::UNUSED, 0) {}

// We override the delete operator because the object's backing memory
// is hand allocated in CreateFromBuffer. We don't override the new operator
// because the constructors are private so there is no way to mismatch
// new & delete.
void CrossCallParamsEx::operator delete(void* raw_memory) throw() {
  if (!raw_memory) {
    // C++ standard allows 'delete 0' behavior.
    return;
  }
  delete[] reinterpret_cast<char*>(raw_memory);
}

// This function uses a SEH try block so cannot use C++ objects that
// have destructors or else you get Compiler Error C2712. So no DCHECKs
// inside this function.
CrossCallParamsEx* CrossCallParamsEx::CreateFromBuffer(void* buffer_base,
                                                       uint32_t buffer_size,
                                                       uint32_t* output_size) {
  // IMPORTANT: Everything inside buffer_base and derived from it such
  // as param_count and declared_size is untrusted.
  if (!buffer_base)
    return nullptr;
  if (buffer_size < sizeof(CrossCallParams))
    return nullptr;
  if (buffer_size > kMaxBufferSize)
    return nullptr;

  char* backing_mem = nullptr;
  uint32_t param_count = 0;
  uint32_t declared_size;
  uint32_t min_declared_size;
  CrossCallParamsEx* copied_params = nullptr;

  // Touching the untrusted buffer is done under a SEH try block. This
  // will catch memory access violations so we don't crash.
  __try {
    CrossCallParams* call_params =
        reinterpret_cast<CrossCallParams*>(buffer_base);

    // Check against the minimum size given the number of stated params
    // if too small we bail out.
    param_count = call_params->GetParamsCount();
    min_declared_size =
        sizeof(CrossCallParams) + ((param_count + 1) * sizeof(ParamInfo));

    // Initial check for the buffer being big enough to determine the actual
    // buffer size.
    if (buffer_size < min_declared_size)
      return nullptr;

    // Retrieve the declared size which if it fails returns 0.
    declared_size = GetActualBufferSize(param_count, buffer_base);

    if (!IsSizeWithinRange(buffer_size, min_declared_size, declared_size))
      return nullptr;

    // Now we copy the actual amount of the message.
    *output_size = declared_size;
    backing_mem = new char[declared_size];
    copied_params = reinterpret_cast<CrossCallParamsEx*>(backing_mem);
    memcpy(backing_mem, call_params, declared_size);

    // Avoid compiler optimizations across this point. Any value stored in
    // memory should be stored for real, and values previously read from memory
    // should be actually read.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    min_declared_size =
        sizeof(CrossCallParams) + ((param_count + 1) * sizeof(ParamInfo));

    // Check that the copied buffer is still valid.
    if (copied_params->GetParamsCount() != param_count ||
        GetActualBufferSize(param_count, backing_mem) != declared_size ||
        !IsSizeWithinRange(buffer_size, min_declared_size, declared_size)) {
      delete[] backing_mem;
      return nullptr;
    }

  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // In case of a windows exception we know it occurred while touching the
    // untrusted buffer so we bail out as is.
    delete[] backing_mem;
    return nullptr;
  }

  // Here and below we're making use of uintptr_t to have well-defined integer
  // overflow when doing pointer arithmetic.
  auto backing_mem_ptr = reinterpret_cast<uintptr_t>(backing_mem);
  auto last_byte = reinterpret_cast<uintptr_t>(&backing_mem[declared_size]);
  auto first_byte =
      reinterpret_cast<uintptr_t>(&backing_mem[min_declared_size]);

  // Verify here that all and each parameters make sense. This is done in the
  // local copy.
  for (uint32_t ix = 0; ix != param_count; ++ix) {
    uint32_t size = 0;
    ArgType type;
    auto address = reinterpret_cast<uintptr_t>(
        copied_params->GetRawParameter(ix, &size, &type));
    if ((!address) ||                                     // No null params.
        (INVALID_TYPE >= type) || (LAST_TYPE <= type) ||  // Unknown type.
        (address < backing_mem_ptr) ||     // Start cannot point before buffer.
        (address < first_byte) ||          // Start cannot point too low.
        (address > last_byte) ||           // Start cannot point past buffer.
        ((address + size) < address) ||    // Invalid size.
        ((address + size) > last_byte)) {  // End cannot point past buffer.
      // Malformed.
      delete[] backing_mem;
      return nullptr;
    }
  }
  // The parameter buffer looks good.
  return copied_params;
}

// Accessors to the parameters in the raw buffer.
void* CrossCallParamsEx::GetRawParameter(uint32_t index,
                                         uint32_t* size,
                                         ArgType* type) {
  if (index >= GetParamsCount())
    return nullptr;
  // The size is always computed from the parameter minus the next
  // parameter, this works because the message has an extra parameter slot
  *size = param_info_[index].size_;
  *type = param_info_[index].type_;

  return param_info_[index].offset_ + reinterpret_cast<char*>(this);
}

// Covers common case for 32 bit integers.
bool CrossCallParamsEx::GetParameter32(uint32_t index, uint32_t* param) {
  uint32_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);
  if (!start || (4 != size) || (UINT32_TYPE != type))
    return false;
  // Copy the 4 bytes.
  *(reinterpret_cast<uint32_t*>(param)) = *(reinterpret_cast<uint32_t*>(start));
  return true;
}

bool CrossCallParamsEx::GetParameterVoidPtr(uint32_t index, void** param) {
  uint32_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);
  if (!start || (sizeof(void*) != size) || (VOIDPTR_TYPE != type))
    return false;
  *param = *(reinterpret_cast<void**>(start));
  return true;
}

// Covers the common case of reading a string. Note that the string is not
// scanned for invalid characters.
bool CrossCallParamsEx::GetParameterStr(uint32_t index, std::wstring* string) {
  DCHECK(string->empty());
  uint32_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);
  if (WCHAR_TYPE != type)
    return false;

  // Check if this is an empty string.
  if (size == 0) {
    *string = std::wstring();
    return true;
  }

  if (!start || ((size % sizeof(wchar_t)) != 0))
    return false;

  string->assign(reinterpret_cast<const wchar_t*>(start),
                 size / sizeof(wchar_t));
  return true;
}

bool CrossCallParamsEx::GetParameterPtr(uint32_t index,
                                        uint32_t expected_size,
                                        void** pointer) {
  uint32_t size = 0;
  ArgType type;
  void* start = GetRawParameter(index, &size, &type);

  if ((size != expected_size) || (INOUTPTR_TYPE != type))
    return false;

  if (!start)
    return false;

  *pointer = start;
  return true;
}

void SetCallError(ResultCode error, CrossCallReturn* call_return) {
  call_return->call_outcome = error;
  call_return->extended_count = 0;
}

void SetCallSuccess(CrossCallReturn* call_return) {
  call_return->call_outcome = SBOX_ALL_OK;
}

Dispatcher* Dispatcher::OnMessageReady(IPCParams* ipc,
                                       CallbackGeneric* callback) {
  DCHECK(callback);
  std::vector<IPCCall>::iterator it = ipc_calls_.begin();
  for (; it != ipc_calls_.end(); ++it) {
    if (it->params.Matches(ipc)) {
      *callback = it->callback;
      return this;
    }
  }
  return nullptr;
}

Dispatcher::Dispatcher() {}

Dispatcher::~Dispatcher() {}

}  // namespace sandbox
