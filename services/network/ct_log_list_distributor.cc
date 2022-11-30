// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ct_log_list_distributor.h"

#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "net/cert/ct_log_verifier.h"
#include "services/network/public/cpp/features.h"

namespace network {

CtLogListDistributor::CtLogListDistributor() = default;

CtLogListDistributor::~CtLogListDistributor() = default;

void CtLogListDistributor::OnNewCtConfig(
    const std::vector<mojom::CTLogInfoPtr>& log_list) {
  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
  for (auto& log : log_list) {
    scoped_refptr<const net::CTLogVerifier> log_verifier =
        net::CTLogVerifier::Create(log->public_key, log->name);
    if (!log_verifier) {
      // TODO(crbug.com/1211056): Signal bad configuration (such as bad key).
      continue;
    }
    ct_logs.push_back(std::move(log_verifier));
  }

  NotifyCallbacks(ct_logs);
}

}  // namespace network
