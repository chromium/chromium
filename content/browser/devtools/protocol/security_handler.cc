// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {
namespace protocol {

// static
std::vector<SecurityHandler*> SecurityHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<SecurityHandler>(Security::Metainfo::domainName);
}

SecurityHandler::SecurityHandler()
    : DevToolsDomainHandler(Security::Metainfo::domainName),
      enabled_(false),
      host_(nullptr) {
}

SecurityHandler::~SecurityHandler() = default;

void SecurityHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Security::Frontend>(dispatcher->channel());
  Security::Dispatcher::wire(dispatcher, this);
}

void SecurityHandler::AttachToRenderFrameHost() {
  DCHECK(host_);
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  WebContentsObserver::Observe(web_contents);

  // Send an initial DidChangeVisibleSecurityState event.
  DCHECK(enabled_);
  DidChangeVisibleSecurityState();
}

void SecurityHandler::SetRenderer(int process_host_id,
                                  RenderFrameHostImpl* frame_host) {
  host_ = frame_host;
  if (enabled_ && host_)
    AttachToRenderFrameHost();
}

void SecurityHandler::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (cert_error_override_mode_ == CertErrorOverrideMode::kHandleEvents) {
    BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        BackForwardCacheDisable::DisabledReason(
            BackForwardCacheDisable::DisabledReasonId::kSecurityHandler));
    FlushPendingCertificateErrorNotifications();
  }
}

void SecurityHandler::FlushPendingCertificateErrorNotifications() {
  for (auto& callback : cert_error_callbacks_) {
    std::move(callback).second.Run(
        content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }
  cert_error_callbacks_.clear();
}

bool SecurityHandler::IsIgnoreCertificateErrorsSet() const {
  return cert_error_override_mode_ == CertErrorOverrideMode::kIgnoreAll;
}

bool SecurityHandler::NotifyCertificateError(int cert_error,
                                             const GURL& request_url,
                                             CertErrorCallback handler) {
  if (cert_error_override_mode_ == CertErrorOverrideMode::kIgnoreAll) {
    if (handler)
      std::move(handler).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE);
    return true;
  }

  if (!enabled_)
    return false;

  frontend_->CertificateError(++last_cert_error_id_,
                              net::ErrorToShortString(cert_error),
                              request_url.spec());

  if (!handler ||
      cert_error_override_mode_ != CertErrorOverrideMode::kHandleEvents) {
    return false;
  }

  cert_error_callbacks_[last_cert_error_id_] = std::move(handler);
  return true;
}

Response SecurityHandler::Enable() {
  if (host_) {
    Response response = AssureTopLevelActiveFrame();
    if (response.IsError())
      return response;
  }
  enabled_ = true;
  if (host_)
    AttachToRenderFrameHost();

  return Response::Success();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
  WebContentsObserver::Observe(nullptr);
  FlushPendingCertificateErrorNotifications();
  return Response::Success();
}

Response SecurityHandler::HandleCertificateError(int event_id,
                                                 const String& action) {
  if (!base::Contains(cert_error_callbacks_, event_id)) {
    return Response::ServerError(
        String("Unknown event id: " + base::NumberToString(event_id)));
  }
  content::CertificateRequestResultType type =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  Response response = Response::Success();
  if (action == Security::CertificateErrorActionEnum::Continue) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE;
  } else if (action == Security::CertificateErrorActionEnum::Cancel) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  } else {
    response = Response::ServerError(
        String("Unknown Certificate Error Action: " + action));
  }
  std::move(cert_error_callbacks_[event_id]).Run(type);
  cert_error_callbacks_.erase(event_id);
  return response;
}

Response SecurityHandler::SetOverrideCertificateErrors(bool override) {
  if (override) {
    if (!enabled_)
      return Response::ServerError("Security domain not enabled");
    if (cert_error_override_mode_ == CertErrorOverrideMode::kIgnoreAll) {
      return Response::ServerError(
          "Certificate errors are already being ignored.");
    }
    cert_error_override_mode_ = CertErrorOverrideMode::kHandleEvents;
  } else {
    cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
    FlushPendingCertificateErrorNotifications();
  }
  return Response::Success();
}

Response SecurityHandler::SetIgnoreCertificateErrors(bool ignore) {
  if (ignore) {
    if (cert_error_override_mode_ == CertErrorOverrideMode::kHandleEvents)
      return Response::ServerError(
          "Certificate errors are already overridden.");
    cert_error_override_mode_ = CertErrorOverrideMode::kIgnoreAll;
  } else {
    cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
  }
  return Response::Success();
}

Response SecurityHandler::AssureTopLevelActiveFrame() {
  DCHECK(host_);
  constexpr char kCommandIsOnlyAvailableAtTopTarget[] =
      "Command can only be executed on top-level targets";
  if (host_->GetParentOrOuterDocument())
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);

  constexpr char kErrorInactivePage[] = "Not attached to an active page";
  if (!host_->IsActive())
    return Response::ServerError(kErrorInactivePage);

  return Response::Success();
}

}  // namespace protocol
}  // namespace content
