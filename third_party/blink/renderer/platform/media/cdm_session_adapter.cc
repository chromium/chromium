// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/cdm_session_adapter.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/cdm/cdm_context_ref_impl.h"
#include "third_party/blink/renderer/platform/media/web_content_decryption_module_session_impl.h"

namespace blink {
namespace {
const char kMediaEME[] = "Media.EME.";
const char kDot[] = ".";
const char kCreateCdmUMAName[] = "CreateCdm";
const char kCreateCdmStatusUMAName[] = "CreateCdmStatus";
const char kTimeToCreateCdmUMAName[] = "CreateCdmTime";
}  // namespace

CdmSessionAdapter::CdmSessionAdapter(media::KeySystems* key_systems)
    : key_systems_(key_systems), trace_id_(0) {
  DCHECK(key_systems_);
}

CdmSessionAdapter::~CdmSessionAdapter() = default;

void CdmSessionAdapter::CreateCdm(media::CdmFactory* cdm_factory,
                                  const media::CdmConfig& cdm_config,
                                  WebCdmCreatedCB web_cdm_created_cb) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "CdmSessionAdapter::CreateCdm",
                                    ++trace_id_);

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Note: WebContentDecryptionModuleImpl::Create() calls this method without
  // holding a reference to the CdmSessionAdapter. Bind OnCdmCreated() with
  // |this| instead of |weak_this| to prevent |this| from being destructed.
  base::WeakPtr<CdmSessionAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();

  DCHECK(!web_cdm_created_cb_);
  web_cdm_created_cb_ = std::move(web_cdm_created_cb);

  cdm_factory->Create(
      cdm_config,
      base::BindRepeating(&CdmSessionAdapter::OnSessionMessage, weak_this),
      base::BindRepeating(&CdmSessionAdapter::OnSessionClosed, weak_this),
      base::BindRepeating(&CdmSessionAdapter::OnSessionKeysChange, weak_this),
      base::BindRepeating(&CdmSessionAdapter::OnSessionExpirationUpdate,
                          weak_this),
      base::BindOnce(&CdmSessionAdapter::OnCdmCreated, this, cdm_config,
                     start_time));
}

void CdmSessionAdapter::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  cdm_->SetServerCertificate(certificate, std::move(promise));
}

void CdmSessionAdapter::GetStatusForPolicy(
    media::HdcpVersion min_hdcp_version,
    std::unique_ptr<media::KeyStatusCdmPromise> promise) {
  cdm_->GetStatusForPolicy(min_hdcp_version, std::move(promise));
}

std::unique_ptr<WebContentDecryptionModuleSessionImpl>
CdmSessionAdapter::CreateSession(WebEncryptedMediaSessionType session_type) {
  return std::make_unique<WebContentDecryptionModuleSessionImpl>(
      this, session_type, key_systems_);
}

bool CdmSessionAdapter::RegisterSession(
    const std::string& session_id,
    base::WeakPtr<WebContentDecryptionModuleSessionImpl> session) {
  // If this session ID is already registered, don't register it again.
  if (base::Contains(sessions_, session_id))
    return false;

  sessions_[session_id] = session;
  return true;
}

void CdmSessionAdapter::UnregisterSession(const std::string& session_id) {
  DCHECK(base::Contains(sessions_, session_id));
  sessions_.erase(session_id);
}

void CdmSessionAdapter::InitializeNewSession(
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    media::CdmSessionType session_type,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  cdm_->CreateSessionAndGenerateRequest(session_type, init_data_type, init_data,
                                        std::move(promise));
}

void CdmSessionAdapter::LoadSession(
    media::CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DVLOG(2) << __func__ << ": session_id = " << session_id;
  cdm_->LoadSession(session_type, session_id, std::move(promise));
}

void CdmSessionAdapter::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(3) << __func__ << ": session_id = " << session_id;
  cdm_->UpdateSession(session_id, response, std::move(promise));
}

void CdmSessionAdapter::CloseSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __func__ << ": session_id = " << session_id;
  cdm_->CloseSession(session_id, std::move(promise));
}

void CdmSessionAdapter::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DVLOG(2) << __func__ << ": session_id = " << session_id;
  cdm_->RemoveSession(session_id, std::move(promise));
}

std::unique_ptr<media::CdmContextRef> CdmSessionAdapter::GetCdmContextRef() {
  DVLOG(2) << __func__;

  if (!cdm_->GetCdmContext()) {
    NOTREACHED_IN_MIGRATION() << "All CDMs should support CdmContext.";
    return nullptr;
  }

  return std::make_unique<media::CdmContextRefImpl>(cdm_);
}

const std::string& CdmSessionAdapter::GetKeySystem() const {
  return cdm_config_.key_system;
}

const std::string& CdmSessionAdapter::GetKeySystemUMAPrefix() const {
  DCHECK(!key_system_uma_prefix_.empty());
  return key_system_uma_prefix_;
}

const media::CdmConfig& CdmSessionAdapter::GetCdmConfig() const {
  DCHECK(cdm_);
  return cdm_config_;
}

void CdmSessionAdapter::OnCdmCreated(
    const media::CdmConfig& cdm_config,
    base::TimeTicks start_time,
    const scoped_refptr<media::ContentDecryptionModule>& cdm,
    media::CreateCdmStatus status) {
  DVLOG(1) << __func__ << ": "
           << (cdm ? "success" : "failure (" + base::ToString(status) + ")");
  DCHECK(!cdm_);

  TRACE_EVENT_NESTABLE_ASYNC_END2("media", "CdmSessionAdapter::CreateCdm",
                                  trace_id_, "success",
                                  (cdm ? "true" : "false"), "status", status);

  auto key_system_name_for_uma = media::GetKeySystemNameForUMA(
      cdm_config.key_system, cdm_config.use_hw_secure_codecs);
  auto key_system_uma_prefix = kMediaEME + key_system_name_for_uma + kDot;
  base::UmaHistogramBoolean(key_system_uma_prefix + kCreateCdmUMAName,
                            cdm ? true : false);
  base::UmaHistogramEnumeration(key_system_uma_prefix + kCreateCdmStatusUMAName,
                                status);

  if (!cdm) {
    std::move(web_cdm_created_cb_).Run(nullptr, status);
    return;
  }

  key_system_uma_prefix_ = std::move(key_system_uma_prefix);

  // Only report time for successful CDM creation.
  base::UmaHistogramTimes(key_system_uma_prefix_ + kTimeToCreateCdmUMAName,
                          base::TimeTicks::Now() - start_time);

  cdm_config_ = cdm_config;

  cdm_ = cdm;

  std::move(web_cdm_created_cb_)
      .Run(std::make_unique<WebContentDecryptionModuleImpl>(
               base::PassKey<CdmSessionAdapter>(), this, key_systems_),
           media::CreateCdmStatus::kSuccess);
}

void CdmSessionAdapter::OnSessionMessage(const std::string& session_id,
                                         media::CdmMessageType message_type,
                                         const std::vector<uint8_t>& message) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __func__ << " for unknown session "
                             << session_id;
  if (session) {
    DVLOG(3) << __func__ << ": session_id = " << session_id;
    session->OnSessionMessage(message_type, message);
  }
}

void CdmSessionAdapter::OnSessionKeysChange(const std::string& session_id,
                                            bool has_additional_usable_key,
                                            media::CdmKeysInfo keys_info) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __func__ << " for unknown session "
                             << session_id;
  if (session) {
    DVLOG(2) << __func__ << ": session_id = " << session_id;
    DVLOG(2) << "  - has_additional_usable_key = " << has_additional_usable_key;
    for (const auto& info : keys_info)
      DVLOG(2) << "  - " << *(info.get());

    session->OnSessionKeysChange(has_additional_usable_key,
                                 std::move(keys_info));
  }
}

void CdmSessionAdapter::OnSessionExpirationUpdate(const std::string& session_id,
                                                  base::Time new_expiry_time) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __func__ << " for unknown session "
                             << session_id;
  if (session) {
    DVLOG(2) << __func__ << ": session_id = " << session_id;
    if (new_expiry_time.is_null())
      DVLOG(2) << "  - new_expiry_time = NaN";
    else
      DVLOG(2) << "  - new_expiry_time = " << new_expiry_time;

    session->OnSessionExpirationUpdate(new_expiry_time);
  }
}

void CdmSessionAdapter::OnSessionClosed(const std::string& session_id,
                                        media::CdmSessionClosedReason reason) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session)
      << __func__ << " for unknown session " << session_id;
  if (session) {
    DVLOG(2) << __func__ << ": session_id = " << session_id
             << ", reason = " << static_cast<int>(reason);
    session->OnSessionClosed(reason);
  }
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::GetSession(
    const std::string& session_id) {
  // Since session objects may get garbage collected, it is possible that there
  // are events coming back from the CDM and the session has been unregistered.
  // We can not tell if the CDM is firing events at sessions that never existed.
  auto session = sessions_.find(session_id);
  return (session != sessions_.end()) ? session->second.get() : NULL;
}

}  // namespace blink
