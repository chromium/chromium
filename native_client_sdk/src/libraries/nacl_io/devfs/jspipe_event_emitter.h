// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_DEVFS_JSPIPE_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_DEVFS_JSPIPE_EVENT_EMITTER_H_

#include <poll.h>
#include <ppapi/c/pp_var.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>

#include "nacl_io/fifo_char.h"
#include "nacl_io/pipe/pipe_event_emitter.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace nacl_io {

class PepperInterface;
class MessagingInterface;
class VarInterface;
class VarArrayInterface;
class VarArrayBufferInterface;
class VarDictionaryInterface;

class JSPipeEventEmitter;
typedef sdk_util::ScopedRef<JSPipeEventEmitter> ScopedJSPipeEventEmitter;

class JSPipeEventEmitter : public EventEmitter {
 public:
  JSPipeEventEmitter(PepperInterface* ppapi, size_t size);

  JSPipeEventEmitter(const JSPipeEventEmitter&) = delete;
  JSPipeEventEmitter& operator=(const JSPipeEventEmitter&) = delete;

  virtual void Destroy();

  Error Read_Locked(char* data, size_t len, int* out_bytes);
  Error Write_Locked(const char* data, size_t len, int* out_bytes);

  size_t GetOSpace() { return post_message_buffer_size_ - BytesOutstanding(); }
  size_t GetISpace() { return input_fifo_.WriteAvailable(); }
  Error SetName(const char* name);
  Error HandleJSMessage(PP_Var message);

 protected:
  size_t HandleJSWrite(const char* data, size_t len);
  void HandleJSAck(size_t byte_count);
  Error HandleJSWrite(PP_Var message);
  Error HandleJSAck(PP_Var message);
  void UpdateStatus_Locked();
  PP_Var VarFromCStr(const char* string);
  Error SendAckMessage(size_t byte_count);
  size_t BytesOutstanding() { return bytes_sent_ - bytes_acked_; }
  Error SendMessageToJS(PP_Var operation, PP_Var payload);
  Error SendWriteMessage(const void* buf, size_t count);
  int VarStrcmp(PP_Var a, PP_Var b);

 private:
  std::string name_;
  FIFOChar input_fifo_;

  // Number of bytes that to send via PostMessage before and ACK
  // is required.
  size_t post_message_buffer_size_;
  size_t bytes_sent_;
  size_t bytes_acked_;
  size_t bytes_read_;

  PepperInterface* ppapi_;
  MessagingInterface* messaging_iface_;
  VarInterface* var_iface_;
  VarArrayInterface* array_iface_;
  VarArrayBufferInterface* buffer_iface_;
  VarDictionaryInterface* dict_iface_;

  PP_Var pipe_name_var_;
  PP_Var pipe_key_;
  PP_Var operation_key_;
  PP_Var payload_key_;
  PP_Var write_var_;
  PP_Var ack_var_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_DEVFS_JSPIPE_EVENT_EMITTER_H_
