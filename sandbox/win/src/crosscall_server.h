// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_CROSSCALL_SERVER_H_
#define SANDBOX_WIN_SRC_CROSSCALL_SERVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/ipc_tags.h"

// This is the IPC server interface for CrossCall: The  IPC for the Sandbox
// On the server, CrossCall needs two things:
// 1) threads: Or better said, someone to provide them, that is what the
//             ThreadPool is for. These thread(s) are
//             the ones that will actually execute the IPC data retrieval.
//
// 2) a dispatcher: This interface represents the way to route and process
//                  an  IPC call given the  IPC tag.
//
// The other class included here CrossCallParamsEx is the server side version
// of the CrossCallParams class of /sandbox/crosscall_params.h The difference
// is that the sever version is paranoid about the correctness of the IPC
// message and will do all sorts of verifications.
//
// A general diagram of the interaction is as follows:
//
//                                 ------------
//                                 |          |
//  ThreadPool<-------(1)Register--|  IPC     |
//      |                          | Implemen |
//      |                          | -tation  |
//     (2)                         |          |  OnMessage
//     IPC fired --callback ------>|          |--(3)---> Dispatcher
//                                 |          |
//                                 ------------
//
//  The  IPC implementation sits as a middleman between the handling of the
//  specifics of scheduling a thread to service the  IPC and the multiple
//  entities that can potentially serve each particular IPC.
namespace sandbox {

class InterceptionManager;

// Models the server-side of the original input parameters.
// Provides IPC buffer validation and it is capable of reading the parameters
// out of the IPC buffer.
class CrossCallParamsEx : public CrossCallParams {
 public:
  // Factory constructor. Pass an IPCbuffer (and buffer size) that contains a
  // pending IPCcall. This constructor will:
  // 1) validate the IPC buffer. returns nullptr is the IPCbuffer is malformed.
  // 2) make a copy of the IPCbuffer (parameter capture)
  static CrossCallParamsEx* CreateFromBuffer(void* buffer_base,
                                             uint32_t buffer_size,
                                             uint32_t* output_size);

  CrossCallParamsEx(const CrossCallParamsEx&) = delete;
  CrossCallParamsEx& operator=(const CrossCallParamsEx&) = delete;

  // Provides IPCinput parameter raw access:
  // index : the parameter to read; 0 is the first parameter
  // returns nullptr if the parameter is non-existent. If it exists it also
  // returns the size in *size
  void* GetRawParameter(uint32_t index, uint32_t* size, ArgType* type);

  // Gets a parameter that is four bytes in size.
  // Returns false if the parameter does not exist or is not 32 bits wide.
  bool GetParameter32(uint32_t index, uint32_t* param);

  // Gets a parameter that is void pointer in size.
  // Returns false if the parameter does not exist or is not void pointer sized.
  bool GetParameterVoidPtr(uint32_t index, void** param);

  // Gets a parameter that is a string. Returns false if the parameter does not
  // exist.
  bool GetParameterStr(uint32_t index, std::wstring* string);

  // Gets a parameter that is an in/out buffer. Returns false is the parameter
  // does not exist or if the size of the actual parameter is not equal to the
  // expected size.
  bool GetParameterPtr(uint32_t index, uint32_t expected_size, void** pointer);

  // Frees the memory associated with the IPC parameters.
  static void operator delete(void* raw_memory) throw();

 private:
  // Only the factory method CreateFromBuffer can construct these objects.
  CrossCallParamsEx();

  ParamInfo param_info_[1];
};

// Simple helper function that sets the members of CrossCallReturn
// to the proper state to signal a basic error.
void SetCallError(ResultCode error, CrossCallReturn* call_return);

// Sets the internal status of call_return to signify the that IPC call
// completed successfully.
void SetCallSuccess(CrossCallReturn* call_return);

// Represents the client process that initiated the IPC which boils down to the
// process handle and the job object handle that contains the client process.
struct ClientInfo {
  HANDLE process;
  DWORD process_id;
};

// All IPC-related information to be passed to the IPC handler.
struct IPCInfo {
  IpcTag ipc_tag;
  raw_ptr<const ClientInfo> client_info;
  CrossCallReturn return_info;
};

// This structure identifies IPC signatures.
struct IPCParams {
  IpcTag ipc_tag;
  ArgType args[kMaxIpcParams];

  bool Matches(IPCParams* other) const {
    return !memcmp(this, other, sizeof(*other));
  }
};

// Models an entity that can process an IPC message or it can route to another
// one that could handle it. When an IPC arrives the IPC implementation will:
// 1) call OnMessageReady() with the tag of the pending IPC. If the dispatcher
//    returns nullptr it means that it cannot handle this IPC but if it returns
//    non-null, it must be the pointer to a dispatcher that can handle it.
// 2) When the  IPC finally obtains a valid Dispatcher the IPC
//    implementation creates a CrossCallParamsEx from the raw IPC buffer.
// 3) It calls the returned callback, with the IPC info and arguments.
class [[clang::lto_visibility_public]] Dispatcher {
 public:
  // Called from the  IPC implementation to handle a specific IPC message.
  typedef bool (Dispatcher::*CallbackGeneric)();
  typedef bool (Dispatcher::*Callback0)(IPCInfo* ipc);
  typedef bool (Dispatcher::*Callback1)(IPCInfo* ipc, void* p1);
  typedef bool (Dispatcher::*Callback2)(IPCInfo* ipc, void* p1, void* p2);
  typedef bool (Dispatcher::*Callback3)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3);
  typedef bool (Dispatcher::*Callback4)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4);
  typedef bool (Dispatcher::*Callback5)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4,
                                        void* p5);
  typedef bool (Dispatcher::*Callback6)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4,
                                        void* p5,
                                        void* p6);
  typedef bool (Dispatcher::*Callback7)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4,
                                        void* p5,
                                        void* p6,
                                        void* p7);
  typedef bool (Dispatcher::*Callback8)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4,
                                        void* p5,
                                        void* p6,
                                        void* p7,
                                        void* p8);
  typedef bool (Dispatcher::*Callback9)(IPCInfo* ipc,
                                        void* p1,
                                        void* p2,
                                        void* p3,
                                        void* p4,
                                        void* p5,
                                        void* p6,
                                        void* p7,
                                        void* p8,
                                        void* p9);

  // Called from the  IPC implementation when an  IPC message is ready override
  // on a derived class to handle a set of  IPC messages. Return nullptr if your
  // subclass does not handle the message or return the pointer to the subclass
  // that can handle it.
  virtual Dispatcher* OnMessageReady(IPCParams* ipc, CallbackGeneric* callback);

  // Called when a target proces is created, to setup the interceptions related
  // with the given service (IPC).
  virtual bool SetupService(InterceptionManager* manager, IpcTag service) = 0;

  Dispatcher();
  virtual ~Dispatcher();

 protected:
  // Structure that defines an IPC Call with all the parameters and the handler.
  struct IPCCall {
    IPCParams params;
    CallbackGeneric callback;
  };

  // List of IPC Calls supported by the class.
  std::vector<IPCCall> ipc_calls_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_CROSSCALL_SERVER_H_
