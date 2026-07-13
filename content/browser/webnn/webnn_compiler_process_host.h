// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_
#define CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom.h"

namespace webnn {
struct ContextProperties;
}

namespace content {

// Manages the WebNN Compiler utility processes lifecycle and related behavior.
// Examples include process launch-on-demand, disconnect handling, crash
// throttling, and brokered CompilerContext creation. Owned by GpuProcessHost.
//
// This class must be used on the UI thread.
class CONTENT_EXPORT WebNNCompilerProcessHost {
 public:
  WebNNCompilerProcessHost();

  WebNNCompilerProcessHost(const WebNNCompilerProcessHost&) = delete;
  WebNNCompilerProcessHost& operator=(const WebNNCompilerProcessHost&) = delete;

  ~WebNNCompilerProcessHost();

  // Requests a new CompilerContext for `target_device` from its Compiler
  // process. Each EP device has its own Compiler process, which is launched on
  // demand if not already running. On failure, the pipe endpoints are simply
  // dropped (disconnecting the peer endpoints held by the GPU/Renderer).
  void RequestCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      const webnn::EpDeviceInfo& target_device,
      mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
          compiler_context_receiver,
      mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote);

 private:
  void OnEpsResolvedForCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      const webnn::EpDeviceInfo& target_device,
      mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
          compiler_context_receiver,
      mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote,
      base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr>
          ep_package_info_map);

  void OnDisconnected(const webnn::EpDeviceInfo& device_info,
                      uint32_t reason,
                      const std::string& description);

  // Per-EP-device remotes to Compiler processes.
  base::flat_map<webnn::EpDeviceInfo,
                 mojo::Remote<webnn::mojom::WebNNCompilerService>>
      webnn_compiler_remotes_;

  base::WeakPtrFactory<WebNNCompilerProcessHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_
