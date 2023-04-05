// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/web_content_runner.h"

#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "fuchsia_web/runners/common/web_component.h"
#include "url/gurl.h"

namespace {

bool IsChannelClosed(const zx::channel& channel) {
  zx_signals_t observed = 0u;
  zx_status_t status =
      channel.wait_one(ZX_ERR_PEER_CLOSED, zx::time(), &observed);
  return status == ZX_OK;
}

}  // namespace

WebContentRunner::WebInstanceConfig::WebInstanceConfig()
    : extra_args(base::CommandLine::NO_PROGRAM) {}
WebContentRunner::WebInstanceConfig::~WebInstanceConfig() = default;

WebContentRunner::WebInstanceConfig::WebInstanceConfig(WebInstanceConfig&&) =
    default;
WebContentRunner::WebInstanceConfig&
WebContentRunner::WebInstanceConfig::operator=(WebInstanceConfig&&) = default;

WebContentRunner::WebContentRunner(
    CreateWebInstanceAndContextCallback create_web_instance_callback,
    GetWebInstanceConfigCallback get_web_instance_config_callback)
    : create_web_instance_callback_(std::move(create_web_instance_callback)),
      get_web_instance_config_callback_(
          std::move(get_web_instance_config_callback)) {
  DCHECK(create_web_instance_callback_);
  DCHECK(get_web_instance_config_callback_);
}

WebContentRunner::WebContentRunner(
    CreateWebInstanceAndContextCallback create_web_instance_callback,
    WebInstanceConfig web_instance_config)
    : create_web_instance_callback_(std::move(create_web_instance_callback)) {
  CreateWebInstanceAndContext(std::move(web_instance_config));
}

WebContentRunner::~WebContentRunner() = default;

fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
WebContentRunner::GetFrameHostRequestHandler() {
  return [this](fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
    EnsureWebInstanceAndContext();
    fdio_service_connect_at(web_instance_services_.channel().get(),
                            fuchsia::web::FrameHost::Name_,
                            request.TakeChannel().release());
  };
}

void WebContentRunner::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> request) {
  EnsureWebInstanceAndContext();

  context_->CreateFrameWithParams(std::move(params), std::move(request));
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
  if (components_.empty() && on_empty_callback_)
    std::move(on_empty_callback_).Run();
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  components_.insert(std::move(component));
}

void WebContentRunner::SetOnEmptyCallback(base::OnceClosure on_empty) {
  on_empty_callback_ = std::move(on_empty);
}

void WebContentRunner::DestroyWebContext() {
  DCHECK(get_web_instance_config_callback_);
  context_ = nullptr;
}

void WebContentRunner::CloseFrameWithTimeout(fuchsia::web::FramePtr frame,
                                             base::TimeDelta timeout) {
  // Signal `frame` to close within the desired `timeout`, and store it to
  // `closing_frames_` which will retain it until it closes.
  frame->Close(std::move(
      fuchsia::web::FrameCloseRequest().set_timeout(timeout.ToZxDuration())));
  closing_frames_.AddInterfacePtr(std::move(frame));
}

void WebContentRunner::EnsureWebInstanceAndContext() {
  // Synchronously check whether the web.Context channel has closed, to reduce
  // the chance of issuing CreateFrameWithParams() to an already-closed channel.
  // This avoids potentially flaking a test - see crbug.com/1173418.
  if (context_ && IsChannelClosed(context_.channel()))
    context_.Unbind();

  if (!context_) {
    DCHECK(get_web_instance_config_callback_);
    CreateWebInstanceAndContext(get_web_instance_config_callback_.Run());
  }
}

void WebContentRunner::CreateWebInstanceAndContext(WebInstanceConfig config) {
  create_web_instance_callback_.Run(std::move(config.params),
                                    web_instance_services_.NewRequest(),
                                    config.extra_args);
  zx_status_t result = fdio_service_connect_at(
      web_instance_services_.channel().get(), fuchsia::web::Context::Name_,
      context_.NewRequest().TakeChannel().release());
  ZX_LOG_IF(ERROR, result != ZX_OK, result)
      << "fdio_service_connect_at(web.Context)";
  context_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Connection to web.Context lost.";
  });
}
