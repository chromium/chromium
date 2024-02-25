# What is content/browser/renderer_host?

This directory contains code that can be loosely categorized as "handling the
renderer," covering a wide range of topics (navigation, compositing, input,
etc). Many of the classes represent a browser-side version of a renderer
concept (e.g., RenderFrameHostImpl is the browser-side equivalent of
RenderFrameImpl). Refer to the class-level comments on how each class relates
to or interacts with the renderer.

Note that many of the key classes here are defined in `content/public` and
exposed to `//content` embedders, with implementations living in
`content/browser/renderer_host`.

## Rough Categories

A non-exhaustive list of rough categories and descriptions for the code within
`renderer_host` is below. When adding something that falls into the
miscellaneous category, consider if it belongs in a separate directory, either
under `content/browser/` or under `content/browser/renderer_host`.

### Browser-side representation of documents and frame-tree-related renderer-side objects
Allows the browser-side code to represent document and frame-tree related
concepts, and communicate with the renderer-side.

Some important classes include:
-   **FrameTree** and **FrameTreeNode**: Represents the frames in the frame tree
of a page.
-   **RenderFrameHost**: Roughly represents a document within a frame, although
it does not (yet) change for every new document.
-   **RenderFrameProxyHost**: A placeholder for a frame in other
SiteInstanceGroups and renderer processes.
-   **RenderViewHost**: Represents a page within a given SiteInstanceGroup.
-   **RenderWidgetHost**: A group of contiguous same-SiteInstanceGroup frames
that can paint or handle input events as a single unit.

For diagrams and explanations of how those classes fit with each other, see also
[this documentation](https://www.chromium.org/developers/design-documents/oop-iframes/)
and [docs/frame_trees.md](https://chromium.googlesource.com/chromium/src/+/main/docs/frame_trees.md).

### Connections between browser and renderer/GPU processes, process-related code
Represents child processes (e.g., renderers, GPU, etc) and their connection to
the browser process.

An important class in this category is **RenderProcessHost**, which represents
the browser side of the browser <-> renderer communication channel. There will
be one RenderProcessHost per renderer process.

### Navigation
Navigation handling code, coordinating the browser & renderer from navigation
start to finish. Also keeps track of the session history entries created by the
committed navigations.

Some important classes include:
- **NavigationRequest**: Represents a navigation attempt, and tracks information
related to it.
- **NavigationController**: Manages the joint session history for a frame tree.
- **NavigationEntry** and **FrameNavigationEntry**: Represents joint session
history items (for pages), made up of a tree of session history items (for
frames).

See also [docs/navigation.md](https://chromium.googlesource.com/chromium/src/+/main/docs/navigation.md)
and [docs/session_history.md](https://chromium.googlesource.com/chromium/src/+/main/docs/session_history.md).

### Compositing, input, display
Coordinates handling of input, display, and compositing between the browser,
renderer, and GPU processes.

Some important classes include:
- **RenderWidgetHostView\***: The browser owned object that mediates the
blink::VisualProperties to be used by an embedded Renderer.
- **DelegatedFrameHost**: Used by RenderWidgetHostView to control which
viz::Surface of an embedded Renderer the GPU process will display
EmbeddedFrameSinkImpl: The browser owned object that mediates between an
embedded Renderer and the GPU process. Allowing for the creation of
Renderer-GPU Mojo connections.
- **viz::HostFrameSinkManager**: The browser owned object, accessed via
GetHostFrameSinkManager, that controls the Browser-GPU Mojo connection. Used to
establish future connections for Renderers, as well as to control what
viz::Surfaces to display.

### Misc features that heavily interact with the renderer
Examples: loading/networking, file/storage, plugins, UI, fonts, media,
accessibility.

## Layering restriction with WebContents
Code in this directory can't call up to the WebContents "layer," except through
delegate interfaces (e.g. RenderFrameHostDelegate). This is to separate out
code that deals with the renderer process and code that deals with the tab.
This is enforced by the [DEPS](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/DEPS).
