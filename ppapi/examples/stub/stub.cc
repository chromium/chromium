// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

// This is the simplest possible C++ Pepper plugin that does nothing.

// This object represents one time the page says <embed>.
class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }
};

// This object is the global object representing this plugin library as long
// as it is loaded.
class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
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
