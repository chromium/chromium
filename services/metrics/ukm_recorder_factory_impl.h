// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_UKM_RECORDER_FACTORY_IMPL_H_
#define SERVICES_METRICS_UKM_RECORDER_FACTORY_IMPL_H_

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace metrics {

// Implements the public mojo UkmRecorderFactory interface by wrapping the
// UkmRecorder instance.
class UkmRecorderFactoryImpl : public ukm::mojom::UkmRecorderFactory {
 public:
  explicit UkmRecorderFactoryImpl(ukm::UkmRecorder* ukm_recorder);

  UkmRecorderFactoryImpl(const UkmRecorderFactoryImpl&) = delete;
  UkmRecorderFactoryImpl& operator=(const UkmRecorderFactoryImpl&) = delete;
  ~UkmRecorderFactoryImpl() override;

  // Binds |ukm_recorder| to the lifetime of UkmRecorderFactory.
  static void Create(
      ukm::UkmRecorder* ukm_recorder,
      mojo::PendingReceiver<ukm::mojom::UkmRecorderFactory> receiver);

 private:
  // ukm::mojom::UkmRecorderFactory:
  void CreateUkmRecorder(
      mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver,
      mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface> client_remote)
      override;

  raw_ptr<ukm::UkmRecorder> ukm_recorder_;
};

}  // namespace metrics

#endif  // SERVICES_METRICS_UKM_RECORDER_FACTORY_IMPL_H_
