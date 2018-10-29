// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter_factory.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_adapter.h"
#include "media/cdm/cdm_module.h"
#include "url/origin.h"

namespace media {

CdmAdapterFactory::CdmAdapterFactory(HelperCreationCB helper_creation_cb)
    : helper_creation_cb_(std::move(helper_creation_cb)) {
  DCHECK(helper_creation_cb_);
}

CdmAdapterFactory::~CdmAdapterFactory() = default;

void CdmAdapterFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DVLOG(1) << __func__ << ": key_system=" << key_system;

  if (security_origin.opaque()) {
    LOG(ERROR) << "Invalid Origin: " << security_origin;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  CdmAdapter::CreateCdmFunc create_cdm_func =
      CdmModule::GetInstance()->GetCreateCdmFunc();
  if (!create_cdm_func) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(cdm_created_cb, nullptr, "CreateCdmFunc not available."));
    return;
  }

  std::unique_ptr<CdmAuxiliaryHelper> cdm_helper = helper_creation_cb_.Run();
  if (!cdm_helper) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(cdm_created_cb, nullptr, "CDM helper creation failed."));
    return;
  }

  CdmAdapter::Create(key_system, security_origin, cdm_config, create_cdm_func,
                     std::move(cdm_helper), session_message_cb,
                     session_closed_cb, session_keys_change_cb,
                     session_expiration_update_cb, cdm_created_cb);
}

}  // namespace media
