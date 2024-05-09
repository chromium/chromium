// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_image_generator_impl.h"

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/modules/csspaint/nativepaint/background_color_paint_definition.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

BackgroundColorPaintImageGenerator*
BackgroundColorPaintImageGeneratorImpl::Create(LocalFrame& local_root) {
  BackgroundColorPaintDefinition* background_color_paint_definition =
      BackgroundColorPaintDefinition::Create(local_root);
  if (!background_color_paint_definition)
    return nullptr;

  BackgroundColorPaintImageGeneratorImpl* generator =
      MakeGarbageCollected<BackgroundColorPaintImageGeneratorImpl>(
          background_color_paint_definition);

  return generator;
}

BackgroundColorPaintImageGeneratorImpl::BackgroundColorPaintImageGeneratorImpl(
    BackgroundColorPaintDefinition* background_color_paint_definition)
    : background_color_paint_definition_(background_color_paint_definition) {}

scoped_refptr<Image> BackgroundColorPaintImageGeneratorImpl::Paint(
    const gfx::SizeF& container_size,
    const Node* node) {
  return background_color_paint_definition_->Paint(container_size, node);
}

Animation* BackgroundColorPaintImageGeneratorImpl::GetAnimationIfCompositable(
    const Element* element) {
  // When this is true, we have a background-color animation in the
  // body element, while the view is responsible for painting the
  // body's background. In this case, we need to let the
  // background-color animation run on the main thread because the
  // body is not painted with BackgroundColorPaintWorklet.
  LayoutObject* layout_object = element->GetLayoutObject();
  bool background_transfers_to_view =
      element->GetLayoutBoxModelObject() &&
      element->GetLayoutBoxModelObject()->BackgroundTransfersToView();

  // The table rows and table cols are painted into table cells,
  // which means their background is never painted using
  // BackgroundColorPaintWorklet, as a result, we should not
  // composite the background color animation on the table rows
  // or cols. Should not be compositing if any of these return true.
  if (layout_object->IsLayoutTableCol() || layout_object->IsTableRow() ||
      background_transfers_to_view) {
    return nullptr;
  }
  return BackgroundColorPaintDefinition::GetAnimationIfCompositable(element);
}

void BackgroundColorPaintImageGeneratorImpl::Shutdown() {
  background_color_paint_definition_->UnregisterProxyClient();
}

void BackgroundColorPaintImageGeneratorImpl::Trace(Visitor* visitor) const {
  visitor->Trace(background_color_paint_definition_);
  BackgroundColorPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
