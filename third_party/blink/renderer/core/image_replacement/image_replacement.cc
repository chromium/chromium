// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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
  // If the image hasn't loaded yet, we don't start the replacement and reset
  // the handle.
  // TODO(b/489470758): We should wait for the image load to complete and then
  // start the replacement (if the load is successful).
  if (!image_element_->complete()) {
    image_element_->ResetImageReplacement();
    return;
  }
  ImageResourceContent* image_content = image_element_->CachedImage();
  if (!image_content || image_content->ErrorOccurred()) {
    image_element_->ResetImageReplacement();
    return;
  }

  // If the image element stopped displaying primary content between now and
  // when `CreateImageReplacement()` was called, we would have already called
  // `Reset()`.
  CHECK(image_element_->IsPrimaryContent());
  image_element_->StartImageReplacement();

  ShadowRoot* shadow_root = image_element_->UserAgentShadowRoot();
  CHECK(shadow_root);

  HTMLIFrameElement* iframe = To<HTMLIFrameElement>(shadow_root->firstChild());
  CHECK(iframe);
  if (LocalFrame* frame = To<LocalFrame>(iframe->ContentFrame())) {
    host_.Bind(std::move(host_remote),
               image_element_->GetDocument().GetTaskRunner(
                   TaskType::kInternalDefault));
    host_->ReplacementFrameAttached(frame->GetLocalFrameToken());
  }
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
  iframe->SetInlineStyleProperty(CSSPropertyID::kWidth, "100%");
  iframe->SetInlineStyleProperty(CSSPropertyID::kHeight, "100%");
  iframe->SetInlineStyleProperty(CSSPropertyID::kBorderStyle, "none");
  iframe->SetInlineStyleProperty(CSSPropertyID::kBorderWidth, "0");

  shadow_root->AppendChild(iframe);
}

void ImageReplacement::Reset(Document& document) {
  receiver_.reset();
  host_.reset();
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

}  // namespace blink
