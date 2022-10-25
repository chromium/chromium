// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/context_impl.h"

#include <lib/fpromise/result.h>
#include <lib/zx/channel.h>
#include <lib/zx/handle.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/strings/stringprintf.h"
#include "build/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/web_engine_devtools_controller.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "components/cast_streaming/browser/public/network_context_getter.h"  // nogncheck
#endif

ContextImpl::ContextImpl(
    std::unique_ptr<content::BrowserContext> browser_context,
    inspect::Node inspect_node,
    WebEngineDevToolsController* devtools_controller)
    : browser_context_(std::move(browser_context)),
      devtools_controller_(devtools_controller),
      inspect_node_(std::move(inspect_node)),
      cookie_manager_(base::BindRepeating(&ContextImpl::GetNetworkContext,
                                          base::Unretained(this))) {
  DCHECK(browser_context_);
  DCHECK(devtools_controller_);
}

ContextImpl::~ContextImpl() = default;

void ContextImpl::DestroyFrame(FrameImpl* frame) {
  auto iter = frames_.find(frame);
  DCHECK(iter != frames_.end());
  frames_.erase(iter);
}

bool ContextImpl::IsJavaScriptInjectionAllowed() {
  return allow_javascript_injection_;
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
void ContextImpl::SetCastStreamingEnabled() {
  cast_streaming_enabled_ = true;
  cast_streaming::SetNetworkContextGetter(base::BindRepeating(
      &ContextImpl::GetNetworkContext, base::Unretained(this)));
}
#endif

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

FrameImpl* ContextImpl::CreateFrameForWebContents(
    std::unique_ptr<content::WebContents> web_contents,
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> frame_request) {
  DCHECK(frame_request.is_valid());

  // Register the new Frame with the DevTools controller. The controller will
  // reject registration if user-debugging is requested, but it is not enabled
  // in the controller.
  const bool user_debugging_requested =
      params.has_enable_remote_debugging() && params.enable_remote_debugging();
  if (!devtools_controller_->OnFrameCreated(web_contents.get(),
                                            user_debugging_requested)) {
    frame_request.Close(ZX_ERR_INVALID_ARGS);
    return nullptr;
  }

  // |params.debug_name| is not currently supported.
  // TODO(crbug.com/1051533): Determine whether it is still needed.

  // Verify the explicit sites filter error page content. If the parameter is
  // present, it will be provided to the FrameImpl after it is created below.
  absl::optional<std::string> explicit_sites_filter_error_page;
  if (params.has_explicit_sites_filter_error_page()) {
    explicit_sites_filter_error_page =
        base::StringFromMemData(params.explicit_sites_filter_error_page());
    if (!explicit_sites_filter_error_page) {
      frame_request.Close(ZX_ERR_INVALID_ARGS);
      return nullptr;
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
    return nullptr;
  }

  // Wrap the WebContents into a FrameImpl owned by |this|.
  auto inspect_node_name =
      base::StringPrintf("frame-%lu", *base::GetKoid(frame_request.channel()));
  auto frame_impl = std::make_unique<FrameImpl>(
      std::move(web_contents), this, std::move(params),
      inspect_node_.CreateChild(inspect_node_name), std::move(frame_request));

  if (explicit_sites_filter_error_page) {
    frame_impl->EnableExplicitSitesFilter(
        std::move(*explicit_sites_filter_error_page));
  }

  FrameImpl* frame_ptr = frame_impl.get();
  frames_.insert(std::move(frame_impl));
  return frame_ptr;
}

void ContextImpl::GetCookieManager(
    fidl::InterfaceRequest<fuchsia::web::CookieManager> request) {
  cookie_manager_bindings_.AddBinding(&cookie_manager_, std::move(request));
}

void ContextImpl::GetRemoteDebuggingPort(
    GetRemoteDebuggingPortCallback callback) {
  devtools_controller_->GetDevToolsPort(base::BindOnce(
      [](GetRemoteDebuggingPortCallback callback, uint16_t port) {
        if (port == 0) {
          callback(fpromise::error(
              fuchsia::web::ContextError::REMOTE_DEBUGGING_PORT_NOT_OPENED));
        } else {
          callback(fpromise::ok(port));
        }
      },
      std::move(callback)));
}

FrameImpl* ContextImpl::GetFrameImplForTest(
    fuchsia::web::FramePtr* frame_ptr) const {
  DCHECK(frame_ptr);

  // Find the FrameImpl whose channel is connected to |frame_ptr| by inspecting
  // the "related" KOIDs of active FrameImpls.
  zx_koid_t channel_koid = base::GetKoid(frame_ptr->channel()).value();
  for (const std::unique_ptr<FrameImpl>& frame : frames_) {
    zx_koid_t peer_koid =
        base::GetRelatedKoid(*frame->GetBindingChannelForTest())  // IN-TEST
            .value();

    if (peer_koid == channel_koid)
      return frame.get();
  }

  return nullptr;
}

network::mojom::NetworkContext* ContextImpl::GetNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_context_->GetDefaultStoragePartition()->GetNetworkContext();
}
