// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_module.h"

namespace net {

static NetModule::ResourceProvider resource_provider;

// static
void NetModule::SetResourceProvider(ResourceProvider func) {
  resource_provider = func;
}

// static
scoped_refptr<base::RefCountedMemory> NetModule::GetResource(int key) {
  return resource_provider ? resource_provider(key) : nullptr;
}

}  // namespace net
