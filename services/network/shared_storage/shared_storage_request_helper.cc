// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_storage/shared_storage_request_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/shared_storage/shared_storage_header_utils.h"

namespace network {

namespace {

[[nodiscard]] bool IsStringLike(const net::structured_headers::Item& item) {
  return item.is_string() || item.is_token() || item.is_byte_sequence();
}

// Adds request header `kSecSharedStorageWritableHeader`.
void AddWritableRequestHeader(net::URLRequest& request) {
  request.SetExtraRequestHeaderByName(kSecSharedStorageWritableHeader,
                                      kSecSharedStorageWritableValue,
                                      /*overwrite=*/true);
}

std::optional<std::string> GetSharedStorageWriteHeader(
    net::URLRequest& request) {
  std::string value;
  if (!request.response_headers() ||
      !request.response_headers()->GetNormalizedHeader(
          kSharedStorageWriteHeader, &value)) {
    return std::nullopt;
  }
  return value;
}

void RemoveSharedStorageWriteHeader(net::URLRequest& request) {
  if (!request.response_headers()) {
    return;
  }
  request.response_headers()->RemoveHeader(kSharedStorageWriteHeader);
}

mojom::SharedStorageOperationPtr MakeSharedStorageOperation(
    const net::structured_headers::ParameterizedMember& parameterized_member) {
  if (parameterized_member.member_is_inner_list ||
      parameterized_member.member.size() != 1) {
    // Not a valid item type for "Shared-Storage-Write" header.
    return nullptr;
  }
  const net::structured_headers::Item& item =
      parameterized_member.member.front().item;
  if (!IsStringLike(item)) {
    // Not a valid item type for "Shared-Storage-Write" header.
    return nullptr;
  }

  std::optional<mojom::SharedStorageOperationType> operation_type =
      StringToSharedStorageOperationType(item.GetString());
  if (!operation_type.has_value()) {
    // Did not find a valid operation type.
    return nullptr;
  }

  std::optional<std::string> key;
  std::optional<std::string> value;
  mojom::OptionalBool ignore_if_present = mojom::OptionalBool::kUnset;

  for (const auto& [param_key, param_item] : parameterized_member.params) {
    if (!IsStringLike(param_item) && !param_item.is_boolean()) {
      // Not a valid parameter item type for "Shared-Storage-Write" header.
      continue;
    }

    std::optional<SharedStorageHeaderParamType> param_type =
        StringToSharedStorageHeaderParamType(param_key);
    if (!param_type.has_value()) {
      // Did not find a valid parameter key.
      continue;
    }

    if (param_item.is_boolean()) {
      if (param_type.value() !=
          SharedStorageHeaderParamType::kIgnoreIfPresent) {
        // Unexpected `param_type` for boolean `param_item`.
        continue;
      }
      ignore_if_present = param_item.GetBoolean() ? mojom::OptionalBool::kTrue
                                                  : mojom::OptionalBool::kFalse;
      continue;
    }

    if (param_type.value() == SharedStorageHeaderParamType::kKey) {
      key = param_item.GetString();
    } else {
      DCHECK_EQ(param_type.value(), SharedStorageHeaderParamType::kValue);
      value = param_item.GetString();
    }
  }

  return mojom::SharedStorageOperation::New(operation_type.value(),
                                            std::move(key), std::move(value),
                                            ignore_if_present);
}

}  // namespace

SharedStorageRequestHelper::SharedStorageRequestHelper(
    bool shared_storage_writable_eligible,
    mojom::URLLoaderNetworkServiceObserver* observer)
    : shared_storage_writable_eligible_(shared_storage_writable_eligible),
      observer_(observer) {}

SharedStorageRequestHelper::~SharedStorageRequestHelper() = default;

void SharedStorageRequestHelper::ProcessOutgoingRequest(
    net::URLRequest& request) {
  if (!shared_storage_writable_eligible_ || !observer_) {
    // This `request` isn't eligible for shared storage writes and/or `this` has
    // no `observer_` to forward any processed shared storage response header
    // to.
    return;
  }

  // This `request` should have the `kSecSharedStorageWritableHeader` added.
  AddWritableRequestHeader(request);
}

bool SharedStorageRequestHelper::ProcessIncomingResponse(
    net::URLRequest& request,
    base::OnceClosure done) {
  DCHECK(done);

  std::optional<std::string> header_value =
      GetSharedStorageWriteHeader(request);
  if (!header_value.has_value()) {
    // This response doesn't have any shared storage response headers yet.
    return false;
  }

  if (!shared_storage_writable_eligible_ || !observer_) {
    // This response has a shared storage header but isn't eligible to write to
    // shared storage (or there is no `observer_` to forward the processed
    // response header to); remove the header(s) to prevent a cross-site data
    // leak to the renderer.
    RemoveSharedStorageWriteHeader(request);
    return false;
  }

  // Process and remove the header; then continue the flow.
  return ProcessResponse(request, header_value.value(), std::move(done));
}

void SharedStorageRequestHelper::UpdateSharedStorageWritableEligible(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers) {
  // Note that in `net::RedirectUtil::UpdateHttpRequest()`, `modified_headers`
  // are set after `removed_headers` are removed, so if the
  // `kSecSharedStorageWritableHeader` is in both of these, ``modified_headers`
  // takes precedence.
  if (GetSecSharedStorageWritableHeader(modified_headers)) {
    shared_storage_writable_eligible_ = true;
  } else if (base::Contains(removed_headers, kSecSharedStorageWritableHeader)) {
    shared_storage_writable_eligible_ = false;
  }
}

bool SharedStorageRequestHelper::ProcessResponse(net::URLRequest& request,
                                                 std::string_view value,
                                                 base::OnceClosure done) {
  DCHECK(observer_);
  DCHECK(done);
  RemoveSharedStorageWriteHeader(request);

  std::optional<net::structured_headers::List> list =
      net::structured_headers::ParseList(value);
  if (!list.has_value()) {
    // Parsing has failed.
    return false;
  }

  // TODO(crbug.com/40064101): Use `parse_results` to record a histogram of
  // whether or not there were any parsing errors.
  std::vector<bool> parse_results;
  std::vector<mojom::SharedStorageOperationPtr> operations;
  for (const auto& member : list.value()) {
    auto operation = MakeSharedStorageOperation(member);
    if (operation) {
      operations.push_back(std::move(operation));
      parse_results.push_back(true);
    } else {
      parse_results.push_back(false);
    }
  }

  if (operations.empty()) {
    // Either the header value parsed to an empty list, or else none of the
    // items on the list parsed to a recognized `SharedStorageOperation`.
    return false;
  }

  observer_->OnSharedStorageHeaderReceived(
      url::Origin::Create(request.url()), std::move(operations),
      base::BindOnce(&SharedStorageRequestHelper::OnOperationsQueued,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done)));
  return true;
}

void SharedStorageRequestHelper::OnOperationsQueued(base::OnceClosure done) {
  std::move(done).Run();
}

}  // namespace network
