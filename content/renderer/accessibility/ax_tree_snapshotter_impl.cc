// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"

#include <set>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/accessibility/ax_error_types.h"
#include "ui/accessibility/ax_tree_update.h"

// Simple macro to make recording histograms easier to read in the code below.
#define RECORD_ERROR(histogram)                                       \
  base::UmaHistogramEnumeration(kAXTreeSnapshotterErrorHistogramName, \
                                AXTreeSnapshotErrorReason::k##histogram)

using ErrorSet = std::set<ui::AXSerializationErrorFlag>;

namespace content {

constexpr char kAXTreeSnapshotterErrorHistogramName[] =
    "Accessibility.AXTreeSnapshotter.Snapshot.Error";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AXTreeSnapshotErrorReason)
enum class AXTreeSnapshotErrorReason {
  kGenericSerializationError = 0,
  kNoWebFrame = 1,
  kNoActiveDocument = 2,
  kNoExistingAXObjectCache = 3,
  kSerializeMaxNodesReached = 4,
  kSerializeTimeoutReached = 5,
  kSerializeMaxNodesAndTimeoutReached = 6,
  kMaxValue = kSerializeMaxNodesAndTimeoutReached,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AXTreeSnapshotErrorReason)

AXTreeSnapshotterImpl::AXTreeSnapshotterImpl(RenderFrameImpl* render_frame,
                                             ui::AXMode ax_mode)
    : content::RenderFrameObserver(render_frame), ax_mode_(ax_mode) {
  // Do not generate inline textboxes, which are expensive to create and just
  // present extra noise to snapshot consumers.
  ax_mode.set_mode(ui::AXMode::kInlineTextBoxes, false);

  CHECK(render_frame->GetWebFrame());
  document_ = render_frame->GetWebFrame()->GetDocument();
}

AXTreeSnapshotterImpl::~AXTreeSnapshotterImpl() = default;

void AXTreeSnapshotterImpl::Snapshot(size_t max_nodes,
                                     base::TimeDelta timeout,
                                     ui::AXTreeUpdate* response) {
  base::UmaHistogramBoolean("Accessibility.AXTreeSnapshotter.Snapshot.Request",
                            true);

  if (!render_frame() || !render_frame()->GetWebFrame()) {
    RECORD_ERROR(NoWebFrame);
    return;
  }

  if (!document_.IsActive()) {
    RECORD_ERROR(NoActiveDocument);
    return;
  }

  SerializeTreeWithLimits(max_nodes, timeout, response);
}

void AXTreeSnapshotterImpl::OnDestruct() {
  // Must implement OnDestruct(), but no need to do anything.
}

void AXTreeSnapshotterImpl::SerializeTreeWithLimits(
    size_t max_nodes,
    base::TimeDelta timeout,
    ui::AXTreeUpdate* response) {
  ErrorSet out_error;
  document_.SnapshotAccessibilityTree(max_nodes, timeout, response, ax_mode_,
                                      &out_error);

  ErrorSet::iterator max_nodes_iter =
      out_error.find(ui::AXSerializationErrorFlag::kMaxNodesReached);
  ErrorSet::iterator timeout_iter =
      out_error.find(ui::AXSerializationErrorFlag::kTimeoutReached);

  if (max_nodes_iter != out_error.end() && timeout_iter != out_error.end()) {
    RECORD_ERROR(SerializeMaxNodesAndTimeoutReached);
  } else if (max_nodes_iter != out_error.end()) {
    RECORD_ERROR(SerializeMaxNodesReached);
  } else if (timeout_iter != out_error.end()) {
    RECORD_ERROR(SerializeTimeoutReached);
  }
}

}  // namespace content
