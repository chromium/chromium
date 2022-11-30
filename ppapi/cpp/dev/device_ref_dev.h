// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_DEVICE_REF_DEV_H_
#define PPAPI_CPP_DEV_DEVICE_REF_DEV_H_

#include "ppapi/c/dev/ppb_device_ref_dev.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class DeviceRef_Dev : public Resource {
 public:
  DeviceRef_Dev();

  explicit DeviceRef_Dev(PP_Resource resource);

  DeviceRef_Dev(PassRef, PP_Resource resource);

  DeviceRef_Dev(const DeviceRef_Dev& other);

  virtual ~DeviceRef_Dev();

  PP_DeviceType_Dev GetType() const;

  Var GetName() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_DEVICE_REF_DEV_H_
