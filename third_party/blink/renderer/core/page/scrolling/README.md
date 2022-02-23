# Scrolling

The `renderer/core/page/scrolling` directory contains utilities and classes for
scrolling that don't belong anywhere else. For example, the majority of
root scroller implementation as well as overscroll and some scroll customization
types live here.

## _rootScroller_

### Implementation Details

There are two notions of _rootScroller_: the _effective rootScroller_, and the
_global rootScroller_.

* The _effective rootScroller_ is the `Node` that's currently being used as the
scrolling root in a given frame. Each iframe will have an _effective
rootScroller_ and this must always be a valid `Node`. The default is to use the
document `Node` (hence, why the _effective rootScroller_ is a `Node` rather
than an `Element`). When the ImplicitRootScrollerEnabled setting is enabled,
Blink may choose an element on the page to make the effective root scroller.
See [Implicit Promotion](#Implicit-Promotion).

* The _global rootScroller_ is the one `Node` on a page that's responsible for
root scroller actions such as hiding/showing the URL bar, producing overscroll
glow, etc. It is determined by starting from the root frame and following the
chain of _effective rootScrollers_ through any iframes. That is, if the
_effective rootScroller_ is an iframe, use its _effective rootScroller_
instead.

Each `Document` owns a `RootScrollerController` that manages the default _effective
rootScroller_ and promotion to _effective rootScroller_. The Page owns a
TopDocumentRootScrollerController that manages the lifecycle of the _global
rootScroller_.

Promotion to an _effective rootScroller_ happens as a step in the main document
lifecycle. After all frames have finished layout, we perform a frame tree walk
(in post-order), performing selection of an _effective rootScroller_ in each
frame. Each time an _effective rootScroller_ is changed, the _global
rootScroller_ is recalculated. Because the _global rootScroller_ can affect
compositing, all of this must happen before the compositing stage of the
lifecycle.

Once a Node has been set as the _global rootScroller_, it will scroll as though
it is the viewport. This is done by setting the ViewportScrollCallback as its
applyScroll callback. The _global rootScroller_ will always have the
ViewportScrollCallback set as its applyScroll.

### Composition and iframes

The rootScroller can be composed across documents. For example, suppose we have
the following document structure:

```
+--------------------------------------+
|       Root Document                  |
|    +-----------------------------+   |
|    |     iFrame                  |   |
|    |    +------------------+     |   |
|    |    |  <scrolling div> |     |   |
|    |    +------------------+     |   |
|    +-----------------------------+   |
+--------------------------------------+
```

We want to make the scrolling div be the _global rootScroller_ (scrolling it
should move the URL bar).

Iframes are special when computing rootScrollers, they delegate computation of
the _global rootScroller_ to the _effective rootScroller_ of their content
document. In the above case, if both the iframe and scrollingDiv are valid
_effective rootScrollers_ then the scrolling div will become the _global
rootScroller_.

One complication here has to do with clipping. Since the iframe must be a valid
rootScroller, it must have `width: 100%; height: 100%`.  This means that when
the URL bar is hidden, the iframe wont fill the viewport and so it'll normally
clip the contents of the _global rootScroller_ (`height 100%` is statically
sized to fill the viewport when the URL bar is showing). We compensate for this
by making the iframe geometry match its parent frame when it is the _effective
rootScroller_. We also set the layout size to match the parent's layout size so
that we preserve the URL bar semantics of the root frame WRT layout. i.e.
`100vh` is larger than `height: 100%` by the size of the URL bar.

We also disabling clipping (`SetMasksToBounds(false)`) on any iframes that have
the _global RootScroller_ as a descendant during the compositing stage of the
document lifecycle. This prevents clipping during URL bar movement, before
Blink receives a resize.

### position: fixed

position: fixed Elements are statically positioned relative to any viewport
scrolling. Generally, they'll be positioned relative to the viewport container
layer so they don't move with scrolling.

When the URL bar is shown or hidden, the viewport will resize and so any
bottom-fixed layers will need move to account for the newly exposed or hidden
space (since they're positioned relative to the layer top). i.e. They appear to
stay fixed to the bottom of the device viewport.  However, this is all done
from Blink when a Resize message is received. For URL bar resizes, a resize
message is sent to Blink only once the user has lifted their finger from the
screen. While the user has their finger down and is scrolling, moving the URL
bar, Blink isn't notified of these changes. Since the layers are positioned
relative to their container's origin (the top), if we want position: bottom
layers to appear fixed to the bottom during the scroll we need to adjust their
position dynamically on the compositor.

This adjustment happens in CC in
LayerTreeHostImpl::UpdateViewportContainerSizes. The inner and outer viewports
have their bounds modified by setting a "bounds delta". This delta is non-0
only while the URL bar is being moved, before the Resize is sent to Blink, and
accounts for changes that Blink doesn't know about yet.  Once Blink gets the
resize, the layers' true bounds are updated and the delta is cleared.  The
bounds\_delta is used by the TransformTree to create a transform  node that
will translate bottom-fixed layers to compensate for the URL bar resize before
Blink has a chance to resize and reposition them.

For a non-defualt _effective rootScroller_, we want position: fixed layers to
behave the same way.  When we set a _global rootScroller_, Blink sets it as
CC's _outer viewport_. This means that we get the container resize and
position: fixed behaviors for free. However, we also want position: fixed
Elements in parent frames to also behave as if they're fixed to the viewport.
For example, a page may want to embed an iframe with some content that's
semantically the rootScroller.  However, it may want to add a position: fixed
banner at the bottom of the page. It can't insert it into the iframe because it
may be cross-origin but we still want it to stay fixed to the bottom of the
viewport, not the iframe. To solve this, all rootScroller's ancestor frames are
marked with a special bit, `is_resized_by_browser_controls`. The transform
tree construction relative to the outer viewport's `bounds_delta`x mentioned
above will apply to any layer marked with this bit. This makes position: fixed
layers for all rootScroller ancestors get translated by the URL bar delta.

### Implicit Promotion

When the ImplicitRootScrollerEnabled setting is enabled, Blink may choose an
element on the page to make the effective root scroller, without input from
the page itself. This attempts to provide a better experience on existing pages
that use an iframe or div to scroll the content of the page.

#### How it works

 - When a PaintLayerScrollableArea becomes scrollable - that is, it has
   overflow: auto or scroll and actuallly has overflowing content - it adds
   itself to the RootScrollerController's implicit candidate list.

 - At root scroller evaluation time, we look at each candidate. Those that are
   no longer valid candidates (i.e. no longer scroll overflow) are removed from
   the list.

 - From the remaining entries, we look only at candidates that meet some
   additional criteria, specified by IsValidImplicit().

 - If more than one candidate is a valid implicit, we promote the one with the
   higher z-index. If there's a tie, we don't promote any elements implicitly.


## ViewportScrollCallback

The ViewportScrollCallback is a Scroll Customization apply-scroll callback
that's used to affect viewport actions. This object packages together all the
things that make scrolling the viewport different from normal elements. Namely,
it applies top control movement, scrolling, followed by overscroll effects.

The _global rootScroller_ on a page is responsible for setting the
ViewportScrollCallback on the appropriate root scrolling `Element` on a page.
