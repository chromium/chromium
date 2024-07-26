// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_impl_cros.h"

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "services/webnn/tflite/buffer_impl_tflite.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/tflite/graph_impl_cros.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn::tflite {

ContextImplCrOS::ContextImplCrOS(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    WebNNContextProviderImpl* context_provider,
    mojom::CreateContextOptionsPtr options)
    : WebNNContextImpl(std::move(receiver),
                       context_provider,
                       GraphBuilderTflite::GetContextProperties(),
                       std::move(options)) {}

ContextImplCrOS::~ContextImplCrOS() = default;

base::WeakPtr<WebNNContextImpl> ContextImplCrOS::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void ContextImplCrOS::LoadModel(
    flatbuffers::DetachedBuffer model_content,
    ml::model_loader::mojom::ModelLoader::LoadCallback callback) {
  if (!model_loader_remote_.is_bound()) {
    // Bootstrap the mojo connection for ml-service first.
    //
    // The remote sent to WebNN Service that is hosted in browser process.
    mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
        webnn_service_remote;
    // The receiver sent to ML Service.
    auto ml_service_receiver =
        webnn_service_remote.InitWithNewPipeAndPassReceiver();
    // "0" means the backend can determine number of threads automatically.
    // TODO(crbug.com/330380801): Support other device types.
    auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New(
        /*num_threads=*/0, ml::model_loader::mojom::ModelFormat::kTfLite,
        ml::model_loader::mojom::DevicePreference::kCpu);
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .CreateWebPlatformModelLoader(
            std::move(ml_service_receiver), std::move(options),
            base::BindOnce(&ContextImplCrOS::OnModelLoaderCreated,
                           weak_factory_.GetWeakPtr(),
                           std::move(webnn_service_remote),
                           std::move(model_content), std::move(callback)));
  } else {
    model_loader_remote_->Load(
        mojo_base::BigBuffer(base::make_span(model_content)),
        std::move(callback));
  }
}

void ContextImplCrOS::OnModelLoaderCreated(
    mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
        webnn_service_remote,
    flatbuffers::DetachedBuffer model_content,
    ml::model_loader::mojom::ModelLoader::LoadCallback callback,
    ml::model_loader::mojom::CreateModelLoaderResult result) {
  switch (result) {
    case ml::model_loader::mojom::CreateModelLoaderResult::kUnknownError: {
      std::move(callback).Run(
          ml::model_loader::mojom::LoadModelResult::kUnknownError,
          mojo::NullRemote(), nullptr);
      return;
    }
    case ml::model_loader::mojom::CreateModelLoaderResult::kNotSupported: {
      std::move(callback).Run(
          ml::model_loader::mojom::LoadModelResult::kNotSupported,
          mojo::NullRemote(), nullptr);
      return;
    }
    case ml::model_loader::mojom::CreateModelLoaderResult::kOk: {
      model_loader_remote_.Bind(std::move(webnn_service_remote));

      model_loader_remote_->Load(
          mojo_base::BigBuffer(base::make_span(model_content)),
          std::move(callback));
      return;
    }
  }
}

void ContextImplCrOS::CreateGraphImpl(
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    CreateGraphImplCallback callback) {
  GraphImplCrOS::CreateAndBuild(this, std::move(graph_info),
                                std::move(compute_resource_info),
                                std::move(callback));
}

void ContextImplCrOS::CreateBufferImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    mojom::BufferInfoPtr buffer_info,
    CreateBufferImplCallback callback) {
  std::move(callback).Run(BufferImplTflite::Create(std::move(receiver), this,
                                                   std::move(buffer_info)));
}

}  // namespace webnn::tflite
