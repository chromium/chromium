// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/internal_module.h"

namespace pp {
namespace {
static Module* g_module_singleton = NULL;
}  // namespace

Module* Module::Get() {
  return g_module_singleton;
}

void InternalSetModuleSingleton(Module* module) {
  g_module_singleton = module;
}

}  // namespace pp
