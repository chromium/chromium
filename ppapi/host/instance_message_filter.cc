// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/instance_message_filter.h"

#include "ppapi/host/ppapi_host.h"

namespace ppapi {
namespace host {

InstanceMessageFilter::InstanceMessageFilter(PpapiHost* host) : host_(host) {
}

InstanceMessageFilter::~InstanceMessageFilter() {
}

}  // namespace host
}  // namespace ppapi
