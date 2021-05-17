// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/scoped_url_forwarder.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"

namespace remoting {

ScopedUrlForwarder::ScopedUrlForwarder() = default;

ScopedUrlForwarder::~ScopedUrlForwarder() = default;

#if !defined(OS_LINUX)

// static
std::unique_ptr<ScopedUrlForwarder> ScopedUrlForwarder::Create() {
  // Returns a no-op object. Can't use std::make_unique since the constructor is
  // protected.
  return base::WrapUnique(new ScopedUrlForwarder());
}

#endif  // !defined(OS_LINUX)

}  // namespace remoting
