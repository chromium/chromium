// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_
#define SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_

#include "base/strings/string_piece.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

inline constexpr base::StringPiece kSecSharedStorageWritableHeader =
    "Sec-Shared-Storage-Writable";
inline constexpr base::StringPiece kSecSharedStorageWritableValue = "?1";
inline constexpr base::StringPiece kSharedStorageWriteHeader =
    "Shared-Storage-Write";

enum class SharedStorageHeaderParamType {
  kKey,
  kValue,
  kIgnoreIfPresent,
};

absl::optional<network::mojom::SharedStorageOperationType>
StringToSharedStorageOperationType(base::StringPiece operation_str);

absl::optional<SharedStorageHeaderParamType>
StringToSharedStorageHeaderParamType(base::StringPiece param_str);

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_HEADER_UTILS_H_
