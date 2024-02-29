// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_ENDPOINT_H_

#include <optional>
#include <string>

#include "base/debug/crash_logging.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

struct MessagingEndpoint {
  using Type = mojom::MessagingEndpointType;

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
  static MessagingEndpoint ForUserScript(ExtensionId extension_id);
  static MessagingEndpoint ForWebPage();
  static MessagingEndpoint ForNativeApp(std::string native_app_name);

  // Returns the `Relationship` between two endpoints.
  static Relationship GetRelationship(const MessagingEndpoint& source_endpoint,
                                      const std::string& target_id);

  // Returns true if the channel between `source_endpoint` and `target_id` is
  // considered external to the target.
  static bool IsExternal(const MessagingEndpoint& source_endpoint,
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
  std::optional<ExtensionId> extension_id;

  // Name of the native application.  It is required for |type| of kNativeApp.
  // It is not used for other types.
  std::optional<std::string> native_app_name;
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
