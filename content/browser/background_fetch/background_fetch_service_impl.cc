// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

namespace {
// Maximum length of a developer-provided |developer_id| for a Background Fetch.
constexpr size_t kMaxDeveloperIdLength = 1024 * 1024;
}  // namespace

// static
void BackgroundFetchServiceImpl::CreateForWorker(
    const net::NetworkIsolationKey& network_isolation_key,
    const ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver) {
  // TODO(rayankans): Remove `network_isolation_key` parameter since it's no
  // longer used.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(info.process_id);

  if (!render_process_host)
    return;

  auto* partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());
  scoped_refptr<BackgroundFetchContext> context =
      WrapRefCounted(partition->GetBackgroundFetchContext());

  // The renderer should have checked and disallowed the request for fenced
  // frames and thrown an exception in blink::BackgroundFetchManager. Ignore the
  // request and mark it as bad if the renderer side check didn't happen for
  // some reason.
  scoped_refptr<ServiceWorkerRegistration> registration =
      partition->GetServiceWorkerContext()->GetLiveRegistration(
          info.registration_id);
  if (registration && registration->ancestor_frame_type() ==
                          blink::mojom::AncestorFrameType::kFencedFrame) {
    bad_message::ReceivedBadMessage(
        render_process_host, bad_message::BFSI_CREATE_FOR_WORKER_FENCED_FRAME);
    return;
  }

  mojo::MakeSelfOwnedReceiver(std::make_unique<BackgroundFetchServiceImpl>(
                                  std::move(context), info.storage_key,
                                  info.storage_key.ToPartialNetIsolationInfo(),
                                  render_process_host, /*rfh=*/nullptr),
                              std::move(receiver));
}

// static
void BackgroundFetchServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames and throw exception in blink::BackgroundFetchManager. Ignore the
    // request and mark it as bad if it didn't happen for some reason.
    // TODO(crbug.com/40205566) Follow-up on this line depending on the
    // conclusion at
    // https://groups.google.com/a/chromium.org/g/navigation-dev/c/BZLlGsL2-64
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::BFSI_CREATE_FOR_FRAME_FENCED_FRAME);
    return;
  }

  auto* rfhi = static_cast<RenderFrameHostImpl*>(render_frame_host);
  RenderProcessHost* render_process_host = rfhi->GetProcess();
  DCHECK(render_process_host);

  scoped_refptr<BackgroundFetchContext> context =
      WrapRefCounted(static_cast<StoragePartitionImpl*>(
                         render_process_host->GetStoragePartition())
                         ->GetBackgroundFetchContext());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BackgroundFetchServiceImpl>(
          std::move(context), rfhi->GetStorageKey(),
          rfhi->GetIsolationInfoForSubresources(), rfhi->GetProcess(), rfhi),
      std::move(receiver));
}

BackgroundFetchServiceImpl::BackgroundFetchServiceImpl(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    blink::StorageKey storage_key,
    net::IsolationInfo isolation_info,
    RenderProcessHost* rph,
    RenderFrameHostImpl* rfh)
    : background_fetch_context_(std::move(background_fetch_context)),
      storage_key_(std::move(storage_key)),
      isolation_info_(std::move(isolation_info)),
      rph_id_(rph->GetID()),
      rfh_id_(rfh ? rfh->GetGlobalId() : GlobalRenderFrameHostId()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(background_fetch_context_);
}

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchServiceImpl::Fetch(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    FetchCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateDeveloperId(developer_id) || !ValidateRequests(requests)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        /* registration= */ nullptr);
    return;
  }

  // New |unique_id|, since this is a new Background Fetch registration. This is
  // the only place new |unique_id|s should be created outside of tests.
  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, storage_key_, developer_id,
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  background_fetch_context_->StartFetch(
      registration_id, std::move(requests), std::move(options), icon,
      std::move(ukm_data), RenderProcessHost::FromID(rph_id_),
      RenderFrameHostImpl::FromID(rfh_id_), isolation_info_,
      std::move(callback));
}

void BackgroundFetchServiceImpl::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_fetch_context_->GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateDeveloperId(developer_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        /* registration= */ nullptr);
    return;
  }

  background_fetch_context_->GetRegistration(service_worker_registration_id,
                                             storage_key_, developer_id,
                                             std::move(callback));
}

void BackgroundFetchServiceImpl::GetDeveloperIds(
    int64_t service_worker_registration_id,
    GetDeveloperIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_fetch_context_->GetDeveloperIdsForServiceWorker(
      service_worker_registration_id, storage_key_, std::move(callback));
}

bool BackgroundFetchServiceImpl::ValidateDeveloperId(
    const std::string& developer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (developer_id.empty() || developer_id.size() > kMaxDeveloperIdLength) {
    mojo::ReportBadMessage("Invalid developer_id");
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateUniqueId(
    const std::string& unique_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::Uuid::ParseLowercase(unique_id).is_valid()) {
    mojo::ReportBadMessage("Invalid unique_id");
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateRequests(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& requests) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (requests.empty()) {
    mojo::ReportBadMessage("Invalid requests");
    return false;
  }

  return true;
}

}  // namespace content
