// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

class Instance : public pp::Instance {
 public:
  explicit Instance(PP_Instance instance) : pp::Instance(instance) {}
  virtual ~Instance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    PostMessage("Hello, multi-platform!");
    return true;
  }
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
