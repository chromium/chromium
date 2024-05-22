// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/mojo_ukm_recorder.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"

namespace ukm {

std::unique_ptr<ukm::MojoUkmRecorder> MojoUkmRecorder::Create(
    mojom::UkmRecorderFactory& factory) {
  return base::WrapUnique(new MojoUkmRecorder(factory));
}

MojoUkmRecorder::MojoUkmRecorder(mojom::UkmRecorderFactory& factory) {
  if (base::FeatureList::IsEnabled(kUkmReduceAddEntryIPC)) {
    factory.CreateUkmRecorder(interface_.BindNewPipeAndPassReceiver(),
                              receiver_.BindNewPipeAndPassRemote());

    receiver_.set_disconnect_handler(base::BindOnce(
        &MojoUkmRecorder::ClientDisconnected, weak_factory_.GetWeakPtr()));
  } else {
    factory.CreateUkmRecorder(interface_.BindNewPipeAndPassReceiver(),
                              mojo::NullRemote());
  }
}

MojoUkmRecorder::~MojoUkmRecorder() = default;

base::WeakPtr<MojoUkmRecorder> MojoUkmRecorder::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MojoUkmRecorder::UpdateSourceURL(SourceId source_id, const GURL& url) {
  interface_->UpdateSourceURL(source_id, url.spec());
}

void MojoUkmRecorder::UpdateAppURL(SourceId source_id,
                                   const GURL& url,
                                   const AppType app_type) {
  NOTREACHED_IN_MIGRATION();
}

void MojoUkmRecorder::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  NOTREACHED_IN_MIGRATION();
}

void MojoUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  if (ShouldDropEntry(*entry)) {
    return;
  }
  interface_->AddEntry(std::move(entry));
}

void MojoUkmRecorder::RecordWebDXFeatures(SourceId source_id,
                                          const std::set<int32_t>& features,
                                          const size_t max_feature_value) {
  NOTREACHED_IN_MIGRATION();
}

void MojoUkmRecorder::MarkSourceForDeletion(ukm::SourceId source_id) {
  NOTREACHED_IN_MIGRATION();
}

void MojoUkmRecorder::SetParameters(
    ukm::mojom::UkmRecorderParametersPtr params) {
  params_ = std::move(params);
  // params_->event_hash_bypass_list needs to be sorted for ShouldDropEntry to
  // work correctly.
  std::sort(params_->event_hash_bypass_list.begin(),
            params_->event_hash_bypass_list.end());
}

void MojoUkmRecorder::ClientDisconnected() {
  // Make sure we do not drop any entry as we are no longer getting param
  // updates from the remote side.
  params_.reset();
}

bool MojoUkmRecorder::ShouldDropEntry(const mojom::UkmEntry& entry) {
  // If params_ is not set, we always send UkmEntry to the browser.
  if (!params_) {
    return false;
  }

  if (std::binary_search(params_->event_hash_bypass_list.begin(),
                         params_->event_hash_bypass_list.end(),
                         entry.event_hash)) {
    return false;
  }

  if (!params_->is_enabled) {
    RecordDroppedEntry(
        entry.event_hash,
        DroppedDataReason::RECORDING_DISABLED_REDUCE_ADDENTRYIPC);
    return true;
  }
  return false;
}

}  // namespace ukm
