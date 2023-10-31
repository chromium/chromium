// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_TEST_UTIL_H_

#include "base/memory/raw_ref.h"
#include "components/ml/mojom/ml_service.mojom-blink.h"
#include "components/ml/mojom/web_platform_model.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"

namespace blink {

using ml::model_loader::mojom::blink::CreateModelLoaderOptionsPtr;
using ml::model_loader::mojom::blink::MLService;
using ml::model_loader::mojom::blink::ModelLoader;

// A fake MLService that intercepts Blink's browser interface request to the
// ml.model_loader.MLService interface.
class FakeMLService : public MLService {
 public:
  FakeMLService();
  FakeMLService(const FakeMLService&) = delete;
  FakeMLService(FakeMLService&&) = delete;
  ~FakeMLService() override;

  using CreateModelLoaderFn =
      base::RepeatingCallback<void(CreateModelLoaderOptionsPtr,
                                   MLService::CreateModelLoaderCallback)>;

  void SetCreateModelLoader(CreateModelLoaderFn fn);

  void BindFakeService(mojo::ScopedMessagePipeHandle pipe);

 private:
  // Override methods from ml::model_loader::mojom::blink::MLService.
  void CreateModelLoader(
      CreateModelLoaderOptionsPtr opts,
      MLService::CreateModelLoaderCallback callback) override;

  CreateModelLoaderFn create_model_loader_;
  mojo::Receiver<MLService> receiver_{this};
};

class ScopedSetMLServiceBinder {
 public:
  ScopedSetMLServiceBinder(FakeMLService* ml_service,
                           const V8TestingScope& scope);
  ~ScopedSetMLServiceBinder();

 private:
  const raw_ref<const BrowserInterfaceBrokerProxy, ExperimentalRenderer>
      interface_broker_;
};

// A fake MLModelLoader Mojo interface implementation that backs a Blink
// MLModelLoader object.
class FakeMLModelLoader : public ModelLoader {
 public:
  FakeMLModelLoader();
  FakeMLModelLoader(const FakeMLModelLoader&) = delete;
  FakeMLModelLoader(FakeMLModelLoader&&) = delete;
  ~FakeMLModelLoader() override;

  using LoadFn = base::RepeatingCallback<void(mojo_base::BigBuffer,
                                              ModelLoader::LoadCallback)>;
  void SetLoad(LoadFn fn);

  FakeMLService::CreateModelLoaderFn CreateFromThis();

  FakeMLService::CreateModelLoaderFn CreateForUnsupportedContext();

  void OnCreateModelLoader(CreateModelLoaderOptionsPtr,
                           MLService::CreateModelLoaderCallback callback);

 private:
  // Override methods from ml::model_loader::mojom::blink::ModelLoader.
  void Load(mojo_base::BigBuffer buffer,
            ModelLoader::LoadCallback callback) override;

  LoadFn load_;
  mojo::Receiver<ModelLoader> receiver_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_MODEL_LOADER_TEST_UTIL_H_
