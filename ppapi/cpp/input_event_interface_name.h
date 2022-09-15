// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INPUT_EVENT_INTERFACE_NAME_H_
#define PPAPI_CPP_INPUT_EVENT_INTERFACE_NAME_H_

#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

// This implementation is shared between instance.cc and input_event.cc
template <>
const char* interface_name<PPB_InputEvent_1_0>() {
  return PPB_INPUT_EVENT_INTERFACE_1_0;
}

}  // namespace
}  // namespace pp

#endif  // PPAPI_CPP_INPUT_EVENT_INTERFACE_NAME_H_
