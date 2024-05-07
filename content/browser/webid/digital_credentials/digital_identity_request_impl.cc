// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
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
#include "third_party/re2/src/re2/re2.h"

using base::Value;
using blink::mojom::RequestDigitalIdentityStatus;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;

namespace content {
namespace {

constexpr char kMdlDocumentType[] = "org.iso.18013.5.1.mDL";
constexpr char kOpenid4vpAgeOverPathRegex[] =
    R"(\$\['org\.iso\.18013\.5\.1'\]\['age_over_\d\d'\])";

// Returns entry if `dict` has a list with a single dict element for key
// `list_key`.
const base::Value::Dict* FindSingleElementListEntry(
    const base::Value::Dict& dict,
    const std::string& list_key) {
  const base::Value::List* list = dict.FindList(list_key);
  if (!list || list->size() != 1u) {
    return nullptr;
  }
  return list->front().GetIfDict();
}

}  // anonymous namespace

// static
void DigitalIdentityRequestImpl::Create(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver) {
  // DigitalIdentityRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  new DigitalIdentityRequestImpl(host, std::move(receiver));
}

// static
bool DigitalIdentityRequestImpl::IsOnlyRequestingAge(
    const base::Value& request) {
  if (!request.is_dict()) {
    return false;
  }

  const base::Value::Dict& request_dict = request.GetDict();
  const base::Value::Dict* presentation_dict =
      request_dict.FindDict("presentation_definition");
  if (!presentation_dict) {
    return false;
  }

  const base::Value::Dict* input_descriptor_dict =
      FindSingleElementListEntry(*presentation_dict, "input_descriptors");
  if (!input_descriptor_dict) {
    return false;
  }

  const std::string* input_descriptor_id =
      input_descriptor_dict->FindString("id");
  if (!input_descriptor_id || *input_descriptor_id != kMdlDocumentType) {
    return false;
  }

  const base::Value::Dict* constraints_dict =
      input_descriptor_dict->FindDict("constraints");
  if (!constraints_dict) {
    return false;
  }

  const base::Value::Dict* field_dict =
      FindSingleElementListEntry(*constraints_dict, "fields");
  if (!field_dict) {
    return false;
  }

  const base::Value::List* field_paths = field_dict->FindList("path");
  if (!field_paths || field_paths->size() != 1u ||
      !field_paths->front().is_string()) {
    return false;
  }

  return re2::RE2::FullMatch(field_paths->front().GetString(),
                             re2::RE2(kOpenid4vpAgeOverPathRegex));
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

std::string BuildRequest(blink::mojom::DigitalCredentialProviderPtr provider) {
  auto result = Value::Dict();

  if (!provider->protocol) {
    return "";
  }
  result.Set("protocol", *provider->protocol);

  if (!provider->request) {
    return "";
  }
  result.Set("request", *provider->request);

  base::Value::Dict out =
      Value::Dict().Set("providers", Value::List().Append(std::move(result)));
  return WriteJsonWithOptions(out, base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or("");
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

  std::optional<std::string> request_json_string =
      digital_credential_provider->request;
  std::string request_to_send =
      BuildRequest(std::move(digital_credential_provider));

  if (!request_json_string || request_to_send.empty()) {
    CompleteRequest("", RequestStatusForMetrics::kErrorOther);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *request_json_string,
      base::BindOnce(&DigitalIdentityRequestImpl::OnRequestJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Abort() {
  CompleteRequestWithStatus(RequestDigitalIdentityStatus::kErrorCanceled, "",
                            RequestStatusForMetrics::kErrorAborted);
}

void DigitalIdentityRequestImpl::OnRequestJsonParsed(
    std::string request_to_send,
    data_decoder::DataDecoder::ValueOrError parsed_result) {
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

  bool is_only_requesting_age =
      parsed_result.has_value() && IsOnlyRequestingAge(*parsed_result);
  provider_->Request(
      WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
      request_to_send,
      base::BindOnce(&DigitalIdentityRequestImpl::ShowInterstitialIfNeeded,
                     weak_ptr_factory_.GetWeakPtr(), is_only_requesting_age));
}

void DigitalIdentityRequestImpl::ShowInterstitialIfNeeded(
    bool is_only_requesting_age,
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
      is_only_requesting_age,
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
