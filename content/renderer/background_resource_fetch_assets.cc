// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/background_resource_fetch_assets.h"

#include "base/check.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

BackgroundResourceFetchAssets::BackgroundResourceFetchAssets(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_loader_factory,
    std::unique_ptr<blink::URLLoaderThrottleProvider> throttle_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const blink::LocalFrameToken& local_frame_token)
    : pending_loader_factory_(std::move(pending_loader_factory)),
      throttle_provider_(std::move(throttle_provider)),
      background_task_runner_(std::move(background_task_runner)),
      local_frame_token_(local_frame_token) {}

const scoped_refptr<base::SequencedTaskRunner>&
BackgroundResourceFetchAssets::GetTaskRunner() {
  return background_task_runner_;
}

scoped_refptr<network::SharedURLLoaderFactory>
BackgroundResourceFetchAssets::GetLoaderFactory() {
  CHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (pending_loader_factory_) {
    loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_loader_factory_));
    pending_loader_factory_.reset();
    CHECK(loader_factory_);
  }
  return loader_factory_;
}

blink::URLLoaderThrottleProvider*
BackgroundResourceFetchAssets::GetThrottleProvider() {
  return throttle_provider_.get();
}

const blink::LocalFrameToken&
BackgroundResourceFetchAssets::GetLocalFrameToken() {
  return local_frame_token_;
}

BackgroundResourceFetchAssets::~BackgroundResourceFetchAssets() {
  background_task_runner_->ReleaseSoon(FROM_HERE, std::move(loader_factory_));
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(throttle_provider_));
}

}  // namespace content
