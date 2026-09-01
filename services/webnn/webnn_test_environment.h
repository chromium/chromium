// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
#define SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "services/webnn/webnn_context_provider_impl.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/host/execution_provider_initializer.h"
#endif

namespace webnn {

#if BUILDFLAG(IS_WIN)
struct EpDeviceInfo;
#endif

namespace test {

// A minimal fake WebNNBrowserHost implementation for testing.
class FakeWebNNBrowserHostForTesting : public mojom::WebNNBrowserHost {
 public:
  FakeWebNNBrowserHostForTesting();
  ~FakeWebNNBrowserHostForTesting() override;

  void Bind(mojo::PendingReceiver<mojom::WebNNBrowserHost> receiver);

#if BUILDFLAG(IS_WIN)
  void EnsureExecutionProvidersReady(
      EnsureExecutionProvidersReadyCallback callback) override;
  void RequestCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      const webnn::EpDeviceInfo& target_device,
      mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
          compiler_context_receiver,
      mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote,
      RequestCompilerContextCallback callback) override;
#endif
  void CreateWeightsFile(CreateWeightsFileCallback callback) override;
#if BUILDFLAG(IS_APPLE)
  void CopyCompiledModel(const base::FilePath& compiler_model_path,
                         CopyCompiledModelCallback callback) override;
#endif  // BUILDFLAG(IS_APPLE)

 private:
  mojo::Receiver<mojom::WebNNBrowserHost> receiver_;
};

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

  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

  // Waits until all WebNNContextImpl instances have been fully destroyed
  // (destructor has run). Call after resetting context remotes to ensure
  // service-side disconnect handlers, removal, and destruction all complete
  // before closing additional pipes.
  void WaitForAllContextsToBeDestroyed();

  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver,
      bool is_incognito = false);

  // Flushes thread pool tasks and waits for all receivers bound via
  // BindWebNNContextProvider to disconnect. Call in test TearDown after
  // resetting remotes to ensure deterministic cleanup.
  void TearDown();

  std::vector<std::string_view> GetContextBackendNames() const {
    return context_provider_->GetContextBackendNamesForTesting();
  }

 private:
  void OnReceiverDisconnected();
  void OnContextDestroyed();

  FakeWebNNBrowserHostForTesting fake_webnn_browser_host_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<WebNNContextProviderImpl> context_provider_;
  size_t pending_receiver_count_ = 0;
  size_t destroyed_context_count_ = 0;
  base::RepeatingClosure destruction_callback_;
};

}  // namespace test

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
