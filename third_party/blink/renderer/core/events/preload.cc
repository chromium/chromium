// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/preload.h"

namespace blink {

Preload::Preload(const KURL& url,
                 const String& as,
                 network::mojom::CredentialsMode credentials_mode,
                 network::mojom::RequestMode request_mode,
                 bool used)
    : url_(url),
      as_(as),
      credentials_mode_(credentials_mode),
      request_mode_(request_mode),
      used_(used) {}

V8CrossOriginMode Preload::crossorigin() const {
  // No crossorigin attribute = no-cors mode (regardless of credentials mode).
  if (request_mode_ == network::mojom::RequestMode::kNoCors) {
    return V8CrossOriginMode(V8CrossOriginMode::Enum::kNone);
  }
  // crossorigin="use-credentials" = cors mode + include credentials.
  if (credentials_mode_ == network::mojom::CredentialsMode::kInclude) {
    return V8CrossOriginMode(V8CrossOriginMode::Enum::kUseCredentials);
  }
  // crossorigin="" or crossorigin="anonymous" = cors mode + same-origin
  // credentials.
  return V8CrossOriginMode(V8CrossOriginMode::Enum::kAnonymous);
}

void Preload::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
