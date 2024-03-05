// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/debug/crash_logging.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"

namespace content {

const size_t kSharedStorageSerializedDataLengthLimitForEventParams = 1024;

std::string MaybeTruncateSerializedData(
    const blink::CloneableMessage& serialized_data) {
  SCOPED_CRASH_KEY_NUMBER("SharedStorageEventParams", "data_size",
                          serialized_data.owned_encoded_message.size());
  size_t length =
      std::min(serialized_data.owned_encoded_message.size(),
               kSharedStorageSerializedDataLengthLimitForEventParams);
  return std::string(serialized_data.owned_encoded_message.begin(),
                     serialized_data.owned_encoded_message.begin() + length);
}

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata() = default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata(
        const GURL& url,
        std::map<std::string, std::string> reporting_metadata)
    : url(url.spec()), reporting_metadata(std::move(reporting_metadata)) {}

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    SharedStorageUrlSpecWithMetadata(const SharedStorageUrlSpecWithMetadata&) =
        default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::
    ~SharedStorageUrlSpecWithMetadata() = default;

SharedStorageEventParams::SharedStorageUrlSpecWithMetadata&
SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::operator=(
    const SharedStorageUrlSpecWithMetadata&) = default;

bool SharedStorageEventParams::SharedStorageUrlSpecWithMetadata::operator==(
    const SharedStorageUrlSpecWithMetadata& other) const {
  return url == other.url && reporting_metadata == other.reporting_metadata;
}

SharedStorageEventParams::SharedStorageEventParams(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::~SharedStorageEventParams() = default;

SharedStorageEventParams& SharedStorageEventParams::operator=(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::SharedStorageEventParams() = default;

SharedStorageEventParams::SharedStorageEventParams(
    std::optional<std::string> script_source_url,
    std::optional<std::string> operation_name,
    std::optional<std::string> serialized_data,
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata,
    std::optional<std::string> key,
    std::optional<std::string> value,
    std::optional<bool> ignore_if_present)
    : script_source_url(std::move(script_source_url)),
      operation_name(std::move(operation_name)),
      serialized_data(std::move(serialized_data)),
      urls_with_metadata(std::move(urls_with_metadata)),
      key(std::move(key)),
      value(std::move(value)),
      ignore_if_present(ignore_if_present) {}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAddModule(
    const GURL& script_source_url) {
  return SharedStorageEventParams(script_source_url.spec(), std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, std::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForRun(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data) {
  return SharedStorageEventParams(std::nullopt, operation_name,
                                  MaybeTruncateSerializedData(serialized_data),
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSelectURL(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata) {
  return SharedStorageEventParams(std::nullopt, operation_name,
                                  MaybeTruncateSerializedData(serialized_data),
                                  std::move(urls_with_metadata), std::nullopt,
                                  std::nullopt, std::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSet(
    const std::string& key,
    const std::string& value,
    bool ignore_if_present) {
  return SharedStorageEventParams(std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, key, value, ignore_if_present);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAppend(
    const std::string& key,
    const std::string& value) {
  return SharedStorageEventParams(std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, key, value, std::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForGetOrDelete(
    const std::string& key) {
  return SharedStorageEventParams(std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, key, std::nullopt,
                                  std::nullopt);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateDefault() {
  return SharedStorageEventParams();
}

}  // namespace content
