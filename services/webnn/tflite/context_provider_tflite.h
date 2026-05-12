// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_PROVIDER_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_PROVIDER_TFLITE_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::tflite {

// Returns true if the request described by `options` should be served by the
// renderer-process TFLite backend instead of any GPU-process backend.
COMPONENT_EXPORT(WEBNN_SERVICE)
bool ShouldUseInProcessTflite(const mojom::CreateContextOptions& options);

// A lightweight WebNNContextProvider implementation for TFLite that runs
// without GPU dependencies (e.g., in the renderer process).
class COMPONENT_EXPORT(WEBNN_SERVICE) ContextProviderTflite
    : public mojom::WebNNContextProvider {
 public:
  ContextProviderTflite(
      mojo::PendingRemote<mojom::WebNNWeightsFileCreator>
          weights_file_creator_remote,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  ~ContextProviderTflite() override;

  ContextProviderTflite(const ContextProviderTflite&) = delete;
  ContextProviderTflite& operator=(const ContextProviderTflite&) = delete;

  // mojom::WebNNContextProvider:
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override;

  // Creates a weights file via the browser process.
  void CreateWeightsFile(base::OnceCallback<void(base::File)> callback);

  base::WeakPtr<ContextProviderTflite> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void RemoveWebNNContextImpl(const blink::WebNNContextToken& handle);

 private:
  void OnCreateWebNNContextImpl(
      CreateWebNNContextCallback callback,
      mojo::PendingRemote<mojom::WebNNContext> remote,
      WebNNContextImpl::WebNNContextImplPtr context_impl);

  // SharedRemote for creating weights files in the browser process.
  // Uses SharedRemote so it can be called from any sequence.
  mojo::SharedRemote<mojom::WebNNWeightsFileCreator>
      shared_weights_file_creator_;

  // Task runner for the main thread (where the Mojo pipe lives).
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Contexts created by this provider. Cleaned up when the provider is
  // destroyed (when the mojo pipe closes).
  WebNNContextProviderImpl::WebNNContextImplSet context_impls_;

  base::WeakPtrFactory<ContextProviderTflite> weak_factory_{this};
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_CONTEXT_PROVIDER_TFLITE_H_
