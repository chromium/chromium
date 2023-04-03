// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/ukm_recorder_interface.h"

#include "base/atomic_sequence_num.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder_client_interface_registry.h"

#include "url/gurl.h"

namespace metrics {

UkmRecorderInterface::UkmRecorderInterface(ukm::UkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}

UkmRecorderInterface::~UkmRecorderInterface() = default;

// static
void UkmRecorderInterface::Create(
    ukm::UkmRecorder* ukm_recorder,
    mojo::PendingReceiver<ukm::mojom::UkmRecorderInterface> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<UkmRecorderInterface>(ukm_recorder),
      std::move(receiver));
}

void UkmRecorderInterface::AddEntry(ukm::mojom::UkmEntryPtr ukm_entry) {
  ukm_recorder_->AddEntry(std::move(ukm_entry));
}

void UkmRecorderInterface::UpdateSourceURL(int64_t source_id,
                                           const std::string& url) {
  ukm_recorder_->UpdateSourceURL(source_id, GURL(url));
}

}  // namespace metrics
