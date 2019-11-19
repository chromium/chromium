// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/mojo_ukm_recorder.h"

#include <utility>

#include "base/logging.h"
#include "services/metrics/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ukm {

MojoUkmRecorder::MojoUkmRecorder(
    mojo::PendingRemote<mojom::UkmRecorderInterface> interface)
    : interface_(std::move(interface)) {}
MojoUkmRecorder::~MojoUkmRecorder() = default;

// static
std::unique_ptr<MojoUkmRecorder> MojoUkmRecorder::Create(
    service_manager::Connector* connector) {
  mojo::PendingRemote<ukm::mojom::UkmRecorderInterface> interface;
  connector->Connect(metrics::mojom::kMetricsServiceName,
                     interface.InitWithNewPipeAndPassReceiver());
  return std::make_unique<MojoUkmRecorder>(std::move(interface));
}

base::WeakPtr<MojoUkmRecorder> MojoUkmRecorder::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MojoUkmRecorder::UpdateSourceURL(SourceId source_id, const GURL& url) {
  interface_->UpdateSourceURL(source_id, url.spec());
}

void MojoUkmRecorder::UpdateAppURL(SourceId source_id, const GURL& url) {
  NOTREACHED();
}

void MojoUkmRecorder::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  NOTREACHED();
}

void MojoUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  interface_->AddEntry(std::move(entry));
}

void MojoUkmRecorder::MarkSourceForDeletion(ukm::SourceId source_id) {
  NOTREACHED();
}

}  // namespace ukm
