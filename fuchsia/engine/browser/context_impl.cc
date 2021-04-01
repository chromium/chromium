// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/context_impl.h"

#include <lib/zx/channel.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/cast_streaming/public/cast_streaming_session.h"
#include "fuchsia/cast_streaming/public/network_context_getter.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

ContextImpl::ContextImpl(
    std::unique_ptr<content::BrowserContext> browser_context,
    WebEngineDevToolsController* devtools_controller)
    : browser_context_(std::move(browser_context)),
      devtools_controller_(devtools_controller),
      cookie_manager_(base::BindRepeating(&ContextImpl::GetNetworkContext,
                                          base::Unretained(this))) {
  DCHECK(browser_context_);
  DCHECK(devtools_controller_);
  devtools_controller_->OnContextCreated();

  cast_streaming::SetNetworkContextGetter(base::BindRepeating(
      &ContextImpl::GetNetworkContext, base::Unretained(this)));
}

ContextImpl::~ContextImpl() {
  devtools_controller_->OnContextDestroyed();
}

void ContextImpl::DestroyFrame(FrameImpl* frame) {
  auto iter = frames_.find(frame);
  DCHECK(iter != frames_.end());
  frames_.erase(iter);
}

bool ContextImpl::IsJavaScriptInjectionAllowed() {
  return allow_javascript_injection_;
}

void ContextImpl::CreateFrame(
    fidl::InterfaceRequest<fuchsia::web::Frame> frame) {
  CreateFrameWithParams(fuchsia::web::CreateFrameParams(), std::move(frame));
}

void ContextImpl::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> frame) {
  // FrameImpl clones the params used to create it when creating popup Frames.
  // Ensure the params can be cloned to avoid problems when handling popups.
  // TODO(fxbug.dev/65750): Consider removing this restriction if clients
  // become responsible for providing parameters for [each] popup.
  fuchsia::web::CreateFrameParams cloned_params;
  zx_status_t status = params.Clone(&cloned_params);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "CreateFrameParams Clone() failed";
    frame.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  // Create a WebContents to host the new Frame.
  content::WebContents::CreateParams create_params(browser_context_.get(),
                                                   nullptr);
  create_params.initially_hidden = true;
  auto web_contents = content::WebContents::Create(create_params);

  CreateFrameForWebContents(std::move(web_contents), std::move(params),
                            std::move(frame));
}

void ContextImpl::CreateFrameForWebContents(
    std::unique_ptr<content::WebContents> web_contents,
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> frame_request) {
  DCHECK(frame_request.is_valid());

  blink::web_pref::WebPreferences web_preferences =
      web_contents->GetOrCreateWebPreferences();

  // Register the new Frame with the DevTools controller. The controller will
  // reject registration if user-debugging is requested, but it is not enabled
  // in the controller.
  const bool user_debugging_requested =
      params.has_enable_remote_debugging() && params.enable_remote_debugging();
  if (!devtools_controller_->OnFrameCreated(web_contents.get(),
                                            user_debugging_requested)) {
    frame_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  // |params.debug_name| is not currently supported.
  // TODO(crbug.com/1051533): Determine whether it is still needed.

  // REQUIRE_USER_ACTIVATION is the default per the FIDL API.
  web_preferences.autoplay_policy =
      blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;

  if (params.has_autoplay_policy()) {
    switch (params.autoplay_policy()) {
      case fuchsia::web::AutoplayPolicy::ALLOW:
        web_preferences.autoplay_policy =
            blink::mojom::AutoplayPolicy::kNoUserGestureRequired;
        break;
      case fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION:
        web_preferences.autoplay_policy =
            blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired;
        break;
    }
  }
  web_contents->SetWebPreferences(web_preferences);

  // Verify the explicit sites filter error page content. If the parameter is
  // present, it will be provided to the FrameImpl after it is created below.
  base::Optional<std::string> explicit_sites_filter_error_page;
  if (params.has_explicit_sites_filter_error_page()) {
    explicit_sites_filter_error_page.emplace();
    if (!cr_fuchsia::StringFromMemData(
            params.explicit_sites_filter_error_page(),
            &explicit_sites_filter_error_page.value())) {
      frame_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
  }

  // FrameImpl clones the params used to create it when creating popup Frames.
  // Ensure the params can be cloned to avoid problems when creating popups.
  // TODO(http://fxbug.dev/65750): Remove this limitation once a soft migration
  // to a new solution has been completed.
  fuchsia::web::CreateFrameParams cloned_params;
  zx_status_t status = params.Clone(&cloned_params);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "CreateFrameParams clone failed";
    frame_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  // Wrap the WebContents into a FrameImpl owned by |this|.
  auto frame_impl =
      std::make_unique<FrameImpl>(std::move(web_contents), this,
                                  std::move(params), std::move(frame_request));

  if (explicit_sites_filter_error_page) {
    frame_impl->EnableExplicitSitesFilter(
        std::move(*explicit_sites_filter_error_page));
  }

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

network::mojom::NetworkContext* ContextImpl::GetNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::BrowserContext::GetDefaultStoragePartition(
             browser_context_.get())
      ->GetNetworkContext();
}
