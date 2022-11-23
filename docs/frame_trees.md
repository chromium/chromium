# Demystifying FrameTree Concepts

## What are Frame Trees?

There are two representations of FrameTrees used in rendering Web Pages.
- Blink's [FrameTrees](../third_party/blink/renderer/core/page/frame_tree.h)
- Content's [FrameTrees](../content/browser/renderer_host/frame_tree.h)

These concepts are very similar, however on the content side a placeholder
[FrameTreeNode](../content/browser/renderer_host/frame_tree_node.h) can
be placed in the FrameTree to hold another frame tree. This `FrameTreeNode`'s
current RenderFrameHost will have a valid
`RenderFrameHostImpl::inner_tree_main_frame_tree_node_id` frame tree node
ID.

The renderer side (Blink) will have no notion of this placeholder in the
frame tree and its frame tree appears as it would for the web exposed
[window.frames](https://developer.mozilla.org/en-US/docs/Web/API/Window/frames)

## Why do we nest frame trees?

Certain features that nest documents require a stronger boundary than what would
be achievable with iframes. We want to prevent the exposure of information
across this boundary. This may be for privacy reasons where we enforce
communication restrictions between documents on the web (e.g. fenced frames).
The boundary could also be between content on the web and parts of the user
agent implemented with web technologies (e.g. chrome's PDF viewer, webview tag).

## What are Outermost Main Frames?

Building on the concept above that a `FrameTree` can have an embedded
`FrameTree` (and many nesting levels of them), there is the concept of
the `OutermostMainFrame`. The OutermostMainFrame is the main frame (root)
of a FrameTree that is not embedded in other FrameTrees.
[See footnote 1.](#footnote_1)

So that does mean there can be __multiple main frames__ in a displayed
tab to the user. For features like `fencedframes` the inner `FrameTree`
has a main frame but it will not be an `OutermostMainFrame`.

To determine whether something is a main frame `RenderFrameHost::GetParent`
is typically used. Likewise there is a `RenderFrameHost::GetParentOrOuterDocument` to determine if something is an `OutermostMainFrame`.

```
Example Frame Tree:
    A
     B (iframe)
     C (fenced frame - placeholder frame) [See footnote 2.]
      C* (main frame in fenced frame).

    C* GetParent returns null.
    C* GetParentOrOuterDocument returns A.
    C GetParent & GetParentOrOuterDocument returns A.
    B GetParent & GetParentOrOuterDocument returns A.
    A GetParent & GetParentOrOuterDocument returns nullptr.
```

## Can I have multiple outermost main frames?

Prerender and back/forward cache are features where there can be
other outermost main frame present in a `WebContents`.

## What are Pages?

Pages can be an overloaded term so we will clarify what we mean by the
class concepts:
- Blink's [Page](../third_party/blink/renderer/core/page/page.h)
- Content's [Page](../content/public/browser/page.h)

The two usages are very similar, they effectively are an object representing
the state of a `FrameTree`. Since frames can be hosted in different renderers
(for isolation) there may be a number of Blink `Page` objects, one for each
renderer that participates in the rendering of a single `Page` in content.

## What is the Primary Page?

There is only ever one Primary Page for a given `WebContents`. The primary
page is defined by the fact that the main frame is the `OutermostMainFrame`
and being actively displayed in the tab.

The primary page can change over time (see
`WebContentsObserver::PrimaryPageChanged`). The primary page can change when
navigating, a `Page` is restored from the `BackForwardCache` or from the
prendering pages.

## Relationships between core classes in content/

A WebContents represents a tab. A WebContents owns a FrameTree, the "primary
frame tree," for what is being displayed in the tab. A WebContents may
indirectly own additional FrameTrees for features such as prerendering.

A FrameTree consists of FrameTreeNodes. A FrameTreeNode contains a
RenderFrameHost. FrameTreeNodes reflect the frame structure in the renderer.
RenderFrameHosts represent documents loaded in a frame (roughly,
[see footnote 3](#footnote_3)). As a frame navigates its RenderFrameHost may
change, but its FrameTreeNode stays the same.

In the case of nested frame trees, the RenderFrameHost corresponding to the
hosting document owns the inner FrameTree (possibly through an intermediate
object, as is the case for content::FencedFrame).

## "MPArch"

"MPArch," short for Multiple Page Architecture, refers to the name of the
project that introduced the capability of having multiple FrameTrees in a
single WebContents.

You may also see comments which describe features relying on multiple FrameTrees
in terms of MPArch (e.g. "ignore navigations from MPArch pages"). These are in
reference to "non-primary" frame trees as described above.

See the original [design doc](https://docs.google.com/document/d/1NginQ8k0w3znuwTiJ5qjYmBKgZDekvEPC22q0I4swxQ/edit?usp=sharing)
for further info.

## Footnotes

<a name="footnote_1"></a>1: GuestViews (embedding of a WebContents inside another WebContents) are
considered embedded FrameTrees as well. However for consideration of
OutermostMainFrames (ie. GetParentOrOuterDocument, Primary page) they do not
escape the WebContents boundary because of the logical embedding boundary.

<a name="footnote_2"></a>2: The placeholder RenderFrameHost is generally not exposed outside
of the content boundary. Iteration APIs such as ForEachRenderFrameHost
do not visit this node.

<a name="footnote_3"></a>3: RenderFrameHost is not 1:1 with a document in the renderer.
See [RenderDocument](/docs/render_document.md).
