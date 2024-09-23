// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_image_annotator.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/accessibility/annotations/ax_image_annotator.h"
#include "content/renderer/accessibility/annotations/ax_image_stopwords.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom-forward.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"
#include "url/gurl.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebNode;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

int GetMessageIdForIconEnum(const std::string& icon_type) {
  static constexpr auto kIconTypeToMessageIdMap =
      base::MakeFixedFlatMap<std::string_view, int>({
          {"ICON_PLUS", IDS_AX_IMAGE_ANNOTATION_ICON_PLUS},
          {"ICON_ARROW_BACKWARD", IDS_AX_IMAGE_ANNOTATION_ICON_ARROW_BACKWARD},
          {"ICON_ARROW_FORWARD", IDS_AX_IMAGE_ANNOTATION_ICON_ARROW_FORWARD},
          {"ICON_CALL", IDS_AX_IMAGE_ANNOTATION_ICON_CALL},
          {"ICON_CHAT", IDS_AX_IMAGE_ANNOTATION_ICON_CHAT},
          {"ICON_CHECK", IDS_AX_IMAGE_ANNOTATION_ICON_CHECK},
          {"ICON_X", IDS_AX_IMAGE_ANNOTATION_ICON_X},
          {"ICON_DELETE", IDS_AX_IMAGE_ANNOTATION_ICON_DELETE},
          {"ICON_EDIT", IDS_AX_IMAGE_ANNOTATION_ICON_EDIT},
          {"ICON_EMOJI", IDS_AX_IMAGE_ANNOTATION_ICON_EMOJI},
          {"ICON_END_CALL", IDS_AX_IMAGE_ANNOTATION_ICON_END_CALL},
          {"ICON_V_DOWNWARD", IDS_AX_IMAGE_ANNOTATION_ICON_V_DOWNWARD},
          {"ICON_HEART", IDS_AX_IMAGE_ANNOTATION_ICON_HEART},
          {"ICON_HOME", IDS_AX_IMAGE_ANNOTATION_ICON_HOME},
          {"ICON_INFO", IDS_AX_IMAGE_ANNOTATION_ICON_INFO},
          {"ICON_LAUNCH_APPS", IDS_AX_IMAGE_ANNOTATION_ICON_LAUNCH_APPS},
          {"ICON_THUMBS_UP", IDS_AX_IMAGE_ANNOTATION_ICON_THUMBS_UP},
          {"ICON_THREE_BARS", IDS_AX_IMAGE_ANNOTATION_ICON_THREE_BARS},
          {"ICON_THREE_DOTS", IDS_AX_IMAGE_ANNOTATION_ICON_THREE_DOTS},
          {"ICON_NOTIFICATIONS", IDS_AX_IMAGE_ANNOTATION_ICON_NOTIFICATIONS},
          {"ICON_PAUSE", IDS_AX_IMAGE_ANNOTATION_ICON_PAUSE},
          {"ICON_PLAY", IDS_AX_IMAGE_ANNOTATION_ICON_PLAY},
          {"ICON_REFRESH", IDS_AX_IMAGE_ANNOTATION_ICON_REFRESH},
          {"ICON_MAGNIFYING_GLASS",
           IDS_AX_IMAGE_ANNOTATION_ICON_MAGNIFYING_GLASS},
          {"ICON_SEND", IDS_AX_IMAGE_ANNOTATION_ICON_SEND},
          {"ICON_SETTINGS", IDS_AX_IMAGE_ANNOTATION_ICON_SETTINGS},
          {"ICON_SHARE", IDS_AX_IMAGE_ANNOTATION_ICON_SHARE},
          {"ICON_STAR", IDS_AX_IMAGE_ANNOTATION_ICON_STAR},
          {"ICON_TAKE_PHOTO", IDS_AX_IMAGE_ANNOTATION_ICON_TAKE_PHOTO},
          {"ICON_TIME", IDS_AX_IMAGE_ANNOTATION_ICON_TIME},
          {"ICON_VIDEOCAM", IDS_AX_IMAGE_ANNOTATION_ICON_VIDEOCAM},
          {"ICON_EXPAND", IDS_AX_IMAGE_ANNOTATION_ICON_EXPAND},
          {"ICON_CONTRACT", IDS_AX_IMAGE_ANNOTATION_ICON_CONTRACT},
          {"ICON_GOOGLE", IDS_AX_IMAGE_ANNOTATION_ICON_GOOGLE},
          {"ICON_TWITTER", IDS_AX_IMAGE_ANNOTATION_ICON_TWITTER},
          {"ICON_FACEBOOK", IDS_AX_IMAGE_ANNOTATION_ICON_FACEBOOK},
          {"ICON_ASSISTANT", IDS_AX_IMAGE_ANNOTATION_ICON_ASSISTANT},
          {"ICON_WEATHER", IDS_AX_IMAGE_ANNOTATION_ICON_WEATHER},
          {"ICON_SHOPPING_CART", IDS_AX_IMAGE_ANNOTATION_ICON_SHOPPING_CART},
          {"ICON_UPLOAD", IDS_AX_IMAGE_ANNOTATION_ICON_UPLOAD},
          {"ICON_QUESTION", IDS_AX_IMAGE_ANNOTATION_ICON_QUESTION},
          {"ICON_MIC", IDS_AX_IMAGE_ANNOTATION_ICON_MIC},
          {"ICON_MIC_MUTE", IDS_AX_IMAGE_ANNOTATION_ICON_MIC_MUTE},
          {"ICON_GALLERY", IDS_AX_IMAGE_ANNOTATION_ICON_GALLERY},
          {"ICON_COMPASS", IDS_AX_IMAGE_ANNOTATION_ICON_COMPASS},
          {"ICON_PEOPLE", IDS_AX_IMAGE_ANNOTATION_ICON_PEOPLE},
          {"ICON_ARROW_UPWARD", IDS_AX_IMAGE_ANNOTATION_ICON_ARROW_UPWARD},
          {"ICON_ENVELOPE", IDS_AX_IMAGE_ANNOTATION_ICON_ENVELOPE},
          {"ICON_EMOJI_FACE", IDS_AX_IMAGE_ANNOTATION_ICON_EMOJI_FACE},
          {"ICON_PAPERCLIP", IDS_AX_IMAGE_ANNOTATION_ICON_PAPERCLIP},
          {"ICON_CAST", IDS_AX_IMAGE_ANNOTATION_ICON_CAST},
          {"ICON_VOLUME_UP", IDS_AX_IMAGE_ANNOTATION_ICON_VOLUME_UP},
          {"ICON_VOLUME_DOWN", IDS_AX_IMAGE_ANNOTATION_ICON_VOLUME_DOWN},
          {"ICON_VOLUME_STATE", IDS_AX_IMAGE_ANNOTATION_ICON_VOLUME_STATE},
          {"ICON_VOLUME_MUTE", IDS_AX_IMAGE_ANNOTATION_ICON_VOLUME_MUTE},
          {"ICON_STOP", IDS_AX_IMAGE_ANNOTATION_ICON_STOP},
          {"ICON_SHOPPING_BAG", IDS_AX_IMAGE_ANNOTATION_ICON_SHOPPING_BAG},
          {"ICON_LIST", IDS_AX_IMAGE_ANNOTATION_ICON_LIST},
          {"ICON_LOCATION", IDS_AX_IMAGE_ANNOTATION_ICON_LOCATION},
          {"ICON_CALENDAR", IDS_AX_IMAGE_ANNOTATION_ICON_CALENDAR},
          {"ICON_THUMBS_DOWN", IDS_AX_IMAGE_ANNOTATION_ICON_THUMBS_DOWN},
          {"ICON_HEADSET", IDS_AX_IMAGE_ANNOTATION_ICON_HEADSET},
          {"ICON_REDO", IDS_AX_IMAGE_ANNOTATION_ICON_REDO},
          {"ICON_UNDO", IDS_AX_IMAGE_ANNOTATION_ICON_UNDO},
          {"ICON_DOWNLOAD", IDS_AX_IMAGE_ANNOTATION_ICON_DOWNLOAD},
          {"ICON_ARROW_DOWNWARD", IDS_AX_IMAGE_ANNOTATION_ICON_ARROW_DOWNWARD},
          {"ICON_V_UPWARD", IDS_AX_IMAGE_ANNOTATION_ICON_V_UPWARD},
          {"ICON_V_FORWARD", IDS_AX_IMAGE_ANNOTATION_ICON_V_FORWARD},
          {"ICON_V_BACKWARD", IDS_AX_IMAGE_ANNOTATION_ICON_V_BACKWARD},
          {"ICON_HISTORY", IDS_AX_IMAGE_ANNOTATION_ICON_HISTORY},
          {"ICON_PERSON", IDS_AX_IMAGE_ANNOTATION_ICON_PERSON},
          {"ICON_HAPPY_FACE", IDS_AX_IMAGE_ANNOTATION_ICON_HAPPY_FACE},
          {"ICON_SAD_FACE", IDS_AX_IMAGE_ANNOTATION_ICON_SAD_FACE},
          {"ICON_MOON", IDS_AX_IMAGE_ANNOTATION_ICON_MOON},
          {"ICON_CLOUD", IDS_AX_IMAGE_ANNOTATION_ICON_CLOUD},
          {"ICON_SUN", IDS_AX_IMAGE_ANNOTATION_ICON_SUN},
      });

  auto iter = kIconTypeToMessageIdMap.find(icon_type);
  if (iter == kIconTypeToMessageIdMap.end())
    return 0;

  return iter->second;
}

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
  int next_depth = obj.IsIgnored() ? max_depth : max_depth - 1;

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

AXImageAnnotator::AXImageAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  DCHECK(render_accessibility_);
  render_accessibility_->image_annotation_debugging_ =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityLabelsDebugging);
}

AXImageAnnotator::~AXImageAnnotator() {}

void AXImageAnnotator::EnableAnnotations() {
  if (annotator_remote_.is_bound() || !render_accessibility_->render_frame()) {
    return;
  }
  mojo::PendingRemote<image_annotation::mojom::Annotator> annotator;
  render_accessibility_->render_frame()
      ->GetBrowserInterfaceBroker()
      .GetInterface(annotator.InitWithNewPipeAndPassReceiver());
  annotator_remote_.Bind(std::move(annotator));
}

void AXImageAnnotator::BindAnnotatorForTesting(
    mojo::PendingRemote<image_annotation::mojom::Annotator> annotator) {
  annotator_remote_.Bind(std::move(annotator));
}

void AXImageAnnotator::CancelAnnotations() {
  has_injected_stylesheet_ = false;
  // Remove the image annotator if the page is loading and it was added for
  // the one-shot image annotation (i.e. AXMode for image annotation is not
  // set).
  if (!annotator_remote_.is_bound() ||
      render_accessibility_->GetAccessibilityMode().has_mode(
          GetAXModeToEnableAnnotations())) {
    return;
  }
  annotator_remote_.reset();
}

uint32_t AXImageAnnotator::GetAXModeToEnableAnnotations() {
  return ui::AXMode::kLabelImages;
}

bool AXImageAnnotator::HasAXActionToEnableAnnotations() {
  return true;
}

ax::mojom::Action AXImageAnnotator::GetAXActionToEnableAnnotations() {
  return ax::mojom::Action::kAnnotatePageImages;
}

void AXImageAnnotator::Annotate(const WebDocument& document,
                                ui::AXTreeUpdate* update,
                                bool load_complete) {
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
  if (load_complete) {
    has_injected_stylesheet_ = false;
  }
}

// Ignore code that limits based on the protocol (like https, file, etc.)
// to enable tests to run.
bool g_ignore_protocol_checks_for_testing;

// static
void AXImageAnnotator::IgnoreProtocolChecksForTesting() {
  g_ignore_protocol_checks_for_testing = true;
}

void AXImageAnnotator::AddImageAnnotationsForNode(WebAXObject& src,
                                                  ui::AXNodeData* dst) {
  // Images smaller than this number, in CSS pixels, will never get annotated.
  // Note that OCR works on pretty small images, so this shouldn't be too large.
  static const int kMinImageAnnotationWidth = 16;
  static const int kMinImageAnnotationHeight = 16;

  // Reject ignored objects
  if (src.IsIgnored()) {
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

  // If the name consists of mostly stopwords, we can add an image
  // annotations. See ax_image_stopwords.h for details.
  if (ImageNameHasMostlyStopwords(web_name.Utf8())) {
    should_annotate_image_with_nonempty_name = true;
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

  if (!annotator_remote_.is_bound()) {
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

  if (HasAnnotationInCache(src)) {
    dst->AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                            GetImageAnnotation(src));
    dst->SetImageAnnotationStatus(GetImageAnnotationStatus(src));
  } else if (HasImageInCache(src)) {
    OnImageUpdated(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  } else if (!HasImageInCache(src)) {
    OnImageAdded(src);
    dst->SetImageAnnotationStatus(
        ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  }
}

void AXImageAnnotator::AddDebuggingAttributes(
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

std::string AXImageAnnotator::GetImageAnnotation(
    blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  const auto lookup = image_annotations_.find(image.AxID());
  if (lookup != image_annotations_.end())
    return lookup->second.annotation();
  return std::string();
}

ax::mojom::ImageAnnotationStatus AXImageAnnotator::GetImageAnnotationStatus(
    blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  const auto lookup = image_annotations_.find(image.AxID());
  if (lookup != image_annotations_.end())
    return lookup->second.status();
  return ax::mojom::ImageAnnotationStatus::kNone;
}

bool AXImageAnnotator::HasAnnotationInCache(blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  if (!HasImageInCache(image))
    return false;
  return image_annotations_.at(image.AxID()).HasAnnotation();
}

bool AXImageAnnotator::HasImageInCache(const blink::WebAXObject& image) const {
  DCHECK(!image.IsDetached());
  return base::Contains(image_annotations_, image.AxID());
}

void AXImageAnnotator::OnImageAdded(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(!base::Contains(image_annotations_, image.AxID()));
  const std::string image_id = GenerateImageSourceId(image);
  if (image_id.empty())
    return;

  image_annotations_.emplace(image.AxID(), image);
  ImageInfo& image_info = image_annotations_.at(image.AxID());
  // Fetch image annotation.
  annotator_remote_->AnnotateImage(
      image_id, render_accessibility_->GetLanguage(),
      image_info.GetImageProcessor(),
      base::BindOnce(&AXImageAnnotator::OnImageAnnotated,
                     weak_factory_.GetWeakPtr(), image));
  VLOG(1) << "Requesting annotation for " << image_id << " with language '"
          << render_accessibility_->GetLanguage() << "' from page "
          << GetDocumentUrl();
}

void AXImageAnnotator::OnImageUpdated(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(base::Contains(image_annotations_, image.AxID()));
  const std::string image_id = GenerateImageSourceId(image);
  if (image_id.empty())
    return;

  ImageInfo& image_info = image_annotations_.at(image.AxID());
  // Update annotation.
  annotator_remote_->AnnotateImage(
      image_id, render_accessibility_->GetLanguage(),
      image_info.GetImageProcessor(),
      base::BindOnce(&AXImageAnnotator::OnImageAnnotated,
                     weak_factory_.GetWeakPtr(), image));
}

void AXImageAnnotator::OnImageRemoved(blink::WebAXObject& image) {
  DCHECK(!image.IsDetached());
  DCHECK(base::Contains(image_annotations_, image.AxID()));
  image_annotations_.erase(image.AxID());
}

// static
int AXImageAnnotator::GetLengthAfterRemovingStopwords(
    const std::string& image_name) {
  // Split the image name into words by splitting on all whitespace and
  // punctuation. Reject any words that are classified as stopwords.
  // Return the number of remaining codepoints.
  const char* separators = "0123456789`~!@#$%^&*()[]{}\\|;:'\",.<>?/-_=+ ";
  std::vector<std::string> words = base::SplitString(
      image_name, separators, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int remaining_codepoints = 0;
  for (const std::string& word : words) {
    if (AXImageStopwords::GetInstance().IsImageStopword(word.c_str())) {
      continue;
    }

    for (base::i18n::UTF8CharIterator iter(word); !iter.end(); iter.Advance()) {
      remaining_codepoints++;
    }
  }

  return remaining_codepoints;
}

// static
bool AXImageAnnotator::ImageNameHasMostlyStopwords(
    const std::string& image_name) {
  // Compute how many characters remain after removing stopwords.
  int remaining_codepoints = GetLengthAfterRemovingStopwords(image_name);

  // If there are 3 or fewer unicode codepoints remaining, classify
  // the string as "mostly stopwords".
  //
  // More details and analysis in this (Google-internal) design doc:
  // http://goto.google.com/augment-existing-image-descriptions
  return (remaining_codepoints <= 3);
}

#if defined(CONTENT_IMPLEMENTATION)
ContentClient* AXImageAnnotator::GetContentClient() const {
  return content::GetContentClient();
}
#else
ContentClient* AXImageAnnotator::GetContentClient() const {
  return nullptr;
}
#endif  // defined(CONTENT_IMPLEMENTATION)

std::string AXImageAnnotator::GenerateImageSourceId(
    const blink::WebAXObject& image) const {
  DCHECK(render_accessibility_);
  DCHECK(!image.IsDetached());
  const std::string document_url =
      render_accessibility_->GetMainDocument().Url().GetString().Utf8();
  const std::string image_src = image.Url().GetString().Utf8();
  if (document_url.empty() || image_src.empty())
    return std::string();

  // The |image_src| might be a URL that is relative to the document's URL.
  const GURL image_url = GURL(document_url).Resolve(image_src);
  if (!image_url.is_valid())
    return std::string();

  // If |image_url| appears to be publicly reachable, return the URL as the
  // image source ID.
  if (image_url.SchemeIsHTTPOrHTTPS())
    return image_url.spec();

  // If |image_url| is not publicly reachable, return a hash of |image_url|.
  // Scheme could be "data", "javascript", "ftp", "file", etc.
  const std::string& content = image_url.GetContent();
  if (content.empty())
    return std::string();
  return base::Base64Encode(crypto::SHA256HashString(content));
}

void AXImageAnnotator::MarkDirty(const blink::WebAXObject& image) const {
  render_accessibility_->MarkWebAXObjectDirty(image);

  // Check two unignored parents. If either of them is a link or root web area,
  // mark it dirty too, because we want a link or document containing exactly
  // a single image and nothing more to get annotated directly, too.
  //
  // TODO(dmazzoni): Expose ParentObjectUnignored in WebAXObject to
  // make this simpler.
  blink::WebAXObject parent = image.ParentObject();
  for (int ancestor_count = 0; !parent.IsDetached() && ancestor_count < 2;
       parent = parent.ParentObject()) {
    if (!parent.IsIgnored()) {
      ++ancestor_count;
      if (ui::IsLink(parent.Role()) || ui::IsPlatformDocument(parent.Role())) {
        render_accessibility_->MarkWebAXObjectDirty(parent);
        return;
      }
    }
  }
}

AXImageAnnotator::ImageInfo::ImageInfo(const blink::WebAXObject& image)
    : image_processor_(
          base::BindRepeating(&AXImageAnnotator::GetImageData, image)),
      status_(ax::mojom::ImageAnnotationStatus::kAnnotationPending),
      annotation_(std::nullopt) {}

AXImageAnnotator::ImageInfo::~ImageInfo() = default;

mojo::PendingRemote<image_annotation::mojom::ImageProcessor>
AXImageAnnotator::ImageInfo::GetImageProcessor() {
  return image_processor_.GetPendingRemote();
}

bool AXImageAnnotator::ImageInfo::HasAnnotation() const {
  switch (status_) {
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    // The user hasn't requested an annotation yet, or a previously pending
    // annotation request had been cancelled.
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return false;
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      DCHECK(annotation_.has_value());
      return true;
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    // Image has been classified as adult content.
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      DCHECK(!annotation_.has_value());
      return true;
  }
}

// static
SkBitmap AXImageAnnotator::GetImageData(const blink::WebAXObject& image) {
  if (image.IsDetached())
    return SkBitmap();
  blink::WebNode node = image.GetNode();
  if (node.IsNull() || !node.IsElementNode())
    return SkBitmap();
  blink::WebElement element = node.To<blink::WebElement>();
  VLOG(1) << "Uploading pixels for " << element.ImageContents().width() << " x "
          << element.ImageContents().height() << " image";
  return element.ImageContents();
}

void AXImageAnnotator::OnImageAnnotated(
    const blink::WebAXObject& image,
    image_annotation::mojom::AnnotateImageResultPtr result) {
  DCHECK(render_accessibility_->GetAXContext());
  render_accessibility_->GetAXContext()->UpdateAXForAllDocuments();

  if (!base::Contains(image_annotations_, image.AxID()))
    return;

  if (image.IsDetached()) {
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
    // We should not mark dirty a detached object.
    return;
  }

  // Get the image size as minimum and maximum dimension.
  blink::WebAXObject offset_container;
  gfx::RectF bounds;
  gfx::Transform container_transform;
  bool clips_children = false;
  image.GetRelativeBounds(offset_container, bounds, container_transform,
                          &clips_children);
  int min_dimension =
      static_cast<int>(std::min(bounds.width(), bounds.height()));
  int max_dimension =
      static_cast<int>(std::max(bounds.width(), bounds.height()));

  // Collect some histograms on the number of characters in the
  // image name, and also the image name after removing stopwords,
  // and also the minimum and maximum dimension,
  // as a function of whether the retrieved image label was
  // a success, an error, or empty.
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  blink::WebString web_name = image.GetName(name_from, name_objects);
  int non_stop_length = GetLengthAfterRemovingStopwords(web_name.Utf8());

  if (result->is_error_code()) {
    base::UmaHistogramCounts100("Accessibility.ImageLabels.ErrorByNameLength",
                                web_name.length());
    base::UmaHistogramCounts100(
        "Accessibility.ImageLabels.ErrorByNonStopNameLength", non_stop_length);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.ErrorByMaxDimension", max_dimension);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.ErrorByMinDimension", min_dimension);
  } else if (!result->is_annotations()) {
    base::UmaHistogramCounts100("Accessibility.ImageLabels.EmptyByNameLength",
                                web_name.length());
    base::UmaHistogramCounts100(
        "Accessibility.ImageLabels.EmptyByNonStopNameLength", non_stop_length);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.EmptyByMaxDimension", max_dimension);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.EmptyByMinDimension", min_dimension);
  } else {
    base::UmaHistogramCounts100("Accessibility.ImageLabels.SuccessByNameLength",
                                web_name.length());
    base::UmaHistogramCounts100(
        "Accessibility.ImageLabels.SuccessByNonStopNameLength",
        non_stop_length);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.SuccessByMaxDimension", max_dimension);
    base::UmaHistogramCounts1000(
        "Accessibility.ImageLabels.SuccessByMinDimension", min_dimension);
  }

  if (result->is_error_code()) {
    DLOG(WARNING) << "Image annotation error.";
    switch (result->get_error_code()) {
      case image_annotation::mojom::AnnotateImageError::kCanceled:
        // By marking the image as having an annotation status of
        // kSilentlyEligibleForAnnotation and not one of
        // kEligibleForAnnotation:, the user will not be asked to visit the
        // context menu to turn on automatic image labels, because there is no
        // way to repeat the operation from that menu yet.
        image_annotations_.at(image.AxID())
            .set_status(ax::mojom::ImageAnnotationStatus::
                            kSilentlyEligibleForAnnotation);
        break;
      case image_annotation::mojom::AnnotateImageError::kFailure:
        image_annotations_.at(image.AxID())
            .set_status(
                ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);
        break;
      case image_annotation::mojom::AnnotateImageError::kAdult:
        image_annotations_.at(image.AxID())
            .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationAdult);
        break;
    }
    MarkDirty(image);
    return;
  }

  if (!result->is_annotations()) {
    DLOG(WARNING) << "No image annotation results.";
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
    MarkDirty(image);
    return;
  }

  bool has_ocr = false;
  bool has_description = false;
  std::vector<std::string> contextualized_strings;
  for (const mojo::InlinedStructPtr<image_annotation::mojom::Annotation>&
           annotation : result->get_annotations()) {
    int message_id = 0;
    switch (annotation->type) {
      case image_annotation::mojom::AnnotationType::kOcr:
        has_ocr = true;
        message_id = IDS_AX_IMAGE_ANNOTATION_OCR_CONTEXT;
        break;
      case image_annotation::mojom::AnnotationType::kCaption:
      case image_annotation::mojom::AnnotationType::kLabel:
        has_description = true;
        message_id = IDS_AX_IMAGE_ANNOTATION_DESCRIPTION_CONTEXT;
        break;
      case image_annotation::mojom::AnnotationType::kIcon: {
        int icon_message_id = GetMessageIdForIconEnum(annotation->text);

        // Skip unrecognized icon annotation enum.
        if (icon_message_id == 0)
          continue;

        DCHECK(GetContentClient());
        contextualized_strings.push_back(base::UTF16ToUTF8(
            GetContentClient()->GetLocalizedString(icon_message_id)));
        break;
      }
    }

    // Skip unrecognized annotation types.
    if (message_id == 0)
      continue;

    int last_meaningful_char = annotation->text.length() - 1;
    while (last_meaningful_char >= 0) {
      bool is_whitespace_or_punct =
          base::IsAsciiWhitespace(annotation->text[last_meaningful_char]) ||
          base::IsAsciiPunctuation(annotation->text[last_meaningful_char]);
      if (!is_whitespace_or_punct) {
        break;
      }
      last_meaningful_char--;
    }

    if (annotation->text.empty() || last_meaningful_char < 0)
      continue;

    std::string text = annotation->text.substr(0, last_meaningful_char + 1);
    if (GetContentClient()) {
      contextualized_strings.push_back(
          base::UTF16ToUTF8(GetContentClient()->GetLocalizedString(
              message_id, base::UTF8ToUTF16(text))));
    } else {
      contextualized_strings.push_back(text);
    }
  }

  if (contextualized_strings.empty()) {
    image_annotations_.at(image.AxID())
        .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
    MarkDirty(image);
    return;
  }

  blink::WebString name = image.GetName(name_from, name_objects);
  bool has_existing_label = !name.IsEmpty();

  ukm::builders::Accessibility_ImageDescriptions(
      render_accessibility_->GetMainDocument().GetUkmSourceId())
      .SetOCR(has_ocr)
      .SetDescription(has_description)
      .SetImageAlreadyHasLabel(has_existing_label)
      .Record(render_accessibility_->ukm_recorder());

  image_annotations_.at(image.AxID())
      .set_status(ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  // TODO(accessibility): join two sentences together in a more i18n-friendly
  // way. Since this is intended for a screen reader, though, a period
  // probably works in almost all languages.
  std::string contextualized_string =
      base::JoinString(contextualized_strings, ". ");
  image_annotations_.at(image.AxID()).set_annotation(contextualized_string);
  MarkDirty(image);

  VLOG(1) << "Annotating image on page " << GetDocumentUrl() << " - "
          << contextualized_string;
}

std::string AXImageAnnotator::GetDocumentUrl() const {
  const blink::WebLocalFrame* frame =
      render_accessibility_->render_frame()->GetWebFrame();
  return frame->GetDocument().Url().GetString().Utf8();
}

}  // namespace content
