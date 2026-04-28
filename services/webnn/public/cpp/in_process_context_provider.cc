// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/in_process_context_provider.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/tflite/context_provider_tflite.h"

namespace webnn::tflite {

mojo::ScopedMessagePipeHandle CreateInProcessContextProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto provider =
      std::make_unique<ContextProviderTflite>(std::move(task_runner));

  mojo::PendingRemote<mojom::WebNNContextProvider> pending_remote;
  mojo::MakeSelfOwnedReceiver(std::move(provider),
                              pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote.PassPipe();
}

}  // namespace webnn::tflite
