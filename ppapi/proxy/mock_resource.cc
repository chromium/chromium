// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/mock_resource.h"

namespace ppapi {
namespace proxy {

MockResource::MockResource(const HostResource& resource)
    : Resource(OBJECT_IS_PROXY, resource) {
}

MockResource::~MockResource() {
}

}  // namespace proxy
}  // namespace ppapi
