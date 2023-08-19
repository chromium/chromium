// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_DATA_HOST_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_DATA_HOST_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace attribution_reporting {
struct OsRegistrationItem;
struct SourceRegistration;
struct TriggerRegistration;
}  // namespace attribution_reporting

namespace mojo {

template <typename Interface>
class PendingReceiver;

}  // namespace mojo

namespace content {

class MockDataHost : public blink::mojom::AttributionDataHost {
 public:
  explicit MockDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost>);
  ~MockDataHost() override;

  void WaitForSourceData(size_t num_source_data);
  void WaitForTriggerData(size_t num_trigger_data);
  void WaitForSourceAndTriggerData(size_t num_source_data,
                                   size_t num_trigger_data);

  const std::vector<attribution_reporting::SourceRegistration>& source_data()
      const {
    return source_data_;
  }

  const std::vector<attribution_reporting::TriggerRegistration>& trigger_data()
      const {
    return trigger_data_;
  }

  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_sources() const {
    return os_sources_;
  }
  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_triggers() const {
    return os_triggers_;
  }
  void WaitForOsSources(size_t);
  void WaitForOsTriggers(size_t);

  mojo::Receiver<blink::mojom::AttributionDataHost>& receiver() {
    return receiver_;
  }

 private:
  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration) override;
  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration,
      std::vector<network::TriggerVerification>) override;
  void OsSourceDataAvailable(
      std::vector<attribution_reporting::OsRegistrationItem>) override;
  void OsTriggerDataAvailable(
      std::vector<attribution_reporting::OsRegistrationItem>) override;

  size_t min_source_data_count_ = 0;
  std::vector<attribution_reporting::SourceRegistration> source_data_;

  size_t min_trigger_data_count_ = 0;
  std::vector<attribution_reporting::TriggerRegistration> trigger_data_;

  size_t min_os_sources_count_ = 0;
  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_sources_;

  size_t min_os_triggers_count_ = 0;
  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_triggers_;

  base::RunLoop wait_loop_;
  mojo::Receiver<blink::mojom::AttributionDataHost> receiver_{this};
};

std::unique_ptr<MockDataHost> GetRegisteredDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost>);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_DATA_HOST_H_
