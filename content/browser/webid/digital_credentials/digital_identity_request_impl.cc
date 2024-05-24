// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/digital_identity_request_impl.h"

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
using DigitalIdentityInterstitialAbortCallback =
    content::ContentBrowserClient::DigitalIdentityInterstitialAbortCallback;

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

DigitalIdentityRequestImpl::RenderFrameHostLifecycleObserver::
    RenderFrameHostLifecycleObserver(
        const raw_ptr<WebContents> web_contents,
        const raw_ptr<RenderFrameHost> render_frame_host,
        DigitalIdentityInterstitialAbortCallback abort_callback)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      abort_callback_(std::move(abort_callback)) {}

DigitalIdentityRequestImpl::RenderFrameHostLifecycleObserver::
    ~RenderFrameHostLifecycleObserver() = default;

void DigitalIdentityRequestImpl::RenderFrameHostLifecycleObserver::
    RenderFrameHostStateChanged(
        content::RenderFrameHost* rfh,
        content::RenderFrameHost::LifecycleState old_state,
        content::RenderFrameHost::LifecycleState new_state) {
  if (rfh != render_frame_host_.get() ||
      new_state == content::RenderFrameHost::LifecycleState::kActive ||
      !abort_callback_) {
    return;
  }
  std::move(abort_callback_).Run();
}

void DigitalIdentityRequestImpl::RenderFrameHostLifecycleObserver::
    RenderFrameHostChanged(RenderFrameHost* old_host,
                           RenderFrameHost* new_host) {
  if (old_host != render_frame_host_.get() || !abort_callback_) {
    return;
  }
  std::move(abort_callback_).Run();
}

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
    const base::expected<std::string, RequestStatusForMetrics>& response) {
  CompleteRequestWithStatus(response.has_value()
                                ? RequestDigitalIdentityStatus::kSuccess
                                : RequestDigitalIdentityStatus::kError,
                            response);
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    RequestDigitalIdentityStatus status,
    const base::expected<std::string, RequestStatusForMetrics>& response) {
  // Invalidate pending requests in case that the request gets aborted.
  weak_ptr_factory_.InvalidateWeakPtrs();

  provider_.reset();
  render_frame_host_lifecycle_observer_.reset();
  update_interstitial_on_abort_callback_.Reset();

  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                response.has_value()
                                    ? RequestStatusForMetrics::kSuccess
                                    : response.error());

  std::move(callback_).Run(status, base::OptionalFromExpected(response));
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

  if (!render_frame_host().HasTransientUserActivation()) {
    CompleteRequest(base::unexpected(RequestStatusForMetrics::kErrorOther));
    return;
  }

  RenderFrameHost* render_frame_host_ptr = &render_frame_host();
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_ptr);
  if (!web_contents) {
    CompleteRequest(base::unexpected(RequestStatusForMetrics::kErrorOther));
    return;
  }

  render_frame_host_lifecycle_observer_.reset(
      new RenderFrameHostLifecycleObserver(
          web_contents, render_frame_host_ptr,
          base::BindOnce(&DigitalIdentityRequestImpl::Abort,
                         weak_ptr_factory_.GetWeakPtr())));

  std::optional<std::string> request_json_string =
      digital_credential_provider->request;
  std::string request_to_send =
      BuildRequest(std::move(digital_credential_provider));

  if (!request_json_string || request_to_send.empty()) {
    CompleteRequest(base::unexpected(RequestStatusForMetrics::kErrorOther));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *request_json_string,
      base::BindOnce(&DigitalIdentityRequestImpl::OnRequestJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Abort() {
  if (update_interstitial_on_abort_callback_) {
    std::move(update_interstitial_on_abort_callback_).Run();
  }

  CompleteRequestWithStatus(
      RequestDigitalIdentityStatus::kErrorCanceled,
      base::unexpected(RequestStatusForMetrics::kErrorAborted));
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
                       weak_ptr_factory_.GetWeakPtr(), "fake_test_token"),
        base::Milliseconds(1));
    return;
  }

  provider_ = GetContentClient()->browser()->CreateDigitalIdentityProvider();
  if (!provider_) {
    CompleteRequest(base::unexpected(RequestStatusForMetrics::kErrorOther));
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
    base::expected<std::string, RequestStatusForMetrics> response) {
  if (!response.has_value()) {
    CompleteRequest(response);
    return;
  }

  if (!render_frame_host().IsActive()) {
    CompleteRequest(base::unexpected(RequestStatusForMetrics::kErrorOther));
    return;
  }

  update_interstitial_on_abort_callback_ =
      GetContentClient()->browser()->ShowDigitalIdentityInterstitialIfNeeded(
          *WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
          is_only_requesting_age,
          base::BindOnce(&DigitalIdentityRequestImpl::OnInterstitialDone,
                         weak_ptr_factory_.GetWeakPtr(), response.value()));
}

void DigitalIdentityRequestImpl::OnInterstitialDone(
    const std::string& response,
    RequestStatusForMetrics status_after_interstitial) {
  CompleteRequest(
      status_after_interstitial == RequestStatusForMetrics::kSuccess
          ? base::expected<std::string, RequestStatusForMetrics>(response)
          : base::unexpected(status_after_interstitial));
}

}  // namespace content
