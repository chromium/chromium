// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_

#include <stddef.h>
#include <string>

namespace content {

class ToRenderFrameHost;

std::string GetSharedStorageDisabledMessage();

void SetBypassIsSharedStorageAllowed(bool allow);

size_t GetAttachedWorkletHostsCountForRenderFrameHost(
    const ToRenderFrameHost& to_rfh);

size_t GetKeepAliveWorkletHostsCountForRenderFrameHost(
    const ToRenderFrameHost& to_rfh);
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
