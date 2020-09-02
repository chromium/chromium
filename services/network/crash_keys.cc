// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/crash_keys.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace network {
namespace debug {

namespace {

base::debug::CrashKeyString* GetRequestUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestInitiatorCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestResourceTypeCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_resource_type", base::debug::CrashKeySize::Size32);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestLoadFlagsCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_load_flags", base::debug::CrashKeySize::Size32);
  return crash_key;
}

}  // namespace

base::debug::CrashKeyString* GetRequestInitiatorOriginLockCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator_origin_lock", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetFactoryDebugTagCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "url_loader_factory_debug_tag", base::debug::CrashKeySize::Size64);
  return crash_key;
}

ScopedRequestCrashKeys::ScopedRequestCrashKeys(
    const network::ResourceRequest& request)
    : url_(GetRequestUrlCrashKey(), request.url.possibly_invalid_spec()),
      request_initiator_(GetRequestInitiatorCrashKey(),
                         base::OptionalOrNullptr(request.request_initiator)),
      resource_type_(GetRequestResourceTypeCrashKey(),
                     base::NumberToString(request.resource_type)),
      load_flags_(GetRequestLoadFlagsCrashKey(),
                  base::NumberToString(request.load_flags)) {}

ScopedRequestCrashKeys::~ScopedRequestCrashKeys() = default;

}  // namespace debug
}  // namespace network
