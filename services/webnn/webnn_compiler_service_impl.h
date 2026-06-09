// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_COMPILER_SERVICE_IMPL_H_
#define SERVICES_WEBNN_WEBNN_COMPILER_SERVICE_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom.h"

namespace webnn {

// Maintains a set of WebNNCompilerContext instances. Runs in the WebNN Compiler
// utility process.
//
// Currently only used by the ORT backend on Windows.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNCompilerServiceImpl
    : public mojom::WebNNCompilerService {
 public:
  explicit WebNNCompilerServiceImpl(
      mojo::PendingReceiver<mojom::WebNNCompilerService> receiver);

  WebNNCompilerServiceImpl(const WebNNCompilerServiceImpl&) = delete;
  WebNNCompilerServiceImpl& operator=(const WebNNCompilerServiceImpl&) = delete;

  ~WebNNCompilerServiceImpl() override;

 private:
  // mojom::WebNNCompilerService:
  void CreateCompilerContext(
      mojom::CreateContextOptionsPtr context_options,
      const ContextProperties& context_properties,
      base::flat_map<std::string, mojom::EpPackageInfoPtr> ep_package_info,
      mojo::PendingRemote<mojom::WebNNModelLoader> model_loader,
      mojo::PendingReceiver<mojom::WebNNCompilerContext> receiver) override;

  // Called when any compiler context in `compiler_contexts_` disconnects.
  void OnCompilerContextDisconnected();

  // Called when the idle timer fires with no active compiler contexts.
  void OnIdleTimeout();

  mojo::UniqueReceiverSet<mojom::WebNNCompilerContext> compiler_contexts_;

  // Started at construction and restarted each time the last compiler context
  // disconnects. Cancelled when a new context is created. If it fires, the
  // process shuts down via ResetWithReason().
  base::OneShotTimer idle_timer_;

  mojo::Receiver<mojom::WebNNCompilerService> receiver_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_COMPILER_SERVICE_IMPL_H_
