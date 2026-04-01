// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_REQUEST_PROVIDER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_REQUEST_PROVIDER_IMPL_H_

#include "third_party/blink/renderer/core/html/user_media_request_provider.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class LocalDOMWindow;

class MODULES_EXPORT UserMediaRequestProviderCallbacks final
    : public UserMediaRequest::Callbacks {
 public:
  explicit UserMediaRequestProviderCallbacks(HTMLUserMediaElement* element);

  void OnSuccess(const MediaStreamVector& streams,
                 CaptureController* capture_controller) override;

  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error,
               CaptureController* capture_controller,
               UserMediaRequestResult result) override;

  void Trace(Visitor* visitor) const override;

 private:
  WeakMember<HTMLUserMediaElement> element_;
};

class MODULES_EXPORT UserMediaRequestProviderImpl final
    : public GarbageCollected<UserMediaRequestProviderImpl>,
      public UserMediaRequestProvider {
 public:
  static void ProvideTo(LocalDOMWindow&);

  explicit UserMediaRequestProviderImpl(LocalDOMWindow&);

  void StartRequest(HTMLUserMediaElement*,
                    const Vector<mojom::blink::PermissionDescriptorPtr>&) override;
  };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_REQUEST_PROVIDER_IMPL_H_
