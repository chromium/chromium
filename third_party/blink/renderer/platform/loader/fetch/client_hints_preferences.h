// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CLIENT_HINTS_PREFERENCES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CLIENT_HINTS_PREFERENCES_H_

#include "third_party/blink/public/platform/web_client_hints_type.h"
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
    virtual void CountClientHints(mojom::WebClientHintsType) = 0;
    virtual void CountPersistentClientHintHeaders() = 0;

   protected:
    virtual ~Context() = default;
  };

  ClientHintsPreferences();

  void UpdateFrom(const ClientHintsPreferences&);

  // Parses the client hints headers, and populates |this| with the client hint
  // preferences. |url| is the URL of the resource whose response included the
  // |header_value|. |context| may be null. If client hints are not allowed for
  // |url|, then |this| would not be updated.
  void UpdateFromAcceptClientHintsHeader(const String& header_value,
                                         const KURL&,
                                         Context*);

  bool ShouldSend(mojom::WebClientHintsType type) const {
    return enabled_hints_.IsEnabled(type);
  }
  void SetShouldSendForTesting(mojom::WebClientHintsType type) {
    enabled_hints_.SetIsEnabled(type, true);
  }

  // Parses the accept-ch-lifetime header, and populates |this| with the client
  // hints persistence duration. |url| is the URL of the resource whose response
  // included the |header_value|. |context| may be null. If client hints are not
  // allowed for |url|, then |this| would not be updated.
  void UpdateFromAcceptClientHintsLifetimeHeader(const String& header_value,
                                                 const KURL& url,
                                                 Context* context);

  // Returns true if client hints are allowed for the provided KURL. Client
  // hints are allowed only on HTTP URLs that belong to secure contexts.
  static bool IsClientHintsAllowed(const KURL&);

  WebEnabledClientHints GetWebEnabledClientHints() const;

  base::TimeDelta GetPersistDuration() const;

 private:
  WebEnabledClientHints enabled_hints_;
  base::TimeDelta persist_duration_;
};

}  // namespace blink

#endif
