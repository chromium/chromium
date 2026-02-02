// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/common/manifest_id.h"

#include <ostream>
#include <string_view>

#include "base/check.h"

namespace webapps {

std::optional<ValidManifestId> ValidManifestId::Create(
    std::string_view url_str) {
  GURL url(url_str);
  if (!url.is_valid()) {
    return std::nullopt;
  }
  return ValidManifestId(std::move(url));
}

std::optional<ValidManifestId> ValidManifestId::Create(const GURL& url) {
  if (!url.is_valid()) {
    return std::nullopt;
  }
  return ValidManifestId(url);
}

ValidManifestId::ValidManifestId(GURL url) : url_(url.GetWithoutRef()) {
  CHECK(is_valid());
}

ValidManifestId::ValidManifestId(const ValidManifestId& other) = default;

ValidManifestId& ValidManifestId::operator=(const ValidManifestId& other) =
    default;

ValidManifestId::~ValidManifestId() = default;

bool ValidManifestId::is_valid() const {
  return url_.is_valid();
}

const std::string& ValidManifestId::spec() const {
  CHECK(is_valid());
  return url_.spec();
}

const GURL& ValidManifestId::value() const {
  CHECK(is_valid());
  return url_;
}

auto operator<=>(const ValidManifestId& lhs, const ValidManifestId& rhs) {
  return lhs.value() <=> rhs.value();
}

bool ValidManifestId::operator==(const ValidManifestId& other) const = default;

std::ostream& operator<<(std::ostream& out,
                         const ValidManifestId& manifest_id) {
  return out << manifest_id.value();
}

}  // namespace webapps
