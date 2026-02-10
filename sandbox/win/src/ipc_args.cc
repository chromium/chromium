// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/ipc_args.h"

#include <base/notreached.h>
#include <stddef.h>

namespace sandbox {

IPCArgs::IPCArgs() : types_(), args_() {}
IPCArgs::~IPCArgs() = default;

// Fills up the list of arguments (args and ipc_params) for an IPC call.
bool IPCArgs::Initialize(CrossCallParamsEx* params) {
  if (args_.size() < params->GetParamsCount()) {
    return false;
  }
  for (uint32_t i = 0; i < params->GetParamsCount(); i++) {
    uint32_t size;
    ArgType type;
    void* arg = params->GetRawParameter(i, &size, &type);
    if (arg) {
      switch (type) {
        case WCHAR_TYPE: {
          std::wstring data;
          if (!params->GetParameterStr(i, &data)) {
            return false;
          }
          args_[i].emplace<std::wstring>(std::move(data));
          break;
        }
        case UINT32_TYPE: {
          uint32_t data;
          if (!params->GetParameter32(i, &data)) {
            return false;
          }
          args_[i].emplace<void*>(reinterpret_cast<void*>(data));
          break;
        }
        case VOIDPTR_TYPE: {
          void* data;
          if (!params->GetParameterVoidPtr(i, &data)) {
            return false;
          }
          args_[i].emplace<void*>(data);
          break;
        }
        case INOUTPTR_TYPE: {
          args_[i].emplace<CountedBuffer>(static_cast<uint8_t*>(arg), size);
          break;
        }
        default:
          return false;
      }
      types_[i] = type;
    }
  }
  return true;
}

void* IPCArgs::operator[](size_t index) LIFETIME_BOUND {
  switch (types_[index]) {
    case WCHAR_TYPE:
      DCHECK(std::holds_alternative<std::wstring>(args_[index]));
      return &std::get<std::wstring>(args_[index]);
    case UINT32_TYPE:
    case VOIDPTR_TYPE:
      DCHECK(std::holds_alternative<void*>(args_[index]));
      return std::get<void*>(args_[index]);
    case INOUTPTR_TYPE:
      DCHECK(std::holds_alternative<CountedBuffer>(args_[index]));
      return &std::get<CountedBuffer>(args_[index]);
    default:
      NOTREACHED();
  }
}

}  // namespace sandbox
