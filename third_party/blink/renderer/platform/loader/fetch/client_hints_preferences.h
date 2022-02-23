// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CLIENT_HINTS_PREFERENCES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CLIENT_HINTS_PREFERENCES_H_

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class KURL;

// TODO (tbansal): Remove PLATFORM_EXPORT, and pass WebClientHintsType
// everywhere.
class PLATFORM_EXPORT ClientHintsPreferences {
  DISALLOW_NEW();

 public:
  class Context {
   public:
    virtual void CountClientHints(network::mojom::WebClientHintsType) = 0;

   protected:
    virtual ~Context() = default;
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);
  void CombineWith(const ClientHintsPreferences&);

  // Parses <meta http-equiv="accept-ch"> or <meta name="accept-ch"> value
  // `header_value`, and updates `this` to enable the requested client hints.
  // `url` is the URL of the page. `context` may be null. `is_http_equiv` is
  // true if 'accept-ch' is an 'http-equiv' attribute and not 'name'.
  // `is_preload_or_sync_parser` is true if the HTML preloader saw the element
  // or if the element was created by the parser. If client hints are not
  // allowed for `url`, then `this` would not be updated. Returns true if
  // client hints were modified.
  bool UpdateFromMetaTagAcceptCH(const String& header_value,
                                 const KURL& url,
                                 Context* context,
                                 bool is_http_equiv,
                                 bool is_preload_or_sync_parser);

  bool ShouldSend(network::mojom::WebClientHintsType type) const;
  void SetShouldSend(network::mojom::WebClientHintsType type);

  // Returns true if client hints are allowed for the provided KURL. Client
  // hints are allowed only on HTTP URLs that belong to secure contexts.
  static bool IsClientHintsAllowed(const KURL&);

  EnabledClientHints GetEnabledClientHints() const;

 private:
  EnabledClientHints enabled_hints_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CLIENT_HINTS_PREFERENCES_H_
