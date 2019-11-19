// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/core_account_id.h"

CoreAccountId::CoreAccountId() = default;

CoreAccountId::CoreAccountId(const CoreAccountId&) = default;

CoreAccountId::CoreAccountId(CoreAccountId&&) noexcept = default;

CoreAccountId::~CoreAccountId() = default;

CoreAccountId& CoreAccountId::operator=(const CoreAccountId&) = default;

CoreAccountId& CoreAccountId::operator=(CoreAccountId&&) noexcept = default;

CoreAccountId::CoreAccountId(const char* id) : id(id) {}

CoreAccountId::CoreAccountId(std::string&& id) : id(std::move(id)) {}

CoreAccountId::CoreAccountId(const std::string& id) : id(id) {}

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
CoreAccountId::operator std::string() const {
  return id;
}
#endif

bool CoreAccountId::empty() const {
  return id.empty();
}

bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id < rhs.id;
}

bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id == rhs.id;
}

bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs) {
  return lhs.id != rhs.id;
}

std::ostream& operator<<(std::ostream& out, const CoreAccountId& a) {
  return out << a.id;
}

std::vector<std::string> ToStringList(
    const std::vector<CoreAccountId>& account_ids) {
  std::vector<std::string> account_ids_string;
  for (const auto& account_id : account_ids)
    account_ids_string.push_back(account_id.id);
  return account_ids_string;
}