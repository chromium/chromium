// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_INTERNAL_MODULE_H_
#define PPAPI_CPP_PRIVATE_INTERNAL_MODULE_H_

namespace pp {
class Module;

// Forcibly sets the value returned by pp::Module::Get(). Do not call this
// function except to support the trusted plugin or the remoting plugin!
void InternalSetModuleSingleton(Module* module);
}

#endif  // PPAPI_CPP_PRIVATE_INTERNAL_MODULE_H_
