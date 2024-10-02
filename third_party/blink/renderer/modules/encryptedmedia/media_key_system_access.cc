// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_access.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/eme_constants.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_media_capability.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/timer.h"

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
      ScriptPromiseResolver<MediaKeys>* resolver,
      const MediaKeysConfig& config,
      const WebVector<WebEncryptedMediaSessionType>& supported_session_types)
      : ContentDecryptionModuleResultPromise(resolver,
                                             config,
                                             EmeApiType::kCreateMediaKeys),
        config_(config),
        supported_session_types_(supported_session_types) {}

  NewCdmResultPromise(const NewCdmResultPromise&) = delete;
  NewCdmResultPromise& operator=(const NewCdmResultPromise&) = delete;

  ~NewCdmResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void CompleteWithContentDecryptionModule(
      std::unique_ptr<WebContentDecryptionModule> cdm) override {
    // NOTE: Continued from step 2.8 of createMediaKeys().

    if (!IsValidToFulfillPromise())
      return;

    // 2.9. Let media keys be a new MediaKeys object.
    auto* media_keys = MakeGarbageCollected<MediaKeys>(GetExecutionContext(),
                                                       supported_session_types_,
                                                       std::move(cdm), config_);

    // 2.10. Resolve promise with media keys.
    Resolve<MediaKeys>(media_keys);
  }

 private:
  MediaKeysConfig config_;
  WebVector<WebEncryptedMediaSessionType> supported_session_types_;
};

// These methods are the inverses of those with the same names in
// NavigatorRequestMediaKeySystemAccess.
Vector<String> ConvertInitDataTypes(
    const WebVector<media::EmeInitDataType>& init_data_types) {
  Vector<String> result(base::checked_cast<wtf_size_t>(init_data_types.size()));
  for (wtf_size_t i = 0; i < result.size(); i++)
    result[i] =
        EncryptedMediaUtils::ConvertFromInitDataType(init_data_types[i]);
  return result;
}

HeapVector<Member<MediaKeySystemMediaCapability>> ConvertCapabilities(
    const WebVector<WebMediaKeySystemMediaCapability>& capabilities) {
  HeapVector<Member<MediaKeySystemMediaCapability>> result(
      base::checked_cast<wtf_size_t>(capabilities.size()));
  for (wtf_size_t i = 0; i < result.size(); i++) {
    MediaKeySystemMediaCapability* capability =
        MediaKeySystemMediaCapability::Create();
    capability->setContentType(capabilities[i].content_type);
    capability->setRobustness(capabilities[i].robustness);

    switch (capabilities[i].encryption_scheme) {
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kNotSpecified:
        // https://w3c.github.io/encrypted-media/#dom-mediakeysystemaccess-getconfiguration
        // "If encryptionScheme was not given by the application, the
        // accumulated configuration MUST still contain a encryptionScheme
        // field with a value of null, so that polyfills can detect the user
        // agent's support for the field without specifying specific values."
        capability->setEncryptionScheme(String());
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kCenc:
        capability->setEncryptionScheme("cenc");
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kCbcs:
        capability->setEncryptionScheme("cbcs");
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kCbcs_1_9:
        capability->setEncryptionScheme("cbcs-1-9");
        break;
      case WebMediaKeySystemMediaCapability::EncryptionScheme::kUnrecognized:
        NOTREACHED_IN_MIGRATION()
            << "Unrecognized encryption scheme should never be returned.";
        break;
    }

    result[i] = capability;
  }
  return result;
}

Vector<String> ConvertSessionTypes(
    const WebVector<WebEncryptedMediaSessionType>& session_types) {
  Vector<String> result(base::checked_cast<wtf_size_t>(session_types.size()));
  for (wtf_size_t i = 0; i < result.size(); i++)
    result[i] = EncryptedMediaUtils::ConvertFromSessionType(session_types[i]);
  return result;
}

void ReportMetrics(ExecutionContext* execution_context,
                   const String& key_system) {
  // TODO(xhwang): Report other key systems here and for
  // requestMediaKeySystemAccess().
  const char kWidevineKeySystem[] = "com.widevine.alpha";
  if (key_system != kWidevineKeySystem)
    return;

  auto* local_dom_window = To<LocalDOMWindow>(execution_context);
  if (!local_dom_window)
    return;

  Document* document = local_dom_window->document();
  if (!document)
    return;

  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return;

  ukm::builders::Media_EME_CreateMediaKeys builder(document->UkmSourceID());
  builder.SetKeySystem(KeySystemForUkmLegacy::kWidevine);
  builder.SetIsAdFrame(static_cast<int>(frame->IsAdFrame()));
  builder.SetIsCrossOrigin(
      static_cast<int>(frame->IsCrossOriginToOutermostMainFrame()));
  builder.SetIsTopFrame(static_cast<int>(frame->IsOutermostMainFrame()));
  builder.Record(document->UkmRecorder());
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
      EncryptedMediaUtils::ConvertMediaKeysRequirementToEnum(
          configuration.distinctive_identifier));
  result->setPersistentState(
      EncryptedMediaUtils::ConvertMediaKeysRequirementToEnum(
          configuration.persistent_state));
  result->setSessionTypes(ConvertSessionTypes(configuration.session_types));

  // |label| will (and should) be a null string if it was not set.
  result->setLabel(configuration.label);
  return result;
}

ScriptPromise<MediaKeys> MediaKeySystemAccess::createMediaKeys(
    ScriptState* script_state) {
  // From http://w3c.github.io/encrypted-media/#createMediaKeys
  // (Reordered to be able to pass values into the promise constructor.)
  // 2.4 Let configuration be the value of this object's configuration value.
  // 2.5-2.8. [Set use distinctive identifier and persistent state allowed
  //          based on configuration.]
  WebMediaKeySystemConfiguration configuration = access_->GetConfiguration();

  // 1. Let promise be a new promise.
  MediaKeysConfig config = {keySystem(), UseHardwareSecureCodecs()};
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MediaKeys>>(script_state);
  NewCdmResultPromise* helper = MakeGarbageCollected<NewCdmResultPromise>(
      resolver, config, configuration.session_types);
  auto promise = resolver->Promise();

  // 2. Asynchronously create and initialize the MediaKeys object.
  // 2.1 Let cdm be the CDM corresponding to this object.
  // 2.2 Load and initialize the cdm if necessary.
  // 2.3 If cdm fails to load or initialize, reject promise with a new
  //     DOMException whose name is the appropriate error name.
  //     (Done if completeWithException() called).
  auto* execution_context = ExecutionContext::From(script_state);
  access_->CreateContentDecryptionModule(
      helper->Result(),
      execution_context->GetTaskRunner(TaskType::kInternalMedia));

  ReportMetrics(execution_context, keySystem());

  // 3. Return promise.
  return promise;
}

}  // namespace blink
