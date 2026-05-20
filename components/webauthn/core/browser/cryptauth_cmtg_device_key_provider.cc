// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "crypto/random.h"

namespace webauthn {
namespace {

class RequestImpl : public CmtgDeviceKeyProvider::Request {
 public:
  explicit RequestImpl(CmtgDeviceKeyProvider::Callback callback)
      : start_time_(base::TimeTicks::Now()), callback_(std::move(callback)) {}

  ~RequestImpl() override = default;

  void Start() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RequestImpl::OnComplete,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void Finish(base::expected<std::vector<std::vector<uint8_t>>,
                             CmtgDeviceKeyProvider::Error> result) {
    base::UmaHistogramMediumTimes(
        "WebAuthentication.CmtgDeviceKeys.RequestDuration",
        base::TimeTicks::Now() - start_time_);
    CmtgDeviceKeysResult metric_result =
        result.has_value() ? CmtgDeviceKeysResult::kSuccess
                           : CmtgDeviceKeysResult::kNetworkError;
    base::UmaHistogramEnumeration("WebAuthentication.CmtgDeviceKeys.Result",
                                  metric_result);
    if (callback_) {
      std::move(callback_).Run(std::move(result));
    }
  }

  void OnComplete() {
    // TODO(crbug.com/485888879): Replace mock random key generator with real
    // Cryptauth network request.
    static const base::NoDestructor<std::vector<std::vector<uint8_t>>> kKeys(
        [] {
          std::vector<uint8_t> key(32);
          crypto::RandBytes(key);
          return std::vector<std::vector<uint8_t>>{std::move(key)};
        }());

    Finish(*kKeys);
  }

  base::TimeTicks start_time_;
  CmtgDeviceKeyProvider::Callback callback_;
  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
};

}  // namespace

CryptauthCmtgDeviceKeyProvider::CryptauthCmtgDeviceKeyProvider() = default;
CryptauthCmtgDeviceKeyProvider::~CryptauthCmtgDeviceKeyProvider() = default;

std::unique_ptr<CmtgDeviceKeyProvider::Request>
CryptauthCmtgDeviceKeyProvider::GetDeviceKeys(Callback callback) {
  auto request = std::make_unique<RequestImpl>(std::move(callback));
  request->Start();
  return request;
}

}  // namespace webauthn
