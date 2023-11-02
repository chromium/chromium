// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {

HostResource::HostResource() : instance_(0), host_resource_(0) {}

// static
HostResource HostResource::MakeInstanceOnly(PP_Instance instance) {
  HostResource resource;
  resource.SetHostResource(instance, 0);
  return resource;
}

void HostResource::SetHostResource(PP_Instance instance, PP_Resource resource) {
  instance_ = instance;
  host_resource_ = resource;
}

}  // namespace ppapi
