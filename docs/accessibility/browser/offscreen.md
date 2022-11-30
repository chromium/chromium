# Offscreen, Invisible and Size

This document explains how Chrome interprets the guidelines to apply the labels
Offscreen and Invisible to nodes, and how the bounding box is calculated.

## Background

In general, screen reading tools may be interested in all nodes regardless of
whether they are presented to sighted users, but other Accessibility tools may
care what is visible to sighted users.

Specifically, tools like Select-to-Speak and Switch Access should not look at
nodes which are “offscreen”, “invisible”, or size=(0,0), as these are not
visible on the screen for mouse interactions. On the other hand, ChromeVox and
other screen readers may care about some of those nodes, which allow developers
to insert buttons visible only to users with a screen reader, or to navigate
below the fold.

## Offscreen
In Chrome, we define Offscreen as:

>Any object is offscreen if it is fully clipped or scrolled out of view by any
of its ancestors so that it is not rendered on the screen.

For example, the staticText node here is offscreen:
```html
<div style="width:0; height; 0; overflow: hidden">
  This text should be marked "offscreen", although its parent is not.
</div>
```

As background, [MSDN](https://msdn.microsoft.com/en-us/library/dd373609(VS.85).aspx)
defines Offscreen as an object is not rendered, but not because it was
programmatically hidden:

>The object is clipped or has scrolled out of view, but it is not
programmatically hidden. If the user makes the viewport larger, more of the
object will be visible on the computer screen.

In Chrome, we interpret this to mean that an object is fully clipped or
scrolled out of view by its parent or ancestors. The main difference is that
we are being explicit that any ancestor clipping a node can make it offscreen,
not just a rootWebArea or scrollable ancestor.

### Technical Implementation
Offscreen is calculated in [AXTree::RelativeToTreeBounds](https://cs.chromium.org/chromium/src/ui/accessibility/ax_tree.cc).
In this function, we walk up the accessibility tree adjusting a node's bounding
box to the frame of its ancestors. If the box is clipped because it lies
outside an ancestor's bounds, and that ancestor clips its children (i.e.
overflow:hidden, overflow:scroll, or it is a rootWebArea), offscreen is set to
true.

## Invisible
A node is marked Invisible in Chrome if it is hidden programmatically. In some
cases, invisible nodes are simply excluded from the accessibility tree. Chrome
defines invisible as:

>Invisible means that a node or its ancestor is explicitly invisible.

This is the same as the definition from [MSDN](https://msdn.microsoft.com/en-us/library/dd373609(VS.85).aspx):

>The object is programmatically hidden.

For example, these nodes are invisible:

```html
<div style="display:none">
  This text should be marked 'invisible', along with its parent div.
</div>

<div style="visibility:hidden">
  This text should also be marked 'invisible' along with its parent div.
</div>
```

### Technical implementation
See `AXObject::IsVisible()` ([source](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/accessibility/ax_object.cc)).

Note: `Opacity: 0` is explicitly not treated as invisible.

## Bounding box calculation
A node's bounding box (location and size) are calculated based on its
intrinsic width, height and location, and the sizes of its ancestors.
We calculate size clipped by ancestors by default, but can also expose an
unclipped size through the [automation API](https://developer.chrome.com/extensions/automation).

The unclipped bounding box is helpful if you want to know the current
coordinates of an element that's scrolled out of view, so you know how
much to scroll to bring it in view.

The clipped bounding box is helpful if you want to draw a visible bounding
box around the element on the screen. Clipping the bounding box helps
sighted users understand what container the element is in, even if it's
not currently visible. Without clipping you end up with elements floating
outside of windows.

### Technical implementation
A node's location and size are calculated in[AXTree::RelativeToTreeBounds](https://cs.chromium.org/chromium/src/ui/accessibility/ax_tree.cc),
and so closely tied to the offscreen calculation. In this function, we walk up
the accessibility tree adjusting a node's bounding box to the frame of its
ancestors.

In general, this calculation is straight forward. But there are several edge
cases:

* If a node has no intrinsic size, its size will be taken from the union of
its children.

```html
    <!-- The outer div here has no size, so we use its child for its bounding box -->
    <div style="visibility:hidden" aria-hidden=false>
      <div style="visibility:visible">
        Visible text
      </div>
    </div>
```

* If a node still has no size after that union, its bounds will be set to the
size of the nearest ancestor which has size. However, this node will be marked
`offscreen`, because it isn't visible to the user.

    * This is useful for exposing nodes to screen reader users.

In addition, [AXTree::RelativeToTreeBounds](https://cs.chromium.org/chromium/src/ui/accessibility/ax_tree.cc)
is used to determine how location and size may be clipped by ancestors,
allowing bounding boxes to reflect the location of a node clipped to its
ancestors.

* If a node is fully clipped by its ancestors such that the intersection of its
bounds and an ancestor's bounds are size 0, it will be pushed to the nearest
edge of the ancestor with width 1 or height 1.

    * We use width and height 1 instead of 0 to distinguish between empty or
    unknown sized nodes vs known small or clipped sized nodes.

Both clipped and unclipped location and size are exposed through the
[Chrome automation API](https://developer.chrome.com/extensions/automation).

* `location` is the global location of a node as clipped by its ancestors. If
a node is fully scrolled off a rootWebArea in X, for example, its location will
be the height of the rootWebArea, and its height will be 1. The Y position and width will be unchanged.

* `unclippedLocation` is the global location of a node ignoring any clipping
by ancestors. If a node is fully scrolled off a rootWebArea in X, for example,
its location will simply be larger than the height of the rootWebArea, and its
size will also be unchanged.
