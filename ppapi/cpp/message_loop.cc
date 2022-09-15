// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/message_loop.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_message_loop.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MessageLoop>() {
  return PPB_MESSAGELOOP_INTERFACE_1_0;
}

}  // namespace

MessageLoop::MessageLoop() : Resource() {}

MessageLoop::MessageLoop(const InstanceHandle& instance) : Resource() {
  if (has_interface<PPB_MessageLoop>()) {
    PassRefFromConstructor(get_interface<PPB_MessageLoop>()->Create(
        instance.pp_instance()));
  }
}

MessageLoop::MessageLoop(const MessageLoop& other) : Resource(other) {}

MessageLoop& MessageLoop::operator=(const MessageLoop& other) {
  Resource::operator=(other);
  return *this;
}

MessageLoop::MessageLoop(PP_Resource pp_message_loop)
    : Resource(pp_message_loop) {
}

// static
MessageLoop MessageLoop::GetForMainThread() {
  if (!has_interface<PPB_MessageLoop>())
    return MessageLoop();
  return MessageLoop(
      get_interface<PPB_MessageLoop>()->GetForMainThread());
}

// static
MessageLoop MessageLoop::GetCurrent() {
  if (!has_interface<PPB_MessageLoop>())
    return MessageLoop();
  return MessageLoop(
      get_interface<PPB_MessageLoop>()->GetCurrent());
}

int32_t MessageLoop::AttachToCurrentThread() {
  if (!has_interface<PPB_MessageLoop>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop>()->AttachToCurrentThread(
      pp_resource());
}

int32_t MessageLoop::Run() {
  if (!has_interface<PPB_MessageLoop>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop>()->Run(pp_resource());
}

int32_t MessageLoop::PostWork(const CompletionCallback& callback,
                                  int64_t delay_ms) {
  if (!has_interface<PPB_MessageLoop>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop>()->PostWork(
      pp_resource(),
      callback.pp_completion_callback(),
      delay_ms);
}

int32_t MessageLoop::PostQuit(bool should_destroy) {
  if (!has_interface<PPB_MessageLoop>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_MessageLoop>()->PostQuit(
      pp_resource(), PP_FromBool(should_destroy));
}

}  // namespace pp
