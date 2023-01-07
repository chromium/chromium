// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
#define SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {

/**
 * A helper wrapper that lets UKM data be recorded on other processes with the
 * same interface that is used in the browser process.
 *
 * Usage Example:
 *
 *  mojo::PendingRemote<mojom::UkmRecorderInterface> recorder;
 *
 *  // This step depends on how the Metrics service is embedded in the
 *  // application.
 *  BindUkmRecorderSomewhere(recorder.InitWithNewPipeAndPassReceiver());
 *
 *  auto ukm_recorder =
 *      std::make_unique<ukm::MojoUkmRecorder>(std::move(recorder));
 *  ukm::builders::MyEvent(source_id)
 *      .SetMyMetric(metric_value)
 *      .Record(ukm_recorder.get());
 */
class METRICS_EXPORT MojoUkmRecorder : public UkmRecorder {
 public:
  explicit MojoUkmRecorder(
      mojo::PendingRemote<mojom::UkmRecorderInterface> recorder_interface);

  MojoUkmRecorder(const MojoUkmRecorder&) = delete;
  MojoUkmRecorder& operator=(const MojoUkmRecorder&) = delete;

  ~MojoUkmRecorder() override;

  base::WeakPtr<MojoUkmRecorder> GetWeakPtr();

 private:
  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id,
                    const GURL& url,
                    const AppType app_type) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;
  void MarkSourceForDeletion(ukm::SourceId source_id) override;

  mojo::Remote<mojom::UkmRecorderInterface> interface_;

  base::WeakPtrFactory<MojoUkmRecorder> weak_factory_{this};
};

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_MOJO_UKM_RECORDER_H_
