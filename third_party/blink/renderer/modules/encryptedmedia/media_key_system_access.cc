// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/base/eme_constants.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_controller.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

// This class wraps the promise resolver used when creating MediaKeys
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithCdm() will resolve the promise with a new MediaKeys object,
// while completeWithError() will reject the promise with an exception.
// All other complete methods are not expected to be called, and will
// reject the promise.
class NewCdmResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  NewCdmResultPromise(
      ScriptState* script_state,
      const WebVector<WebEncryptedMediaSessionType>& supported_session_types,
      const char* interface_name,
      const char* property_name)
      : ContentDecryptionModuleResultPromise(script_state,
                                             interface_name,
                                             property_name),
        supported_session_types_(supported_session_types) {}

  ~NewCdmResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void CompleteWithContentDecryptionModule(
      WebContentDecryptionModule* cdm) override {
    // NOTE: Continued from step 2.8 of createMediaKeys().

    if (!IsValidToFulfillPromise())
      return;

    // 2.9. Let media keys be a new MediaKeys object.
    auto* media_keys = MakeGarbageCollected<MediaKeys>(
        GetExecutionContext(), supported_session_types_, base::WrapUnique(cdm));

    // 2.10. Resolve promise with media keys.
    Resolve(media_keys);
  }

 private:
  WebVector<WebEncryptedMediaSessionType> supported_session_types_;

  DISALLOW_COPY_AND_ASSIGN(NewCdmResultPromise);
};

// These methods are the inverses of those with the same names in
// NavigatorRequestMediaKeySystemAccess.
static Vector<String> ConvertInitDataTypes(
    const WebVector<media::EmeInitDataType>& init_data_types) {
  Vector<String> result(SafeCast<wtf_size_t>(init_data_types.size()));
  for (wtf_size_t i = 0; i < result.size(); i++)
    result[i] =
        EncryptedMediaUtils::ConvertFromInitDataType(init_data_types[i]);
  return result;
}

static HeapVector<Member<MediaKeySystemMediaCapability>> ConvertCapabilities(
    const WebVector<WebMediaKeySystemMediaCapability>& capabilities) {
  HeapVector<Member<MediaKeySystemMediaCapability>> result(
      SafeCast<wtf_size_t>(capabilities.size()));
  for (wtf_size_t i = 0; i < result.size(); i++) {
    MediaKeySystemMediaCapability* capability =
        MediaKeySystemMediaCapability::Create();
    capability->setContentType(capabilities[i].content_type);
    capability->setRobustness(capabilities[i].robustness);

    switch (capabilities[i].encryption_scheme) {
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kNotSpecified:
        capability->setEncryptionSchemeToNull();
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kCenc:
        capability->setEncryptionScheme("cenc");
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kCbcs:
        capability->setEncryptionScheme("cbcs");
        break;
    }

    result[i] = capability;
  }
  return result;
}

static Vector<String> ConvertSessionTypes(
    const WebVector<WebEncryptedMediaSessionType>& session_types) {
  Vector<String> result(SafeCast<wtf_size_t>(session_types.size()));
  for (wtf_size_t i = 0; i < result.size(); i++)
    result[i] = EncryptedMediaUtils::ConvertFromSessionType(session_types[i]);
  return result;
}

}  // namespace

MediaKeySystemAccess::MediaKeySystemAccess(
    std::unique_ptr<WebContentDecryptionModuleAccess> access)
    : access_(std::move(access)) {}

MediaKeySystemAccess::~MediaKeySystemAccess() = default;

MediaKeySystemConfiguration* MediaKeySystemAccess::getConfiguration() const {
  WebMediaKeySystemConfiguration configuration = access_->GetConfiguration();
  MediaKeySystemConfiguration* result = MediaKeySystemConfiguration::Create();
  // |initDataTypes|, |audioCapabilities|, and |videoCapabilities| can only be
  // empty if they were not present in the requested configuration.
  if (!configuration.init_data_types.empty())
    result->setInitDataTypes(
        ConvertInitDataTypes(configuration.init_data_types));
  if (!configuration.audio_capabilities.empty())
    result->setAudioCapabilities(
        ConvertCapabilities(configuration.audio_capabilities));
  if (!configuration.video_capabilities.empty())
    result->setVideoCapabilities(
        ConvertCapabilities(configuration.video_capabilities));

  // |distinctiveIdentifier|, |persistentState|, and |sessionTypes| are always
  // set by requestMediaKeySystemAccess().
  result->setDistinctiveIdentifier(
      EncryptedMediaUtils::ConvertMediaKeysRequirementToString(
          configuration.distinctive_identifier));
  result->setPersistentState(
      EncryptedMediaUtils::ConvertMediaKeysRequirementToString(
          configuration.persistent_state));
  result->setSessionTypes(ConvertSessionTypes(configuration.session_types));

  // |label| will (and should) be a null string if it was not set.
  result->setLabel(configuration.label);
  return result;
}

ScriptPromise MediaKeySystemAccess::createMediaKeys(ScriptState* script_state) {
  // From http://w3c.github.io/encrypted-media/#createMediaKeys
  // (Reordered to be able to pass values into the promise constructor.)
  // 2.4 Let configuration be the value of this object's configuration value.
  // 2.5-2.8. [Set use distinctive identifier and persistent state allowed
  //          based on configuration.]
  WebMediaKeySystemConfiguration configuration = access_->GetConfiguration();

  // 1. Let promise be a new promise.
  NewCdmResultPromise* helper = MakeGarbageCollected<NewCdmResultPromise>(
      script_state, configuration.session_types, "MediaKeySystemAccess",
      "createMediaKeys");
  ScriptPromise promise = helper->Promise();

  // 2. Asynchronously create and initialize the MediaKeys object.
  // 2.1 Let cdm be the CDM corresponding to this object.
  // 2.2 Load and initialize the cdm if necessary.
  // 2.3 If cdm fails to load or initialize, reject promise with a new
  //     DOMException whose name is the appropriate error name.
  //     (Done if completeWithException() called).
  access_->CreateContentDecryptionModule(
      helper->Result(), ExecutionContext::From(script_state)
                            ->GetTaskRunner(TaskType::kInternalMedia));

  // 3. Return promise.
  return promise;
}

}  // namespace blink
