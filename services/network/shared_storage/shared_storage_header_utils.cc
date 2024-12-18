// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_storage/shared_storage_header_utils.h"

#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

namespace {

constexpr auto kSharedStorageModifierMethodTypeMap =
    base::MakeFixedFlatMap<std::string_view, SharedStorageModifierMethodType>(
        {{"set", SharedStorageModifierMethodType::kSet},
         {"append", SharedStorageModifierMethodType::kAppend},
         {"delete", SharedStorageModifierMethodType::kDelete},
         {"clear", SharedStorageModifierMethodType::kClear}});

constexpr auto kSharedStorageHeaderParamTypeMap =
    base::MakeFixedFlatMap<std::string_view, SharedStorageHeaderParamType>(
        {{"key", SharedStorageHeaderParamType::kKey},
         {"value", SharedStorageHeaderParamType::kValue},
         {"ignore_if_present", SharedStorageHeaderParamType::kIgnoreIfPresent},
         {"with_lock", SharedStorageHeaderParamType::kWithLock}});

}  // namespace

std::optional<SharedStorageModifierMethodType>
StringToSharedStorageModifierMethodType(std::string_view method_str) {
  auto method_it =
      kSharedStorageModifierMethodTypeMap.find(base::ToLowerASCII(method_str));
  if (method_it == kSharedStorageModifierMethodTypeMap.end()) {
    return std::nullopt;
  }

  return method_it->second;
}

bool IsHeaderItemBatchOptions(std::string_view item_str) {
  return base::ToLowerASCII(item_str) == "options";
}

std::optional<SharedStorageHeaderParamType>
StringToSharedStorageHeaderParamType(std::string_view param_str) {
  auto param_it =
      kSharedStorageHeaderParamTypeMap.find(base::ToLowerASCII(param_str));
  if (param_it == kSharedStorageHeaderParamTypeMap.end()) {
    return std::nullopt;
  }

  return param_it->second;
}

bool GetSecSharedStorageWritableHeader(const net::HttpRequestHeaders& headers) {
  std::optional<std::string> value =
      headers.GetHeader(kSecSharedStorageWritableHeader);
  if (!value) {
    return false;
  }
  std::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(*value);
  if (!item || !item->is_boolean() || !item->GetBoolean()) {
    // We only expect the value "?1", which parses to boolean true.
    // TODO(cammie): Log a histogram to see if this ever happens.
    LOG(ERROR) << "Unexpected value '" << *value << "' found for '"
               << kSecSharedStorageWritableHeader << "' header.";
    return false;
  }
  return true;
}

}  // namespace network
