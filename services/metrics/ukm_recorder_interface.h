// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_INTERFACE_H_
#define COMPONENTS_UKM_UKM_INTERFACE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {
class UkmRecorder;
}

namespace metrics {

class UkmRecorderInterface : public ukm::mojom::UkmRecorderInterface {
 public:
  UkmRecorderInterface(ukm::UkmRecorder* ukm_recorder);
  ~UkmRecorderInterface() override;

  static void Create(
      ukm::UkmRecorder* ukm_recorder,
      mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver);

 private:
  // ukm::mojom::UkmRecorderInterface:
  void AddEntry(ukm::mojom::UkmEntryPtr entry) override;
  void UpdateSourceURL(int64_t source_id, const std::string& url) override;

  ukm::UkmRecorder* ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UkmRecorderInterface);
};

}  // namespace metrics

#endif  // COMPONENTS_UKM_UKM_INTERFACE_H_
