// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_event_params.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>

#include "base/debug/crash_logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

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

std::string Escape(std::string text) {
  std::string escaped;
  escaped.reserve(text.length() * 4);
  char backslash = '\\';
  for (size_t i = 0; i < text.length(); ++i) {
    unsigned char unsigned_ch = static_cast<unsigned char>(text[i]);
    if (base::IsAsciiPrintable(unsigned_ch)) {
      char ascii_ch = static_cast<char>(unsigned_ch);
      if (ascii_ch == '"' || ascii_ch == '\'' || ascii_ch == backslash) {
        escaped.push_back(backslash);
      }
      escaped.push_back(ascii_ch);
    } else {
      escaped.push_back(backslash);
      switch (unsigned_ch) {
        case '\0':
          // null
          escaped.push_back('0');
          break;
        case '\a':
          // bell
          escaped.push_back('a');
          break;
        case '\b':
          // backspace
          escaped.push_back('b');
          break;
        case '\t':
          // horizontal tab
          escaped.push_back('t');
          break;
        case '\n':
          // new line
          escaped.push_back('n');
          break;
        case '\v':
          // vertical tab
          escaped.push_back('v');
          break;
        case '\f':
          // new page
          escaped.push_back('f');
          break;
        case '\r':
          // carriage return
          escaped.push_back('r');
          break;
        default:
          // hex value
          escaped.push_back('x');
          base::AppendHexEncodedByte(text[i], escaped);
      }
    }
  }
  escaped.shrink_to_fit();
  return escaped;
}

std::string SerializeAndEscapeOptionalString(std::optional<std::string> str) {
  if (str) {
    return Escape(*str);
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

  bool comma = false;
  std::ostringstream ss;
  ss << "[ ";
  for (const auto& url_with_metadata : *urls_with_metadata) {
    if (comma) {
      ss << ", ";
    } else {
      comma = true;
    }
    ss << url_with_metadata;
  }
  ss << " ]";

  return ss.str();
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
    const SharedStorageUrlSpecWithMetadata& other) const = default;

std::ostream& operator<<(
    std::ostream& os,
    const SharedStorageEventParams::SharedStorageUrlSpecWithMetadata&
        url_with_metadata) {
  bool comma = false;
  os << "{ URL: '" << url_with_metadata.url << "', Reporting Metadata: {";
  for (const auto& metadata_pair : url_with_metadata.reporting_metadata) {
    if (comma) {
      os << ",";
    } else {
      comma = true;
    }
    os << " '" << metadata_pair.first << "': '" << metadata_pair.second << "'";
  }
  if (!url_with_metadata.reporting_metadata.empty()) {
    os << " ";
  }
  os << "} }";

  return os;
}

SharedStorageEventParams::SharedStorageEventParams(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::~SharedStorageEventParams() = default;

SharedStorageEventParams& SharedStorageEventParams::operator=(
    const SharedStorageEventParams&) = default;

SharedStorageEventParams::SharedStorageEventParams() = default;

SharedStorageEventParams::SharedStorageEventParams(
    std::optional<std::string> script_source_url,
    std::optional<std::string> data_origin,
    std::optional<std::string> operation_name,
    std::optional<std::string> serialized_data,
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata,
    std::optional<std::string> key,
    std::optional<std::string> value,
    std::optional<bool> ignore_if_present,
    std::optional<int> worklet_id)
    : script_source_url(std::move(script_source_url)),
      data_origin(std::move(data_origin)),
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
  return SharedStorageEventParams::CreateForWorkletCreation(
      script_source_url, /*data_origin=*/std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForCreateWorklet(
    const GURL& script_source_url,
    const std::string& data_origin,
    int worklet_id) {
  return SharedStorageEventParams::CreateForWorkletCreation(
      script_source_url, data_origin, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForRun(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    int worklet_id) {
  return SharedStorageEventParams::CreateForWorkletOperation(
      operation_name, serialized_data,
      /*urls_with_metadata=*/std::nullopt, worklet_id);
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
      key, value,
      /*ignore_if_present=*/std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForGetOrDelete(
    const std::string& key,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      key,
      /*value=*/std::nullopt,
      /*ignore_if_present=*/std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateWithWorkletId(
    int worklet_id) {
  return SharedStorageEventParams::CreateForModifierMethod(
      /*key=*/std::nullopt,
      /*value=*/std::nullopt,
      /*ignore_if_present=*/std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateDefault() {
  return SharedStorageEventParams();
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForWorkletCreation(
    const GURL& script_source_url,
    std::optional<std::string> data_origin,
    int worklet_id) {
  return SharedStorageEventParams(
      script_source_url.spec(), std::move(data_origin),
      /*operation_name=*/std::nullopt,
      /*serialized_data=*/std::nullopt,
      /*urls_with_metadata=*/std::nullopt,
      /*key=*/std::nullopt,
      /*value=*/std::nullopt,
      /*ignore_if_present=*/std::nullopt, worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForWorkletOperation(
    const std::string& operation_name,
    const blink::CloneableMessage& serialized_data,
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata,
    int worklet_id) {
  return SharedStorageEventParams(/*script_source_url=*/std::nullopt,
                                  /*data_origin=*/std::nullopt, operation_name,
                                  MaybeTruncateSerializedData(serialized_data),
                                  std::move(urls_with_metadata),
                                  /*key=*/std::nullopt,
                                  /*value=*/std::nullopt,
                                  /*ignore_if_present=*/std::nullopt,
                                  worklet_id);
}

// static
SharedStorageEventParams SharedStorageEventParams::CreateForModifierMethod(
    std::optional<std::string> key,
    std::optional<std::string> value,
    std::optional<bool> ignore_if_present,
    std::optional<int> worklet_id) {
  return SharedStorageEventParams(
      /*script_source_url=*/std::nullopt,
      /*data_origin=*/std::nullopt,
      /*operation_name=*/std::nullopt,
      /*serialized_data*/ std::nullopt,
      /*urls_with_metadata=*/std::nullopt, std::move(key), std::move(value),
      ignore_if_present, worklet_id);
}

// Note that for `serialized_data`, we only match its presence or absence.
bool operator==(const SharedStorageEventParams& lhs,
                const SharedStorageEventParams& rhs) {
  return lhs.script_source_url == rhs.script_source_url &&
         lhs.data_origin == rhs.data_origin &&
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
     << "; Data Origin: " << SerializeOptionalString(params.data_origin)
     << "; Operation Name: " << SerializeOptionalString(params.operation_name)
     << "; Serialized Data: "
     << SerializeAndEscapeOptionalString(params.serialized_data)
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
