/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_item_list.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/drag_image.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-blink.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

class DraggedNodeImageBuilder {
  STACK_ALLOCATED();

 public:
  DraggedNodeImageBuilder(LocalFrame& local_frame, Node& node)
      : local_frame_(&local_frame),
        node_(&node)
#if DCHECK_IS_ON()
        ,
        dom_tree_version_(node.GetDocument().DomTreeVersion())
#endif
  {
    for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*node_))
      descendant.SetDragged(true);
  }

  ~DraggedNodeImageBuilder() {
#if DCHECK_IS_ON()
    DCHECK_EQ(dom_tree_version_, node_->GetDocument().DomTreeVersion());
#endif
    for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*node_))
      descendant.SetDragged(false);
  }

  std::unique_ptr<DragImage> CreateImage() {
#if DCHECK_IS_ON()
    DCHECK_EQ(dom_tree_version_, node_->GetDocument().DomTreeVersion());
#endif
    // Construct layout object for |node_| with pseudo class "-webkit-drag"
    local_frame_->View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kDragImage);
    LayoutObject* const dragged_layout_object = node_->GetLayoutObject();
    if (!dragged_layout_object)
      return nullptr;
    // Paint starting at the nearest stacking context, clipped to the object
    // itself. This will also paint the contents behind the object if the
    // object contains transparency and there are other elements in the same
    // stacking context which stacked below.
    PaintLayer* layer = dragged_layout_object->EnclosingLayer();
    if (!layer->GetLayoutObject().IsStackingContext())
      layer = layer->AncestorStackingContext();

    gfx::Rect absolute_bounding_box =
        dragged_layout_object->AbsoluteBoundingBoxRectIncludingDescendants();

    gfx::RectF bounding_box =
        layer->GetLayoutObject()
            .AbsoluteToLocalQuad(gfx::QuadF(gfx::RectF(absolute_bounding_box)))
            .BoundingBox();
    gfx::RectF cull_rect = bounding_box;
    cull_rect.Offset(
        gfx::Vector2dF(layer->GetLayoutObject().FirstFragment().PaintOffset()));
    OverriddenCullRectScope cull_rect_scope(
        *layer, CullRect(gfx::ToEnclosingRect(cull_rect)),
        /*disable_expansion*/ true);
    PaintRecordBuilder builder;

    dragged_layout_object->GetDocument().Lifecycle().AdvanceTo(
        DocumentLifecycle::kInPaint);
    PaintLayerPainter(*layer).Paint(builder.Context(),
                                    PaintFlag::kOmitCompositingInfo);
    dragged_layout_object->GetDocument().Lifecycle().AdvanceTo(
        DocumentLifecycle::kPaintClean);

    gfx::Vector2dF paint_offset = bounding_box.OffsetFromOrigin();
    PropertyTreeState border_box_properties = layer->GetLayoutObject()
                                                  .FirstFragment()
                                                  .LocalBorderBoxProperties()
                                                  .Unalias();
    // We paint in the containing transform node's space. Add the offset from
    // the layer to this transform space.
    paint_offset +=
        gfx::Vector2dF(layer->GetLayoutObject().FirstFragment().PaintOffset());

    return DataTransfer::CreateDragImageForFrame(
        *local_frame_, 1.0f, bounding_box.size(), paint_offset, builder,
        border_box_properties);
  }

 private:
  LocalFrame* const local_frame_;
  Node* const node_;
#if DCHECK_IS_ON()
  const uint64_t dom_tree_version_;
#endif
};

std::optional<DragOperationsMask> ConvertEffectAllowedToDragOperationsMask(
    const AtomicString& op) {
  // Values specified in
  // https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-effectallowed
  if (op == "uninitialized")
    return kDragOperationEvery;
  if (op == "none")
    return kDragOperationNone;
  if (op == "copy")
    return kDragOperationCopy;
  if (op == "link")
    return kDragOperationLink;
  if (op == "move")
    return kDragOperationMove;
  if (op == "copyLink") {
    return static_cast<DragOperationsMask>(kDragOperationCopy |
                                           kDragOperationLink);
  }
  if (op == "copyMove") {
    return static_cast<DragOperationsMask>(kDragOperationCopy |
                                           kDragOperationMove);
  }
  if (op == "linkMove") {
    return static_cast<DragOperationsMask>(kDragOperationLink |
                                           kDragOperationMove);
  }
  if (op == "all")
    return kDragOperationEvery;
  return std::nullopt;
}

AtomicString ConvertDragOperationsMaskToEffectAllowed(DragOperationsMask op) {
  if (((op & kDragOperationMove) && (op & kDragOperationCopy) &&
       (op & kDragOperationLink)) ||
      (op == kDragOperationEvery))
    return AtomicString("all");
  if ((op & kDragOperationMove) && (op & kDragOperationCopy))
    return AtomicString("copyMove");
  if ((op & kDragOperationMove) && (op & kDragOperationLink))
    return AtomicString("linkMove");
  if ((op & kDragOperationCopy) && (op & kDragOperationLink))
    return AtomicString("copyLink");
  if (op & kDragOperationMove)
    return AtomicString("move");
  if (op & kDragOperationCopy)
    return AtomicString("copy");
  if (op & kDragOperationLink)
    return AtomicString("link");
  return keywords::kNone;
}

// We provide the IE clipboard types (URL and Text), and the clipboard types
// specified in the HTML spec. See
// https://html.spec.whatwg.org/multipage/dnd.html#the-datatransfer-interface
String NormalizeType(const String& type, bool* convert_to_url = nullptr) {
  String clean_type = type.StripWhiteSpace().LowerASCII();
  if (clean_type == kMimeTypeText ||
      clean_type.StartsWith(kMimeTypeTextPlainEtc))
    return kMimeTypeTextPlain;
  if (clean_type == kMimeTypeURL) {
    if (convert_to_url)
      *convert_to_url = true;
    return kMimeTypeTextURIList;
  }
  return clean_type;
}

}  // namespace

// static
DataTransfer* DataTransfer::Create() {
  DataTransfer* data = Create(
      kCopyAndPaste, DataTransferAccessPolicy::kWritable, DataObject::Create());
  data->drop_effect_ = keywords::kNone;
  data->effect_allowed_ = keywords::kNone;
  return data;
}

// static
DataTransfer* DataTransfer::Create(DataTransferType type,
                                   DataTransferAccessPolicy policy,
                                   DataObject* data_object) {
  return MakeGarbageCollected<DataTransfer>(type, policy, data_object);
}

DataTransfer::~DataTransfer() = default;

void DataTransfer::setDropEffect(const AtomicString& effect) {
  if (!IsForDragAndDrop())
    return;

  // The attribute must ignore any attempts to set it to a value other than
  // none, copy, link, and move.
  if (effect != "none" && effect != "copy" && effect != "link" &&
      effect != "move")
    return;

  // The specification states that dropEffect can be changed at all times, even
  // if the DataTransfer instance is protected or neutered.
  drop_effect_ = effect;
}

void DataTransfer::setEffectAllowed(const AtomicString& effect) {
  if (!IsForDragAndDrop())
    return;

  if (!ConvertEffectAllowedToDragOperationsMask(effect)) {
    // This means that there was no conversion, and the effectAllowed that
    // we are passed isn't a valid effectAllowed, so we should ignore it,
    // and not set |effect_allowed_|.

    // The attribute must ignore any attempts to set it to a value other than
    // none, copy, copyLink, copyMove, link, linkMove, move, all, and
    // uninitialized.
    return;
  }

  if (CanWriteData())
    effect_allowed_ = effect;
}

void DataTransfer::clearData(const String& type) {
  if (!CanWriteData()) {
    return;
  }
  if (type.IsNull()) {
    // As per spec
    // https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-cleardata,
    // `clearData()` doesn't remove `kFileKind` objects from `item_list_`.
    data_object_->ClearStringItems();
  } else {
    data_object_->ClearData(NormalizeType(type));
  }
}

String DataTransfer::getData(const String& type) const {
  if (!CanReadData())
    return String();

  bool convert_to_url = false;
  String data = data_object_->GetData(NormalizeType(type, &convert_to_url));
  if (!convert_to_url)
    return data;
  return ConvertURIListToURL(data);
}

void DataTransfer::setData(const String& type, const String& data) {
  if (!CanWriteData())
    return;

  data_object_->SetData(NormalizeType(type), data);
}

bool DataTransfer::hasDataStoreItemListChanged() const {
  return data_store_item_list_changed_ || !CanReadTypes();
}

void DataTransfer::OnItemListChanged() {
  data_store_item_list_changed_ = true;
  files_->clear();
}

Vector<String> DataTransfer::types() {
  if (!CanReadTypes())
    return Vector<String>();

  data_store_item_list_changed_ = false;
  return data_object_->Types();
}

FileList* DataTransfer::files() const {
  if (!CanReadData()) {
    files_->clear();
    return files_.Get();
  }

  if (!files_->IsEmpty())
    return files_.Get();

  for (uint32_t i = 0; i < data_object_->length(); ++i) {
    if (data_object_->Item(i)->Kind() == DataObjectItem::kFileKind) {
      Blob* blob = data_object_->Item(i)->GetAsFile();
      if (auto* file = DynamicTo<File>(blob))
        files_->Append(file);
    }
  }

  return files_.Get();
}

void DataTransfer::setDragImage(Element* image, int x, int y) {
  DCHECK(image);

  if (!IsForDragAndDrop())
    return;

  // Convert `drag_loc_` from CSS px to physical pixels.
  // `LocalFrame::LayoutZoomFactor` converts from CSS px to physical px by
  // taking into account both device scale factor and page zoom.
  LocalFrame* frame = image->GetDocument().GetFrame();
  gfx::Point location =
      gfx::ScaleToRoundedPoint(gfx::Point(x, y), frame->LayoutZoomFactor());

  auto* html_image_element = DynamicTo<HTMLImageElement>(image);
  if (html_image_element && !image->isConnected())
    SetDragImageResource(html_image_element->CachedImage(), location);
  else
    SetDragImageElement(image, location);
}

void DataTransfer::ClearDragImage() {
  setDragImage(nullptr, nullptr, gfx::Point());
}

void DataTransfer::SetDragImageResource(ImageResourceContent* img,
                                        const gfx::Point& loc) {
  setDragImage(img, nullptr, loc);
}

void DataTransfer::SetDragImageElement(Node* node, const gfx::Point& loc) {
  setDragImage(nullptr, node, loc);
}

// static
gfx::RectF DataTransfer::ClipByVisualViewport(const gfx::RectF& absolute_rect,
                                              const LocalFrame& frame) {
  gfx::Rect viewport_in_root_frame =
      ToEnclosingRect(frame.GetPage()->GetVisualViewport().VisibleRect());
  gfx::RectF absolute_viewport(
      frame.View()->ConvertFromRootFrame(viewport_in_root_frame));
  return IntersectRects(absolute_viewport, absolute_rect);
}

// Returns a DragImage whose bitmap contains |contents|, positioned and scaled
// in device space.
//
// static
std::unique_ptr<DragImage> DataTransfer::CreateDragImageForFrame(
    LocalFrame& frame,
    float opacity,
    const gfx::SizeF& layout_size,
    const gfx::Vector2dF& paint_offset,
    PaintRecordBuilder& builder,
    const PropertyTreeState& property_tree_state) {
  float layout_to_device_scale = frame.GetPage()->GetVisualViewport().Scale();

  gfx::SizeF device_size = gfx::ScaleSize(layout_size, layout_to_device_scale);
  AffineTransform transform;
  gfx::Vector2dF device_paint_offset =
      gfx::ScaleVector2d(paint_offset, layout_to_device_scale);
  transform.Translate(-device_paint_offset.x(), -device_paint_offset.y());
  transform.Scale(layout_to_device_scale);

  // Rasterize upfront, since DragImage::create() is going to do it anyway
  // (SkImage::asLegacyBitmap).
  SkSurfaceProps surface_props;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(device_size.width(), device_size.height()),
      &surface_props);
  if (!surface)
    return nullptr;

  SkiaPaintCanvas skia_paint_canvas(surface->getCanvas());
  skia_paint_canvas.concat(AffineTransformToSkM44(transform));
  builder.EndRecording(skia_paint_canvas, property_tree_state);

  scoped_refptr<Image> image =
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());

  // There is no orientation information in the image, so pass
  // kDoNotRespectImageOrientation in order to avoid wasted work looking
  // at orientation.
  return DragImage::Create(image.get(), kDoNotRespectImageOrientation,
                           kInterpolationDefault, opacity);
}

// static
std::unique_ptr<DragImage> DataTransfer::NodeImage(LocalFrame& frame,
                                                   Node& node) {
  DraggedNodeImageBuilder image_node(frame, node);
  return image_node.CreateImage();
}

std::unique_ptr<DragImage> DataTransfer::CreateDragImage(
    gfx::Point& loc,
    float device_scale_factor,
    LocalFrame* frame) const {
  loc = drag_loc_;
  if (drag_image_element_) {
    return NodeImage(*frame, *drag_image_element_);
  }
  std::unique_ptr<DragImage> drag_image =
      drag_image_ ? DragImage::Create(drag_image_->GetImage()) : nullptr;
  if (drag_image) {
    drag_image->Scale(device_scale_factor, device_scale_factor);
    return drag_image;
  }
  return nullptr;
}

static ImageResourceContent* GetImageResourceContent(Element* element) {
  // Attempt to pull ImageResourceContent from element
  DCHECK(element);
  if (auto* image = DynamicTo<LayoutImage>(element->GetLayoutObject())) {
    if (image->CachedImage() && !image->CachedImage()->ErrorOccurred())
      return image->CachedImage();
  }
  return nullptr;
}

static void WriteImageToDataObject(DataObject* data_object,
                                   Element* element,
                                   const KURL& image_url) {
  // Shove image data into a DataObject for use as a file
  ImageResourceContent* cached_image = GetImageResourceContent(element);
  if (!cached_image || !cached_image->GetImage() || !cached_image->IsLoaded())
    return;

  Image* image = cached_image->GetImage();
  scoped_refptr<SharedBuffer> image_buffer = image->Data();
  if (!image_buffer || !image_buffer->size())
    return;

  data_object->AddFileSharedBuffer(
      image_buffer, cached_image->IsAccessAllowed(), image_url,
      image->FilenameExtension(),
      cached_image->GetResponse().HttpHeaderFields().Get(
          http_names::kContentDisposition));
}

void DataTransfer::DeclareAndWriteDragImage(Element* element,
                                            const KURL& link_url,
                                            const KURL& image_url,
                                            const String& title) {
  if (!data_object_)
    return;

  data_object_->SetURLAndTitle(link_url.IsValid() ? link_url : image_url,
                               title);

  // Write the bytes in the image to the file format.
  WriteImageToDataObject(data_object_.Get(), element, image_url);

  // Put img tag on the clipboard referencing the image
  data_object_->SetData(kMimeTypeTextHTML,
                        CreateMarkup(element, kIncludeNode, kResolveAllURLs));
}

void DataTransfer::WriteURL(Node* node, const KURL& url, const String& title) {
  if (!data_object_)
    return;
  DCHECK(!url.IsEmpty());

  data_object_->SetURLAndTitle(url, title);

  // The URL can also be used as plain text.
  data_object_->SetData(kMimeTypeTextPlain, url.GetString());

  // The URL can also be used as an HTML fragment.
  data_object_->SetHTMLAndBaseURL(
      CreateMarkup(node, kIncludeNode, kResolveAllURLs), url);
}

void DataTransfer::WriteSelection(const FrameSelection& selection) {
  if (!data_object_)
    return;

  if (!EnclosingTextControl(
          selection.ComputeVisibleSelectionInDOMTree().Start())) {
    data_object_->SetHTMLAndBaseURL(selection.SelectedHTMLForClipboard(),
                                    selection.GetFrame()->GetDocument()->Url());
  }

  String str = selection.SelectedTextForClipboard();
#if BUILDFLAG(IS_WIN)
  ReplaceNewlinesWithWindowsStyleNewlines(str);
#endif
  ReplaceNBSPWithSpace(str);
  data_object_->SetData(kMimeTypeTextPlain, str);
}

void DataTransfer::SetAccessPolicy(DataTransferAccessPolicy policy) {
  // once you go numb, can never go back
  DCHECK(policy_ != DataTransferAccessPolicy::kNumb ||
         policy == DataTransferAccessPolicy::kNumb);
  policy_ = policy;
}

bool DataTransfer::CanReadTypes() const {
  return policy_ == DataTransferAccessPolicy::kReadable ||
         policy_ == DataTransferAccessPolicy::kTypesReadable ||
         policy_ == DataTransferAccessPolicy::kWritable;
}

bool DataTransfer::CanReadData() const {
  return policy_ == DataTransferAccessPolicy::kReadable ||
         policy_ == DataTransferAccessPolicy::kWritable;
}

bool DataTransfer::CanWriteData() const {
  return policy_ == DataTransferAccessPolicy::kWritable;
}

bool DataTransfer::CanSetDragImage() const {
  return policy_ == DataTransferAccessPolicy::kWritable;
}

DragOperationsMask DataTransfer::SourceOperation() const {
  std::optional<DragOperationsMask> op =
      ConvertEffectAllowedToDragOperationsMask(effect_allowed_);
  DCHECK(op);
  return *op;
}

ui::mojom::blink::DragOperation DataTransfer::DestinationOperation() const {
  DCHECK(DropEffectIsInitialized());
  std::optional<DragOperationsMask> op =
      ConvertEffectAllowedToDragOperationsMask(drop_effect_);
  return static_cast<ui::mojom::blink::DragOperation>(*op);
}

void DataTransfer::SetSourceOperation(DragOperationsMask op) {
  effect_allowed_ = ConvertDragOperationsMaskToEffectAllowed(op);
}

void DataTransfer::SetDestinationOperation(ui::mojom::blink::DragOperation op) {
  drop_effect_ = ConvertDragOperationsMaskToEffectAllowed(
      static_cast<DragOperationsMask>(op));
}

DataTransferItemList* DataTransfer::items() {
  // TODO(crbug.com/331320416): According to the spec, we are supposed to
  // return the same collection of items each time. We now return a wrapper
  // that always wraps the *same* set of items, so JS shouldn't be able to
  // tell, but we probably still want to fix this.
  return MakeGarbageCollected<DataTransferItemList>(this, data_object_);
}

DataObject* DataTransfer::GetDataObject() const {
  return data_object_.Get();
}

DataTransfer::DataTransfer(DataTransferType type,
                           DataTransferAccessPolicy policy,
                           DataObject* data_object)
    : policy_(policy),
      effect_allowed_("uninitialized"),
      transfer_type_(type),
      data_object_(data_object),
      data_store_item_list_changed_(true),
      files_(MakeGarbageCollected<FileList>()) {
  data_object_->AddObserver(this);
}

void DataTransfer::setDragImage(ImageResourceContent* image,
                                Node* node,
                                const gfx::Point& loc) {
  if (!CanSetDragImage())
    return;

  drag_image_ = image;
  drag_loc_ = loc;
  drag_image_element_ = node;
}

bool DataTransfer::HasFileOfType(const String& type) const {
  if (!CanReadTypes())
    return false;

  for (uint32_t i = 0; i < data_object_->length(); ++i) {
    if (data_object_->Item(i)->Kind() == DataObjectItem::kFileKind) {
      Blob* blob = data_object_->Item(i)->GetAsFile();
      if (blob && blob->IsFile() &&
          DeprecatedEqualIgnoringCase(blob->type(), type))
        return true;
    }
  }
  return false;
}

bool DataTransfer::HasStringOfType(const String& type) const {
  if (!CanReadTypes())
    return false;

  return data_object_->Types().Contains(type);
}

void DataTransfer::Trace(Visitor* visitor) const {
  visitor->Trace(data_object_);
  visitor->Trace(drag_image_);
  visitor->Trace(drag_image_element_);
  visitor->Trace(files_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
