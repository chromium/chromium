# Accessibility Overview

Accessibility means ensuring that all users, including users with disabilities,
have equal access to software. One piece of this involves basic design
principles such as using appropriate font sizes and color contrast,
avoiding using color to convey important information, and providing keyboard
alternatives for anything that is normally accomplished with a pointing device.
However, when you see the word "accessibility" in a directory name in Chromium,
that code's purpose is to provide full access to Chromium's UI via external
accessibility APIs that are utilized by assistive technology.

**Assistive technology** here refers to software or hardware which
makes use of these APIs to create an alternative interface for the user to
accommodate some specific needs, for example:

Assistive technology includes:

* Screen readers for blind users that describe the screen using
  synthesized speech or braille
* Voice control applications that let you speak to the computer,
* Switch Access that lets you control the computer with a small number
  of physical switches,
* Magnifiers that magnify a portion of the screen, and often highlight the
  cursor and caret for easier viewing, and
* Assistive learning and literacy software that helps users who have a hard
  time reading print, by highlighting and/or speaking selected text

In addition, because accessibility APIs provide a convenient and universal
way to explore and control applications, they're often used for automated
testing scripts, and UI automation software like password managers.

Web browsers play an important role in this ecosystem because they need
to not only provide access to their own UI, but also provide access to
all of the content of the web.

Each operating system has its own native accessibility API. While the
core APIs tend to be well-documented, it's unfortunately common for
screen readers in particular to depend on additional undocumented or
vendor-specific APIs in order to fully function, especially with web
browsers, because the standard APIs are insufficient to handle the
complexity of the web.

Chromium needs to support all of these operating system and
vendor-specific accessibility APIs in order to be usable with the full
ecosystem of assistive technology on all platforms. Just like Chromium
sometimes mimics the quirks and bugs of older browsers, Chromium often
needs to mimic the quirks and bugs of other browsers' implementation
of accessibility APIs, too.

## Concepts

While each operating system and vendor accessibility API is different,
there are some concepts all of them share.

1. The *tree*, which models the entire interface as a tree of objects, exposed
   to assistive technology via accessibility APIs;
2. *Events*, which let assistive technology know that a part of the tree has
   changed somehow;
3. *Actions*, which come from assistive technology and ask the interface to
   change.

Consider the following small HTML file:

```
<html>
<head>
  <title>How old are you?</title>
</head>
<body>
  <label for="age">Age</label>
  <input id="age" type="number" name="age" value="42">
  <div>
    <button>Back</button>
    <button>Next</button>
  </div>
</body>
</html>
```

### The Accessibility Tree and Accessibility Attributes

Internally, Chromium represents the accessibility tree for that web page
using a data structure something like this:

```
id=1 role=WebArea name="How old are you?"
    id=2 role=Label name="Age"
    id=3 role=TextField labelledByIds=[2] value="42"
    id=4 role=Group
        id=5 role=Button name="Back"
        id=6 role=Button name="Next"
```

Note that the tree structure closely resembles the structure of the
HTML elements, but slightly simplified. Each node in the accessibility
tree has an ID and a role. Many have a name. The text field has a value,
and instead of a name it has labelledByIds, which indicates that its
accessible name comes from another node in the tree, the label node
with id=2.

On a particular platform, each node in the accessibility tree is implemented
by an object that conforms to a particular protocol.

On Windows, the root node implements the IAccessible protocol and
if you call IAccessible::get_accRole, it returns ROLE_SYSTEM_DOCUMENT,
and if you call IAccessible::get_accName, it returns "How old are you?".
Other methods let you walk the tree.

On macOS, the root node implements the NSAccessibility protocol and
if you call [NSAccessibility accessibilityRole], it returns @"AXWebArea",
and if you call [NSAccessibility accessibilityLabel], it returns
"How old are you?".

The Linux accessibility API, ATK, is more similar to the Windows APIs;
they were developed together. (Chrome's support for desktop Linux
accessibility is unfinished.)

The Android accessibility API is of course based on Java. The main
data structure is AccessibilityNodeInfo. It doesn't have a role, but
if you call AccessibilityNodeInfo.getClassName() on the root node
it returns "android.webkit.WebView", and if you call
AccessibilityNodeInfo.getContentDescription() it returns "How old are you?".

On Chrome OS, we use our own accessibility API that closely maps to
Chrome's internal accessibility API.

So while the details of the interface vary, the underlying concepts are
similar. Both IAccessible and NSAccessibility have a concept of a role,
but IAccessible uses a role of "document" for a web page, while NSAccessibility
uses a role of "web area". Both IAccessible and NSAccessibility have a
concept of the primary accessible text for a node, but IAccessible calls
it the "name" while NSAccessibility calls it the "label", and Android
calls it a "content description".

**Historical note:** The internal names of roles and attributes in
Chrome often tend to most closely match the macOS accessibility API
because Chromium was originally based on WebKit, where most of the
accessibility code was written by Apple. Over time we're slowly
migrating internal names to match what those roles and attributes are
called in web accessibility standards, like ARIA.

### Accessibility Events

In Chromium's internal terminology, an Accessibility Event always represents
communication from the app to the assistive technology, indicating that the
accessibility tree changed in some way.

As an example, if the user were to press the Tab key and the text
field from the example above became focused, Chromium would fire a
"focus" accessibility event that assistive technology could listen
to. A screen reader might then announce the name and current value of
the text field. A magnifier might zoom the screen to its bounding
box. If the user types some text into the text field, Chromium would
fire a "value changed" accessibility event.

As with nodes in the accessibility tree, each platform has a slightly different
API for accessibility events. On Windows we'd fire EVENT_OBJECT_FOCUS for
a focus change, and on Mac we'd fire @"AXFocusedUIElementChanged".
Those are pretty similar. Sometimes they're quite different - to support
live regions (notifications that certain key parts of a web page have changed),
on Mac we simply fire @"AXLiveRegionChanged", but on Windows we need to
fire IA2_EVENT_TEXT_INSERTED and IA2_EVENT_TEXT_REMOVED events individually
on each affected node within the changed region, with additional attributes
like "container-live:polite" to indicate that the affected node was part of
a live region. This discussion is not meant to explain all of the technical
details but just to illustrate that the concepts are similar,
but the details of notifying software on each platform about changes can
vary quite a bit.

### Accessibility Actions

Each native object that implements a platform's native accessibility API
supports a number of actions, which are requests from the assistive
technology to control or change the UI. This is the opposite of events,
which are messages from Chromium to the assistive technology.

For example, if the user had a voice control application running, such as
Voice Access on Android, the user could just speak the name of one of the
buttons on the page, like "Next". Upon recognizing that text and finding
that it matches one of the UI elements on the page, the voice control
app executes the action to click the button id=6 in Chromium's accessibility
tree. Internally we call that action "do default" rather than click, since
it represents the default action for any type of control.

Other examples of actions include setting focus, changing the value of
a control, and scrolling the page.

### Parameterized attributes

In addition to accessibility attributes, events, and actions, native
accessibility APIs often have so-called "parameterized attributes".
The most common example of this is for text - for example there may be
a function to retrieve the bounding box for a range of text, or a
function to retrieve the text properties (font family, font size,
weight, etc.) at a specific character position.

Parameterized attributes are particularly tricky to implement because
of Chromium's multi-process architecture. More on this below.

### Tools for inspecting the Accessibility tree

Developers can inspect the accessibility tree in several ways:

* By navigating to [chrome://accessibility/](chrome://accessibility)
and inspecting a tree directly. Note that you may want to enable the
'Internal' option. Click 'show accessibility tree' for a particular tab,
then click again to refresh that tree.
* Using the [https://developer.chrome.com/extensions/automation](
Automation API).
* Installing the [https://github.com/google/automation-inspector](
Automation Inspector Chrome extension).
* Or by using native tools:

  - Android: UIAutomatorViewer
  - macOS: Accessibility Inspector
  - Windows: Inspect, AViewer, accProbe (and many others)


## Chromium's multi-process architecture

Native accessibility APIs tend to have a *functional* interface, where
Chromium implements an interface for a canonical accessible object that
includes methods to return various attributes, walk the tree, or perform
an action like click(), focus(), or setValue(...).

In contrast, the web has a largely *declarative* interface. The shape
of the accessibility tree is determined by the DOM tree (occasionally
influenced by CSS), and the accessible semantics of a DOM element can
be modified by adding ARIA attributes.

One important complication is that all of these native accessibility APIs
are *synchronous*, while Chromium is multi-process, with the contents of
each web page living in a different process than the process that
implements Chromium's UI and the native accessibility APIs. Furthermore,
the renderer processes are *sandboxed*, so they can't implement
operating system APIs directly.

If you're unfamiliar with Chrome's multi-process architecture, see
[this blog post introducing the concept](
https://blog.chromium.org/2008/09/multi-process-architecture.html) or
[the design doc on chromium.org](
https://www.chromium.org/developers/design-documents/multi-process-architecture)
for an intro.

Chromium's multi-process architecture means that we can't implement
accessibility APIs the same way that a single-process browser can -
namely, by calling directly into the DOM to compute the result of each
API call. For example, on some operating systems there might be an API
to get the bounding box for a particular range of characters on the
page.  In other browsers, this might be implemented by creating a DOM
selection object and asking for its bounding box.

That implementation would be impossible in Chromium because it'd require
blocking the main thread while waiting for a response from the renderer
process that implements that web page's DOM. (Not only is blocking the
main thread strictly disallowed, but the latency of doing this for every
API call makes it prohibitively slow anyway.) Instead, Chromium takes an
approach where a representation of the entire accessibility tree is
cached in the main process. Great care needs to be taken to ensure that
this representation is as concise as possible.

In Chromium, we build a data structure representing all of the
information for a web page's accessibility tree, send the data
structure from the renderer process to the main browser process, cache
it in the main browser process, and implement native accessibility
APIs using solely the information in that cache.

As the accessibility tree changes, tree updates and accessibility events
get sent from the renderer process to the browser process. The browser
cache is updated atomically in the main thread, so whenever an external
client (like assistive technology) calls an accessibility API function,
we're always returning something from a complete and consistent snapshot
of the accessibility tree. From time to time, the cache may lag what's
in the renderer process by a fraction of a second.

Here are some of the specific challenges faced by this approach and
how we've addressed them.

### Sparse data

There are a *lot* of possible accessibility attributes for any given
node in an accessibility tree. For example, there are more than 150
unique accessibility API methods that Chrome implements on the Windows
platform alone. We need to implement all of those APIs, many of which
request rather rare or obscure attributes, but storing all possible
attribute values in a single struct would be quite wasteful.

To avoid each accessible node object containing hundreds of fields the
data for each accessibility node is stored in a relatively compact
data structure, ui::AXNodeData. Every AXNodeData has an integer ID, a
role enum, and a couple of other mandatory fields, but everything else
is stored in attribute arrays, one for each major data type.

```
struct AXNodeData {
  int32_t id;
  ax::mojom::Role role;
  ...
  std::vector<std::pair<ax::mojom::StringAttribute, std::string>> string_attributes;
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>> int_attributes;
  ...
}
```

So if a text field has a placeholder attribute, we can store
that by adding an entry to `string_attributes` with an attribute
of ax::mojom::StringAttribute::kPlaceholder and the placeholder string as the value.

### Incremental tree updates

Web pages change frequently. It'd be terribly inefficient to send a
new copy of the accessibility tree every time any part of it changes.
However, the accessibility tree can change shape in complicated ways -
for example, whole subtrees can be reparented dynamically.

Rather than writing code to deal with every possible way the
accessibility tree could be modified, Chromium has a general-purpose
tree serializer class that's designed to send small incremental
updates of a tree from one process to another. The tree serializer has
just a few requirements:

* Every node in the tree must have a unique integer ID.
* The tree must be acyclic.
* The tree serializer must be notified when a node's data changes.
* The tree serializer must be notified when the list of child IDs of a
  node changes.

The tree serializer doesn't know anything about accessibility attributes.
It keeps track of the previous state of the tree, and every time the tree
structure changes (based on notifications of a node changing or a node's
children changing), it walks the tree and builds up an incremental tree
update that serializes as few nodes as possible.

In the other process, the Unserialization code applies the incremental
tree update atomically.

### Text bounding boxes

One challenge faced by Chromium is that accessibility clients want to be
able to query the bounding box of an arbitrary range of text - not necessarily
just the current cursor position or selection. As discussed above, it's
not possible to block Chromium's main browser process while waiting for this
information from Blink, so instead we cache enough information to satisfy these
queries in the accessibility tree.

To compactly store the bounding box of every character on the page, we
split the text into *inline text boxes*, sometimes called *text runs*.
For example, in a typical paragraph, each line of text would be its own
inline text box. In general, an inline text box or text run contians a
sequence of text characters that are all oriented in the same direction,
in a line, with the same font, size, and style.

Each inline text box stores its own bounding box, and then the relative
x-coordinate of each character in its text (assuming left-to-right).
From that it's possible to compute the bounding box
of any individual character.

The inline text boxes are part of Chromium's internal accessibility tree.
They're used purely internally and aren't ever exposed directly via any
native accessibility APIs.

For example, suppose that a document contains a text field with the text
"Hello world", but the field is narrow, so "Hello" is on the first line and
"World" is on the second line. Internally Chromium's accessibility tree
might look like this:

```
staticText location=(8, 8) size=(38, 36) name='Hello world'
    inlineTextBox location=(0, 0) size=(36, 18) name='Hello ' characterOffsets=12,19,23,28,36
    inlineTextBox location=(0, 18) size=(38, 18) name='world' characterOffsets=12,20,25,29,37
```

### Scrolling, transformations, and animation

Native accessibility APIs typically want the bounding box of every element in the
tree, either in window coordinates or global screen coordinates. If we
stored the global screen coordinates for every node, we'd be constantly
re-serializing the whole tree every time the user scrolls or drags the
window.

Instead, we store the bounding box of each node in the accessibility tree
relative to its *offset container*, which can be any ancestor. If no offset
container is specified, it's assumed to be the root of the tree.

In addition, any offset container can contain scroll offsets, which can be
used to scroll the bounding boxes of anything in that subtree.

Finally, any offset container can also include an arbitrary 4x4 transformation
matrix, which can be used to represent arbitrary 3-D rotations, translations, and
scaling, and more. The transformation matrix applies to the whole subtree.

Storing coordinates this way means that any time an object scrolls, moves, or
animates its position and scale, only the root of the scrolling or animation
needs to post updates to the accessibility tree. Everything in the subtree
remains valid relative to that offset container.

Computing the global screen coordinates for an object in the accessibility
tree just means walking up its ancestor chain and applying offsets and
occasionally multiplying by a 4x4 matrix.

### Site isolation / out-of-process iframes

At one point in time, all of the content of a single Tab or other web view
was contained in the same Blink process, and it was possible to serialize
the accessibility tree for a whole frame tree in a single pass.

Today the situation is a bit more complicated, as Chromium supports
out-of-process iframes. (It also supports "browser plugins" such as
the `<webview>` tag in Chrome packaged apps, which embeds a whole
browser inside a browser, but for the purposes of accessibility this
is handled the same as frames.)

Rather than a mix of in-process and out-of-process frames that are handled
differently, Chromium builds a separate independent accessibility tree
for each frame. Each frame gets its own tree ID, and it keeps track of
the tree ID of its parent frame (if any) and any child frames.

In Chrome's main browser process, the accessibility trees for each frame
are cached separately, and when an accessibility client (assistive
technology) walks the accessibility tree, Chromium dynamically composes
all of the frames into a single virtual accessibility tree on the fly,
using those aforementioned tree IDs.

The node IDs for accessibility trees only need to be unique within a
single frame. Where necessary, separate unique IDs are used within
Chrome's main browser process. In Chromium accessibility, a "node ID"
always means that ID that's only unique within a frame, and a "unique ID"
means an ID that's globally unique.

## Blink

Blink constructs an accessibility tree (a hierarchy of [WebAXObject]s) from the
page it is rendering. WebAXObject is the public API wrapper around [AXObject],
which is the core class of Blink's accessibility tree. AXObject is an abstract
class; the most commonly used concrete subclass of it is [AXNodeObject], which
wraps a [Node]. In turn, most AXNodeObjects are actually [AXLayoutObject]s,
which wrap both a [Node] and a [LayoutObject]. Access to the LayoutObject is
important because some elements are only in the AXObject tree depending on their
visibility, geometry, linewrapping, and so on. There are some subclasses of
AXLayoutObject that implement special-case logic for specific types of Node.
There are also other subclasses of AXObject, which are mostly used for testing.

Note that not all AXLayoutObjects correspond to actual Nodes; some are synthetic
layout objects which group related inline elements or similar.

The central class responsible for dealing with accessibility events in Blink is
[AXObjectCacheImpl], which is responsible for caching the corresponding
AXObjects for Nodes or LayoutObjects. This class has many methods named
`handleFoo`, which are called throughout Blink to notify the AXObjectCacheImpl
that it may need to update its tree. Since this class is already aware of all
accessibility events in Blink, it is also responsible for relaying accessibility
events from Blink to the embedding content layer.

## The content layer

The content layer lives on both sides of the renderer/browser split. The content
layer translates WebAXObjects into [AXContentNodeData], which is a subclass of
[ui::AXNodeData]. The ui::AXNodeData class and related classes are Chromium's
cross-platform accessibility tree. The translation is implemented in
[BlinkAXTreeSource]. This translation happens on the renderer side, so the
ui::AXNodeData tree now needs to be sent to the browser, which is done by
sending [AccessibilityHostMsg_EventParams] with the payload being serialized
delta-updates to the tree, so that changes that happen on the renderer side can
be reflected on the browser side.

On the browser side, these IPCs are received by [RenderFrameHostImpl], and then
usually forwarded to [BrowserAccessibilityManager] which is responsible for:

1. Merging AXNodeData trees into one tree of [BrowserAccessibility] objects,
   by linking to other BrowserAccessibilityManagers. This is important because
   each page has its own accessibility tree, but each Chromium *window* must
   have only one accessibility tree, so trees from multiple pages need to be
   combined (possibly also with trees from Views UI).
2. Dispatching outgoing accessibility events to the platform's accessibility
   APIs. This is done in the platform-specific subclasses of
   BrowserAccessibilityManager, in a method named `NotifyAccessibilityEvent`.
3. Dispatching incoming accessibility actions to the appropriate recipient, via
   [BrowserAccessibilityDelegate]. For messages destined for a renderer,
   [RenderFrameHostImpl], which is a BrowserAccessibilityDelegate, is
   responsible for sending appropriate `AccessibilityMsg_Foo` IPCs to the
   renderer, where they will be received by [RenderAccessibilityImpl].

On Chrome OS, RenderFrameHostImpl does not route events to
BrowserAccessibilityManager at all, since there is no platform screenreader
outside Chromium to integrate with.

## Views

Views generates a [ViewAccessibility] for each View, which is used as the
delegate for an [AXPlatformNode] representing that View. This part is relatively
straightforward, but then the generated tree must be combined with the web
accessibility tree, which is handled by BrowserAccessibilityManager.

## WebUI

Since WebUI surfaces have renderer processes as normal, WebUI accessibility goes
through the blink-to-content-to-platform pipeline described above. Accessibility
for WebUI is largely implemented in JavaScript in [webui-js]; these classes take
care of adding ARIA attributes and so on to DOM nodes as needed.

## The Chrome OS layer

The accessibility tree is also exposed via the [chrome.automation API], which
gives extension JavaScript access to the accessibility tree, events, and
actions. This API is implemented in C++ by [AutomationInternalCustomBindings],
which is renderer-side code, and in JavaScript by the [automation API]. The API
is defined by [automation.idl], which must be kept synchronized with
[ax_enums.mojom].

[AccessibilityHostMsg_EventParams]: https://cs.chromium.org/chromium/src/content/common/accessibility_messages.h?sq=package:chromium&l=75
[AutomationInternalCustomBindings]: https://cs.chromium.org/chromium/src/extensions/renderer/api/automation/automation_internal_custom_bindings.h
[AXContentNodeData]: https://cs.chromium.org/chromium/src/content/common/ax_content_node_data.h
[AXLayoutObject]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/accessibility/ax_layout_object.h
[AXNodeObject]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/accessibility/ax_node_object.h
[AXObject]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/accessibility/ax_object.h
[AXObjectImpl]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/accessibility/ax_object_impl.h
[AXObjectCacheImpl]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h
[AXPlatformNode]: https://cs.chromium.org/chromium/src/ui/accessibility/platform/ax_platform_node.h
[AXTreeSerializer]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_tree_serializer.h
[BlinkAXTreeSource]: https://cs.chromium.org/chromium/src/content/renderer/accessibility/blink_ax_tree_source.h
[BrowserAccessibility]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility.h
[BrowserAccessibilityDelegate]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility_manager.h?sq=package:chromium&l=64
[BrowserAccessibilityManager]: https://cs.chromium.org/chromium/src/content/browser/accessibility/browser_accessibility_manager.h
[LayoutObject]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/layout/layout_object.h
[ViewAccessibility]: https://cs.chromium.org/chromium/src/ui/views/accessibility/view_accessibility.h
[Node]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/dom/node.h
[RenderAccessibilityImpl]: https://cs.chromium.org/chromium/src/content/renderer/accessibility/render_accessibility_impl.h
[RenderFrameHostImpl]: https://cs.chromium.org/chromium/src/content/browser/frame_host/render_frame_host_impl.h
[ui::AXNodeData]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_node_data.h
[WebAXObject]: https://cs.chromium.org/chromium/src/third_party/blink/public/web/web_ax_object.h
[automation API]: https://cs.chromium.org/chromium/src/chrome/renderer/resources/extensions/automation
[automation.idl]: https://cs.chromium.org/chromium/src/extensions/common/api/automation.idl
[ax_enums.mojom]: https://cs.chromium.org/chromium/src/ui/accessibility/ax_enums.mojom
[chrome.automation API]: https://developer.chrome.com/extensions/automation
[webui-js]: https://cs.chromium.org/chromium/src/ui/webui/resources/js/cr/ui/
