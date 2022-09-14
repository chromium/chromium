// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

class LoadProgressInstance : public pp::Instance {
 public:
  explicit LoadProgressInstance(PP_Instance instance)
      : pp::Instance(instance) {}
  virtual ~LoadProgressInstance() {}
};

class LoadProgressModule : public pp::Module {
 public:
  LoadProgressModule() : pp::Module() {}
  virtual ~LoadProgressModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new LoadProgressInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new LoadProgressModule(); }
}  // namespace pp
