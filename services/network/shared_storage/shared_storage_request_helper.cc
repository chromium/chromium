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
#include "base/strings/utf_string_conversions.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/shared_storage_utils.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
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
  if (!request.response_headers()) {
    return std::nullopt;
  }
  return request.response_headers()->GetNormalizedHeader(
      kSharedStorageWriteHeader);
}

void RemoveSharedStorageWriteHeader(net::URLRequest& request) {
  if (!request.response_headers()) {
    return;
  }
  request.response_headers()->RemoveHeader(kSharedStorageWriteHeader);
}

const net::structured_headers::Item* ParseIntoSharedStorageHeaderItem(
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

  return &item;
}

std::optional<std::string> ParseWithLockParam(
    const net::structured_headers::Parameters& params) {
  for (const auto& [param_key, param_item] : params) {
    if (!IsStringLike(param_item)) {
      // Not a valid parameter item type for 'with_lock' option.
      continue;
    }

    std::optional<SharedStorageHeaderParamType> param_type =
        StringToSharedStorageHeaderParamType(param_key);
    if (!param_type.has_value()) {
      // Did not find a valid parameter key.
      continue;
    }

    if (param_type.value() == SharedStorageHeaderParamType::kWithLock) {
      // Skip on reserved lock name.
      if (!IsReservedLockName(param_item.GetString())) {
        return param_item.GetString();
      }
    }
  }

  return std::nullopt;
}

mojom::SharedStorageModifierMethodWithOptionsPtr
MakeSharedStorageModifierMethodWithOptions(
    const net::structured_headers::Item& item,
    const net::structured_headers::Parameters& params) {
  std::optional<SharedStorageModifierMethodType> method_type =
      StringToSharedStorageModifierMethodType(item.GetString());
  if (!method_type.has_value()) {
    // Did not find a valid method type.
    return nullptr;
  }

  std::optional<std::u16string> key;
  std::optional<std::u16string> value;
  bool ignore_if_present = false;
  std::optional<std::string> with_lock;

  for (const auto& [param_key, param_item] : params) {
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
      ignore_if_present = param_item.GetBoolean();
      continue;
    }

    if (param_type.value() == SharedStorageHeaderParamType::kWithLock) {
      // Skip on reserved lock name.
      if (!IsReservedLockName(param_item.GetString())) {
        with_lock = param_item.GetString();
      }
      continue;
    }

    std::u16string u16_key_or_value;
    if (!base::UTF8ToUTF16(param_item.GetString().c_str(),
                           param_item.GetString().size(), &u16_key_or_value)) {
      continue;
    }

    switch (param_type.value()) {
      case SharedStorageHeaderParamType::kKey: {
        if (!IsValidSharedStorageKeyStringLength(u16_key_or_value.size())) {
          continue;
        }
        key = std::move(u16_key_or_value);
        break;
      }
      case SharedStorageHeaderParamType::kValue: {
        if (!IsValidSharedStorageValueStringLength(u16_key_or_value.size())) {
          continue;
        }
        value = std::move(u16_key_or_value);
        break;
      }
      case SharedStorageHeaderParamType::kIgnoreIfPresent: {
        NOTREACHED();
      }
      case SharedStorageHeaderParamType::kWithLock: {
        NOTREACHED();
      }
    }
  }

  mojom::SharedStorageModifierMethodPtr method;

  switch (method_type.value()) {
    case SharedStorageModifierMethodType::kSet: {
      if (!key || !value) {
        return nullptr;
      }

      method = mojom::SharedStorageModifierMethod::NewSetMethod(
          mojom::SharedStorageSetMethod::New(key.value(), value.value(),
                                             ignore_if_present));
      break;
    }
    case SharedStorageModifierMethodType::kAppend: {
      if (!key || !value) {
        return nullptr;
      }

      method = mojom::SharedStorageModifierMethod::NewAppendMethod(
          mojom::SharedStorageAppendMethod::New(key.value(), value.value()));
      break;
    }
    case SharedStorageModifierMethodType::kDelete: {
      if (!key) {
        return nullptr;
      }

      method = mojom::SharedStorageModifierMethod::NewDeleteMethod(
          mojom::SharedStorageDeleteMethod::New(key.value()));
      break;
    }
    case SharedStorageModifierMethodType::kClear: {
      method = mojom::SharedStorageModifierMethod::NewClearMethod(
          mojom::SharedStorageClearMethod::New());
      break;
    }
  }

  return mojom::SharedStorageModifierMethodWithOptions::New(
      std::move(method), std::move(with_lock));
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
  parse_results.reserve(list.value().size());

  std::vector<mojom::SharedStorageModifierMethodWithOptionsPtr>
      methods_with_options;
  methods_with_options.reserve(list.value().size());

  std::optional<std::string> with_lock;

  for (const auto& member : list.value()) {
    const net::structured_headers::Item* item =
        ParseIntoSharedStorageHeaderItem(member);
    if (!item) {
      parse_results.push_back(false);
      continue;
    }

    auto method_with_options =
        MakeSharedStorageModifierMethodWithOptions(*item, member.params);
    if (method_with_options) {
      methods_with_options.push_back(std::move(method_with_options));
      parse_results.push_back(true);
    } else if (IsHeaderItemBatchOptions(item->GetString())) {
      // The batch "options" item may appear multiple times in the list, and
      // should override any previously parsed value. Currently, the only
      // relevant parameter is "with_lock".
      with_lock = ParseWithLockParam(member.params);
      parse_results.push_back(true);
    } else {
      parse_results.push_back(false);
    }
  }

  // While header parsing is generally lenient, we enforce strict validation
  // for the deprecated 'with_lock' parameter in batch inner methods to prevent
  // potential misuse.
  if (!IsValidSharedStorageBatchUpdateMethodsArgument(methods_with_options)) {
    return false;
  }

  observer_->OnSharedStorageHeaderReceived(
      url::Origin::Create(request.url()), std::move(methods_with_options),
      with_lock,
      base::BindOnce(&SharedStorageRequestHelper::OnMethodsQueued,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done)));
  return true;
}

void SharedStorageRequestHelper::OnMethodsQueued(base::OnceClosure done) {
  std::move(done).Run();
}

}  // namespace network
