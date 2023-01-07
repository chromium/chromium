// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MODULE_IMPL_H_
#define PPAPI_CPP_MODULE_IMPL_H_

/// @file
/// This file defines some simple function templates that help the C++ wrappers
/// (and are not for external developers to use).

#include "ppapi/cpp/module.h"

namespace pp {

namespace {

// Specialize this function to return the interface string corresponding to the
// PP?_XXX structure.
template <typename T>
const char* interface_name();

template <typename T> inline T const* get_interface() {
  static T const* funcs = reinterpret_cast<T const*>(
      pp::Module::Get()->GetBrowserInterface(interface_name<T>()));
  return funcs;
}

template <typename T> inline bool has_interface() {
  return get_interface<T>() != NULL;
}

}  // namespace

}  // namespace pp

#endif  // PPAPI_CPP_MODULE_IMPL_H_

