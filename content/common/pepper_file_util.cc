// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_file_util.h"
#include "ppapi/shared_impl/platform_file.h"

namespace content {

int IntegerFromSyncSocketHandle(
    const base::SyncSocket::Handle& socket_handle) {
  return ppapi::PlatformFileToInt(socket_handle);
}

}  // namespace content
