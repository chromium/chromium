// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

namespace {

class Module : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new pp::Instance(instance);
  }
};

}  // namespace

namespace pp {

Module* CreateModule() {
  return new ::Module();
}

}  // namespace pp
