// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_action.h"

#include <tuple>
#include <utility>

#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions::declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

// Helper to connvert a flat::HeaderOperation to an
// api::declarative_net_request::HeaderOperation.
dnr_api::HeaderOperation ConvertFlatHeaderOperation(
    flat::HeaderOperation operation) {
  switch (operation) {
    case flat::HeaderOperation_append:
      return dnr_api::HeaderOperation::kAppend;
    case flat::HeaderOperation_set:
      return dnr_api::HeaderOperation::kSet;
    case flat::HeaderOperation_remove:
      return dnr_api::HeaderOperation::kRemove;
  }
}

}  // namespace

RequestAction::HeaderInfo::HeaderInfo(std::string header,
                                      dnr_api::HeaderOperation operation,
                                      std::optional<std::string> value)
    : header(std::move(header)),
      operation(operation),
      value(std::move(value)) {}

RequestAction::HeaderInfo::HeaderInfo(const flat::ModifyHeaderInfo& info)
    : header(CreateString<std::string>(*info.header())),
      operation(ConvertFlatHeaderOperation(info.operation())) {
  if (info.value()) {
    value = CreateString<std::string>(*info.value());
  }
}

RequestAction::HeaderInfo::~HeaderInfo() = default;
RequestAction::HeaderInfo::HeaderInfo(const RequestAction::HeaderInfo& other) =
    default;
RequestAction::HeaderInfo& RequestAction::HeaderInfo::operator=(
    const RequestAction::HeaderInfo& other) = default;
RequestAction::HeaderInfo::HeaderInfo(RequestAction::HeaderInfo&&) = default;
RequestAction::HeaderInfo& RequestAction::HeaderInfo::operator=(
    RequestAction::HeaderInfo&&) = default;

RequestAction::RequestAction(RequestAction::Type type,
                             uint32_t rule_id,
                             uint64_t index_priority,
                             RulesetID ruleset_id,
                             const ExtensionId& extension_id)
    : type(type),
      rule_id(rule_id),
      index_priority(index_priority),
      ruleset_id(ruleset_id),
      extension_id(extension_id) {}
RequestAction::~RequestAction() = default;
RequestAction::RequestAction(RequestAction&&) = default;
RequestAction& RequestAction::operator=(RequestAction&&) = default;

RequestAction RequestAction::Clone() const {
  // Use the private copy constructor to create a copy.
  return *this;
}

RequestAction::RequestAction(const RequestAction&) = default;

bool operator<(const RequestAction& lhs, const RequestAction& rhs) {
  return std::tie(lhs.index_priority, lhs.ruleset_id, lhs.rule_id) <
         std::tie(rhs.index_priority, rhs.ruleset_id, rhs.rule_id);
}

bool operator>(const RequestAction& lhs, const RequestAction& rhs) {
  return std::tie(lhs.index_priority, lhs.ruleset_id, lhs.rule_id) >
         std::tie(rhs.index_priority, rhs.ruleset_id, rhs.rule_id);
}

std::optional<RequestAction> GetMaxPriorityAction(
    std::optional<RequestAction> lhs,
    std::optional<RequestAction> rhs) {
  if (!lhs) {
    return rhs;
  }
  if (!rhs) {
    return lhs;
  }
  return lhs > rhs ? std::move(lhs) : std::move(rhs);
}

}  // namespace extensions::declarative_net_request
