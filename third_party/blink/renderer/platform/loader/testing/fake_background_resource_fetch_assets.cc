// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/fake_background_resource_fetch_assets.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/platform/loader/testing/fake_url_loader_factory_for_background_thread.h"

namespace blink {

FakeBackgroundResourceFetchAssets::FakeBackgroundResourceFetchAssets(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    LoadStartCallback load_start_callback)
    : background_task_runner_(std::move(background_task_runner)),
      pending_loader_factory_(
          base::MakeRefCounted<FakeURLLoaderFactoryForBackgroundThread>(
              std::move(load_start_callback))
              ->Clone()) {}

FakeBackgroundResourceFetchAssets::~FakeBackgroundResourceFetchAssets() {
  if (url_loader_factory_) {
    // `url_loader_factory_` must be released in the background thread.
    background_task_runner_->ReleaseSoon(FROM_HERE,
                                         std::move(url_loader_factory_));
  }
}

const scoped_refptr<base::SequencedTaskRunner>&
FakeBackgroundResourceFetchAssets::GetTaskRunner() {
  return background_task_runner_;
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeBackgroundResourceFetchAssets::GetLoaderFactory() {
  if (!url_loader_factory_) {
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_loader_factory_));
  }
  return url_loader_factory_;
}

const blink::LocalFrameToken&
FakeBackgroundResourceFetchAssets::GetLocalFrameToken() {
  return local_frame_token_;
}

}  // namespace blink
