// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_

#include <stddef.h>
#include <string>

#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace content {

class RenderFrameHost;
class SharedStorageWorkletHostManager;
class StoragePartition;

using FencedFrameNavigationTarget = absl::variant<GURL, std::string>;

SharedStorageWorkletHostManager*
GetSharedStorageWorkletHostManagerForStoragePartition(
    StoragePartition* storage_partition);

std::string GetSharedStorageDisabledMessage();

std::string GetSharedStorageSelectURLDisabledMessage();

std::string GetSharedStorageAddModuleDisabledMessage();

void SetBypassIsSharedStorageAllowed(bool allow);

size_t GetAttachedSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

size_t GetKeepAliveSharedStorageWorkletHostsCount(
    StoragePartition* storage_partition);

// TODO(crbug.com/1414429): This function should be removed. Use
// `CreateFencedFrame` in fenced_frame_test_util.h instead.
RenderFrameHost* CreateFencedFrame(RenderFrameHost* root,
                                   const FencedFrameNavigationTarget& target);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SHARED_STORAGE_TEST_UTILS_H_
