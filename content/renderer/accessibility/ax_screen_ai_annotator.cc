// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_screen_ai_annotator.h"

#include "net/base/data_url.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {
bool GetImageFromWebAXObject(const blink::WebAXObject& object,
                             SkBitmap& bitmap) {
  const std::string image_data_url = object.ImageDataUrl(gfx::Size()).Utf8();
  if (image_data_url.empty()) {
    VLOG(1) << "Screen AI could not get image for " << object.ToString().Utf8();
    return false;
  }

  std::string mimetype;
  std::string charset;
  std::string png_data;

  if (!net::DataURL::Parse(GURL(image_data_url), &mimetype, &charset,
                           &png_data)) {
    VLOG(1) << "Screen AI could not parse image.";
    return false;
  }

  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const unsigned char*>(png_data.data()),
          png_data.size(), &bitmap)) {
    VLOG(2) << "Screen AI could not decode image.";
    return false;
  }

  return true;
}
}  // namespace

namespace content {

AXScreenAIAnnotator::AXScreenAIAnnotator(
    RenderAccessibilityImpl* const render_accessibility,
    mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator)
    : render_accessibility_(render_accessibility),
      screen_ai_annotator_(std::move(screen_ai_annotator)) {}

AXScreenAIAnnotator::~AXScreenAIAnnotator() = default;

bool AXScreenAIAnnotator::ShouldAnnotateObject(
    const blink::WebAXObject& object) {
  // TODO(https://crbug.com/1278249): Add snapshot archiving and comparison to
  // prevent re-annotating an object unless its image is modified.

  // TODO(https://crbug.com/1278249): Update heuristic.
  return object.Role() == ax::mojom::Role::kRootWebArea;
}

bool AXScreenAIAnnotator::ApplyAnnotationsIfAvailable(
    const blink::WebAXObject& src,
    ui::AXNodeData& dst) {
  const auto lookup = annotations_.find(src.AxID());
  if (lookup == annotations_.end())
    return false;

  // TODO(https://crbug.com/1278249): Apply annotations.
  return true;
}

void AXScreenAIAnnotator::MaybeRunScreenAI(const blink::WebAXObject& object) {
  if (ShouldAnnotateObject(object)) {
    SkBitmap bitmap;
    if (!GetImageFromWebAXObject(object, bitmap))
      return;

    annotations_[object.AxID()] = std::vector<screen_ai::mojom::Node>();
    screen_ai_annotator_->Annotate(
        bitmap, base::BindOnce(&AXScreenAIAnnotator::OnAnnotationReceived,
                               weak_ptr_factory_.GetWeakPtr(), object.AxID()));
  }
}

void AXScreenAIAnnotator::OnAnnotationReceived(
    ui::AXNodeID ax_id,
    screen_ai::mojom::ErrorType error_type,
    std::vector<screen_ai::mojom::NodePtr> annotation) {
  if (error_type != screen_ai::mojom::ErrorType::kOK)
    return;

  // TODO(https://crbug.com/1278249): Perform required conversions on members of
  // |annotation| and store them in |annotations_|

  // TODO(https://crbug.com/1278249): Apply received annotation on |object|.
  // Depending on the data received from Screen AI library, we may decide to
  // update the node here, or just add the data during serialization.
  blink::WebAXObject object = blink::WebAXObject::FromWebDocumentByID(
      render_accessibility_->GetMainDocument(), ax_id);

  render_accessibility_->MarkWebAXObjectDirty(object, true /* subtree */);
}

}  // namespace content