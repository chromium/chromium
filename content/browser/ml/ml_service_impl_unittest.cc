// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ml/ml_service_impl.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/ml/mojom/ml_service.mojom.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class MLServiceImplTest : public RenderViewHostTestHarness {};

// Tests model loader creation. It should return "not supported".
TEST_F(MLServiceImplTest, CreateModelLoaderNotSupported) {
  mojo::Remote<ml::model_loader::mojom::MLService> service_remote;
  MLServiceImpl::Create(service_remote.BindNewPipeAndPassReceiver());
  auto options = ml::model_loader::mojom::CreateModelLoaderOptions::New();
  options->num_threads = 1;
  options->model_format = ml::model_loader::mojom::ModelFormat::kTfLite;
  bool is_callback_called = false;
  base::RunLoop run_loop;
  service_remote->CreateModelLoader(
      std::move(options),
      base::BindLambdaForTesting(
          [&](ml::model_loader::mojom::CreateModelLoaderResult result,
              mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
                  remote) {
            // Shouldn't be supported by default (non-chromeOS platforms) yet.
            EXPECT_EQ(result, ml::model_loader::mojom::CreateModelLoaderResult::
                                  kNotSupported);
            EXPECT_FALSE(remote.is_valid());
            is_callback_called = true;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace content
