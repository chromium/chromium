// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEY_SYSTEM_ACCESS_INITIALIZER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEY_SYSTEM_ACCESS_INITIALIZER_BASE_H_

#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_system_configuration.h"
#include "third_party/blink/renderer/platform/encrypted_media_request.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaKeySystemAccessInitializerBase : public EncryptedMediaRequest,
                                            public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaKeySystemAccessInitializerBase);

 public:
  MediaKeySystemAccessInitializerBase(
      ScriptState* script_state,
      const String& key_system,
      const HeapVector<Member<MediaKeySystemConfiguration>>&
          supported_configurations);
  ~MediaKeySystemAccessInitializerBase() override = default;

  // EncryptedMediaRequest implementation.
  WebString KeySystem() const override { return key_system_; }
  const WebVector<WebMediaKeySystemConfiguration>& SupportedConfigurations()
      const override {
    return supported_configurations_;
  }
  const SecurityOrigin* GetSecurityOrigin() const override;

  // IMPORTANT: Acquire the promise immediately after creating the |this|.
  // Otherwise the promise returned to JS will be undefined. See comment above
  // Promise() in script_promise_resolver.h
  ScriptPromise Promise();

  void Trace(blink::Visitor* visitor) override;

 protected:
  // Returns true if the ExecutionContext is valid, false otherwise.
  bool IsExecutionContextValid() const;

  // For widevine key system, generate warning and report to UMA if
  // |m_supportedConfigurations| contains any video capability with empty
  // robustness string.
  void CheckVideoCapabilityRobustness() const;

  Member<ScriptPromiseResolver> resolver_;
  const String key_system_;
  WebVector<WebMediaKeySystemConfiguration> supported_configurations_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeySystemAccessInitializerBase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEY_SYSTEM_ACCESS_INITIALIZER_BASE_H_
