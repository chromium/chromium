// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "content/renderer/accessibility/annotations/ax_image_annotator.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom-forward.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/models/image_model.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebNode;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

// Helper function that searches in the subtree of |obj| to a max
// depth of |max_depth| for an image.
//
// Returns true on success, or false if it finds more than one image,
// or any node with a name, or anything deeper than |max_depth|.
bool SearchForExactlyOneInnerImage(WebAXObject obj,
                                   WebAXObject* inner_image,
                                   int max_depth) {
  DCHECK(inner_image);

  // If it's the first image, set |inner_image|. If we already
  // found an image, fail.
  if (ui::IsImage(obj.Role())) {
    if (!inner_image->IsDetached()) {
      return false;
    }
    *inner_image = obj;
  } else {
    // If we found something else with a name, fail.
    if (!ui::IsPlatformDocument(obj.Role()) && !ui::IsLink(obj.Role())) {
      WebString web_name = obj.GetName();
      if (!base::ContainsOnlyChars(web_name.Utf8(), base::kWhitespaceASCII)) {
        return false;
      }
    }
  }

  // Fail if we recursed to |max_depth| and there's more of a subtree.
  if (max_depth == 0 && obj.ChildCount()) {
    return false;
  }

  // Don't count ignored nodes toward depth.
  int next_depth = obj.AccessibilityIsIgnored() ? max_depth : max_depth - 1;

  // Recurse.
  for (unsigned int i = 0; i < obj.ChildCount(); i++) {
    if (!SearchForExactlyOneInnerImage(obj.ChildAt(i), inner_image,
                                       next_depth)) {
      return false;
    }
  }

  return !inner_image->IsDetached();
}

// Return true if the subtree of |obj|, to a max depth of 3, contains
// exactly one image. Return that image in |inner_image|.
bool FindExactlyOneInnerImageInMaxDepthThree(WebAXObject obj,
                                             WebAXObject* inner_image) {
  DCHECK(inner_image);
  return SearchForExactlyOneInnerImage(obj, inner_image, /* max_depth = */ 3);
}

}  // namespace

AXAnnotatorsManager::AXAnnotatorsManager(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  DCHECK(render_accessibility_);

  render_accessibility_->image_annotation_debugging_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityLabelsDebugging);
}

AXAnnotatorsManager::~AXAnnotatorsManager() {}

void AXAnnotatorsManager::Annotate(const WebDocument& document,
                                   ui::AXTreeUpdate* update) {
  AddImageAnnotations(document, update);
}

void AXAnnotatorsManager::AccessibilityModeChanged(ui::AXMode old_mode,
                                                   ui::AXMode new_mode) {
  // Automatically labels images for accessibility if the accessibility mode for
  // this feature is turned on, otherwise stops automatic labeling and removes
  // any automatic annotations that might have been added before.
  if (!old_mode.has_mode(ui::AXMode::kLabelImages) &&
      new_mode.has_mode(ui::AXMode::kLabelImages)) {
    CreateAXImageAnnotator();
  } else if (old_mode.has_mode(ui::AXMode::kLabelImages) &&
             !new_mode.has_mode(ui::AXMode::kLabelImages)) {
    ax_image_annotator_.reset();
  }
}

void AXAnnotatorsManager::CancelAnnotations() {
  // Remove the image annotator if the page is loading and it was added for
  // the one-shot image annotation (i.e. AXMode for image annotation is not
  // set).
  if (!ax_image_annotator_ ||
      render_accessibility_->GetAccessibilityMode().has_mode(
          ui::AXMode::kLabelImages)) {
    return;
  }
  ax_image_annotator_.reset();
}

void AXAnnotatorsManager::PerformAction(ax::mojom::Action action) {
  // Ensure we aren't already labeling images, in which case this should
  // not change.
  if (action != ax::mojom::Action::kAnnotatePageImages) {
    return;
  }
  if (ax_image_annotator_) {
    return;
  }
  CreateAXImageAnnotator();
  // Rebuild the document tree so that images become annotated.
  DCHECK(render_accessibility_->GetAXContext());
  render_accessibility_->GetAXContext()->MarkDocumentDirty();
}

void AXAnnotatorsManager::CreateAXImageAnnotator() {
  if (ax_image_annotator_ || !render_accessibility_->render_frame()) {
    return;
  }
  mojo::PendingRemote<image_annotation::mojom::Annotator> remote;
  render_accessibility_->render_frame()
      ->GetBrowserInterfaceBroker()
      ->GetInterface(remote.InitWithNewPipeAndPassReceiver());
  ax_image_annotator_ = std::make_unique<AXImageAnnotator>(
      render_accessibility_, std::move(remote));
}

void AXAnnotatorsManager::AddImageAnnotations(const WebDocument& document,
                                              ui::AXTreeUpdate* update) {
  std::vector<ui::AXNodeData*> nodes;
  render_accessibility_->GetAXContext()->GetImagesToAnnotate(*update, nodes);
  if (render_accessibility_->GetAccessibilityMode().has_mode(
          ui::AXMode::kPDFPrinting)) {
    return;
  }
  for (auto* node : nodes) {
    WebAXObject src = WebAXObject::FromWebDocumentByID(document, node->id);

    if (ui::IsImage(node->role)) {
      AddImageAnnotationsForNode(src, node);
    } else {
      DCHECK((ui::IsLink(node->role) || ui::IsPlatformDocument(node->role)) &&
             node->GetNameFrom() != ax::mojom::NameFrom::kAttribute);
      WebAXObject inner_image;
      if (FindExactlyOneInnerImageInMaxDepthThree(src, &inner_image)) {
        AddImageAnnotationsForNode(inner_image, node);
      }
    }
  }
}

// Ignore code that limits based on the protocol (like https, file, etc.)
// to enable tests to run.
bool g_ignore_protocol_checks_for_testing;

// static
void AXAnnotatorsManager::IgnoreProtocolChecksForTesting() {
  g_ignore_protocol_checks_for_testing = true;
}

void AXAnnotatorsManager::AddImageAnnotationsForNode(WebAXObject& src,
                                                     ui::AXNodeData* dst) {
  // Images smaller than this number, in CSS pixels, will never get annotated.
  // Note that OCR works on pretty small images, so this shouldn't be too large.
  static const int kMinImageAnnotationWidth = 16;
  static const int kMinImageAnnotationHeight = 16;

  // Reject ignored objects
  if (src.AccessibilityIsIgnored()) {
    return;
  }

  // Reject images that are explicitly empty, or that have a
  // meaningful name already.
  ax::mojom::NameFrom name_from;
  WebVector<WebAXObject> name_objects;
  WebString web_name = src.GetName(name_from, name_objects);

  // If an image has a nonempty name, compute whether we should add an
  // image annotation or not.
  bool should_annotate_image_with_nonempty_name = false;

  // When visual debugging is enabled, the "title" attribute is set to a
  // string beginning with a "%". If the name comes from that string we
  // can ignore it, and treat the name as empty.
  if (render_accessibility_->image_annotation_debugging_ &&
      base::StartsWith(web_name.Utf8(), "%", base::CompareCase::SENSITIVE)) {
    should_annotate_image_with_nonempty_name = true;
  }

  if (features::IsAugmentExistingImageLabelsEnabled()) {
    // If the name consists of mostly stopwords, we can add an image
    // annotations. See ax_image_stopwords.h for details.
    if (ax_image_annotator_->ImageNameHasMostlyStopwords(web_name.Utf8())) {
      should_annotate_image_with_nonempty_name = true;
    }
  }

  // If the image's name is explicitly empty, or if it has a name (and
  // we're not treating the name as empty), then it's ineligible for
  // an annotation.
  if ((name_from == ax::mojom::NameFrom::kAttributeExplicitlyEmpty ||
       !web_name.IsEmpty()) &&
      !should_annotate_image_with_nonempty_name) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // TODO(abigailbklein): Can this be replaced with GetMainDocument?
  WebDocument document =
      render_accessibility_->render_frame()->GetWebFrame()->GetDocument();

  // If the name of a document (root web area) starts with the filename,
  // it probably means the user opened an image in a new tab.
  // If so, we can treat the name as empty and give it an annotation.
  std::string dst_name =
      dst->GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (ui::IsPlatformDocument(dst->role)) {
    std::string filename = GURL(document.Url()).ExtractFileName();
    if (base::StartsWith(dst_name, filename, base::CompareCase::SENSITIVE)) {
      should_annotate_image_with_nonempty_name = true;
    }
  }

  // |dst| may be a document or link containing an image. Skip annotating
  // it if it already has text other than whitespace.
  if (!base::ContainsOnlyChars(dst_name, base::kWhitespaceASCII) &&
      !should_annotate_image_with_nonempty_name) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // Skip images that are too small to label. This also catches
  // unloaded images where the size is unknown.
  WebAXObject offset_container;
  gfx::RectF bounds;
  gfx::Transform container_transform;
  bool clips_children = false;
  src.GetRelativeBounds(offset_container, bounds, container_transform,
                        &clips_children);
  if (bounds.width() < kMinImageAnnotationWidth ||
      bounds.height() < kMinImageAnnotationHeight) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    return;
  }

  // Skip images in documents which are not http, https, file and data schemes.
  WebString protocol = document.GetSecurityOrigin().Protocol();
  if (!g_ignore_protocol_checks_for_testing && protocol != url::kHttpScheme &&
      protocol != url::kHttpsScheme && protocol != url::kFileScheme &&
      protocol != url::kDataScheme) {
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme);
    return;
  }

  // Skip images that do not have an image_src url (e.g. SVGs), or are in
  // documents that do not have a document_url.
  // TODO(accessibility): Remove this check when support for SVGs is added.
  if (!g_ignore_protocol_checks_for_testing &&
      (src.Url().GetString().Utf8().empty() ||
       document.Url().GetString().Utf8().empty())) {
    return;
  }

  if (!ax_image_annotator_) {
    if (!render_accessibility_->first_unlabeled_image_id_.has_value() ||
        render_accessibility_->first_unlabeled_image_id_.value() ==
            src.AxID()) {
      dst->SetImageAnnotationStatus(
          ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
      render_accessibility_->first_unlabeled_image_id_ = src.AxID();
    } else {
      dst->SetImageAnnotationStatus(
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);
    }
    return;
  }

  if (ax_image_annotator_->HasAnnotationInCache(src)) {
    dst->AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                            ax_image_annotator_->GetImageAnnotation(src));
    dst->SetImageAnnotationStatus(
        ax_image_annotator_->GetImageAnnotationStatus(src));
  } else if (ax_image_annotator_->HasImageInCache(src)) {
    ax_image_annotator_->OnImageUpdated(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  } else if (!ax_image_annotator_->HasImageInCache(src)) {
    ax_image_annotator_->OnImageAdded(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  }
}

void AXAnnotatorsManager::AddImageAnnotationDebuggingAttributes(
    const std::vector<ui::AXTreeUpdate>& updates) {
  if (!render_accessibility_->image_annotation_debugging_) {
    return;
  }

  for (auto& update : updates) {
    for (auto& node : update.nodes) {
      if (!node.HasIntAttribute(
              ax::mojom::IntAttribute::kImageAnnotationStatus)) {
        continue;
      }

      ax::mojom::ImageAnnotationStatus status = node.GetImageAnnotationStatus();
      bool should_set_attributes = false;
      switch (status) {
        case ax::mojom::ImageAnnotationStatus::kNone:
        case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
        case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
        case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
        case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
          break;
        case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
        case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
        case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
        case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
          should_set_attributes = true;
          break;
      }

      if (!should_set_attributes) {
        continue;
      }

      WebDocument document = render_accessibility_->GetMainDocument();
      if (document.IsNull()) {
        continue;
      }
      WebAXObject obj = WebAXObject::FromWebDocumentByID(document, node.id);
      if (obj.IsDetached()) {
        continue;
      }

      if (!has_injected_stylesheet_) {
        document.InsertStyleSheet(
            "[imageannotation=annotationPending] { outline: 3px solid #9ff; } "
            "[imageannotation=annotationSucceeded] { outline: 3px solid #3c3; "
            "} "
            "[imageannotation=annotationEmpty] { outline: 3px solid #ee6; } "
            "[imageannotation=annotationAdult] { outline: 3px solid #f90; } "
            "[imageannotation=annotationProcessFailed] { outline: 3px solid "
            "#c00; } ");
        has_injected_stylesheet_ = true;
      }

      WebNode web_node = obj.GetNode();
      if (web_node.IsNull() || !web_node.IsElementNode()) {
        continue;
      }

      WebElement element = web_node.To<WebElement>();
      std::string status_str = ui::ToString(status);
      if (element.GetAttribute("imageannotation").Utf8() != status_str) {
        element.SetAttribute("imageannotation",
                             WebString::FromUTF8(status_str));
      }

      std::string title = "%" + status_str;
      std::string annotation =
          node.GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation);
      if (!annotation.empty()) {
        title = title + ": " + annotation;
      }
      if (element.GetAttribute("title").Utf8() != title) {
        element.SetAttribute("title", WebString::FromUTF8(title));
      }
    }
  }
}

}  // namespace content
