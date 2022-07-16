// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_
#define CONTENT_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "url/gurl.h"

namespace content {

// A single tuple of (protocol, url, last_modified) that indicates how URLs
// of the given protocol should be rewritten to be handled.
// The |last_modified| field is used to correctly perform deletion
// of protocol handlers based on time ranges.
class CONTENT_EXPORT ProtocolHandler {
 public:
  static ProtocolHandler CreateProtocolHandler(
      const std::string& protocol,
      const GURL& url,
      blink::ProtocolHandlerSecurityLevel security_level =
          blink::ProtocolHandlerSecurityLevel::kStrict);

  ProtocolHandler(const std::string& protocol,
                  const GURL& url,
                  base::Time last_modified,
                  blink::ProtocolHandlerSecurityLevel security_level);

  static ProtocolHandler CreateWebAppProtocolHandler(
      const std::string& protocol,
      const GURL& url,
      const std::string& app_id);

  ProtocolHandler(const std::string& protocol,
                  const GURL& url,
                  const std::string& app_id,
                  base::Time last_modified,
                  blink::ProtocolHandlerSecurityLevel security_level);

  ProtocolHandler(const ProtocolHandler& other);
  ~ProtocolHandler();

  // Creates a ProtocolHandler with fields from the dictionary. Returns an
  // empty ProtocolHandler if the input is invalid.
  static ProtocolHandler CreateProtocolHandler(
      const base::DictionaryValue* value);

  // Returns true if the dictionary value has all the necessary fields to
  // define a ProtocolHandler.
  static bool IsValidDict(const base::DictionaryValue* value);

  // Return true if the protocol handler meets security constraints.
  bool IsValid() const;

  // Returns true if this handler's url has the same origin as the given one.
  bool IsSameOrigin(const ProtocolHandler& handler) const;

  // Canonical empty ProtocolHandler.
  static const ProtocolHandler& EmptyProtocolHandler();

  // Interpolates the given URL into the URL template of this handler.
  GURL TranslateUrl(const GURL& url) const;

  // Returns true if the handlers are considered equivalent when determining
  // if both handlers can be registered, or if a handler has previously been
  // ignored.
  bool IsEquivalent(const ProtocolHandler& other) const;

  // Encodes this protocol handler as a DictionaryValue.
  std::unique_ptr<base::DictionaryValue> Encode() const;

  // Returns a friendly name for |protocol| if one is available, otherwise
  // this function returns |protocol|.
  static std::u16string GetProtocolDisplayName(const std::string& protocol);

  // Returns a friendly name for |this.protocol_| if one is available, otherwise
  // this function returns |this.protocol_|.
  std::u16string GetProtocolDisplayName() const;

  const std::string& protocol() const { return protocol_; }
  const GURL& url() const { return url_; }
  const absl::optional<std::string>& web_app_id() const { return web_app_id_; }
  const base::Time& last_modified() const { return last_modified_; }

  bool IsEmpty() const {
    return protocol_.empty();
  }

#if !defined(NDEBUG)
  // Returns a string representation suitable for use in debugging.
  std::string ToString() const;
#endif

  bool operator==(const ProtocolHandler& other) const;
  bool operator<(const ProtocolHandler& other) const;

 private:
  ProtocolHandler();

  std::string protocol_;
  GURL url_;
  absl::optional<std::string> web_app_id_;
  base::Time last_modified_;
  blink::ProtocolHandlerSecurityLevel security_level_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CUSTOM_HANDLERS_PROTOCOL_HANDLER_H_
