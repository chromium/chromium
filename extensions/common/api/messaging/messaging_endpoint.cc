// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/messaging_endpoint.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace extensions {

namespace {

base::debug::CrashKeyString* GetMessagingSourceTypeCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "MessagingSource-type", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetMessagingSourceExtensionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "MessagingSource-extension_id", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetMessagingSourceNativeAppNameCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "MessagingSource-native_app_name", base::debug::CrashKeySize::Size64);
  return crash_key;
}

const char* ConvertMessagingSourceTypeToString(
    const MessagingEndpoint::Type& type) {
  switch (type) {
    case MessagingEndpoint::Type::kExtension:
      return "Extension";
    case MessagingEndpoint::Type::kTab:
      return "Tab";
    case MessagingEndpoint::Type::kNativeApp:
      return "NativeApp";
  }
  NOTREACHED();
  return "<unrecognized enum value>";
}

base::debug::ScopedCrashKeyString CreateExtensionIdOrNativeAppNameScopedKey(
    const MessagingEndpoint& endpoint) {
  switch (endpoint.type) {
    case MessagingEndpoint::Type::kExtension:
    case MessagingEndpoint::Type::kTab:
      return base::debug::ScopedCrashKeyString(
          GetMessagingSourceExtensionIdCrashKey(),
          endpoint.extension_id.value_or("<base::nullopt>"));

    case MessagingEndpoint::Type::kNativeApp:
      return base::debug::ScopedCrashKeyString(
          GetMessagingSourceNativeAppNameCrashKey(),
          endpoint.native_app_name.value_or("<base::nullopt>"));
  }

  NOTREACHED();
  return base::debug::ScopedCrashKeyString(
      GetMessagingSourceExtensionIdCrashKey(),
      endpoint.extension_id.value_or("<unrecognized MessagingEndpoint::Type>"));
}

}  // namespace

// static
MessagingEndpoint MessagingEndpoint::ForExtension(ExtensionId extension_id) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kExtension;
  messaging_endpoint.extension_id = std::move(extension_id);
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForContentScript(
    ExtensionId extension_id) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kTab;
  messaging_endpoint.extension_id = std::move(extension_id);
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForWebPage() {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kTab;
  return messaging_endpoint;
}

// static
MessagingEndpoint MessagingEndpoint::ForNativeApp(std::string native_app_name) {
  MessagingEndpoint messaging_endpoint;
  messaging_endpoint.type = MessagingEndpoint::Type::kNativeApp;
  messaging_endpoint.native_app_name = std::move(native_app_name);
  return messaging_endpoint;
}

MessagingEndpoint::MessagingEndpoint() = default;

MessagingEndpoint::MessagingEndpoint(const MessagingEndpoint&) = default;

MessagingEndpoint::MessagingEndpoint(MessagingEndpoint&&) = default;

MessagingEndpoint& MessagingEndpoint::operator=(const MessagingEndpoint&) =
    default;

MessagingEndpoint::~MessagingEndpoint() = default;

namespace debug {

ScopedMessagingEndpointCrashKeys::ScopedMessagingEndpointCrashKeys(
    const MessagingEndpoint& endpoint)
    : type_(GetMessagingSourceTypeCrashKey(),
            ConvertMessagingSourceTypeToString(endpoint.type)),
      extension_id_or_app_name_(
          CreateExtensionIdOrNativeAppNameScopedKey(endpoint)) {}

ScopedMessagingEndpointCrashKeys::~ScopedMessagingEndpointCrashKeys() = default;

}  // namespace debug

}  // namespace extensions
