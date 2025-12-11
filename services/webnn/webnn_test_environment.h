// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
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
          lose_all_contexts_callback = base::DoNothing(),
      std::unique_ptr<base::test::TaskEnvironment> task_environment =
          std::make_unique<base::test::TaskEnvironment>());

  ~WebNNTestEnvironment();

  WebNNContextProviderImpl* context_provider() const {
    return context_provider_.get();
  }

  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver);

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<WebNNContextProviderImpl> context_provider_;
};

}  // namespace webnn::test

#endif  // SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
