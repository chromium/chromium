// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/pdf.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_PDF>() {
  return PPB_PDF_INTERFACE;
}

}  // namespace

// static
bool PDF::IsAvailable() {
  return has_interface<PPB_PDF>();
}

// static
void PDF::Print(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->Print(instance.pp_instance());
}

}  // namespace pp
