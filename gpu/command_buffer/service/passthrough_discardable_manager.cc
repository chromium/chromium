// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/passthrough_discardable_manager.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {

PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue() =
    default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::DiscardableCacheValue(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    const DiscardableCacheValue& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue&
PassthroughDiscardableManager::DiscardableCacheValue::operator=(
    DiscardableCacheValue&& other) = default;
PassthroughDiscardableManager::DiscardableCacheValue::~DiscardableCacheValue() =
    default;

PassthroughDiscardableManager::PassthroughDiscardableManager(
    const GpuPreferences& preferences) {}

PassthroughDiscardableManager::~PassthroughDiscardableManager() {
}

}  // namespace gpu
