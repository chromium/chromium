// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_error_types.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;

// Simple macro to make recording histograms easier to read in the code below.
#define RECORD_ERROR(histogram)                                       \
  base::UmaHistogramEnumeration(kAXTreeSnapshotterErrorHistogramName, \
                                AXTreeSnapshotErrorReason::k##histogram)

using ErrorSet = std::set<ui::AXSerializationErrorFlag>;
namespace content {

namespace {

constexpr int kMaxNodesHistogramLimit = 20000;
constexpr int kTimeoutInMillisecondsHistogramLimit = 3000;

}  // namespace

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
    : render_frame_(render_frame) {
  // Do not generate inline textboxes, which are expensive to create and just
  // present extra noise to snapshot consumers.
  ax_mode.set_mode(ui::AXMode::kInlineTextBoxes, false);

  DCHECK(render_frame->GetWebFrame());
  blink::WebDocument document_ = render_frame->GetWebFrame()->GetDocument();
  context_ = std::make_unique<WebAXContext>(document_, ax_mode);
}

AXTreeSnapshotterImpl::~AXTreeSnapshotterImpl() = default;

void AXTreeSnapshotterImpl::Snapshot(size_t max_node_count,
                                     base::TimeDelta timeout,
                                     ui::AXTreeUpdate* response) {
  base::UmaHistogramBoolean("Accessibility.AXTreeSnapshotter.Snapshot.Request",
                            true);

  if (!render_frame_->GetWebFrame()) {
    RECORD_ERROR(NoWebFrame);
    return;
  }

  if (!context_->HasActiveDocument()) {
    RECORD_ERROR(NoActiveDocument);
    return;
  }

  if (!context_->HasAXObjectCache()) {
    RECORD_ERROR(NoExistingAXObjectCache);
    // TODO(chrishtr): not clear why this can happen.
    NOTREACHED();
  }

#if !BUILDFLAG(IS_ANDROID)
  if (SerializeTreeWithLimits(max_node_count, timeout, response)) {
    return;
  }
#else
  // On Android, experiment with serialization without any limits.
  if (features::IsAccessibilitySnapshotStressTestsEnabled()) {
    if (SerializeTree(response)) {
      return;
    }
  } else {
    if (SerializeTreeWithLimits(max_node_count, timeout, response)) {
      return;
    }
  }
#endif

  RECORD_ERROR(GenericSerializationError);
  // It failed again. Clear the response object because it might have errors.
  *response = ui::AXTreeUpdate();
  LOG(WARNING) << "Unable to serialize accessibility tree.";

  // As a confidence check, node_id_to_clear and event_from should be
  // uninitialized if this is a full tree snapshot. They'd only be set to
  // something if this was indeed a partial update to the tree (which we don't
  // want).
  DCHECK_EQ(0, response->node_id_to_clear);
  DCHECK_EQ(ax::mojom::EventFrom::kNone, response->event_from);
  DCHECK_EQ(ax::mojom::Action::kNone, response->event_from_action);
}

bool AXTreeSnapshotterImpl::SerializeTreeWithLimits(
    size_t max_node_count,
    base::TimeDelta timeout,
    ui::AXTreeUpdate* response) {
  ErrorSet out_error;
  if (!context_->SerializeEntireTree(max_node_count, timeout, response,
                                     &out_error)) {
    return false;
  }

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

  return true;
}

bool AXTreeSnapshotterImpl::SerializeTree(ui::AXTreeUpdate* response) {
#if !BUILDFLAG(IS_ANDROID)
  int max_nodes_count = 0;
#else
  // Experiment with different max values for end-to-end timing. An arbitrarily
  // large value will simulate there being no max nodes count.
  int max_nodes_count = base::GetFieldTrialParamByFeatureAsInt(
      features::kAccessibilitySnapshotStressTests,
      "AccessibilitySnapshotStressTestsMaxNodes", 100000);
#endif

  base::ElapsedTimer timer = base::ElapsedTimer();
  timer.start_time();
  if (!context_->SerializeEntireTree(max_nodes_count, {}, response)) {
    return false;
  }

  base::TimeDelta snapshotDuration = timer.Elapsed();
  base::LinearHistogram::FactoryGet(
      "Accessibility.AXTreeSnapshotter.Snapshot.NoRestrictions.Nodes", 0,
      kMaxNodesHistogramLimit, 100,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(response->nodes.size());

  base::LinearHistogram::FactoryGet(
      "Accessibility.AXTreeSnapshotter.Snapshot.NoRestrictions.Time", 0,
      kTimeoutInMillisecondsHistogramLimit, 100,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(snapshotDuration.InMilliseconds());

  return true;
}

}  // namespace content
