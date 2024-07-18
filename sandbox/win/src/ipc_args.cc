// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/win/src/ipc_args.h"

#include <stddef.h>

#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/crosscall_server.h"

namespace sandbox {

// Releases memory allocated for IPC arguments, if needed.
void ReleaseArgs(const IPCParams* ipc_params, void* args[kMaxIpcParams]) {
  for (size_t i = 0; i < kMaxIpcParams; i++) {
    switch (ipc_params->args[i]) {
      case WCHAR_TYPE: {
        delete reinterpret_cast<std::wstring*>(args[i]);
        args[i] = nullptr;
        break;
      }
      case INOUTPTR_TYPE: {
        delete reinterpret_cast<CountedBuffer*>(args[i]);
        args[i] = nullptr;
        break;
      }
      default:
        break;
    }
  }
}

// Fills up the list of arguments (args and ipc_params) for an IPC call.
bool GetArgs(CrossCallParamsEx* params,
             IPCParams* ipc_params,
             void* args[kMaxIpcParams]) {
  if (kMaxIpcParams < params->GetParamsCount())
    return false;

  for (uint32_t i = 0; i < params->GetParamsCount(); i++) {
    uint32_t size;
    ArgType type;
    args[i] = params->GetRawParameter(i, &size, &type);
    if (args[i]) {
      ipc_params->args[i] = type;
      switch (type) {
        case WCHAR_TYPE: {
          std::unique_ptr<std::wstring> data(new std::wstring);
          if (!params->GetParameterStr(i, data.get())) {
            args[i] = 0;
            ReleaseArgs(ipc_params, args);
            return false;
          }
          args[i] = data.release();
          break;
        }
        case UINT32_TYPE: {
          uint32_t data;
          if (!params->GetParameter32(i, &data)) {
            ReleaseArgs(ipc_params, args);
            return false;
          }
          IPCInt ipc_int(data);
          args[i] = ipc_int.AsVoidPtr();
          break;
        }
        case VOIDPTR_TYPE: {
          void* data;
          if (!params->GetParameterVoidPtr(i, &data)) {
            ReleaseArgs(ipc_params, args);
            return false;
          }
          args[i] = data;
          break;
        }
        case INOUTPTR_TYPE: {
          if (!args[i]) {
            ReleaseArgs(ipc_params, args);
            return false;
          }
          CountedBuffer* buffer = new CountedBuffer(args[i], size);
          args[i] = buffer;
          break;
        }
        default:
          break;
      }
    }
  }
  return true;
}

}  // namespace sandbox
