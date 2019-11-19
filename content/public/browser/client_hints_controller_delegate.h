// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/client_hints.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "url/origin.h"

class GURL;

namespace blink {
struct WebEnabledClientHints;
struct UserAgentMetadata;
}  // namespace blink

namespace network {
class NetworkQualityTracker;

}

namespace content {

class CONTENT_EXPORT ClientHintsControllerDelegate
    : public client_hints::mojom::ClientHints {
 public:
  virtual network::NetworkQualityTracker* GetNetworkQualityTracker() = 0;

  // Get which client hints opt-ins were persisted on current origin.
  virtual void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) = 0;

  virtual bool IsJavaScriptAllowed(const GURL& url) = 0;

  virtual std::string GetAcceptLanguageString() = 0;

  virtual blink::UserAgentMetadata GetUserAgentMetadata() = 0;

  virtual void Bind(
      mojo::PendingReceiver<client_hints::mojom::ClientHints> receiver) {}

  // mojom::ClientHints implementation.
  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<blink::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override = 0;

  virtual void ResetForTesting() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
