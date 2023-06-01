// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_model_loader_test_util.h"

#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "mojo/public/cpp/system/message_pipe.h"
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
  std::move(create_model_loader_).Run(std::move(opts), std::move(callback));
}

ScopedSetMLServiceBinder::ScopedSetMLServiceBinder(FakeMLService* ml_service,
                                                   const V8TestingScope& scope)
    : interface_broker_(
          scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
  interface_broker_->SetBinderForTesting(
      MLService::Name_,
      WTF::BindRepeating(&FakeMLService::BindFakeService,
                         // Safe to WTF::Unretained, we unregister the
                         // binder when the test finishes.
                         WTF::Unretained(ml_service)));
}

ScopedSetMLServiceBinder::~ScopedSetMLServiceBinder() {
  interface_broker_->SetBinderForTesting(MLService::Name_,
                                         base::NullCallback());
}

FakeMLModelLoader::FakeMLModelLoader() = default;

FakeMLModelLoader::~FakeMLModelLoader() = default;

void FakeMLModelLoader::SetLoad(LoadFn fn) {
  load_ = std::move(fn);
}

FakeMLService::CreateModelLoaderFn FakeMLModelLoader::CreateFromThis() {
  return WTF::BindOnce(
      &FakeMLModelLoader::OnCreateModelLoader,
      // Safe to WTF::Unretained, method won't be called after test finishes.
      WTF::Unretained(this));
}

FakeMLService::CreateModelLoaderFn
FakeMLModelLoader::CreateForUnsupportedContext() {
  return WTF::BindOnce([](CreateModelLoaderOptionsPtr,
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
  std::move(load_).Run(std::move(buf), std::move(callback));
}

}  // namespace blink
