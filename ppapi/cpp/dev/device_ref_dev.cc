// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/device_ref_dev.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_DeviceRef_Dev>() {
  return PPB_DEVICEREF_DEV_INTERFACE;
}

}  // namespace

DeviceRef_Dev::DeviceRef_Dev() {
}

DeviceRef_Dev::DeviceRef_Dev(PP_Resource resource) : Resource(resource) {
}

DeviceRef_Dev::DeviceRef_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

DeviceRef_Dev::DeviceRef_Dev(const DeviceRef_Dev& other) : Resource(other) {
}

DeviceRef_Dev::~DeviceRef_Dev() {
}

PP_DeviceType_Dev DeviceRef_Dev::GetType() const {
  if (!has_interface<PPB_DeviceRef_Dev>())
    return PP_DEVICETYPE_DEV_INVALID;
  return get_interface<PPB_DeviceRef_Dev>()->GetType(pp_resource());
}

Var DeviceRef_Dev::GetName() const {
  if (!has_interface<PPB_DeviceRef_Dev>())
    return Var();
  return Var(PASS_REF,
             get_interface<PPB_DeviceRef_Dev>()->GetName(pp_resource()));
}

}  // namespace pp
