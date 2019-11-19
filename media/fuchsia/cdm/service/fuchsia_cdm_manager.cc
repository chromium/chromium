// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "url/origin.h"

namespace media {

// Provisioner for one origin. It makes sure only one provision request is sent
// to provisioning server. All the concurrent provision request will be cached
// until it finishes current provision request.
class FuchsiaCdmManager::OriginProvisioner {
 public:
  explicit OriginProvisioner(KeySystemHandler* handler);
  ~OriginProvisioner();

  void CheckOrProvision(CreateFetcherCB create_fecther_cb,
                        base::OnceCallback<void(bool)> provision_cb);

 private:
  enum class ProvisionStatus {
    UNKNOWN,
    PENDING,
    SUCCESS,
    FAIL,
  };

  // Called when any error happens during provision flow.
  void OnProvisionFail();

  // The following functions are used to complete origin provision flow. If any
  // error happens, provision flow will be stopped.
  // 1. Check if current origin provisioned or not.
  void CheckOrProvisionImpl();
  void OnProvisionStatus(fuchsia::media::drm::ProvisioningStatus status);
  // 2. Generate provision request if current origin is not provisioned.
  void HandleDeviceProvision();
  // 3. Send provision request to provisioning server.
  void OnProvisioningRequest(fuchsia::media::drm::ProvisioningRequest request);
  // 4. Provide provision response to CDM.
  void OnProvisioningResponse(bool success, const std::string& response);
  void OnProvisioningResponseResult(
      fuchsia::media::drm::Provisioner_ProcessProvisioningResponse_Result
          result);

  void ProcessPendingCallbacks();

  KeySystemHandler* const handler_;

  ProvisionStatus provision_status_ = ProvisionStatus::UNKNOWN;
  std::vector<base::OnceCallback<void(bool)>> pending_cbs_;

  // Cdm used for provision.
  fuchsia::media::drm::ProvisionerPtr provisioner_;

  CreateFetcherCB create_fetcher_cb_;
  std::unique_ptr<ProvisionFetcher> provision_fetcher_;
};

FuchsiaCdmManager::OriginProvisioner::OriginProvisioner(
    KeySystemHandler* handler)
    : handler_(handler) {
  DCHECK(handler_);
}

FuchsiaCdmManager::OriginProvisioner::~OriginProvisioner() = default;

void FuchsiaCdmManager::OriginProvisioner::CheckOrProvision(
    CreateFetcherCB create_fetcher_cb,
    base::OnceCallback<void(bool)> provision_cb) {
  pending_cbs_.push_back(std::move(provision_cb));

  if (provision_status_ == ProvisionStatus::UNKNOWN) {
    DCHECK(!provisioner_);

    provision_status_ = ProvisionStatus::PENDING;
    create_fetcher_cb_ = std::move(create_fetcher_cb);
    CheckOrProvisionImpl();
    return;
  }

  // Pending provision. Wait for provision finish.
  if (provision_status_ == ProvisionStatus::PENDING) {
    return;
  }

  ProcessPendingCallbacks();
}

void FuchsiaCdmManager::OriginProvisioner::CheckOrProvisionImpl() {
  if (!provisioner_) {
    provisioner_ = handler_->CreateProvisioner();
    if (!provisioner_) {
      // No provisioner means provision is not needed at all.
      provision_status_ = ProvisionStatus::SUCCESS;
      ProcessPendingCallbacks();
      return;
    }

    provisioner_.set_error_handler([this](zx_status_t status) {
      ZX_DLOG(ERROR, status) << "The fuchsia.media.drm.Provisioner"
                             << " channel was terminated.";
      OnProvisionFail();
    });
  }

  provisioner_->GetStatus(
      fit::bind_member(this, &OriginProvisioner::OnProvisionStatus));
}

void FuchsiaCdmManager::OriginProvisioner::OnProvisionStatus(
    fuchsia::media::drm::ProvisioningStatus status) {
  if (status == fuchsia::media::drm::ProvisioningStatus::PROVISIONED) {
    provision_status_ = ProvisionStatus::SUCCESS;
    ProcessPendingCallbacks();
    return;
  }

  DCHECK_EQ(status, fuchsia::media::drm::ProvisioningStatus::NOT_PROVISIONED);
  HandleDeviceProvision();
}

void FuchsiaCdmManager::OriginProvisioner::HandleDeviceProvision() {
  DCHECK(provisioner_);

  DVLOG(2) << "Start device provision.";

  provisioner_->GenerateProvisioningRequest(
      fit::bind_member(this, &OriginProvisioner::OnProvisioningRequest));
}

void FuchsiaCdmManager::OriginProvisioner::OnProvisioningRequest(
    fuchsia::media::drm::ProvisioningRequest request) {
  std::string request_str;
  if (!cr_fuchsia::StringFromMemBuffer(request.message, &request_str)) {
    DLOG(ERROR) << "Failed to get provision request.";
    OnProvisionFail();
    return;
  }
  if (!request.default_provisioning_server_url) {
    DLOG(ERROR) << "Missing default provisioning server URL.";
    OnProvisionFail();
    return;
  }

  DCHECK(create_fetcher_cb_);
  provision_fetcher_ = std::move(create_fetcher_cb_).Run();
  DCHECK(provision_fetcher_);
  provision_fetcher_->Retrieve(
      request.default_provisioning_server_url.value(), request_str,
      base::BindRepeating(&OriginProvisioner::OnProvisioningResponse,
                          base::Unretained(this)));
}

void FuchsiaCdmManager::OriginProvisioner::OnProvisioningResponse(
    bool success,
    const std::string& response) {
  provision_fetcher_ = nullptr;

  if (!success) {
    LOG(ERROR) << "Failed to fetch provision response. response " << response;
    OnProvisionFail();
    return;
  }

  fuchsia::media::drm::ProvisioningResponse provision_response;
  provision_response.message =
      cr_fuchsia::MemBufferFromString(response, "cr-drm-provision-response");

  provisioner_->ProcessProvisioningResponse(
      std::move(provision_response),
      fit::bind_member(this, &OriginProvisioner::OnProvisioningResponseResult));
}

void FuchsiaCdmManager::OriginProvisioner::OnProvisioningResponseResult(
    fuchsia::media::drm::Provisioner_ProcessProvisioningResponse_Result
        result) {
  if (result.is_err()) {
    LOG(ERROR) << "Fail to process provisioning response "
               << static_cast<int>(result.err());
    OnProvisionFail();
    return;
  }

  DVLOG(2) << "Provision success!";

  provision_status_ = ProvisionStatus::SUCCESS;
  ProcessPendingCallbacks();
}

void FuchsiaCdmManager::OriginProvisioner::ProcessPendingCallbacks() {
  DCHECK_NE(provision_status_, ProvisionStatus::UNKNOWN);
  DCHECK_NE(provision_status_, ProvisionStatus::PENDING);
  DCHECK(!pending_cbs_.empty());

  if (provisioner_) {
    // Close the channel by dropping the returned InterfaceHandle.
    auto close_channel = provisioner_.Unbind();
  }

  auto pending_cbs = std::move(pending_cbs_);
  for (auto& cb : pending_cbs) {
    std::move(cb).Run(provision_status_ == ProvisionStatus::SUCCESS);
  }
}

void FuchsiaCdmManager::OriginProvisioner::OnProvisionFail() {
  provision_status_ = ProvisionStatus::FAIL;
  ProcessPendingCallbacks();
}

FuchsiaCdmManager::FuchsiaCdmManager(KeySystemHandlerMap handlers)
    : handlers_(std::move(handlers)) {
  DETACH_FROM_THREAD(thread_checker_);
}

FuchsiaCdmManager::~FuchsiaCdmManager() = default;

void FuchsiaCdmManager::CreateAndProvision(
    const std::string& key_system,
    const url::Origin& origin,
    CreateFetcherCB create_fetcher_cb,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = handlers_.find(key_system);
  if (it == handlers_.end()) {
    DLOG(ERROR) << "Key system is not supported: " << key_system;
    return;
  }
  KeySystemHandler* handler = it->second.get();

  OriginProvisioner* origin_provisioner =
      GetProvisioner(key_system, origin, handler);
  DCHECK(origin_provisioner);

  origin_provisioner->CheckOrProvision(
      std::move(create_fetcher_cb),
      base::BindOnce(&FuchsiaCdmManager::OnProvisionResult,
                     base::Unretained(this), handler, std::move(request)));
}

// TODO(yucliu): This should return different OriginProvisioner for different
// origins. Due to platform limitation, we can only support one for all the
// origins. (crbug.com/991723)
FuchsiaCdmManager::OriginProvisioner* FuchsiaCdmManager::GetProvisioner(
    const std::string& key_system,
    const url::Origin&,
    KeySystemHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = origin_provisioners_.find(key_system);
  if (it == origin_provisioners_.end()) {
    it = origin_provisioners_
             .emplace(key_system, std::make_unique<OriginProvisioner>(handler))
             .first;
  }

  return it->second.get();
}

void FuchsiaCdmManager::OnProvisionResult(
    KeySystemHandler* handler,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!success) {
    LOG(ERROR) << "Failed to provision origin";
    return;
  }

  handler->CreateCdm(std::move(request));
}

}  // namespace media
