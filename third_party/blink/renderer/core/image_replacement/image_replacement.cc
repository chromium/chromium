// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image_replacement.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace blink {

namespace {

mojom::blink::ImageDataPtr ImageDataForImageResource(
    ImageResourceContent* image_content) {
  Image* image = image_content->GetImage();
  if (!image) {
    return nullptr;
  }
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!image->HasDefaultOrientation()) {
    paint_image =
        Image::ResizeAndOrientImage(paint_image, image->Orientation());
  }
  sk_sp<SkImage> sk_image = paint_image.GetSwSkImage();
  if (!sk_image) {
    return nullptr;
  }
  SkPixmap pixmap;
  Vector<uint8_t> buffer;
  if (!sk_image->peekPixels(&pixmap)) {
    SkImageInfo info = sk_image->imageInfo();
    buffer.resize(info.computeMinByteSize());
    pixmap.reset(info, buffer.data(), info.minRowBytes());
    if (!sk_image->readPixels(pixmap, 0, 0)) {
      return nullptr;
    }
  }

  // TODO(b/501538138): Consider encoding on a background thread, rescaling
  // the image if it's larger than 2048 in some dimension, and passing the
  // original bytes unmodified if it is already suitable, as possible future
  // optimizations.
  Vector<unsigned char> webp_bytes;
  if (!ImageEncoder::Encode(&webp_bytes, pixmap,
                            ImageEncoder::ComputeWebpOptions(0.8))) {
    return nullptr;
  }

  mojom::blink::ImageDataPtr image_data = mojom::blink::ImageData::New();
  image_data->webp_bytes = base::span<const uint8_t>(webp_bytes);
  return image_data;
}

}  // namespace

// static
base::expected<mojo::PendingRemote<mojom::blink::ImageReplacement>, String>
ImageReplacement::CreateAndBindReceiver(HTMLImageElement& image_element) {
  if (!base::FeatureList::IsEnabled(features::kImageReplacement)) {
    return base::unexpected("ImageReplacement feature is not enabled.");
  }

  if (!image_element.isConnected()) {
    return base::unexpected("HTMLImageElement is not connected to a document.");
  }

  DocumentImageReplacements& replacements =
      DocumentImageReplacements::From(image_element.GetDocument());
  if (replacements.GetImageReplacement(&image_element)) {
    return base::unexpected(
        "HTMLImageElement already has a pending/active replacement.");
  }

  if (image_element.complete()) {
    ImageResourceContent* image_content = image_element.CachedImage();
    if (!image_content) {
      return base::unexpected("HTMLImageElement has no src");
    }
    if (image_content->ErrorOccurred()) {
      return base::unexpected("HTMLImageElement had a loading error");
    }
  }
  if (!image_element.IsPrimaryContent()) {
    return base::unexpected(
        "HTMLImageElement is not displaying primary content.");
  }

  ImageReplacement* replacement = MakeGarbageCollected<ImageReplacement>(
      base::PassKey<ImageReplacement>(), image_element);
  replacements.RegisterImageReplacement(&image_element, replacement);
  return replacement->BindReceiver();
}

ImageReplacement::ImageReplacement(base::PassKey<ImageReplacement>,
                                   HTMLImageElement& image_element)
    : image_element_(&image_element),
      receiver_(this, image_element.GetExecutionContext()),
      host_(image_element.GetExecutionContext()) {}

// static
void ImageReplacement::ResetImageReplacement(base::PassKey<HTMLImageElement>,
                                             HTMLImageElement& image_element,
                                             Document& document) {
  if (!base::FeatureList::IsEnabled(features::kImageReplacement)) {
    return;
  }
  if (auto* replacements = DocumentImageReplacements::FromIfExists(document)) {
    if (auto* replacement =
            replacements->UnregisterImageReplacement(&image_element)) {
      replacement->Reset(document);
    }
  }
}

void ImageReplacement::StartReplacement(
    mojo::PendingRemote<mojom::blink::ImageReplacementHost> host_remote) {
  CHECK(base::FeatureList::IsEnabled(features::kImageReplacement));
  CHECK(image_element_->isConnected());
  // If there's already an active replacement, we do nothing.
  if (image_element_->HasImageReplacement()) {
    return;
  }
  if (!image_element_->complete()) {
    pending_host_remote_ = std::move(host_remote);
    return;
  }
  ImageResourceContent* image_content = image_element_->CachedImage();
  if (!image_content || image_content->ErrorOccurred()) {
    image_element_->ResetImageReplacement();
    return;
  }
  mojom::blink::ImageDataPtr image_data =
      ImageDataForImageResource(image_content);
  if (!image_data) {
    image_element_->ResetImageReplacement();
    return;
  }

  // If the image element stopped displaying primary content between now and
  // when `CreateImageReplacement()` was called, we would have already called
  // `Reset()`.
  CHECK(image_element_->IsPrimaryContent());
  original_image_source_url_ = image_element_->ImageSourceURL();
  image_element_->StartImageReplacement();

  ShadowRoot* shadow_root = image_element_->UserAgentShadowRoot();
  CHECK(shadow_root);

  HTMLIFrameElement* iframe = To<HTMLIFrameElement>(shadow_root->firstChild());
  CHECK(iframe);
  if (LocalFrame* frame = To<LocalFrame>(iframe->ContentFrame())) {
    host_.Bind(std::move(host_remote),
               image_element_->GetDocument().GetTaskRunner(
                   TaskType::kInternalDefault));

    gfx::QuadF quad;
    if (LayoutBox* box = image_element_->GetLayoutBox()) {
      PhysicalRect local_rect = box->PhysicalBorderBoxRect();
      quad = box->LocalRectToAncestorQuad(
          local_rect, nullptr,
          kTraverseDocumentBoundaries | kApplyRemoteViewportTransform);
    }

    host_->ReplacementFrameAttached(frame->GetLocalFrameToken(), quad,
                                    std::move(image_data));
  }
}

void ImageReplacement::RenderReplacement() {
  CHECK(image_element_);
  should_paint_original_image_ = false;

  if (auto* layout_image_replacement =
          To<LayoutImageReplacement>(image_element_->GetLayoutObject())) {
    layout_image_replacement
        ->SetShouldDoFullPaintInvalidationWithoutLayoutChange(
            PaintInvalidationReason::kImage);
  }

  auto& iframe = To<HTMLIFrameElement>(
      *image_element_->UserAgentShadowRoot()->firstChild());
  iframe.SetInlineStyleProperty(CSSPropertyID::kOpacity, "1");
}

void ImageReplacement::Trace(Visitor* visitor) const {
  visitor->Trace(image_element_);
  visitor->Trace(receiver_);
  visitor->Trace(host_);
}

mojo::PendingRemote<mojom::blink::ImageReplacement>
ImageReplacement::BindReceiver() {
  mojo::PendingRemote<mojom::blink::ImageReplacement> pending_remote =
      receiver_.BindNewPipeAndPassRemote(
          image_element_->GetDocument().GetTaskRunner(
              TaskType::kInternalDefault));
  receiver_.set_disconnect_handler(
      BindOnce(&ImageReplacement::OnDisconnect, WrapWeakPersistent(this)));
  return pending_remote;
}

// static
void ImageReplacement::CreateImageReplacementShadowTree(
    base::PassKey<HTMLImageElement>,
    HTMLImageElement& image_element) {
  ShadowRoot* shadow_root = image_element.UserAgentShadowRoot();
  CHECK(shadow_root);
  HTMLIFrameElement* iframe =
      MakeGarbageCollected<HTMLIFrameElement>(image_element.GetDocument());
  iframe->SetInlineStyleProperty(CSSPropertyID::kBorderStyle, "none");
  iframe->SetInlineStyleProperty(CSSPropertyID::kBorderWidth, "0");
  iframe->SetInlineStyleProperty(CSSPropertyID::kPointerEvents, "none");
  // Note: We use opacity: 0 (instead of visibility: hidden) to ensure that
  // the iframe's rendering isn't throttled.
  iframe->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0");
  iframe->SetInlineStyleProperty(CSSPropertyID::kDisplay, "block");
  // We add this to ensure the iframe is always in a self-painting layer,
  // otherwise it won't get painted.
  iframe->SetInlineStyleProperty(CSSPropertyID::kIsolation, "isolate");

  shadow_root->AppendChild(iframe);
}

void ImageReplacement::Reset(Document& document) {
  receiver_.reset();
  host_.reset();
  pending_host_remote_.reset();
  image_element_ = nullptr;
}

void ImageReplacement::OnDisconnect() {
  if (image_element_) {
    // Note: We want to go through the HTMLImageElement::ResetImageReplacement()
    // path to ensure that the image replacement is reset in the shadow tree and
    // the primary content is displayed. This method will callback into
    // ImageReplacement::Reset(), so `image_element_` will be set to nullptr
    // after this line.
    image_element_->ResetImageReplacement();
  }
}

bool ImageReplacement::ResumeReplacementAfterImageLoad() {
  if (!pending_host_remote_.is_valid()) {
    return false;
  }
  CHECK(image_element_ && image_element_->complete());
  mojo::PendingRemote<mojom::blink::ImageReplacementHost> remote =
      std::move(pending_host_remote_);
  StartReplacement(std::move(remote));
  // Note: `image_element_` can be nullptr here if the image load failed with
  // an error (StartReplacement will reset the image replacement in that case).
  return image_element_ && image_element_->HasImageReplacement();
}

}  // namespace blink
