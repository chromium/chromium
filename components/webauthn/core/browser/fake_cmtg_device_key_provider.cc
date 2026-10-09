// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/fake_cmtg_device_key_provider.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace webauthn {

namespace {

// The size of a CMTG wrapper key, in bytes.
constexpr size_t kWrapperKeySize = 32;

// Returns the JSON encoding of a `CmtgWrapperKeyInfo` message.
base::DictValue KeyToJson(base::span<const uint8_t> key,
                          base::Time create_time) {
  return base::DictValue()
      .Set("keyMaterial", base::Base64Encode(key))
      .Set("keyAlgorithm", "AES256_GCM")
      .Set("createTime", base::TimeFormatAsIso8601(create_time));
}

// Checks that `request` looks like one sent by CryptauthCmtgDeviceKeyProvider.
void CheckRequest(const network::ResourceRequest& request) {
  CHECK_EQ(request.url.query(), "alt=json");
  CHECK_EQ(request.method, net::HttpRequestHeaders::kPostMethod);
  CHECK(base::StartsWith(
      request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization)
          .value_or(""),
      "Bearer "));
  CHECK(request.request_body);
  const std::vector<network::DataElement>* const elements =
      request.request_body->elements();
  CHECK_EQ(elements->size(), 1u);
  CHECK_EQ(elements->at(0).type(), network::DataElement::Tag::kBytes);
  CHECK_EQ(elements->at(0).As<network::DataElementBytes>().AsStringPiece(),
           "{}");
}

}  // namespace

FakeCmtgDeviceKeyProvider::RequestImpl::RequestImpl() = default;
FakeCmtgDeviceKeyProvider::RequestImpl::~RequestImpl() = default;

FakeCmtgDeviceKeyProvider::FakeCmtgDeviceKeyProvider() = default;
FakeCmtgDeviceKeyProvider::~FakeCmtgDeviceKeyProvider() = default;

std::unique_ptr<CmtgDeviceKeyProvider::Request>
FakeCmtgDeviceKeyProvider::GetDeviceKeys(Operation operation,
                                         Callback callback) {
  if (hold_callback_) {
    pending_callback_ = std::move(callback);
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeCmtgDeviceKeyProvider::DeliverResult,
                                  weak_ptr_factory_.GetWeakPtr(), operation,
                                  std::move(callback)));
  }
  return std::make_unique<RequestImpl>();
}

base::RepeatingCallback<
    FakeCmtgDeviceKeyProvider::MaybeResponse(const network::ResourceRequest&)>
FakeCmtgDeviceKeyProvider::GetCallback() {
  return base::BindRepeating(
      [](base::WeakPtr<FakeCmtgDeviceKeyProvider> fake,
         const network::ResourceRequest& request) -> MaybeResponse {
        // Passing a WeakPtr into BindRepeating is only supported if the
        // function returns void. Thus the WeakPtr is handled directly here.
        if (!fake) {
          return std::nullopt;
        }
        return fake->OnRequest(request);
      },
      weak_ptr_factory_.GetWeakPtr());
}

size_t FakeCmtgDeviceKeyProvider::AddDevice() {
  // The new device's ID is unused as a trust group, so it starts in its own.
  const size_t device = devices_.size();
  devices_.emplace_back(/*trust_group=*/device);
  return device;
}

void FakeCmtgDeviceKeyProvider::MergeTrustGroups(size_t device_a,
                                                 size_t device_b) {
  const size_t group_a = devices_[device_a].trust_group;
  const size_t group_b = devices_[device_b].trust_group;
  for (Device& device : devices_) {
    if (device.trust_group == group_b) {
      device.trust_group = group_a;
    }
  }
}

void FakeCmtgDeviceKeyProvider::SetCurrentDevice(size_t device) {
  current_device_ = device;
}

void FakeCmtgDeviceKeyProvider::SetNextKeys(
    std::vector<std::vector<uint8_t>> keys) {
  next_result_ = std::move(keys);
}

void FakeCmtgDeviceKeyProvider::SetNextError(Error error) {
  next_result_ = base::unexpected(error);
}

void FakeCmtgDeviceKeyProvider::SetHoldCallback(bool hold) {
  hold_callback_ = hold;
}

void FakeCmtgDeviceKeyProvider::ResolvePending(
    std::vector<std::vector<uint8_t>> keys) {
  CHECK(pending_callback_);
  std::move(pending_callback_).Run(std::move(keys));
}

void FakeCmtgDeviceKeyProvider::RejectPending(Error error) {
  CHECK(pending_callback_);
  std::move(pending_callback_).Run(base::unexpected(error));
}

FakeCmtgDeviceKeyProvider::Key::Key(std::vector<uint8_t> key,
                                    base::Time create_time)
    : key(std::move(key)), create_time(create_time) {}
FakeCmtgDeviceKeyProvider::Key::Key(const Key&) = default;
FakeCmtgDeviceKeyProvider::Key& FakeCmtgDeviceKeyProvider::Key::operator=(
    const Key&) = default;
FakeCmtgDeviceKeyProvider::Key::~Key() = default;

FakeCmtgDeviceKeyProvider::Device::Device(size_t trust_group)
    : trust_group(trust_group) {}
FakeCmtgDeviceKeyProvider::Device::Device(const Device&) = default;
FakeCmtgDeviceKeyProvider::Device& FakeCmtgDeviceKeyProvider::Device::operator=(
    const Device&) = default;
FakeCmtgDeviceKeyProvider::Device::~Device() = default;

void FakeCmtgDeviceKeyProvider::DeliverResult(Operation operation,
                                              Callback callback) {
  std::move(callback).Run(
      TakeNextResult(operation).transform([](std::vector<Key> keys) {
        std::vector<std::vector<uint8_t>> result;
        for (Key& key : keys) {
          result.push_back(std::move(key.key));
        }
        return result;
      }));
}

FakeCmtgDeviceKeyProvider::MaybeResponse FakeCmtgDeviceKeyProvider::OnRequest(
    const network::ResourceRequest& request) {
  if (request.url.GetWithEmptyPath() != GURL(kCmtgServiceUrl)) {
    return std::nullopt;
  }
  const std::string path(request.url.path());
  CHECK(path == kCmtgGetOrCreatePath || path == kCmtgBatchGetPath) << path;
  CheckRequest(request);
  if (hold_callback_) {
    return std::nullopt;
  }

  base::expected<std::vector<Key>, Error> result =
      TakeNextResult(path == kCmtgGetOrCreatePath ? Operation::kMakeCredential
                                                  : Operation::kGetAssertion);
  if (!result.has_value()) {
    switch (result.error()) {
      case Error::kNetworkError:
        return std::make_pair(net::HTTP_INTERNAL_SERVER_ERROR, std::string());
      case Error::kParseError:
        return std::make_pair(net::HTTP_OK, std::string("not json"));
      case Error::kAccessTokenError:
        NOTREACHED() << "Access token errors cannot be simulated by the "
                        "fake service. Use the identity test environment.";
    }
  }

  base::DictValue response;
  if (path == kCmtgGetOrCreatePath) {
    // An empty result is answered without a key, which the client rejects.
    if (!result->empty()) {
      response.Set("cmtgWrapperKeyInfo",
                   KeyToJson(result->front().key, result->front().create_time));
    }
  } else {
    base::ListValue keys;
    for (const Key& key : *result) {
      keys.Append(KeyToJson(key.key, key.create_time));
    }
    response.Set("cmtgWrapperKeys", std::move(keys));
  }
  return std::make_pair(net::HTTP_OK, *base::WriteJson(response));
}

const FakeCmtgDeviceKeyProvider::Key&
FakeCmtgDeviceKeyProvider::GetOrCreateCurrentDeviceKey() {
  std::optional<Key>& key = devices_[current_device_].key;
  if (!key) {
    // Give each device a distinct, obviously fake, key.
    key.emplace(std::vector<uint8_t>(kWrapperKeySize,
                                     static_cast<uint8_t>(current_device_ + 1)),
                base::Time::Now());
  }
  return *key;
}

base::expected<std::vector<FakeCmtgDeviceKeyProvider::Key>,
               CmtgDeviceKeyProvider::Error>
FakeCmtgDeviceKeyProvider::TakeNextResult(Operation operation) {
  if (next_result_) {
    return std::exchange(next_result_, std::nullopt)
        ->transform([](std::vector<std::vector<uint8_t>> keys) {
          std::vector<Key> result;
          for (std::vector<uint8_t>& key : keys) {
            result.emplace_back(std::move(key), base::Time::Now());
          }
          return result;
        });
  }
  std::vector<Key> keys = {GetOrCreateCurrentDeviceKey()};
  if (operation == Operation::kGetAssertion) {
    const size_t trust_group = devices_[current_device_].trust_group;
    for (size_t i = 0; i < devices_.size(); ++i) {
      const Device& device = devices_[i];
      if (i != current_device_ && device.trust_group == trust_group &&
          device.key) {
        keys.push_back(*device.key);
      }
    }
  }
  return keys;
}

}  // namespace webauthn
