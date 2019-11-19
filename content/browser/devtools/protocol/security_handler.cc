// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/security_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"

namespace content {
namespace protocol {

using Explanations = protocol::Array<Security::SecurityStateExplanation>;

namespace {

std::string SecurityStyleToProtocolSecurityState(
    blink::SecurityStyle security_style) {
  switch (security_style) {
    case blink::SecurityStyle::kUnknown:
      return Security::SecurityStateEnum::Unknown;
    case blink::SecurityStyle::kNeutral:
      return Security::SecurityStateEnum::Neutral;
    case blink::SecurityStyle::kInsecure:
      return Security::SecurityStateEnum::Insecure;
    case blink::SecurityStyle::kSecure:
      return Security::SecurityStateEnum::Secure;
    case blink::SecurityStyle::kInsecureBroken:
      return Security::SecurityStateEnum::InsecureBroken;
    default:
      NOTREACHED();
      return Security::SecurityStateEnum::Unknown;
  }
}

std::string MixedContentTypeToProtocolMixedContentType(
    blink::WebMixedContentContextType mixed_content_type) {
  switch (mixed_content_type) {
    case blink::WebMixedContentContextType::kNotMixedContent:
      return Security::MixedContentTypeEnum::None;
    case blink::WebMixedContentContextType::kBlockable:
      return Security::MixedContentTypeEnum::Blockable;
    case blink::WebMixedContentContextType::kOptionallyBlockable:
      return Security::MixedContentTypeEnum::OptionallyBlockable;
    case blink::WebMixedContentContextType::kShouldBeBlockable:
      // kShouldBeBlockable is not used for explanations.
      NOTREACHED();
      return Security::MixedContentTypeEnum::OptionallyBlockable;
    default:
      NOTREACHED();
      return Security::MixedContentTypeEnum::None;
  }
}

void AddExplanations(
    const std::string& security_style,
    const std::vector<SecurityStyleExplanation>& explanations_to_add,
    Explanations* explanations) {
  for (const auto& it : explanations_to_add) {
    auto certificate = std::make_unique<protocol::Array<String>>();
    if (it.certificate) {
      certificate->emplace_back();
      base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(
                             it.certificate->cert_buffer()),
                         &certificate->back());

      for (const auto& cert : it.certificate->intermediate_buffers()) {
        certificate->emplace_back();
        base::Base64Encode(
            net::x509_util::CryptoBufferAsStringPiece(cert.get()),
            &certificate->back());
      }
    }

    auto recommendations =
        std::make_unique<protocol::Array<String>>(it.recommendations);

    explanations->emplace_back(
        Security::SecurityStateExplanation::Create()
            .SetSecurityState(security_style)
            .SetTitle(it.title)
            .SetSummary(it.summary)
            .SetDescription(it.description)
            .SetCertificate(std::move(certificate))
            .SetMixedContentType(MixedContentTypeToProtocolMixedContentType(
                it.mixed_content_type))
            .SetRecommendations(std::move(recommendations))
            .Build());
  }
}

}  // namespace

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

SecurityHandler::~SecurityHandler() {
}

void SecurityHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Security::Frontend(dispatcher->channel()));
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

void SecurityHandler::DidChangeVisibleSecurityState() {
  DCHECK(enabled_);
  if (!web_contents()->GetDelegate())
    return;

  SecurityStyleExplanations security_style_explanations;
  blink::SecurityStyle security_style =
      web_contents()->GetDelegate()->GetSecurityStyle(
          web_contents(), &security_style_explanations);

  const std::string security_state =
      SecurityStyleToProtocolSecurityState(security_style);

  auto explanations = std::make_unique<Explanations>();
  AddExplanations(Security::SecurityStateEnum::Insecure,
                  security_style_explanations.insecure_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Neutral,
                  security_style_explanations.neutral_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Secure,
                  security_style_explanations.secure_explanations,
                  explanations.get());
  AddExplanations(Security::SecurityStateEnum::Info,
                  security_style_explanations.info_explanations,
                  explanations.get());

  // We can set everything to default values because this field is ignored by
  // the frontend, though it's still required by the protocol. Once the field is
  // deleted in the protocol, we can delete it here.
  std::unique_ptr<Security::InsecureContentStatus> insecure_status =
      Security::InsecureContentStatus::Create()
          .SetRanMixedContent(false)
          .SetDisplayedMixedContent(false)
          .SetContainedMixedForm(false)
          .SetRanContentWithCertErrors(false)
          .SetDisplayedContentWithCertErrors(false)
          .SetRanInsecureContentStyle(Security::SecurityStateEnum::Unknown)
          .SetDisplayedInsecureContentStyle(
              Security::SecurityStateEnum::Unknown)
          .Build();

  frontend_->SecurityStateChanged(
      security_state,
      security_style_explanations.scheme_is_cryptographic,
      std::move(explanations),
      std::move(insecure_status),
      Maybe<std::string>(security_style_explanations.summary));
}

void SecurityHandler::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (cert_error_override_mode_ == CertErrorOverrideMode::kHandleEvents) {
    BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        "content::protocol::SecurityHandler");
    FlushPendingCertificateErrorNotifications();
  }
}

void SecurityHandler::FlushPendingCertificateErrorNotifications() {
  for (auto callback : cert_error_callbacks_)
    callback.second.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  cert_error_callbacks_.clear();
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
  enabled_ = true;
  if (host_)
    AttachToRenderFrameHost();

  return Response::OK();
}

Response SecurityHandler::Disable() {
  enabled_ = false;
  cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
  WebContentsObserver::Observe(nullptr);
  FlushPendingCertificateErrorNotifications();
  return Response::OK();
}

Response SecurityHandler::HandleCertificateError(int event_id,
                                                 const String& action) {
  if (cert_error_callbacks_.find(event_id) == cert_error_callbacks_.end()) {
    return Response::Error(
        String("Unknown event id: " + std::to_string(event_id)));
  }
  content::CertificateRequestResultType type =
      content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  Response response = Response::OK();
  if (action == Security::CertificateErrorActionEnum::Continue) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE;
  } else if (action == Security::CertificateErrorActionEnum::Cancel) {
    type = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  } else {
    response =
        Response::Error(String("Unknown Certificate Error Action: " + action));
  }
  cert_error_callbacks_[event_id].Run(type);
  cert_error_callbacks_.erase(event_id);
  return response;
}

Response SecurityHandler::SetOverrideCertificateErrors(bool override) {
  if (override) {
    if (!enabled_)
      return Response::Error("Security domain not enabled");
    if (cert_error_override_mode_ == CertErrorOverrideMode::kIgnoreAll)
      return Response::Error("Certificate errors are already being ignored.");
    cert_error_override_mode_ = CertErrorOverrideMode::kHandleEvents;
  } else {
    cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
    FlushPendingCertificateErrorNotifications();
  }
  return Response::OK();
}

Response SecurityHandler::SetIgnoreCertificateErrors(bool ignore) {
  if (ignore) {
    if (cert_error_override_mode_ == CertErrorOverrideMode::kHandleEvents)
      return Response::Error("Certificate errors are already overridden.");
    cert_error_override_mode_ = CertErrorOverrideMode::kIgnoreAll;
  } else {
    cert_error_override_mode_ = CertErrorOverrideMode::kDisabled;
  }
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
