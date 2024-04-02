// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/digital_credentials/digital_identity_provider_utils.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using base::Value;
using blink::mojom::RequestDigitalIdentityStatus;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;

namespace content {

// static
void DigitalIdentityRequestImpl::Create(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver) {
  // DigitalIdentityRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new DigitalIdentityRequestImpl(host, std::move(receiver));
}

DigitalIdentityRequestImpl::DigitalIdentityRequestImpl(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver)
    : DocumentService(host, std::move(receiver)) {}

DigitalIdentityRequestImpl::~DigitalIdentityRequestImpl() = default;

void DigitalIdentityRequestImpl::CompleteRequest(
    const std::string& response,
    RequestStatusForMetrics status_for_metrics) {
  CompleteRequestWithStatus(
      (status_for_metrics == RequestStatusForMetrics::kSuccess)
          ? RequestDigitalIdentityStatus::kSuccess
          : RequestDigitalIdentityStatus::kError,
      response, status_for_metrics);
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    RequestDigitalIdentityStatus status,
    const std::string& response,
    RequestStatusForMetrics status_for_metrics) {
  // Invalidate pending requests in case that the request gets aborted.
  weak_ptr_factory_.InvalidateWeakPtrs();
  provider_.reset();

  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                status_for_metrics);

  std::move(callback_).Run(status, response);
}

base::Value::Dict BuildRequest(
    blink::mojom::DigitalCredentialProviderPtr provider) {
  auto result = Value::Dict();

  if (provider->params) {
    auto params = Value::Dict();
    for (const auto& pair : *provider->params) {
      params.Set(pair.first, pair.second);
    }
    result.Set("params", std::move(params));
  }

  if (provider->selector) {
    auto formats = Value::List();
    for (auto& format : provider->selector->format) {
      formats.Append(format);
    }

    auto fields = Value::List();

    if (provider->selector->doctype) {
      auto doctype = Value::Dict();
      doctype.Set("name", "doctype");
      doctype.Set("equals", provider->selector->doctype.value());
      fields.Append(std::move(doctype));
    }

    for (auto& value : provider->selector->fields) {
      auto field = Value::Dict();
      field.Set("name", value->name);
      if (value->equals) {
        field.Set("equals", value->equals.value());
      }
      fields.Append(std::move(field));
    }

    result.Set("selector", Value::Dict().Set("fields", std::move(fields)));
    result.Set("responseFormat", std::move(formats));
  }

  if (provider->protocol) {
    result.Set("protocol", *provider->protocol);
  }

  if (provider->request) {
    result.Set("request", *provider->request);
  }

  if (provider->publicKey) {
    result.Set("publicKey", *provider->publicKey);
  }

  return Value::Dict().Set("providers",
                           Value::List().Append(std::move(result)));
}

void DigitalIdentityRequestImpl::Request(
    blink::mojom::DigitalCredentialProviderPtr digital_credential_provider,
    RequestCallback callback) {
  if (!IsWebIdentityDigitalCredentialsEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError, std::nullopt);
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            std::nullopt);
    return;
  }

  callback_ = std::move(callback);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(), "fake_test_token",
                       RequestStatusForMetrics::kSuccess),
        base::Milliseconds(1));
    return;
  }

  provider_ = CreateProvider();
  if (!provider_) {
    CompleteRequest("", RequestStatusForMetrics::kErrorOther);
    return;
  }

  auto request = BuildRequest(std::move(digital_credential_provider));
  provider_->Request(
      WebContents::FromRenderFrameHost(&render_frame_host()), origin(), request,
      base::BindOnce(&DigitalIdentityRequestImpl::ShowInterstitialIfNeeded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityRequestImpl::Abort() {
  CompleteRequestWithStatus(RequestDigitalIdentityStatus::kErrorCanceled, "",
                            RequestStatusForMetrics::kErrorAborted);
}

void DigitalIdentityRequestImpl::ShowInterstitialIfNeeded(
    const std::string& response,
    RequestStatusForMetrics status_for_metrics) {
  if (status_for_metrics != RequestStatusForMetrics::kSuccess) {
    CompleteRequest("", status_for_metrics);
    return;
  }

  if (!render_frame_host().IsActive()) {
    CompleteRequest("", RequestStatusForMetrics::kErrorOther);
    return;
  }

  GetContentClient()->browser()->ShowDigitalIdentityInterstitialIfNeeded(
      *WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
      base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), response));
}

std::unique_ptr<DigitalIdentityProvider>
DigitalIdentityRequestImpl::CreateProvider() {
  // A provider may only be created in browser tests by this moment.
  std::unique_ptr<DigitalIdentityProvider> provider =
      GetContentClient()->browser()->CreateDigitalIdentityProvider();

  if (!provider) {
    return CreateDigitalIdentityProvider();
  }
  return provider;
}

}  // namespace content
