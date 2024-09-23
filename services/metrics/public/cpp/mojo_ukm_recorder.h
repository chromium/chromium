// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder_impl_utils.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {

/**
 * A helper wrapper that lets UKM data be recorded on other processes with the
 * same interface that is used in the browser process. When feature
 * |kUkmReduceAddEntryIPC| is enabled, MojoUkmRecorder is able to decide whether
 * to send the UKM data to browser process or not.
 *
 * Usage Example:
 *
 *  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
 *
 *  // This step depends on how the Metrics service is embedded in the
 *  // application.
 *  BindUkmRecorderFactorySomewhere(factory.BindNewPipeAndPassReceiver());
 *
 *  auto ukm_recorder = ukm::MojoUkmRecorder::Create(*factory);
 *  ukm::builders::MyEvent(source_id)
 *      .SetMyMetric(metric_value)
 *      .Record(ukm_recorder.get());
 */
class METRICS_EXPORT MojoUkmRecorder
    : public UkmRecorder,
      public ukm::mojom::UkmRecorderClientInterface {
 public:
  static std::unique_ptr<MojoUkmRecorder> Create(
      mojom::UkmRecorderFactory& factory);

  MojoUkmRecorder(const MojoUkmRecorder&) = delete;
  MojoUkmRecorder& operator=(const MojoUkmRecorder&) = delete;

  ~MojoUkmRecorder() override;

  base::WeakPtr<MojoUkmRecorder> GetWeakPtr();

 protected:
  explicit MojoUkmRecorder(mojom::UkmRecorderFactory& factory);

 private:
  bool ShouldDropEntry(const mojom::UkmEntry& entry);
  void ClientDisconnected();

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id,
                    const GURL& url,
                    const AppType app_type) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;
  void RecordWebDXFeatures(SourceId source_id,
                           const std::set<int32_t>& features,
                           const size_t max_feature_value) override;
  void MarkSourceForDeletion(ukm::SourceId source_id) override;

  // UkmRecorderClientInterface:
  void SetParameters(ukm::mojom::UkmRecorderParametersPtr params) override;

  mojo::Remote<mojom::UkmRecorderInterface> interface_;
  mojo::Receiver<ukm::mojom::UkmRecorderClientInterface> receiver_{this};
  // params_->event_hash_bypass_list needs to be sorted for ShouldDropEntry to
  // work correctly, since a binary search is done for finding event hash of
  // UkmEntry in event_hash_bypass_list.
  ukm::mojom::UkmRecorderParametersPtr params_;

  base::WeakPtrFactory<MojoUkmRecorder> weak_factory_{this};
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
