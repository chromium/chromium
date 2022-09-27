// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_

#include <stddef.h>
#include <string>

class GURL;

namespace content {

class RenderFrameHost;
class StoragePartition;

std::string GetSharedStorageDisabledMessage();

void SetBypassIsSharedStorageAllowed(bool allow);

size_t GetAttachedSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

size_t GetKeepAliveSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

RenderFrameHost* CreateFencedFrame(RenderFrameHost* root, const GURL& url);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
