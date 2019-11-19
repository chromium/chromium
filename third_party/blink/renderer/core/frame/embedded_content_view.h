// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_EMBEDDED_CONTENT_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_EMBEDDED_CONTENT_VIEW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CullRect;
class LayoutEmbeddedContent;
class LocalFrameView;
class GraphicsContext;

// EmbeddedContentView is a pure virtual class which is implemented by
// LocalFrameView, RemoteFrameView, and WebPluginContainerImpl.
class CORE_EXPORT EmbeddedContentView : public GarbageCollectedMixin {
 public:
  EmbeddedContentView(const IntRect& frame_rect) : frame_rect_(frame_rect) {}
  virtual ~EmbeddedContentView() = default;

  virtual bool IsFrameView() const { return false; }
  virtual bool IsLocalFrameView() const { return false; }
  virtual bool IsPluginView() const { return false; }

  virtual LocalFrameView* ParentFrameView() const = 0;
  virtual LayoutEmbeddedContent* GetLayoutEmbeddedContent() const = 0;
  virtual void AttachToLayout() = 0;
  virtual void DetachFromLayout() = 0;
  virtual void Paint(GraphicsContext&,
                     const GlobalPaintFlags,
                     const CullRect&,
                     const IntSize& paint_offset = IntSize()) const = 0;
  // Called when the size of the view changes.  Implementations of
  // EmbeddedContentView should call LayoutEmbeddedContent::UpdateGeometry in
  // addition to any internal logic.
  virtual void UpdateGeometry() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Dispose() = 0;

  virtual void SetFrameRect(const IntRect&);

  // This method pushes information about our frame rect to consumers.
  // Typically, it will be invoked by FrameRectsChanged; but it can also be
  // called directly to push frame rect information without changing it.
  virtual void PropagateFrameRects() = 0;

  IntRect FrameRect() const { return IntRect(Location(), Size()); }
  IntPoint Location() const;
  int X() const { return Location().X(); }
  int Y() const { return Location().Y(); }
  int Width() const { return Size().Width(); }
  int Height() const { return Size().Height(); }
  IntSize Size() const { return frame_rect_.Size(); }
  void Resize(int width, int height) { Resize(IntSize(width, height)); }
  void Resize(const IntSize& size) {
    SetFrameRect(IntRect(frame_rect_.Location(), size));
  }
  bool IsAttached() const { return is_attached_; }
  // The visibility flags are set for iframes based on style properties of the
  // HTMLFrameOwnerElement in the embedding document.
  bool IsSelfVisible() const { return self_visible_; }
  void SetSelfVisible(bool);
  bool IsParentVisible() const { return parent_visible_; }
  void SetParentVisible(bool);
  bool IsVisible() const { return self_visible_ && parent_visible_; }

 protected:
  // Called when our frame rect changes (or the rect/scroll offset of an
  // ancestor changes).
  virtual void FrameRectsChanged(const IntRect&) { PropagateFrameRects(); }
  virtual void SelfVisibleChanged() {}
  virtual void ParentVisibleChanged() {}
  void SetAttached(bool attached) { is_attached_ = attached; }
  // Location() is in frame coordinates, DocumentLocation() is in document
  // coordinates. For more information:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  IntPoint DocumentLocation() const { return frame_rect_.Location(); }

 private:
  // Note that frame_rect_ is actually in document coordinates, but the
  // FrameRect() and Location() methods convert to frame coordinates.
  IntRect frame_rect_;
  bool self_visible_;
  bool parent_visible_;
  bool is_attached_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_EMBEDDED_CONTENT_VIEW_H_
