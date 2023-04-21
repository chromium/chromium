// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_

#include <string>

#include "base/debug/crash_logging.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

struct MessagingEndpoint {
  // Type of the messaging source or destination - i.e., the type of the
  // component which talks to a messaging channel.
  enum class Type {
    // An extension.
    kExtension = 0,
    // A web page or a hosted app.
    kWebPage = 1,
    // A content script.
    kContentScript = 2,
    // A native application.
    kNativeApp = 3,

    // This item must be equal to the last actual enum item.
    kLast = kNativeApp,
  };

  // The relationship between two messaging endpoints.
  enum class Relationship {
    // The same extension, either between trusted contexts or between a trusted
    // context and a content script.
    kInternal,
    // An external extension connection.
    kExternalExtension,
    // An external web page connection.
    kExternalWebPage,
    // An external native app.
    kExternalNativeApp,
  };

  // Creation methods for different endpoint types.
  static MessagingEndpoint ForExtension(ExtensionId extension_id);
  static MessagingEndpoint ForContentScript(ExtensionId extension_id);
  static MessagingEndpoint ForWebPage();
  static MessagingEndpoint ForNativeApp(std::string native_app_name);

  // Returns the `Relationship` between two endpoints.
  static Relationship GetRelationship(const MessagingEndpoint& source_endpoint,
                                      const std::string& target_id);

  MessagingEndpoint();
  MessagingEndpoint(const MessagingEndpoint&);
  MessagingEndpoint(MessagingEndpoint&&);

  MessagingEndpoint& operator=(const MessagingEndpoint&);

  ~MessagingEndpoint();

  Type type = Type::kExtension;

  // Identifier of the extension (or the content script).  It is required for
  // |type| of kExtension.  For |type| of kTab, it is set if the endpoint is a
  // content script (otherwise, it's the web page).
  absl::optional<ExtensionId> extension_id;

  // Name of the native application.  It is required for |type| of kNativeApp.
  // It is not used for other types.
  absl::optional<std::string> native_app_name;
};

namespace debug {

class ScopedMessagingEndpointCrashKeys {
 public:
  explicit ScopedMessagingEndpointCrashKeys(const MessagingEndpoint& endpoint);
  ~ScopedMessagingEndpointCrashKeys();

  ScopedMessagingEndpointCrashKeys(const ScopedMessagingEndpointCrashKeys&) =
      delete;
  ScopedMessagingEndpointCrashKeys& operator=(
      const ScopedMessagingEndpointCrashKeys&) = delete;

 private:
  base::debug::ScopedCrashKeyString type_;
  base::debug::ScopedCrashKeyString extension_id_or_app_name_;
};

}  // namespace debug

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_
