// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_
#define SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_

#include <optional>
#include <string_view>

#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace network {

inline constexpr std::string_view kSecSharedStorageWritableHeader =
    "Sec-Shared-Storage-Writable";
inline constexpr std::string_view kSecSharedStorageWritableValue = "?1";
inline constexpr std::string_view kSharedStorageWriteHeader =
    "Shared-Storage-Write";

enum class SharedStorageModifierMethodType {
  kSet,
  kAppend,
  kDelete,
  kClear,
};

enum class SharedStorageHeaderParamType {
  kKey,
  kValue,
  kIgnoreIfPresent,
  kWithLock,
};

std::optional<SharedStorageModifierMethodType>
StringToSharedStorageModifierMethodType(std::string_view method_str);

// Returns whether `item_str` is a valid "options" structured header item. This
// item is used to specify options for a batch of shared storage methods.
bool IsHeaderItemBatchOptions(std::string_view item_str);

std::optional<SharedStorageHeaderParamType>
StringToSharedStorageHeaderParamType(std::string_view param_str);

bool GetSecSharedStorageWritableHeader(const net::HttpRequestHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_
