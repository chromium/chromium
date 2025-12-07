// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/data_decoder_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/origin.h"

namespace content {

DataDecoderManager::Handle::Handle(base::PassKey<DataDecoderManager>,
                                   DataDecoderManager* manager,
                                   DecoderMap::iterator decoder_it)
    : manager_(manager), decoder_it_(decoder_it) {
  ++decoder_it_->second.num_handles;
}

DataDecoderManager::Handle::~Handle() {
  CHECK_GT(decoder_it_->second.num_handles, 0u);
  --decoder_it_->second.num_handles;
  manager_->OnHandleDestroyed(decoder_it_);
}

data_decoder::DataDecoder& DataDecoderManager::Handle::data_decoder() {
  return decoder_it_->second.decoder;
}

DataDecoderManager::DataDecoderManager() = default;

DataDecoderManager::~DataDecoderManager() {
  // Check that no decoders have any outstanding handles.
  for (const auto& decoder : decoder_map_) {
    CHECK_EQ(decoder.second.num_handles, 0u);
  }
}

std::unique_ptr<DataDecoderManager::Handle> DataDecoderManager::GetHandle(
    const url::Origin& main_frame_origin,
    const url::Origin& owner_origin) {
  auto [decoder_it, unused_inserted] =
      decoder_map_.try_emplace({main_frame_origin, owner_origin});
  decoder_it->second.expected_service_teardown_time.reset();
  return std::make_unique<Handle>(base::PassKey<DataDecoderManager>(), this,
                                  decoder_it);
}

size_t DataDecoderManager::NumDecodersForTesting() const {
  return decoder_map_.size();
}

std::optional<size_t> DataDecoderManager::GetHandleCountForTesting(
    const url::Origin& main_frame_origin,
    const url::Origin& owner_origin) const {
  auto decoder_it = decoder_map_.find({main_frame_origin, owner_origin});
  if (decoder_it == decoder_map_.end()) {
    return std::nullopt;
  }
  return decoder_it->second.num_handles;
}

void DataDecoderManager::OnHandleDestroyed(DecoderMap::iterator decoder_it) {
  if (decoder_it->second.num_handles > 0) {
    return;
  }

  decoder_it->second.expected_service_teardown_time =
      base::TimeTicks::Now() + kIdleTimeout;
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kIdleTimeout,
                 base::BindOnce(&DataDecoderManager::CleanUpIdleDecoders,
                                base::Unretained(this)));
  }
}

void DataDecoderManager::CleanUpIdleDecoders() {
  bool should_restart_timer = false;
  base::TimeTicks now = base::TimeTicks::Now();
  for (auto next_it = decoder_map_.begin(); next_it != decoder_map_.end();) {
    auto it = next_it;
    ++next_it;
    // For decoders with live Handles, nothing to do.
    if (!it->second.expected_service_teardown_time) {
      DCHECK_GT(it->second.num_handles, 0u);
      continue;
    }

    DCHECK_EQ(it->second.num_handles, 0u);

    // Destroy expired decoders.
    if (*it->second.expected_service_teardown_time <= now) {
      decoder_map_.erase(it);
      continue;
    }

    // If there are any decoders with an expiration time, but that expiration
    // time has not yet been reached, need to queue a cleanup task. Such
    // decoders should have a teardown time within `kIdleTimeout` of now.
    DCHECK_LE(*it->second.expected_service_teardown_time, now + kIdleTimeout);
    should_restart_timer = true;
  }

  if (should_restart_timer) {
    timer_.Start(FROM_HERE, kIdleTimeout,
                 base::BindOnce(&DataDecoderManager::CleanUpIdleDecoders,
                                base::Unretained(this)));
  }
}

}  // namespace content
