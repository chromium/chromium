// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_main_node_annotator.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;

namespace {

const char kHistogramsName[] =
    "Accessibility.MainNodeAnnotations.AnnotationResult";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MainNodeAnnotationResult)
enum class MainNodeAnnotationResult {
  kSuccess = 0,
  kInvalid = 1,
  kDuplicate = 2,
  kMaxValue = kDuplicate,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:MainNodeAnnotationResult)

}  // namespace

AXMainNodeAnnotator::AXMainNodeAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  CHECK(render_accessibility_);
}

AXMainNodeAnnotator::~AXMainNodeAnnotator() = default;

void AXMainNodeAnnotator::EnableAnnotations() {
  annotator_enabled_ = true;
}

void AXMainNodeAnnotator::CancelAnnotations() {
  if (render_accessibility_->GetAccessibilityMode().has_mode(
          GetAXModeToEnableAnnotations())) {
    return;
  }
  annotator_enabled_ = false;
  annotator_remote_.reset();
}

uint32_t AXMainNodeAnnotator::GetAXModeToEnableAnnotations() {
  return ui::AXMode::kAnnotateMainNode;
}

bool AXMainNodeAnnotator::HasAXActionToEnableAnnotations() {
  return false;
}

ax::mojom::Action AXMainNodeAnnotator::GetAXActionToEnableAnnotations() {
  NOTREACHED();
}

void AXMainNodeAnnotator::Annotate(const WebDocument& document,
                                   ui::AXTreeUpdate* update,
                                   bool load_complete) {
  // Annotate is called every time RenderAccessibilityImpl sends a serialized
  // tree, in the form of an AXTreeUpdate, to the browser. Before sending to
  // the browser, we annotate the main node of the AXTreeUpdate here.
  if (main_node_id_ != ui::kInvalidAXNodeID) {
    // TODO: Replace with binary search as nodes should be in order by id.
    for (ui::AXNodeData& node : update->nodes) {
      if (node.id != main_node_id_) {
        continue;
      }
      // TODO: Replace this with a role specifically for annotations.
      node.role = ax::mojom::Role::kMain;
      return;
    }
    // If the main node was set, even if if is not found in the tree anymore,
    // do nothing.
    // TODO: Consider whether we should call Screen2x again.
    return;
  }

  // Do nothing if this is not a load complete event.
  if (!load_complete) {
    return;
  }

  // Check whether the author has already labeled a main node in this tree.
  ComputeAuthorStatus(update);
  if (author_status_ == AXMainNodeAnnotatorAuthorStatus::kAuthorProvidedMain) {
    return;
  }
  CHECK_EQ(author_status_,
           AXMainNodeAnnotatorAuthorStatus::kAuthorDidNotProvideMain);

  // TODO(crbug.com/327248295): Promote the feature if the user has not enabled
  // it and is on a page without a main node annotation.
  if (!annotator_enabled_) {
    return;
  }

  if (!annotator_remote_.is_bound() || !annotator_remote_.is_connected()) {
    if (!render_accessibility_->render_frame()) {
      return;
    }

    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
        annotator;
    render_accessibility_->render_frame()
        ->GetBrowserInterfaceBroker()
        .GetInterface(annotator.InitWithNewPipeAndPassReceiver());
    annotator_remote_.Bind(std::move(annotator));
    annotator_remote_.reset_on_disconnect();
  }

  // Identify the main node using Screen2x.
  annotator_remote_->ExtractMainNode(
      *update, base::BindOnce(&AXMainNodeAnnotator::ProcessScreen2xResult,
                              weak_ptr_factory_.GetWeakPtr(), document));
}

void AXMainNodeAnnotator::ProcessScreen2xResult(const WebDocument& document,
                                                ui::AXNodeID main_node_id) {
  // If Screen2x returned an invalid main node id, do nothing.
  if (main_node_id == ui::kInvalidAXNodeID) {
    base::UmaHistogramEnumeration(kHistogramsName,
                                  MainNodeAnnotationResult::kInvalid);
    return;
  }
  // If the main node id was already set, do nothing.
  if (main_node_id_ != ui::kInvalidAXNodeID) {
    base::UmaHistogramEnumeration(kHistogramsName,
                                  MainNodeAnnotationResult::kDuplicate);
    return;
  }
  WebAXObject object = WebAXObject::FromWebDocumentByID(document, main_node_id);
  // If the tree has changed, do nothing.
  if (!object.IsIncludedInTree()) {
    base::UmaHistogramEnumeration(kHistogramsName,
                                  MainNodeAnnotationResult::kInvalid);
    return;
  }
  main_node_id_ = main_node_id;
  render_accessibility_->MarkWebAXObjectDirty(object);
  base::UmaHistogramEnumeration(kHistogramsName,
                                MainNodeAnnotationResult::kSuccess);
}

void AXMainNodeAnnotator::ComputeAuthorStatus(ui::AXTreeUpdate* update) {
  if (author_status_ != AXMainNodeAnnotatorAuthorStatus::kUnconfirmed) {
    return;
  }
  for (ui::AXNodeData node : update->nodes) {
    if (node.role == ax::mojom::Role::kMain) {
      author_status_ = AXMainNodeAnnotatorAuthorStatus::kAuthorProvidedMain;
      return;
    }
  }
  author_status_ = AXMainNodeAnnotatorAuthorStatus::kAuthorDidNotProvideMain;
}

void AXMainNodeAnnotator::BindAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
        annotator) {
  annotator_remote_.Bind(std::move(annotator));
  annotator_enabled_ = true;
}

}  // namespace content
