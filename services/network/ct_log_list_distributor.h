// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CT_LOG_LIST_DISTRIBUTOR_H_
#define SERVICES_NETWORK_CT_LOG_LIST_DISTRIBUTOR_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"

namespace network {

// Handles distribution of component updater delivered Certificate
// Transparency enforcement configuration to classes that need to be
// notified about changes.

// TODO(crbug.com/1211074): Once CT log list configuration is exposed via
// CertVerifier::Config, we should configure changes using that instead.
class COMPONENT_EXPORT(NETWORK_SERVICE) CtLogListDistributor
    : public net::MultiLogCTVerifier::CTLogProvider {
 public:
  CtLogListDistributor();
  virtual ~CtLogListDistributor();

  void OnNewCtConfig(const std::vector<mojom::CTLogInfoPtr>& log_list);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CT_LOG_LIST_DISTRIBUTOR_H_
