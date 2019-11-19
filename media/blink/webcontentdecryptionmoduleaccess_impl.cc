// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webcontentdecryptionmoduleaccess_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/blink/webencryptedmediaclient_impl.h"

namespace media {

// The caller owns the created cdm (passed back using |result|).
static void CreateCdm(
    const base::WeakPtr<WebEncryptedMediaClientImpl>& client,
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    const CdmConfig& cdm_config,
    std::unique_ptr<blink::WebContentDecryptionModuleResult> result) {
  // If |client| is gone (due to the frame getting destroyed), it is
  // impossible to create the CDM, so fail.
  if (!client) {
    result->CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionInvalidStateError, 0,
        "Failed to create CDM.");
    return;
  }

  client->CreateCdm(key_system, security_origin, cdm_config, std::move(result));
}

// static
WebContentDecryptionModuleAccessImpl*
WebContentDecryptionModuleAccessImpl::From(
    blink::WebContentDecryptionModuleAccess* cdm_access) {
  return static_cast<WebContentDecryptionModuleAccessImpl*>(cdm_access);
}

std::unique_ptr<WebContentDecryptionModuleAccessImpl>
WebContentDecryptionModuleAccessImpl::Create(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    const blink::WebMediaKeySystemConfiguration& configuration,
    const CdmConfig& cdm_config,
    const base::WeakPtr<WebEncryptedMediaClientImpl>& client) {
  return std::make_unique<WebContentDecryptionModuleAccessImpl>(
      key_system, security_origin, configuration, cdm_config, client);
}

WebContentDecryptionModuleAccessImpl::WebContentDecryptionModuleAccessImpl(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    const blink::WebMediaKeySystemConfiguration& configuration,
    const CdmConfig& cdm_config,
    const base::WeakPtr<WebEncryptedMediaClientImpl>& client)
    : key_system_(key_system),
      security_origin_(security_origin),
      configuration_(configuration),
      cdm_config_(cdm_config),
      client_(client) {
}

WebContentDecryptionModuleAccessImpl::~WebContentDecryptionModuleAccessImpl() =
    default;

blink::WebString WebContentDecryptionModuleAccessImpl::GetKeySystem() {
  return key_system_;
}

blink::WebMediaKeySystemConfiguration
WebContentDecryptionModuleAccessImpl::GetConfiguration() {
  return configuration_;
}

void WebContentDecryptionModuleAccessImpl::CreateContentDecryptionModule(
    blink::WebContentDecryptionModuleResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // This method needs to run asynchronously, as it may need to load the CDM.
  // As this object's lifetime is controlled by MediaKeySystemAccess on the
  // blink side, copy all values needed by CreateCdm() in case the blink object
  // gets garbage-collected.
  std::unique_ptr<blink::WebContentDecryptionModuleResult> result_copy(
      new blink::WebContentDecryptionModuleResult(result));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateCdm, client_, key_system_, security_origin_,
                     cdm_config_, base::Passed(&result_copy)));
}

bool WebContentDecryptionModuleAccessImpl::UseHardwareSecureCodecs() const {
  return cdm_config_.use_hw_secure_codecs;
}

}  // namespace media
