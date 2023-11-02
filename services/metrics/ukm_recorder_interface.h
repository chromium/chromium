// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_UKM_RECORDER_INTERFACE_H_
#define SERVICES_METRICS_UKM_RECORDER_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {
class UkmRecorder;
}

namespace metrics {

class UkmRecorderInterface : public ukm::mojom::UkmRecorderInterface {
 public:
  UkmRecorderInterface(ukm::UkmRecorder* ukm_recorder);

  UkmRecorderInterface(const UkmRecorderInterface&) = delete;
  UkmRecorderInterface& operator=(const UkmRecorderInterface&) = delete;

  ~UkmRecorderInterface() override;

  static void Create(
      ukm::UkmRecorder* ukm_recorder,
      mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver);

 private:
  // ukm::mojom::UkmRecorderInterface:
  void AddEntry(ukm::mojom::UkmEntryPtr entry) override;
  void UpdateSourceURL(int64_t source_id, const std::string& url) override;

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;
};

}  // namespace metrics

#endif  // SERVICES_METRICS_UKM_RECORDER_INTERFACE_H_
