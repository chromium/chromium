// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/message_handler.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/utility/threading/simple_thread.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

class MessageHandler : public pp::MessageHandler {
 public:
  virtual void HandleMessage(pp::InstanceHandle instance,
                             const pp::Var& message_data) {
    if (!message_data.is_array()) {
      return;
    }

    pp::VarArray array(message_data);
    pp::Var response = SumArray(array);

    // Send the response back to JavaScript asynchronously.
    pp::Instance(instance.pp_instance()).PostMessage(response);
  }

  virtual pp::Var HandleBlockingMessage(pp::InstanceHandle instance,
                                        const pp::Var& message_data) {
    if (!message_data.is_array()) {
      // Return an undefined value.
      return pp::Var();
    }

    pp::VarArray array(message_data);

    // Send the response back to JavaScript synchronously.
    return SumArray(array);
  }

  virtual void WasUnregistered(pp::InstanceHandle instance) {}

  pp::Var SumArray(const pp::VarArray& array) {
    int32_t result = 0;
    for (uint32_t i = 0, length = array.GetLength(); i < length; ++i) {
      if (!array.Get(i).is_int()) {
        continue;
      }

      result += array.Get(i).AsInt();
    }

    return pp::Var(result);
  }
};

class Instance : public pp::Instance {
 public:
  explicit Instance(PP_Instance instance)
      : pp::Instance(instance), thread_(this) {}

  virtual bool Init(uint32_t /*argc*/,
                    const char* /*argn*/ [],
                    const char* /*argv*/ []) {
    thread_.Start();

    // The message handler must be registered using a message loop that is not
    // the main thread's messaging loop. This call will fail otherwise.
    RegisterMessageHandler(&message_handler_, thread_.message_loop());
    return true;
  }

 private:
  pp::SimpleThread thread_;
  MessageHandler message_handler_;
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
Module* CreateModule() { return new ::Module(); }
}  // namespace pp
