# Explanation of `old_paint_offset` adjustment for
`LayoutShiftTracker::NotifyBoxPrePaint()`

Suppose from the layout shift root (see
[PaintPropertyTreeBuilder](../paint/paint_property_tree_builder.h) for the
definition) to a LayoutObject, there are transform nodes:
```
  {Troot, T1, T2, ... Tn}
```
where T1, T2, ... Tn are all 2d translations including
* `PaintOffsetTranslation`s
* `Transform`s with 2d translation matrixes
* `ScrollTranslation`

The location of the LayoutObject in the layout shift root can be calculated
from:
* the paint offset,
* `acc_2d = sum(2d-offset(Ti), i=1..n)`

We can calculate the old location and the new location in the layout shift
root:
```
  old_location = old_paint_offset + old_acc_2d
  new_location = new_paint_offset + new_acc_2d
```

`LayoutShiftTracker` then could use `old_location`, `new_location` to check
if the object has shifted within the layout shift root. Then it could map
`old_location` and `new_location` from the layout shift root's property tree
state to the viewport's property tree state to get the old and new location
in viewport, then check if the object has shifted in viewport. This would
need `PaintInvalidator` to pass the following parameters to
`LayoutShiftTracker`:
* `old_location`
* `new_location`
* property tree state of the layout shift root [^1]

[^1] Changes of paint properties above the layout shift root are ignored
     intentionally because
     * It's hard to track such changes.
     * Some of such changes (e.g. 3d transform) should be ignored according to
       the spec.
     * Layout shift of the root itself should have already been reported, so
       the descendants just need to report their shift relative to the layout
       shift root.

However, the above set of parameters requires `PaintInvalidator` to track
all of them. To reduce the amount of data to track, `PaintInvalidator` passes
the following parameters instead [2]:
* `adjusted_old_paint_offset = old_paint_offset - acc_2d_delta`
* `new_paint_offset`
* property tree state of the current object
most of which can be gotten from the current context except `acc_2d_delta`.
Then `LayoutShiftTracker` can use `adjusted_old_paint_offset` and
`new_paint_offset` instead of `old_location` and `new_location` to check if
the object has shifted within the layout shift root because
```
  new_location - old_location == new_paint_offset - adjusted_old_paint_offset
```
and it can map `adjusted_old_paint_offset` and `new_paint_offset` from the
current paint property tree state of the object to the viewport, as if there
were old paint property tree state below the layout shift root because the
changes of paint properties below the layout shift root is accounted in
`adjusted_old_paint_offset`.

[^2] Actually `PaintInvalidator` also passes the following parameters:
     * `translation_delta`
     * `scroll_delta`
     so that `LayoutShiftTracker` can check shift by ignoring (or not) 2d
     translation and scroll changes below the layout shift root.
     See [the explainer](https://github.com/WICG/layout-instability#transform-changes)
     for why we ignore transform and scroll changes by default. However,
     in case that a layout shift is countered by a transform change and/or a
     scroll change making the element not visually move, we should ignore the
     shift. These situations require `LayoutShiftTracker` to determine shift
     by both including and not including the transform/scroll changes.
