# APC Geometry Computation

This document records how the AI Page Content (APC) agent derives geometry for
each node today. The implementation lives in
`third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.cc`.

## Geometry Outputs

- **`outer_bounding_box`** (viewport space)
  Computed from `LayoutObject::AbsoluteBoundingBoxRect()` using viewport
  mapping flags. When a local `clip-path` is present, the clip-path bounds are
  mapped to viewport space and replace the standard box outright.

- **`visible_bounding_box`** (viewport space)
  - This uses LocalBoundingBoxRectForAccessibility().

- **`fragment_visible_bounding_boxes`** (viewport space)
  Derived from the layout fragments produced by
  `LayoutObject::QuadsInAncestor()`. Each quad is mapped to the viewport,
  intersected with `visible_bounding_box`, and empty results are dropped.
  We only emit the vector when more than one non-empty fragment survives, so
  single-fragment cases do not duplicate the primary box.

All rectangles are integer enclosures of the underlying floating-point quads.

## Validation

In debug builds `ValidateBoundingBoxes()` asserts a few invariants:

- `visible_bounding_box` must be expressed in viewport coordinates with a
  non-negative origin.
- `visible_bounding_box` must lie within `outer_bounding_box` (within a small
  tolerance) because both derive from the same fragment set.
- When a `clip-path` exists, `visible_bounding_box` must also be contained
  inside the clip-path’s viewport-space bounds.

These checks help catch regressions where upstream mapping or clipping becomes
inconsistent.
