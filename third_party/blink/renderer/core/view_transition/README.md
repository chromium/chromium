# View Transitions

This directory contains the script interface and implementation of the View
Transition, and View Transition APIs.

A View Transition is a type of an animated transition that allows content to
animate to a new DOM state easily. For instance, modifying the DOM to change the
background color is a change that can easily be done without view
transitions. However, view transition also allows the new background state
to, for example, slide in from the left instead of simply atomically appearing
on top of the content.

For a detailed explanation, please see the
[explainer](https://github.com/WICG/view-transitions/blob/main/README.md)

The specification governing this feature is located in the following places:
* [Level 1](https://drafts.csswg.org/css-view-transitions-1/)
* [Level 2](https://drafts.csswg.org/css-view-transitions-2/)

## Code Structure

A new method is exposed on window.document, called startViewTransition(). This is
the main interface to getting a new transition object from JavaScript. It is
specified in the `view_transition_supplement.idl` and is implemented in
corresponding `.cc` and `.h` files.

When called, `startViewTransition()` constructs a ViewTransition object which
is specified in `view_transition.idl` and is implemented in corresponding
`.cc` and `.h` files.

The rest of the script interactions happen with this object.

## Pseudo Element Tree

During the transition, the browser creates a sparse post layout representation
including content from nodes in the old and new DOM. This representation is
rendered using the following steps :

- The browser executes the Document lifecycle phases (until paint) to generate
  the state required to render a DOM element as an image (bounding box size,
  transform mapping the box to viewport space and relative paint order between
  elements). This state is tracked for a subset of elements called "view
  transition elements" (or just "transition elements" when the context is clear)
  which should be animated independently during a transition.

- A tree of pseudo elements is generated to render the transition elements using
  this state. The pseudo element tree is styled after a style recalc pass is
  executed on the author DOM during a Document lifecycle update.

``` text
html
|_ ::view-transition
   |_ ::view-transition-group(foo)
   |  |_ ::view-transition-image-pair(foo)
   |     |_ ::view-transition-old(foo)
   |     |_ ::view-transition-new(foo)
   |
   |_ ::view-transition-group(bar)
   |  |_ ::view-transition-image-pair(bar)
   |     |_ ::view-transition-old(bar)
   |     |_ ::view-transition-new(bar)
```

The ::view-transition pseudo element is the root of this pseudo element tree. This
provides a shared stacking context for painting pseudo elements corresponding to
a transition element.

Each transition element is rendered using the following new pseudo elements :

- ::view-transition-group generates a box which maps to the transition element's
quad in author DOM.
- ::view-transition-image-pair is a box that contains two images representing
the contents of the old and new transition element. It is isolated to allow for
mix-blend-modes that don't interact with outside content.
- ::view-transition-old is a replaced element displaying the transition
element's cached content from the old DOM.
- ::view-transition-new is a replaced element displaying the
transition element's live content from the new DOM.

Each transition element is tagged with a developer provided string which can be
used as a custom ident to uniquely identify and target the corresponding
generated pseudo elements in UA and developer stylesheets. This string is
tracked on the PseudoElement class.

### Pseudo-element traversal

Pseudo elements are not considered part of tree structure of the ordinary DOM
tree and thus have no sibling or child pointers like ordinary nodes. However, an
ordering is defined via special "PseudoAware" methods for child and sibling
operations.

Within the ::view-transition subtree, view-transition-group siblings are ordered
based on ordering of the view-transition-name, which is sorted by the paint
order of the elements they represent, see
[ViewTransitionStyleTracker::AddTransitionElementsFromCSSRecursive](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/view_transition/view_transition_style_tracker.cc;l=559;drc=7172fffc3c545134d5c88af8ab07b04fcb1d628e).
::view-transition-old always precedes ::view-transition-new.

In terms of ordering, only ::view-transition is relevant in relation to non-VT
elements and pseudos, since VT-pseudos can only appear within ::view-transition and
no other elements can. ::view-transition is placed after ::after:

```
  Element
    ::marker
    ::before
    Ordinary Elements
    ::after
    ::view-transition
```

## ViewTransitionElementResourceId
ViewTransitionElementResourceId is an identifier used to tag the rendered output (called
a snapshot) of transition elements and the root stacking context generated by the
compositor. The snapshot provides the content for the ::view-transition-old
and ::view-transition-new elements referenced above.

The snapshot can be in 2 states :

* Live : This refers to a render pass generated in the current frame. As such
the snapshot stays in sync with associated DOM element.
* Cached : This refers to a cached copy of a render pass saved in the Viz
process.

We generate 2 set of ViewTransitionElementResourceIds for snapshots of elements
in the old and new DOM as follows :

* At the prepare phase (before the DOM is updated to the new state),
old_snapshot_id tags elements in the old DOM. This ID refers to a live snapshot
and the Viz process executes an async operation to save a cached version keyed
using the same ID. This ID provides the content for ::view-transition-old
pseudo elements.

* Once the cached version has been saved, the developer can update the DOM to
the new state called the start phase. At this point new_snapshot_id is created
to tag elements in the new DOM. This ID always refers to a live snapshot and
provides the content for ::view-transition-new pseudo elements. The
old_snapshot_id now refers to the cached version displayed by
::view-transition-old pseudo elements.

## Viewport Sizes

See [SET and Viewports](https://docs.google.com/document/d/1UAxgN6fWDiUUsSlBOksxn3UEQ7GStjMbW8LT-UPvTdQ/edit?usp=sharing)
for some of the design discussion and more details.

A transition can take place between states with different viewport sizes. For
example, the mobile URL bar is forced to show when a navigation occurs; in a
situation like this the snapshot will be taken with the URL bar hidden but
transition to a document shrunken by the URL bar. Other examples of UI widgets
like this are the virtual-keyboard and root scrollbars.

Viewport widgets inset the "fixed viewport" so to avoid a moving coordinate
space we introduce the concept of a "snapshot root" rect. The snapshot root is
invariant with respect to whether viewport widgets are shown or not, neither
its position nor its size change. When all widgets are hidden, it is equal to
the fixed viewport.

``` text
┌──────────────────────┐                              ┌────────────────────┐
│┼────────────────────┼│                              │                    │
││    URL BAR         ││                              │                    │
├┴────────────────────┴┤   ┌───────────────────────┐  │                    │
│                      │   │                       │  │                    │
│                      │   │                       │  │                    │
│   <PAGE CONTENT>     │   │    Fixed Viewport     │  │                    │
│                      │   │                       │  │  Snapshot Root     │
│                      │   │                       │  │                    │
│                      │   │                       │  │                    │
├┬────────────────────┬┤   └───────────────────────┘  │                    │
││                    ││                              │                    │
││  Virtual Keyboard  ││                              │                    │
││                    ││                              │                    │
│┼────────────────────┼│                              │                    │
└──────────────────────┘                              └────────────────────┘
```
_The snapshot root and fixed viewport when the mobile URL bar and virtual
keyboard are shown._

The root ::view-transition pseudo is shifted up and left so that its origin is
at the snapshot root origin. This is a no-op if viewport widgets aren't showing.
If the widgets are showing this causes ::view-transition to be positioned at a
negative offset relative to the fixed viewport (i.e. it's origin is underneath
the mobile URL bar). This creates a stable coordinate space during the
transition. Transition element's viewport transforms account for this and are
computed relative to the snapshot root origin.

The root snapshot is sized to the snapshot root's size, which may be larger
than the current fixed viewport. Painting is offset within the snapshot so that
page content is rendered at the correct location (i.e. the snapshot will paint
the background color in the region overlaid by the URL bar).

