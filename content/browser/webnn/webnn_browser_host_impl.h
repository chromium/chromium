// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBNN_WEBNN_BROWSER_HOST_IMPL_H_
#define CONTENT_BROWSER_WEBNN_WEBNN_BROWSER_HOST_IMPL_H_

#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_browser_host.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/ep_device_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
#include "base/files/file_path.h"
#endif  // BUILDFLAG(IS_APPLE)

namespace content {

// Browser-side implementation of the WebNN host interface used by the WebNN
// service in the GPU process. It brokers operations that must be performed in
// the browser process on behalf of the sandboxed WebNN service. See
// webnn::mojom::WebNNBrowserHost for the exact set of operations.
//
// This class also manages the WebNN Compiler utility process lifecycle
// (launch-on-demand, disconnect handling, and crash throttling). Each EP
// device has its own dedicated Compiler process, launched on first use.
//
// One instance is created per GPU process and bound via that process's
// ChildProcessHost (see content::GpuProcessHost::BindHostReceiver). It is a
// self-owned receiver: when the GPU process disconnects (or restarts), the
// instance is deleted.
//
// This class must be used on the UI thread.
class CONTENT_EXPORT WebNNBrowserHostImpl
    : public webnn::mojom::WebNNBrowserHost {
 public:
  // Creates an instance bound to `receiver`. See the class comment for its
  // ownership and lifetime.
  static void Create(
      mojo::PendingReceiver<webnn::mojom::WebNNBrowserHost> receiver);

  // Public so std::make_unique() (used by Create()) can construct it; the
  // PassKey ensures only Create() can.
  explicit WebNNBrowserHostImpl(base::PassKey<WebNNBrowserHostImpl>);

  WebNNBrowserHostImpl(const WebNNBrowserHostImpl&) = delete;
  WebNNBrowserHostImpl& operator=(const WebNNBrowserHostImpl&) = delete;

  ~WebNNBrowserHostImpl() override;

  // webnn::mojom::WebNNBrowserHost:
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
#endif  // BUILDFLAG(IS_WIN)
  void CreateWeightsFile(CreateWeightsFileCallback callback) override;
#if BUILDFLAG(IS_APPLE)
  void CopyCompiledModel(const base::FilePath& compiler_model_path,
                         CopyCompiledModelCallback callback) override;
#endif  // BUILDFLAG(IS_APPLE)

 private:
#if BUILDFLAG(IS_WIN)
  // Continues RequestCompilerContext() once the execution providers have been
  // resolved and the target EP's library path is known. Launches the Compiler
  // process for `target_device` if one is not already running, then requests
  // the CompilerContext from it.
  void OnEpsResolvedForCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      const webnn::EpDeviceInfo& target_device,
      mojo::PendingReceiver<webnn::mojom::WebNNCompilerContext>
          compiler_context_receiver,
      mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote,
      RequestCompilerContextCallback callback,
      base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr>
          ep_package_info_map);

  // Handles disconnection of the Compiler process for `device_info`, erasing
  // its remote and updating crash accounting.
  void OnDisconnected(const webnn::EpDeviceInfo& device_info,
                      uint32_t reason,
                      const std::string& description);

  // Per-EP-device remotes to Compiler processes, launched on demand.
  base::flat_map<webnn::EpDeviceInfo,
                 mojo::Remote<webnn::mojom::WebNNCompilerService>>
      webnn_compiler_remotes_;

  base::WeakPtrFactory<WebNNBrowserHostImpl> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBNN_WEBNN_BROWSER_HOST_IMPL_H_
