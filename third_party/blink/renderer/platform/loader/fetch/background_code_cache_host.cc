// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/background_code_cache_host.h"

#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"

namespace blink {

BackgroundCodeCacheHost::BackgroundCodeCacheHost(
    mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote)
    : pending_remote_(std::move(pending_remote)) {}

BackgroundCodeCacheHost::~BackgroundCodeCacheHost() {
  if (background_task_runner_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(code_cache_host_));
  }
}

CodeCacheHost& BackgroundCodeCacheHost::GetCodeCacheHost(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  CHECK(background_task_runner->RunsTasksInCurrentSequence());
  if (!code_cache_host_) {
    CHECK(pending_remote_);
    CHECK(!background_task_runner_);
    code_cache_host_ = std::make_unique<CodeCacheHost>(
        mojo::Remote<mojom::blink::CodeCacheHost>(std::move(pending_remote_)));
    background_task_runner_ = background_task_runner;
  }
  return *code_cache_host_.get();
}

}  // namespace blink
