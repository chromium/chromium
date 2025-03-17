// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_event_params.h"

#include <algorithm>
#include <ostream>

#include "base/debug/crash_logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace content {

namespace {
using SharedStorageUrlSpecWithMetadata =
    SharedStorageEventParams::SharedStorageUrlSpecWithMetadata;

const size_t kSharedStorageSerializedDataLengthLimitForEventParams = 1024;

std::string SerializeOptionalString(std::optional<std::string> str) {
  if (str) {
    return *str;
  }

  return "std::nullopt";
}

std::string SerializeOptionalBool(std::optional<bool> b) {
  if (b) {
    return base::ToString(*b);
  }

  return "std::nullopt";
}

std::string SerializeOptionalInt(std::optional<int> i) {
  if (i) {
    return base::NumberToString(*i);
  }

  return "std::nullopt";
}

std::string SerializeOptionalUrlsWithMetadata(
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata) {
  if (!urls_with_metadata) {
    return "std::nullopt";
  }

  std::vector<std::string> urls_str_vector = {"{ "};
  for (const auto& url_with_metadata : *urls_with_metadata) {
    urls_str_vector.push_back("{url: ");
    urls_str_vector.push_back(url_with_metadata.url);
    urls_str_vector.push_back(", reporting_metadata: { ");
    for (const auto& metadata_pair : url_with_metadata.reporting_metadata) {
      urls_str_vector.push_back("{");
      urls_str_vector.push_back(metadata_pair.first);
      urls_str_vector.push_back(" : ");
      urls_str_vector.push_back(metadata_pair.second);
      urls_str_vector.push_back("} ");
    }
    urls_str_vector.push_back("}} ");
  }
  urls_str_vector.push_back("}");

  return base::StrCat(urls_str_vector);
}

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

}  // namespace

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
    std::optional<bool> ignore_if_present,
    std::optional<int> worklet_id)
    : script_source_url(std::move(script_source_url)),
      operation_name(std::move(operation_name)),
      serialized_data(std::move(serialized_data)),
      urls_with_metadata(std::move(urls_with_metadata)),
      key(std::move(key)),
      value(std::move(value)),
      ignore_if_present(ignore_if_present),
      worklet_id(worklet_id) {}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAddModule(
    const GURL& script_source_url,
    int worklet_id) {
  return SharedStorageEventParams(script_source_url.spec(), std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt,
                                  std::nullopt, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForRun(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    int worklet_id) {
  return SharedStorageEventParams::CreateForWorkletOperation(
      operation_name, serialized_data, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSelectURL(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    std::vector<SharedStorageUrlSpecWithMetadata> urls_with_metadata,
    int worklet_id) {
  return SharedStorageEventParams::CreateForWorkletOperation(
      operation_name, serialized_data, std::move(urls_with_metadata),
      worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForSet(
    const std::string& key,
    const std::string& value,
    bool ignore_if_present,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      key, value, ignore_if_present, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForAppend(
    const std::string& key,
    const std::string& value,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      key, value, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForGetOrDelete(
    const std::string& key,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      key, std::nullopt, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateWithWorkletId(
    int worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      std::nullopt, std::nullopt, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateDefault() {
  return SharedStorageEventParams();
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForWorkletOperation(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata,
    int worklet_id) {
  return SharedStorageEventParams(std::nullopt, operation_name,
                                  MaybeTruncateSerializedData(serialized_data),
                                  std::move(urls_with_metadata), std::nullopt,
                                  std::nullopt, std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForModifierMethod(
    std::optional<std::string> key,
    std::optional<std::string> value,
    std::optional<bool> ignore_if_present,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams(
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::move(key),
      std::move(value), ignore_if_present, worklet_id);
}

// Note that for `serialized_data`, we only match its presence or absence.
bool operator==(const SharedStorageEventParams& lhs,
                const SharedStorageEventParams& rhs) {
  return lhs.script_source_url == rhs.script_source_url &&
         lhs.operation_name == rhs.operation_name &&
         !!lhs.serialized_data == !!rhs.serialized_data &&
         lhs.urls_with_metadata == rhs.urls_with_metadata &&
         lhs.key == rhs.key && lhs.value == rhs.value &&
         lhs.ignore_if_present == rhs.ignore_if_present &&
         lhs.worklet_id == rhs.worklet_id;
}

std::ostream& operator<<(std::ostream& os,
                         const SharedStorageEventParams& params) {
  os << "{ Script Source URL: "
     << SerializeOptionalString(params.script_source_url)
     << "; Operation Name: " << SerializeOptionalString(params.operation_name)
     << "; Serialized Data: " << SerializeOptionalString(params.serialized_data)
     << "; URLs With Metadata: "
     << SerializeOptionalUrlsWithMetadata(params.urls_with_metadata)
     << "; Key: " << SerializeOptionalString(params.key)
     << "; Value: " << SerializeOptionalString(params.value)
     << "; Ignore If Present: "
     << SerializeOptionalBool(params.ignore_if_present)
     << "; Worklet ID: " << SerializeOptionalInt(params.worklet_id) << " }";
  return os;
}

}  // namespace content
