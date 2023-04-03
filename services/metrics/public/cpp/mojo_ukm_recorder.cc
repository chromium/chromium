// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/mojo_ukm_recorder.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"

namespace ukm {

MojoUkmRecorder::MojoUkmRecorder(
    mojo::PendingRemote<mojom::UkmRecorderInterface> interface)
    : interface_(std::move(interface)), receiver_(this) {
  if (!interface_.is_bound()) {
    // Interface might not be bound in tests.
    return;
  }
  if (base::FeatureList::IsEnabled(kUkmReduceAddEntryIPC)) {
    interface_->BindClient(receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(base::BindOnce(
        &MojoUkmRecorder::ClientDisconnected, weak_factory_.GetWeakPtr()));
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
  NOTREACHED();
}

void MojoUkmRecorder::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  NOTREACHED();
}

void MojoUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  if (ShouldDropEntry(*entry)) {
    return;
  }
  interface_->AddEntry(std::move(entry));
}

void MojoUkmRecorder::MarkSourceForDeletion(ukm::SourceId source_id) {
  NOTREACHED();
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
    RecordDroppedEntry(entry.event_hash, DroppedDataReason::RECORDING_DISABLED);
    return true;
  }
  return false;
}

}  // namespace ukm
