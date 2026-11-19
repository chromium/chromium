// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "net/http/http_util.h"

namespace net {

WebSocketExtension::Parameter::Parameter(std::string name)
    : name_(std::move(name)) {}

WebSocketExtension::Parameter::Parameter(std::string name, std::string value)
    : name_(std::move(name)), value_(std::move(value)) {
  DCHECK(!value_.empty());
  // `extension-param` must be a token.
  DCHECK(HttpUtil::IsToken(value_));
}

WebSocketExtension::Parameter::Parameter(const Parameter&) = default;
WebSocketExtension::Parameter& WebSocketExtension::Parameter::operator=(
    const Parameter&) = default;
WebSocketExtension::Parameter::Parameter(Parameter&&) noexcept = default;
WebSocketExtension::Parameter& WebSocketExtension::Parameter::operator=(
    Parameter&&) noexcept = default;
WebSocketExtension::Parameter::~Parameter() = default;

WebSocketExtension::WebSocketExtension(std::string name)
    : name_(std::move(name)) {}

WebSocketExtension::WebSocketExtension(const WebSocketExtension&) = default;
WebSocketExtension& WebSocketExtension::operator=(const WebSocketExtension&) =
    default;
WebSocketExtension::WebSocketExtension(WebSocketExtension&&) noexcept = default;
WebSocketExtension& WebSocketExtension::operator=(
    WebSocketExtension&&) noexcept = default;
WebSocketExtension::~WebSocketExtension() = default;

bool WebSocketExtension::Equivalent(const WebSocketExtension& other) const {
  if (name_ != other.name_) return false;
  if (parameters_.size() != other.parameters_.size()) return false;

  // Take copies in order to sort.
  std::vector<Parameter> mine_sorted = parameters_;
  std::vector<Parameter> other_sorted = other.parameters_;

  auto comparator = std::less<std::string>();
  auto extract_name = [](const Parameter& param) -> const std::string& {
    return param.name();
  };

  // Sort by key, preserving order of values.
  std::ranges::stable_sort(mine_sorted, comparator, extract_name);
  std::ranges::stable_sort(other_sorted, comparator, extract_name);

  return mine_sorted == other_sorted;
}

std::string WebSocketExtension::ToString() const {
  if (name_.empty())
    return std::string();

  std::string result = name_;

  for (const auto& param : parameters_) {
    result += "; " + param.name();
    if (!param.HasValue())
      continue;

    // |extension-param| must be a token and we don't need to quote it.
    DCHECK(HttpUtil::IsToken(param.value()));
    result += "=" + param.value();
  }
  return result;
}

}  // namespace net
