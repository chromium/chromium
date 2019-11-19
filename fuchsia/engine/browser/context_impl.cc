// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/context_impl.h"

#include <lib/zx/channel.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/browser/web_engine_memory_pressure_evaluator.h"

ContextImpl::ContextImpl(content::BrowserContext* browser_context,
                         WebEngineDevToolsController* devtools_controller)
    : browser_context_(browser_context),
      devtools_controller_(devtools_controller),
      cookie_manager_(base::BindRepeating(
          &content::StoragePartition::GetNetworkContext,
          base::Unretained(content::BrowserContext::GetDefaultStoragePartition(
              browser_context_)))) {
  DCHECK(browser_context_);
  DCHECK(devtools_controller_);
  devtools_controller_->OnContextCreated();

  // In browser tests there will be no MemoryPressureMonitor.
  if (base::MemoryPressureMonitor::Get()) {
    memory_pressure_evaluator_ =
        std::make_unique<WebEngineMemoryPressureEvaluator>(
            static_cast<util::MultiSourceMemoryPressureMonitor*>(
                base::MemoryPressureMonitor::Get())
                ->CreateVoter());
  }
}

ContextImpl::~ContextImpl() {
  devtools_controller_->OnContextDestroyed();
}

void ContextImpl::DestroyFrame(FrameImpl* frame) {
  auto iter = frames_.find(frame);
  DCHECK(iter != frames_.end());
  frames_.erase(frames_.find(frame));
}

bool ContextImpl::IsJavaScriptInjectionAllowed() {
  return allow_javascript_injection_;
}

fidl::InterfaceHandle<fuchsia::web::Frame>
ContextImpl::CreateFrameForPopupWebContents(
    std::unique_ptr<content::WebContents> web_contents) {
  fidl::InterfaceHandle<fuchsia::web::Frame> frame_handle;
  frames_.insert(std::make_unique<FrameImpl>(std::move(web_contents), this,
                                             frame_handle.NewRequest()));
  return frame_handle;
}

void ContextImpl::CreateFrame(
    fidl::InterfaceRequest<fuchsia::web::Frame> frame) {
  CreateFrameWithParams(fuchsia::web::CreateFrameParams(), std::move(frame));
}

void ContextImpl::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> frame) {
  // Create a WebContents to host the new Frame.
  content::WebContents::CreateParams create_params(browser_context_, nullptr);
  create_params.initially_hidden = true;
  auto web_contents = content::WebContents::Create(create_params);

  // Register the new Frame with the DevTools controller. The controller will
  // reject registration if user-debugging is requested, but it is not enabled
  // in the controller.
  const bool user_debugging_requested =
      params.has_enable_remote_debugging() && params.enable_remote_debugging();
  if (!devtools_controller_->OnFrameCreated(web_contents.get(),
                                            user_debugging_requested)) {
    frame.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  // Wrap the WebContents into a FrameImpl owned by |this|.
  auto frame_impl = std::make_unique<FrameImpl>(std::move(web_contents), this,
                                                std::move(frame));
  frames_.insert(std::move(frame_impl));
}

void ContextImpl::GetCookieManager(
    fidl::InterfaceRequest<fuchsia::web::CookieManager> request) {
  cookie_manager_bindings_.AddBinding(&cookie_manager_, std::move(request));
}

void ContextImpl::GetRemoteDebuggingPort(
    GetRemoteDebuggingPortCallback callback) {
  devtools_controller_->GetDevToolsPort(base::BindOnce(
      [](GetRemoteDebuggingPortCallback callback, uint16_t port) {
        fuchsia::web::Context_GetRemoteDebuggingPort_Result result;
        if (port == 0) {
          result.set_err(
              fuchsia::web::ContextError::REMOTE_DEBUGGING_PORT_NOT_OPENED);
        } else {
          fuchsia::web::Context_GetRemoteDebuggingPort_Response response;
          response.port = port;
          result.set_response(std::move(response));
        }
        callback(std::move(result));
      },
      std::move(callback)));
}

FrameImpl* ContextImpl::GetFrameImplForTest(
    fuchsia::web::FramePtr* frame_ptr) const {
  DCHECK(frame_ptr);

  // Find the FrameImpl whose channel is connected to |frame_ptr| by inspecting
  // the related_koids of active FrameImpls.
  zx_info_handle_basic_t handle_info{};
  zx_status_t status = frame_ptr->channel().get_info(
      ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
      nullptr, nullptr);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";
  zx_handle_t client_handle_koid = handle_info.koid;

  for (const std::unique_ptr<FrameImpl>& frame : frames_) {
    status = frame->GetBindingChannelForTest()->get_info(
        ZX_INFO_HANDLE_BASIC, &handle_info, sizeof(zx_info_handle_basic_t),
        nullptr, nullptr);
    ZX_CHECK(status == ZX_OK, status) << "zx_object_get_info";

    if (client_handle_koid == handle_info.related_koid)
      return frame.get();
  }

  return nullptr;
}
