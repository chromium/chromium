// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"
#include "services/webnn/webnn_context_provider_impl.h"

#ifndef SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
#define SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_

namespace webnn::test {

class WebNNTestEnvironment {
 public:
  explicit WebNNTestEnvironment(
      WebNNContextProviderImpl::WebNNStatus status =
          WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
      WebNNContextProviderImpl::LoseAllContextsCallback
          lose_all_contexts_callback = base::DoNothing());
  ~WebNNTestEnvironment();

  WebNNContextProviderImpl* context_provider() const {
    return context_provider_.get();
  }

  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver);

 private:
  // Initialize a GPU Scheduler so tests can also use a scheduler
  // runner without the GPU service. The sync point manager must come first
  // since it is passed to the scheduler as a naked pointer.
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler scheduler_{&sync_point_manager_};
  std::unique_ptr<WebNNContextProviderImpl> context_provider_;
};

}  // namespace webnn::test

#endif  // SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
