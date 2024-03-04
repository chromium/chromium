// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension.h"

#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "net/http/http_util.h"

namespace net {

WebSocketExtension::Parameter::Parameter(const std::string& name)
    : name_(name) {}

WebSocketExtension::Parameter::Parameter(const std::string& name,
                                         const std::string& value)
    : name_(name), value_(value) {
  DCHECK(!value.empty());
  // |extension-param| must be a token.
  DCHECK(HttpUtil::IsToken(value));
}

bool WebSocketExtension::Parameter::operator==(const Parameter& other) const =
    default;

WebSocketExtension::WebSocketExtension() = default;

WebSocketExtension::WebSocketExtension(const std::string& name)
    : name_(name) {}

WebSocketExtension::WebSocketExtension(const WebSocketExtension& other) =
    default;

WebSocketExtension::~WebSocketExtension() = default;

bool WebSocketExtension::Equivalent(const WebSocketExtension& other) const {
  if (name_ != other.name_) return false;
  if (parameters_.size() != other.parameters_.size()) return false;

  // Take copies in order to sort.
  std::vector<Parameter> mine_sorted = parameters_;
  std::vector<Parameter> other_sorted = other.parameters_;

  auto comparator = std::less<std::string>();
  auto extract_name = [](const Parameter& param) { return param.name(); };
  // Sort by key, preserving order of values.
  base::ranges::stable_sort(mine_sorted, comparator, extract_name);
  base::ranges::stable_sort(other_sorted, comparator, extract_name);

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
