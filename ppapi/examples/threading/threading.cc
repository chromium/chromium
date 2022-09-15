// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

class MyInstance : public pp::Instance {
 public:
  MyInstance(PP_Instance instance) : pp::Instance(instance) {
    thread_ = new pp::SimpleThread(this);
    factory_.Initialize(this);
  }

  virtual ~MyInstance() {
    delete thread_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    thread_->Start();
    thread_->message_loop().PostWork(
        factory_.NewCallback(&MyInstance::CallOnBackground));
    return true;
  }

  virtual void DidChangeView(const pp::View& view) {
  }

 private:
  void CallOnBackground(int32_t result) {
  }

  pp::CompletionCallbackFactory<MyInstance> factory_;

  pp::SimpleThread* thread_;
};


class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
