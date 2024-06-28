// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model_loader_test_util.h"

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using ml::model_loader::mojom::blink::CreateModelLoaderResult;

}  // namespace

FakeMLService::FakeMLService() = default;

FakeMLService::~FakeMLService() = default;

void FakeMLService::SetCreateModelLoader(CreateModelLoaderFn fn) {
  create_model_loader_ = std::move(fn);
}

void FakeMLService::BindFakeService(mojo::ScopedMessagePipeHandle pipe) {
  receiver_.reset();
  receiver_.Bind(mojo::PendingReceiver<MLService>(std::move(pipe)));
}

void FakeMLService::CreateModelLoader(CreateModelLoaderOptionsPtr opts,
                                      CreateModelLoaderCallback callback) {
  create_model_loader_.Run(std::move(opts), std::move(callback));
}

// A fake WebNNContextProvider that partially sets up the mojo pipe without
// binding a `WebNNContext` remote.
class FakePartialWebNNContextProvider
    : public webnn::mojom::blink::WebNNContextProvider {
 public:
  explicit FakePartialWebNNContextProvider() : receiver_(this) {}
  FakePartialWebNNContextProvider(const FakePartialWebNNContextProvider&) =
      delete;
  FakePartialWebNNContextProvider(FakePartialWebNNContextProvider&&) = delete;
  ~FakePartialWebNNContextProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(
        mojo::PendingReceiver<webnn::mojom::blink::WebNNContextProvider>(
            std::move(handle)));
  }

 private:
  // Override methods from webnn::mojom::WebNNContextProvider.
  void CreateWebNNContext(webnn::mojom::blink::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override {
    // Skip binding for `blink_remote` as it's not expected to be used by model
    // loader.
    mojo::PendingRemote<webnn::mojom::blink::WebNNContext> blink_remote;

    webnn::ContextProperties context_properties{
        webnn::InputOperandLayout::kNchw,
        /*input_supported_data_types=*/webnn::SupportedDataTypes::All(),
        /*constant_supported_data_types=*/webnn::SupportedDataTypes::All(),
        /*gather_input_supported_data_types=*/webnn::SupportedDataTypes::All(),
        /*gather_indices_supported_data_types=*/
        webnn::SupportedDataTypes::All()};
    auto success = webnn::mojom::blink::CreateContextSuccess::New(
        std::move(blink_remote), std::move(context_properties));
    std::move(callback).Run(
        webnn::mojom::blink::CreateContextResult::NewSuccess(
            std::move(success)));
  }

  mojo::Receiver<webnn::mojom::blink::WebNNContextProvider> receiver_;
};

ScopedSetMLServiceBinder::ScopedSetMLServiceBinder(FakeMLService* ml_service,
                                                   const V8TestingScope& scope)
    : interface_broker_(
          scope.GetExecutionContext()->GetBrowserInterfaceBroker()),
      fake_webnn_context_provider_(
          std::make_unique<FakePartialWebNNContextProvider>()) {
  interface_broker_->SetBinderForTesting(
      MLService::Name_,
      WTF::BindRepeating(&FakeMLService::BindFakeService,
                         // Safe to WTF::Unretained, we unregister the
                         // binder when the test finishes.
                         WTF::Unretained(ml_service)));
  interface_broker_->SetBinderForTesting(
      webnn::mojom::blink::WebNNContextProvider::Name_,
      WTF::BindRepeating(&FakePartialWebNNContextProvider::BindRequest,
                         WTF::Unretained(fake_webnn_context_provider_.get())));
}

ScopedSetMLServiceBinder::~ScopedSetMLServiceBinder() {
  interface_broker_->SetBinderForTesting(MLService::Name_,
                                         base::NullCallback());
  interface_broker_->SetBinderForTesting(
      webnn::mojom::blink::WebNNContextProvider::Name_, base::NullCallback());
}

FakeMLModelLoader::FakeMLModelLoader() = default;

FakeMLModelLoader::~FakeMLModelLoader() = default;

void FakeMLModelLoader::SetLoad(LoadFn fn) {
  load_ = std::move(fn);
}

FakeMLService::CreateModelLoaderFn FakeMLModelLoader::CreateFromThis() {
  return WTF::BindRepeating(
      &FakeMLModelLoader::OnCreateModelLoader,
      // Safe to WTF::Unretained, method won't be called after test finishes.
      WTF::Unretained(this));
}

FakeMLService::CreateModelLoaderFn
FakeMLModelLoader::CreateForUnsupportedContext() {
  return WTF::BindRepeating([](CreateModelLoaderOptionsPtr,
                               MLService::CreateModelLoaderCallback callback) {
    std::move(callback).Run(CreateModelLoaderResult::kNotSupported,
                            mojo::NullRemote());
  });
}

void FakeMLModelLoader::OnCreateModelLoader(
    CreateModelLoaderOptionsPtr,
    MLService::CreateModelLoaderCallback callback) {
  receiver_.reset();
  std::move(callback).Run(CreateModelLoaderResult::kOk,
                          receiver_.BindNewPipeAndPassRemote());
}

void FakeMLModelLoader::Load(mojo_base::BigBuffer buf,
                             ModelLoader::LoadCallback callback) {
  load_.Run(std::move(buf), std::move(callback));
}

}  // namespace blink
