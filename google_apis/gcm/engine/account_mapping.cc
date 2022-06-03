// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/account_mapping.h"

#include <stdint.h>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace gcm {

namespace {

const char kSeparator[] = "&";
const uint32_t kEmailIndex = 0;
const uint32_t kStatusIndex = 1;
const uint32_t kStatusChangeTimestampIndex = 2;
const uint32_t kSizeWithNoMessage = kStatusChangeTimestampIndex + 1;
const uint32_t kMessageIdIndex = 3;
const uint32_t kSizeWithMessage = kMessageIdIndex + 1;

const char kStatusNew[] = "new";
const char kStatusAdding[] = "adding";
const char kStatusMapped[] = "mapped";
const char kStatusRemoving[] = "removing";

std::string StatusToString(AccountMapping::MappingStatus status) {
  switch (status) {
    case AccountMapping::NEW:
      return kStatusNew;
    case AccountMapping::ADDING:
      return kStatusAdding;
    case AccountMapping::MAPPED:
      return kStatusMapped;
    case AccountMapping::REMOVING:
      return kStatusRemoving;
  }
  NOTREACHED();
  return std::string();
}

bool StringToStatus(const std::string& status_str,
                    AccountMapping::MappingStatus* status) {
  if (status_str.compare(kStatusAdding) == 0)
    *status = AccountMapping::ADDING;
  else if (status_str.compare(kStatusMapped) == 0)
    *status = AccountMapping::MAPPED;
  else if (status_str.compare(kStatusRemoving) == 0)
    *status = AccountMapping::REMOVING;
  else if (status_str.compare(kStatusNew) == 0)
    *status = AccountMapping::NEW;
  else
    return false;

  return true;
}

}  // namespace

AccountMapping::AccountMapping() : status(NEW) {
}

AccountMapping::AccountMapping(const AccountMapping& other) = default;

AccountMapping::~AccountMapping() {
}

std::string AccountMapping::SerializeAsString() const {
  std::string value;
  value.append(email);
  value.append(kSeparator);
  value.append(StatusToString(status));
  value.append(kSeparator);
  value.append(base::NumberToString(status_change_timestamp.ToInternalValue()));
  if (!last_message_id.empty()) {
    value.append(kSeparator);
    value.append(last_message_id);
  }

  return value;
}

bool AccountMapping::ParseFromString(const std::string& value) {
  std::vector<std::string> values = base::SplitString(
      value, kSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (values.size() != kSizeWithNoMessage &&
      values.size() != kSizeWithMessage) {
    return false;
  }

  if (values[kEmailIndex].empty() ||
      values[kStatusChangeTimestampIndex].empty() ||
      values[kStatusIndex].empty()) {
    return false;
  }

  if (values.size() == kSizeWithMessage && values[kMessageIdIndex].empty())
    return false;

  MappingStatus temp_status;
  if (!StringToStatus(values[kStatusIndex], &temp_status))
    return false;

  if (values.size() == kSizeWithNoMessage && temp_status == ADDING)
    return false;

  int64_t status_change_ts_internal = 0LL;
  if (!base::StringToInt64(values[kStatusChangeTimestampIndex],
                           &status_change_ts_internal)) {
    return false;
  }

  status_change_timestamp =
      base::Time::FromInternalValue(status_change_ts_internal);
  status = temp_status;
  email = values[kEmailIndex];
  access_token.clear();

  if (values.size() == kSizeWithMessage)
    last_message_id = values[kMessageIdIndex];
  else
    last_message_id.clear();

  return true;
}

}  // namespace gcm
