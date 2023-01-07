// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/devfs/jspipe_event_emitter.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <algorithm>

#define TRACE(format, ...) \
  LOG_TRACE("jspipe[%s]: " format, name_.c_str(), ##__VA_ARGS__)
#define ERROR(format, ...) \
  LOG_ERROR("jspipe[%s]: " format, name_.c_str(), ##__VA_ARGS__)

#include "nacl_io/log.h"
#include "nacl_io/osinttypes.h"
#include "nacl_io/pepper_interface.h"

namespace {
const size_t kMaxPostMessageSize = 64 * 1024;
const char* kDictKeyPipe = "pipe";
const char* kDictKeyOperation = "operation";
const char* kDictKeyPayload = "payload";
const char* kOperationNameAck = "ack";
const char* kOperationNameWrite = "write";
}

namespace nacl_io {

JSPipeEventEmitter::JSPipeEventEmitter(PepperInterface* ppapi, size_t size)
    : input_fifo_(size),
      post_message_buffer_size_(size),
      bytes_sent_(0),
      bytes_acked_(0),
      bytes_read_(0),
      ppapi_(ppapi),
      messaging_iface_(NULL),
      var_iface_(NULL),
      array_iface_(NULL),
      buffer_iface_(NULL),
      dict_iface_(NULL),
      pipe_name_var_(PP_MakeUndefined()),
      pipe_key_(PP_MakeUndefined()),
      operation_key_(PP_MakeUndefined()),
      payload_key_(PP_MakeUndefined()),
      write_var_(PP_MakeUndefined()),
      ack_var_(PP_MakeUndefined()) {
  UpdateStatus_Locked();
  if (ppapi == NULL) {
    TRACE("missing PPAPI provider");
    return;
  }
  messaging_iface_ = ppapi->GetMessagingInterface();
  var_iface_ = ppapi->GetVarInterface();
  array_iface_ = ppapi->GetVarArrayInterface();
  buffer_iface_ = ppapi->GetVarArrayBufferInterface();
  dict_iface_ = ppapi->GetVarDictionaryInterface();

  if (var_iface_ == NULL)
    return;

  pipe_key_ = VarFromCStr(kDictKeyPipe);
  operation_key_ = VarFromCStr(kDictKeyOperation);
  payload_key_ = VarFromCStr(kDictKeyPayload);
  write_var_ = VarFromCStr(kOperationNameWrite);
  ack_var_ = VarFromCStr(kOperationNameAck);
}

void JSPipeEventEmitter::Destroy() {
  if (var_iface_ == NULL)
    return;
  var_iface_->Release(pipe_name_var_);
  var_iface_->Release(pipe_key_);
  var_iface_->Release(operation_key_);
  var_iface_->Release(payload_key_);
  var_iface_->Release(write_var_);
  var_iface_->Release(ack_var_);
}

PP_Var JSPipeEventEmitter::VarFromCStr(const char* string) {
  assert(var_iface_);
  return var_iface_->VarFromUtf8(string, strlen(string));
}

void JSPipeEventEmitter::UpdateStatus_Locked() {
  uint32_t status = 0;
  if (!input_fifo_.IsEmpty())
    status |= POLLIN;

  if (GetOSpace() > 0)
    status |= POLLOUT;

  ClearEvents_Locked(~status);
  RaiseEvents_Locked(status);
}

Error JSPipeEventEmitter::Read_Locked(char* data, size_t len, int* out_bytes) {
  *out_bytes = input_fifo_.Read(data, len);
  if (*out_bytes > 0) {
    bytes_read_ += *out_bytes;
    Error err = SendAckMessage(bytes_read_);
    if (err != 0)
      ERROR("Sending ACK failed: %d\n", err.error);
  }

  UpdateStatus_Locked();
  return 0;
}

Error JSPipeEventEmitter::SendWriteMessage(const void* buf, size_t count) {
  TRACE("SendWriteMessage [%" PRIuS "] total=%" PRIuS, count, bytes_sent_);
  if (!var_iface_ || !buffer_iface_) {
    ERROR("Got NULL interface(s): %s%s",
          var_iface_ ? "" : "Var ",
          buffer_iface_ ? "" : "ArrayBuffer");
    return EIO;
  }

  // Copy payload data in a new ArrayBuffer
  PP_Var buffer = buffer_iface_->Create(count);
  memcpy(buffer_iface_->Map(buffer), buf, count);
  buffer_iface_->Unmap(buffer);

  Error rtn = SendMessageToJS(write_var_, buffer);
  var_iface_->Release(buffer);
  return rtn;
}

Error JSPipeEventEmitter::SetName(const char* name) {
  if (var_iface_ == NULL) {
    // No error here: many of the tests trigger this message.
    LOG_TRACE("Got NULL interface: Var");
    return EIO;
  }

  // name can only be set once
  if (!name_.empty()) {
    LOG_ERROR("Attempting to set name more than once.");
    return EIO;
  }

  // new name must not be empty
  if (!name || strlen(name) == 0) {
    LOG_ERROR("Empty name is invalid.");
    return EIO;
  }

  TRACE("set name: %s", name);
  name_ = name;
  pipe_name_var_ = VarFromCStr(name);
  return 0;
}

Error JSPipeEventEmitter::SendMessageToJS(PP_Var operation, PP_Var payload) {
  if (!ppapi_) {
    LOG_ERROR("ppapi_ is NULL.");
    return EIO;
  }

  if (!messaging_iface_ || !var_iface_ || !dict_iface_) {
    LOG_ERROR("Got NULL interface(s): %s%s%s",
              messaging_iface_ ? "" : "Messaging ",
              dict_iface_ ? "" : "Dictionary ",
              var_iface_ ? "" : "Var");
    return EIO;
  }

  // Create dict object which will be sent to JavaScript.
  PP_Var dict = dict_iface_->Create();

  // Set three keys in the dictionary: 'pipe', 'operation', and 'payload'
  dict_iface_->Set(dict, pipe_key_, pipe_name_var_);
  dict_iface_->Set(dict, operation_key_, operation);
  dict_iface_->Set(dict, payload_key_, payload);

  // Send the dict via PostMessage
  messaging_iface_->PostMessage(ppapi_->GetInstance(), dict);

  // Release the dict
  var_iface_->Release(dict);
  return 0;
}

Error JSPipeEventEmitter::SendAckMessage(size_t byte_count) {
  TRACE("SendAckMessage %" PRIuS, byte_count);
  PP_Var payload;
  payload.type = PP_VARTYPE_INT32;
  payload.value.as_int = (int32_t)byte_count;

  return SendMessageToJS(ack_var_, payload);
}

size_t JSPipeEventEmitter::HandleJSWrite(const char* data, size_t len) {
  AUTO_LOCK(GetLock());
  size_t out_len = input_fifo_.Write(data, len);
  UpdateStatus_Locked();
  return out_len;
}

void JSPipeEventEmitter::HandleJSAck(size_t byte_count) {
  AUTO_LOCK(GetLock());
  if (byte_count > bytes_sent_) {
    ERROR("Unexpected byte count: %" PRIuS, byte_count);
    return;
  }

  bytes_acked_ = byte_count;
  TRACE("HandleAck: %" SCNuS "/%" PRIuS, bytes_acked_, bytes_sent_);
  UpdateStatus_Locked();
}

Error JSPipeEventEmitter::HandleJSWrite(struct PP_Var message) {
  TRACE("HandleJSWrite");
  if (message.type != PP_VARTYPE_ARRAY_BUFFER) {
    ERROR("Expected ArrayBuffer but got %d.", message.type);
    return EINVAL;
  }
  uint32_t length;
  if (buffer_iface_->ByteLength(message, &length) != PP_TRUE) {
    ERROR("ArrayBuffer.ByteLength returned PP_FALSE");
    return EINVAL;
  }

  char* buffer = (char*)buffer_iface_->Map(message);

  // Write data to the input fifo
  size_t wrote = HandleJSWrite(buffer, length);
  buffer_iface_->Unmap(message);
  if (wrote != length) {
    ERROR("Only wrote %d of %d bytes to pipe", (int)wrote, (int)length);
    return EIO;
  }
  TRACE("done HandleWrite: %d", length);
  return 0;
}

Error JSPipeEventEmitter::HandleJSAck(PP_Var message) {
  if (message.type != PP_VARTYPE_INT32) {
    ERROR("Integer object expected but got %d.", message.type);
    return EINVAL;
  }
  HandleJSAck(message.value.as_int);
  return 0;
}

int JSPipeEventEmitter::VarStrcmp(PP_Var a, PP_Var b) {
  uint32_t length_a = 0;
  uint32_t length_b = 0;
  const char* cstring_a = var_iface_->VarToUtf8(a, &length_a);
  const char* cstring_b = var_iface_->VarToUtf8(a, &length_b);
  std::string string_a(cstring_a, length_a);
  std::string string_b(cstring_b, length_a);
  return strcmp(string_a.c_str(), string_b.c_str());
}

Error JSPipeEventEmitter::HandleJSMessage(struct PP_Var message) {
  Error err = 0;
  if (!messaging_iface_ || !var_iface_ || !dict_iface_ || !buffer_iface_) {
    ERROR("Got NULL interface(s): %s%s%s%s",
          messaging_iface_ ? "" : "Messaging ",
          var_iface_ ? "" : "Var ",
          dict_iface_ ? "" : "Dictionary ",
          buffer_iface_ ? "" : "ArrayBuffer");
    return ENOSYS;
  }

  // Verify that we have an array with size two.
  if (message.type != PP_VARTYPE_DICTIONARY) {
    ERROR("Expected Dictionary but got %d.", message.type);
    return EINVAL;
  }

#ifndef NDEBUG
  PP_Var pipe_name_var = dict_iface_->Get(message, pipe_key_);
  if (VarStrcmp(pipe_name_var, pipe_name_var_)) {
    ERROR("Wrong pipe name.");
    return EINVAL;
  }
  var_iface_->Release(pipe_name_var);
#endif

  PP_Var operation_var = dict_iface_->Get(message, operation_key_);
  if (operation_var.type != PP_VARTYPE_STRING) {
    ERROR("Expected String but got %d.", operation_var.type);
    err = EINVAL;
  } else {
    uint32_t length;
    const char* operation_string;
    operation_string = var_iface_->VarToUtf8(operation_var, &length);
    std::string message_type(operation_string, length);

    TRACE("HandleJSMessage %s", message_type.c_str());
    PP_Var payload = dict_iface_->Get(message, payload_key_);
    if (message_type == kOperationNameWrite) {
      err = HandleJSWrite(payload);
    } else if (message_type == kOperationNameAck) {
      err = HandleJSAck(payload);
    } else {
      ERROR("Unknown message type: %s", message_type.c_str());
      err = EINVAL;
    }
    var_iface_->Release(payload);
  }

  var_iface_->Release(operation_var);
  return err;
}

Error JSPipeEventEmitter::Write_Locked(const char* data,
                                       size_t len,
                                       int* out_bytes) {
  if (GetOSpace() == 0) {
    *out_bytes = 0;
    return 0;
  }

  if (len > GetOSpace())
    len = GetOSpace();

  // Limit the size of the data we send with PostMessage to kMaxPostMessageSize
  if (len > kMaxPostMessageSize)
    len = kMaxPostMessageSize;

  Error err = SendWriteMessage(data, len);
  if (err != 0)
    return err;
  *out_bytes = len;
  bytes_sent_ += len;

  UpdateStatus_Locked();
  return 0;
}

}  // namespace nacl_io
