// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/mock_data_host.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/gurl.h"

namespace content {

MockDataHost::MockDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  receiver_.Bind(std::move(data_host));
}

MockDataHost::~MockDataHost() = default;

void MockDataHost::WaitForSourceData(size_t num_source_data) {
  WaitForSourceAndTriggerData(num_source_data, /*num_trigger_data=*/0);
}

void MockDataHost::WaitForTriggerData(size_t num_trigger_data) {
  WaitForSourceAndTriggerData(/*num_source_data=*/0, num_trigger_data);
}

void MockDataHost::WaitForSourceAndTriggerData(size_t num_source_data,
                                               size_t num_trigger_data) {
  min_source_data_count_ = num_source_data;
  min_trigger_data_count_ = num_trigger_data;
  if (source_data_.size() >= min_source_data_count_ &&
      trigger_data_.size() >= min_trigger_data_count_) {
    return;
  }
  wait_loop_.Run();
}

void MockDataHost::SourceDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data) {
  source_data_.push_back(std::move(data));
  if (source_data_.size() < min_source_data_count_ ||
      trigger_data_.size() < min_trigger_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

void MockDataHost::TriggerDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    absl::optional<network::TriggerAttestation> attestation) {
  trigger_data_.push_back(std::move(data));
  if (trigger_data_.size() < min_trigger_data_count_ ||
      source_data_.size() < min_source_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

#if BUILDFLAG(IS_ANDROID)

void MockDataHost::OsSourceDataAvailable(const GURL& registration_url) {
  os_sources_.push_back(registration_url);
  if (os_sources_.size() < min_os_sources_count_) {
    return;
  }
  wait_loop_.Quit();
}

void MockDataHost::OsTriggerDataAvailable(const GURL& registration_url) {
  os_triggers_.push_back(registration_url);
  if (os_triggers_.size() < min_os_triggers_count_) {
    return;
  }
  wait_loop_.Quit();
}

void MockDataHost::WaitForOsSources(size_t num_os_sources) {
  min_os_sources_count_ = num_os_sources;
  if (os_sources_.size() >= min_os_sources_count_) {
    return;
  }
  wait_loop_.Run();
}

void MockDataHost::WaitForOsTriggers(size_t num_os_triggers) {
  min_os_triggers_count_ = num_os_triggers;
  if (os_triggers_.size() >= min_os_triggers_count_) {
    return;
  }
  wait_loop_.Run();
}

#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<MockDataHost> GetRegisteredDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  return std::make_unique<MockDataHost>(std::move(data_host));
}

}  // namespace content
