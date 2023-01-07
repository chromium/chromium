// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstdio>
#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

class Instance : public pp::Instance {
 public:
  explicit Instance(PP_Instance instance) :
      pp::Instance(instance),
      callback_factory_(this),
      delay_milliseconds_(10),
      active_(true) {
    DoSomething(PP_OK);
  }
  virtual ~Instance() {}

  virtual void HandleMessage(const pp::Var& message_var) {
    std::string message_string = message_var.AsString();
    if (message_string == "be idle") {
      active_ = false;
    } else {
      PostMessage("Unhandled control message.");
    }
  }

  void DoSomething(int32_t result) {
    if (active_) {
      pp::MessageLoop loop = pp::MessageLoop::GetCurrent();
      pp::CompletionCallback c = callback_factory_.NewCallback(
          &Instance::DoSomething);
      loop.PostWork(c, delay_milliseconds_);
    }
  }

  pp::CompletionCallbackFactory<Instance> callback_factory_;
  int delay_milliseconds_;
  bool active_;
};

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Instance(instance);
  }
};

namespace pp {
Module* CreateModule() {
  return new ::Module();
}
}  // namespace pp

