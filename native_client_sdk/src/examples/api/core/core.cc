// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurrence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="file_histogram.nmf"
class CoreInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit CoreInstance(PP_Instance instance)
      : pp::Instance(instance), callback_factory_(this) {}

 private:
  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message will contain the requested delay time.
  ///
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {
    int32_t delay = var_message.AsInt();
    if (delay) {
      // If a delay is requested, issue a callback after delay ms.
      last_receive_time_ = pp::Module::Get()->core()->GetTimeTicks();
      pp::Module::Get()->core()->CallOnMainThread(
          delay, callback_factory_.NewCallback(&CoreInstance::DelayedPost), 0);
    } else {
      // If no delay is requested, reply immediately with zero time elapsed.
      pp::Var msg(0);
      PostMessage(msg);
    }
  }

  void DelayedPost(int32_t) {
    // Send the time elapsed until the callbacked fired.
    pp::Var msg(pp::Module::Get()->core()->GetTimeTicks() - last_receive_time_);
    PostMessage(msg);
  }

 private:
  pp::CompletionCallbackFactory<CoreInstance> callback_factory_;
  PP_TimeTicks last_receive_time_;
};

class CoreModule : public pp::Module {
 public:
  CoreModule() : pp::Module() {}
  virtual ~CoreModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new CoreInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new CoreModule(); }
}  // namespace pp
