---
name: blink/content_extraction/maintain-coordinate-space-invariants
description: >-
  Keep extracted geometry in local-root Blink device pixels by mapping clipped
  and unclipped rects through MapToVisualRectInAncestorSpace and converting CSS
  or screen DIPs via FrameWidget::DIPsToBlinkSpace().
rubric: correctness
severity: blocking
---

**Rule.** Maintain a single consistent coordinate space (local-root viewport
Blink device pixels) across clipped boxes, unclipped outer boxes, subframes, and
popups:

1. **Map clipped and unclipped boxes from the same descendant-excluding local
   rect through `MapToVisualRectInAncestorSpace`**: Obtain a node's own local
   rect with `LayoutObject::IncludeDescendants(false)` (or
   `ClipPathClipper::LocalClipPathBoundingBox`) so child layout boxes do not
   inflate the parent's box. Map both `visible_bounding_box`
   (`kVisualRectFlags`) and `outer_bounding_box`
   (`kVisualRectFlags | kSkipAncestorAndViewportClips`) through
   `MapToVisualRectInAncestorSpace` in local-root Blink device pixels rather
   than mixing `LocalToAbsoluteQuad` or `AbsoluteBoundingBoxRect`.
2. **Convert screen DIPs and CSS viewport sizes into local-root Blink pixels**:
   `MapToVisualRectInAncestorSpace` produces coordinates in local-root Blink
   (device) pixels. When combining, offsetting, or clamping against
   `LocalFrameView::ViewportWidth/Height()` (CSS/layout pixels) or
   `WebPagePopup::ViewRect()` (screen/window DIPs), subtract the main frame's
   screen origin in DIPs (`main_frame_view_rect_in_dips.OffsetFromOrigin()`) for
   screen rects and convert via `FrameWidget::DIPsToBlinkSpace()` on
   `frame->GetWidgetForLocalRoot()`.

**Why.** Mixing `LocalToAbsoluteQuad` / `AbsoluteBoundingBoxRect` with
`MapToVisualRectInAncestorSpace` produces inconsistent coordinate spaces and
violates `outer_bounding_box.Contains(visible_bounding_box)` under page scale,
scrollbars, or multicol fragmentation. Mixing screen DIPs or CSS pixels with
Blink device pixels shifts or clips hit targets under non-unit device scale
factors or moved browser windows.

## Examples

Bad:

```cpp
// BUG 1: Mixes LocalToAbsoluteQuad for outer_bounding_box with
// MapToVisualRectInAncestorSpace for visible_bounding_box!
gfx::QuadF outer_quad =
    object.LocalToAbsoluteQuad(gfx::QuadF(local_rect), kMapToViewportFlags);
geometry->outer_bounding_box = gfx::ToEnclosingRect(outer_quad.BoundingBox());

// BUG 2: Clamps a Blink-space visual rect against ViewportWidth/Height (CSS
// pixels) without converting via FrameWidget::DIPsToBlinkSpace()!
gfx::RectF viewport_rect(0, 0, view->ViewportWidth(), view->ViewportHeight());
visual_rect_in_blink_space.Intersect(viewport_rect);
```

Good:

```cpp
// 1. Map both visible and outer boxes through MapToVisualRectInAncestorSpace
// in local-root Blink device pixels:
gfx::RectF unclipped_box = local_bounding_box;
object.MapToVisualRectInAncestorSpace(
    nullptr, unclipped_box,
    base::Union(kVisualRectFlags,
                {VisualRectFlag::kSkipAncestorAndViewportClips}));
geometry->outer_bounding_box = gfx::ToEnclosingRect(unclipped_box);

// 2. Convert viewport dimensions to Blink space via the local-root FrameWidget:
float width_blink = viewport_view->ViewportWidth();
float height_blink = viewport_view->ViewportHeight();
if (FrameWidget* widget = frame->GetWidgetForLocalRoot()) {
  width_blink = widget->DIPsToBlinkSpace(width_blink);
  height_blink = widget->DIPsToBlinkSpace(height_blink);
}
visual_rect_in_blink_space.Intersect(
    gfx::RectF(0, 0, width_blink, height_blink));
```
