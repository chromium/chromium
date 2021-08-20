# How Chrome Accessibility Works, Part 2

This document explains the technical details behind Chrome accessibility
code by starting at a high level and progressively adding more levels of
detail.

See [Part 1](how_a11y_works.md) first.

[TOC]

## A multi-process browser

There are different ways that a web browser can be multi-process, so it's
important to first discuss the model Chromium uses.

In Chromium, there's a single browser process. That process is the "main"
process that's launched by the user. It owns all of the windows and UI
elements, and it handles nearly all of the interaction with operating
system APIs. Then there are multiple render processes that handle running
each individual web page. Render processes are sandboxed - this means that
they're basically forbidden from directly talking to the operating system.

Each renderer handles one web page. For now let's forget about iframes,
we'll deal with that added complexity down below. A renderer handles
the entire lifecycle of a page - managing the HTTP connection, parsing
the HTML, resolving CSS styles, executing the JavaScript, and figuring out
what to draw to the screen.

Because the renderer is in a sandboxed process, it doesn't directly get user
input like key presses or mouse clicks, and it doesn't directly draw to the
screen.  These things are all handled by communicating with the browser process.
The browser process owns the window; when there's a user input event like a
mouse click or key press, it forwards that event to the appropriate
renderer. The renderer figures out how to draw the webpage, but it doesn't draw
directly to the screen - because it's sandboxed - it either sends pixels (in
software rendering mode) or sends the drawing commands to Chromium's separate
gpu process.

A simplified diagram of Chromium's multi-process architecture is shown here:

![Multi process system diagram showing a web page renderer sitting inside a
sandboxed render process; it receives user input events from a browser window
in the browser process and sends pixels to that browser window; the browser
window communicates with the operating system.](
figures/multi_process_browser.png)

In most system diagrams we're only going to show a single web page,
because supporting multiple windows and tabs doesn't complicate the
accessibility architecture much. But, it's helpful to understand how
the system looks when the user has multiple windows and tabs open.
Notably:

* The browser process owns all of the windows and the tabs within them.
* Each browser tab maintains a connection to a web page running in a
  render process.
* There can be multiple render processes, but there's no correspondence
  between browser windows, tabs, and render processes. The different
  web page renderers might all be in one process, they might all be in
  different processes, or they might be split across processes in any
  arbitrary way, as shown in this diagram:

![Diagram showing two browser windows with two tabs each, communicating
with four web page renderers that are split across three render processes,
illustrating what the system diagram might look like when there are
multiple windows and tabs open, and showing that there's no correspondence
between a specific tab or window and which process its renderer might live in.](
figures/multi_process_multiple_tabs.png)

You can read more about Chromium's multi-process architecture here - note that
this document is old, so it's more Windows-centric and some of the details
are out of date, but the basic design is still quite similar to today:

https://www.chromium.org/developers/design-documents/multi-process-architecture

### Blink (Rendering Engine).

The majority of the code inside each web renderer is implemented in a module
called [Blink](https://www.chromium.org/blink)
(the Blink Rendering Engine). Historically, when Chromium was first released,
this module was WebKit, but it was forked and renamed Blink in 2013.
As described in [How Blink Works](
https://docs.google.com/document/d/1aitSOucL0VHZa9Z2vbRJSyAIsAz24kX8LFByQ5xQnUg/view),
Blink implements everything that renders content inside a browser tab:

* Implement the specs of the web platform (e.g., HTML standard),
  including DOM, CSS and Web IDL
* Embed V8 and run JavaScript
* Request resources from the underlying network stack
* Build DOM trees
* Calculate style and layout
* Embed Chrome Compositor and draw graphics

There are a few small layers in Chromium's render processes outside of Blink,
containing:

* Handling the multi-process communication
* The renderer side of Chrome browser features (like spell check or autofill)
  that aren't a core part of the web platform

### Other multi-process models

There are other ways that a multi-process web browser could be implemented.
Another possible approach is that each tab could be in its own process,
but each tab still communicates directly with the operating system.
In this model, the operating system can send input events directly to
the active tab, and the tab can paint its own contents directly.

![System diagram showing how some other browsers use multiple processes.
In this diagram, a browser process communicates with two tab processes
that each own both the web rendering but also the UI for their tabs;
these tabs also communicate directly with the operating system.](
figures/other_multi_process_browser.png)

Both Apple Safari and Microsoft Edge Legacy (i.e. Edge before Chromium) use
variations of this multi-process model. They get some of these multi-process
advantages, shared with Chromium:

* Stability: a stuck tab won't hang the whole browser, a crashed tab
  won't crash the whole browser
* Performance: a slow tab won't prevent other tabs from being responsive
* Isolation: a compromised tab won't have access to user data from other tabs

Chromium chose its multi-process model with sandboxed render processes because
it provides much stronger protection against exploits:

* Security: a compromised sandboxed renderer has no access to the operating
  system, so it can't compromise the user's system

Unfortunately, Chromium's architecture makes accessibility more complex.
Accessibility APIs are operating system APIs. In a non-sandboxed
multi-process browser like in the diagram above, each tab can directly
handle its own accessibility. In Chromium, accessibility APIs need to be
handled by the browser process, even though most of the information about
the web page lives in one of the sandboxed render processes.

### A dead-end: proxying accessibility requests

Under Chrome's architecture, operating system accessibility APIs
can only talk to the browser process. In fact, from the point of view
of assistive technology, they don't even know about other processes.
All of the windows are owned by the main browser process, so all of
the accessibility APIs get called in that process.

Let's consider the following scenario: assistive technology has found
a node in the accessibility tree corresponding to a checkbox, and
wants to query it to find out its current state (enabled, checked,
focused, etc.).

When we were first building accessibility support in Chromium, one approach we
tried was for the browser process to have a lightweight tree of proxy objects,
each one corresponding to a node in the accessibility tree in the render
process. Upon receiving a call to getState, the browser process would make a
blocking IPC to the render process, get the state of that particular node, then
reply to the accessibility API call with that result.

![In this diagram, when the operating system wants to call an accessibility
API, it's calling it on a node in the Proxy accessibility tree in the
Browser process. The diagram shows how this node makes a blocking IPC call
to a corresponding node in the accessibility tree in the Render process,
which determines the result by querying the DOM tree in the same process.](
figures/proxy_approach.png)

We discovered two problems with this approach.

First, making blocking IPCs from the browser to renderer was highly
discouraged. It introduced "jankiness" into the browser and introduced
the possibility of deadlock. Unfortunately nearly all accessibility APIs
are synchronous method calls, so there's no easy way around this.

Second, we discovered that some assistive technology was calling many thousands
of accessibility APIs in a row when loading a page. For example, both JAWS
and NVDA scanned the entire web page from top to bottom on first load in order
to build their virtual buffers. This proxy model was slowing things down
dramatically. Even though most calls only took a millisecond to return,
when accessing thousands of nodes sequentially that resulted in even
medium-sized web pages taking 10 seconds or more to load.

In addition, while most calls would be fast, some blocking calls could take
much longer because they'd need to block until not only the render process's
main thread was free, but also until the document was in a clean layout
state (more on layout below under Blink). And if a renderer was hung
(long-running JavaScript or an endless loop), the blocking call might never
return or might be forced to time out.

### Caching the full accessibility tree

Instead of the proxying approach, Chromium caches the full accessibility
tree for every web page in the browser process. When accessibility API
calls come in from the operating system or assistive technology, they're
handled immediately out of the cache, never blocking on a render process.
Separately, renderers send atomic updates to the browser process to keep
the accessibility tree up-to-date.

Here's a diagram of this approach:

![In this diagram, when the operating system calls an accessibility API in
the browser process, the API is satisfied directly from the cached
accessibility tree. Separately, atomic updates flow from the accessibility
tree in the render process to the cache in the browser process.](
figures/caching_approach.png)

One advantage to this approach is that handling operating system accessibility
API calls is quite fast. In fact, this design leads to even faster performance
than a traditional single-process browser, where many API calls would be
handled by querying the DOM tree or Layout tree for details.

This approach is also completely free from blocking IPCs or deadlocks.

There are some drawbacks to this approach, though:

Memory usage is higher. The cache necessarily duplicates information that
was already stored elsewhere, so this is unavoidable. We mitigate this in
part by ensuring that the data structure we use to store each accessibility
node is sparse and compact.

Second, the accessibility tree can't be computed lazily. Whenever a web page
changes, updates have to be pushed to the browser process cache right away,
so that the cache is up-to-date as soon as possible. Now, when a large and
complex page loads and this page is immediately consumed by assistive
technology, then this approach is no worse - we're just shifting the burden
from providing the accessibility tree on-demand to precomputing it, but
essentially doing the same work. However, when assistive technology is not
actually consuming the changes, this approach can be inefficient. More work
is needed to mitigate this performance issue.

One question that comes up is: isn't it a problem that the browser cache
is potentially "behind", showing a snapshot of the web page as it existed
a moment ago? This is true, but in practice it ends up being insignificant.
What's most important is that the cache always represents a complete and
consistent snapshot of the accessibility tree. Also, note that the visual
representation you see of a web page is also delayed slightly from the
"source of truth" in the DOM. A typical graphics frame is calculated every
~17 ms (assuming a display refresh rate of 60 fps), and there's some
additional latency from when a graphics frame is computed and when it's
actually shown on the screen - so in a sense whenever a web page makes a
change to JavaScript, what you see on the screen is usually 10 - 20 ms
behind that. If you've ever clicked the mouse at the exact instant the
web page scrolled out from under you and you clicked on the wrong thing,
you've observed this phenomenon.

Chromium's caching approach to multi-process accessibility has led to several
advantages or insights that were not immediately apparent in the initial design:

* The cache can be anywhere, it doesn't have to be in the browser process.
  On Chrome OS we put the accessibility cache in the process running the
  assistive technology. On Windows we have explored the idea of making a
  separate accessibility process for assistive technology to talk to, or
  possibly pushing the cache to the assistive technology's process.
* It's all data. By design the accessibility cache is just data, it's a
  serialization of what the accessibility tree looks like at any point in
  time. This view ends up having a lot of nice advantages, described more
  below.

### Push vs pull

One way to think about different ways to architect an accessibility system
is to explore what triggers data to move in the system.

Most operating system accessibility APIs are based around a "pull" model.
When assistive technology requests information from the accessibility tree,
it calls a method on the app to requests it. In a single-process browser or
in the proxy approach described above, the underlying data behind that node
is pulled from the source accessibility tree, which pulls from the underlying
data model (the DOM and layout trees).

In contrast, Chrome's "push" model tracks changes that happen to the
accessibility tree and then pushes changes from the web directly to
the cached accessibility tree cache in the browser process. This incurs
from upfront cost when a page changes, but makes access to accessibility
APIs very fast.

### It's all data

Most accessibility APIs are based around a functional API, where you override
methods in order to answer whatever queries assistive technology has about
any particular accessibility object.

This approach is consistent with many common design principles, such as
DRY (don't repeat yourself) - that there should be a single source of truth
for any piece of information (like whether a control is visible or not),
rather than two copies of that information (which could get out of sync).
(This is harder to achieve in a multi-process app, though.)

However, the functional approach has its downsides.

To query the current state of an object, you end up calling basically all of its
methods. Even in a single process browser where there's no IPC overhead, every
single API might go through several layers of indirection in order to be
satisfied.

For example, suppose the operating system calls the isEnabled() method on a
checkbox. The implementation might make a series of method calls that end up
querying the DOM or the Layout tree, before returning the result.  Subsequent
calls to isVisible(), isFocused(), and isChecked() might go through the same
series of calls, unless the app specifically cached them temporarily.

In comparison, if you ask that same checkbox to just fill in a simple
data structure with its accessibility state, it might be able to quickly
compute isEnabled, isVisible, isFocused, and isChecked all at the same time,
with no additional layers of indirection needed.

Another advantage of this approach is that you can take advantage of default
values using a sparse data structure - think of a node in the accessibility
cache as a key/value store like a hash map.  If the default value of isChecked
is false, then for any accessible object that isn't currently checked you don't
have to do anything.  Only if it is checked do you add a "checked" attribute to
the map.

Another advantage of the accessibility tree being data is that you can
save the complete state of the accessibility tree - either making a copy
in memory, or dumping a human-readable text version to a log file. We use
this concept extensively in accessibility browser tests.

One powerful consequence of this approach is that an accessibility tree
doesn't need a backing web page in order to function. It's possible to save
an accessibility tree and a series of atomic mutations, and then "replay"
them later and get identical results, without any backing web page.
Chromium currently has some experimental support for recording changes to
a web page in the chrome://accessibility page, and we also take advantage
of this snapshotting in order to implement support for the Android
"freeze-dried tabs" feature where a frozen snapshot of the page is
displayed (with accessibility support) while the real page is being
fetched from a slow connection.

### Data structures used by the accessibility cache

In this section we'll cover the data structures used in order to implement
the accessibility cache. For the moment, we'll ignore the render processes
and how this data is generated, in order to focus on what data is received
and how it's used.

The key data structures used throughout accessibility code are found in the
[ui/accessibility](
https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/)
directory.

The underlying data from one node is stored in AXNodeData. A simplified
version of this struct is:

```C++
struct AXNodeData {
    int id;
    Role role;
    vector<pair<StringAttribute, string>> string_attributes;
    vector<pair<IntAttribute, int>> int_attributes;
    vector<pair<FloatAttribute, float>> float_attributes;
    ...
    vector<int> child_ids;
    AXRelativeBounds relative_bounds;
};
```

Every node has an ID, but IDs are only required to be unique within the
same web frame. The IDs are used to express the tree structure - each node
has a vector of its child IDs.

Every node has a role, since that's a fundamental concept in accessibility
and every node needs one. Every node also has a bounding box (we'll go into
why it's a *relative* bounding box later). Nearly all of the other
attributes are stored as sparse vectors of (attribute type, attribute value)
pairs. There are currently over 100 different attributes that Chromium
can associate with a single accessible node, but most nodes only have
5 - 10 of them set. Anything unset is treated as having the default value.

An AXTreeUpdate is a pure-data snapshot of an accessibility tree or an
update to an existing tree. A slightly simplified version of that struct is:

```C++
struct AXTreeUpdate {
    AXTreeData tree_data;
    int root_id;
    vector<AXNodeData> nodes;
};
```

AXTreeData just has a few attributes that apply to the entire
tree rather than just one particular node.

A valid AXTreeUpdate corresponding to a complete accessibility tree just has to
contain every node exactly once with no duplicates or redundant nodes. In other
words, root_id must contain the ID of one node; that node must have the IDs of
its children, and every node must either be the root or the child of some other
node. The nodes can come in any order.

A valid AXTreeUpdate can also represent the changes needed to change an
existing accessibility tree. Any node that's unchanged can be completely
left out; only nodes that change need to be included. To insert a node,
just add its AXNodeData and be sure to update the child_ids in its parent.
To remove a node, remove it from its parent's child_ids.

An AXTreeUpdate is *stateful* - if you're using it to update an existing
tree then the code that generated the update needs to understand the
state that the tree was in previously.

There are two classes that represent a live node and a live tree, here
are the important details:

```C++
class AXNode {
  public:
    ...
    const AXNodeData& data();
    AXNode* GetParent() const;
    vector<AXNode*>& GetAllChildren() const;
    ...
};

class AXTree {
  public:
    AXTree();
    explicit AXTree(const AXTreeUpdate& initial_state);
    bool Unserialize(const AXTreeUpdate& update);
    AXNode* root() const;
    AXNode* GetFromId(int id) const;
    ...
};
```

You can construct an AXTree from an AXTreeUpdate directly, or create an
empty tree first. Then call Unserialize to take an AXTreeUpdate and
apply those changes to the current tree. It will return false if the
AXTreeUpdate is malformed or can't be applied to the current tree.

Once you have an AXTree, you can get its root node, or look up nodes by ID.
Each node is just a thin wrapper around its underlying AXNodeData plus
some convenience functions to walk the tree.

AXTree is essentially the underlying data model used to implement
accessibility in the browser process. As described in
[Part 1](how_a11y_works.md), there's a platform-specific accessibility
object for each node in the tree. When a platform accessibility API is
called on one of those nodes, it uses the corresponding AXNode's
AXNodeData to get the details of the node to satisfy that query.

Nearly all accessibility APIs can be satisfied directly from AXNodeData:
for example a node's name, role, state, value, bounding box, allowed
actions, and relationships with other nodes. There are a few exceptions
that will be covered in [Part 3](how_a11y_works_3.md).

BrowserAccessibilityManager is a layer on top of AXTree. It hooks up
the cross-platform accessibility data structures to the platform-specific
tree of native accessibility objects. That will be discussed in more
detail in [Part 3](how_a11y_works_3.md).

### Serializing the accessibility tree from the renderer

The other half of the puzzle here is what happens on the renderer side.
Given a web page that's constantly changing, how do we serialize a
representation of the accessibility tree and send small, atomic updates
to the browser process to keep the cache in sync?

See above for a reminder of Blink and how it fits into the render process.
Render accessibility consists of two main pieces:

* A tree of lightweight accessibility nodes inside Blink that represent
  the current state of the accessibility tree
* Code outside Blink that keeps track of nodes that need to be updated,
  and periodically serializes updates to send to the browser process.
  
#### The Blink Accessibility Tree

Inside Blink, we use the following classes.

AXObject is the base class representing one node in the accessibility
tree. Each AXObject is a wrapper, it either wraps a DOM Node (a blink::Node)
or a Blink layout object (a blink::LayoutObject). The AXObject contains
very little state; it caches a few attributes that are expensive to recompute
but otherwise doesn't store its full serialization.

AXObjectCache is the class that represents all of the AXObjects for one
web page. It's owned by the Document class, only if accessibility is enabled.

AXObjects are built lazily, on-demand. When an AXObject is added or deleted,
its parent marks its list of child objects as dirty so that the next time
it's queried it knows to compute them.

The vast majority of the web-specific accessibility logic is in AXObject and
AXObjectCache. Besides just support for ARIA attributes and getting
accessibility information from DOM elements, here you'll find all of the code
that interprets the [ACCNAME](https://w3c.github.io/accname/) spec to compute
the accessible name and description for any HTML element, by checking
aria-labelledby, aria-label, and other relevant attributes, for example.

In addition, the code in AXObject and AXObjectCache builds the structure of
the accessibility tree, especially in cases where it differs from the DOM tree,
such as CSS generated content, aria-hidden, or aria-owns.

When the accessibility tree changes, AXObjectCache sends an event
notification to the render accessibility code outside of Blink indicating
that a node has changed and needs to be re-serialized.

Blink does not currently have listener interfaces for all of the changes
that accessibility cares about. Rather, Blink code is generally
specifically instrumented for accessibility. So when a new DOM node is
inserted, or when the value of a form control changes, you'll see explicit
code in Blink to update the AXObjectCache.

In earlier versions, Blink accessibility code used to post a lot of
specific event notifications indicating exactly what changed, for
example: NameChanged, ValueChanged, StateChanged, ChildrenChanged, etc. - but
over time we've moved away from that model. Now we usually just mark a
node as dirty - or the event notification is still there for historical
reasons but it's never consumed and only marking the node is dirty ends up
mattering.

The reason for this was because we started adding code to automatically
generate events from tree mutations. This helped avoid entire classes of
problems like duplicate events.

So in a nutshell, Blink is just responsible for notifying which nodes have
changed. It's more efficient to just re-serialize nodes that probably changed
(occasionally doing a bit of extra work) than it is to write very careful logic
to only update nodes that actually changed.

#### Render Process Accessibility code (outside of Blink)

The main render process accessibility code outside of Blink is in
[RenderAccessibilityImpl](https://source.chromium.org/chromium/chromium/src/+/main:content/renderer/accessibility/render_accessibility_impl.h).
That code maintains the connection with the browser accessibility
code and handles serializing updates to the accessibility tree.

Updates to the accessibility tree are always batched.

One important reason is because the accessibility tree can only be serialized
when the document's lifecycle state is *clean*. In a nutshell, every time a
change happens to a web page that could conceivably affect how it appears
on-screen, the document is dirty until Blink has had a chance to do CSS style
resolution and layout. Accessibility code will fail assertions if you try to
query certain properties when the document is in a dirty state, because
it could lead to inconsistent results or even crashes.

So, accessibility changes are always queued up and then sent periodically
only after first ensuring that layout is complete.

When it is time to send accessibility updates, we make use of an
abstraction called AXTreeSerializer. AXTreeSerializer is a class that
knows how to walk a tree of nodes and generate valid AXTreeUpdates that
incrementally update a remote AXTree.

AXTreeSerializer is designed so that it doesn't know anything about Blink,
and it doesn't interpret any accessibility logic, it just knows how to
work with the AXNodeData and AXTreeUpdate data structures. In fact, we're
using AXTreeSerializer for other accessibility trees in Chromium outside
of Blink, and we have extensive unit tests for AXTreeSerializer that
serialize from one AXTree into another AXTree to test the logic in isolation.

AXTreeSerializer is stateful; it keeps track of what nodes have been sent
to its counterpart. When walking the tree, if it encounters a node that
it hasn't serialized before, it automatically serializes it. If it
encounters a node that was previously serialized and wasn't marked as
dirty, it automatically skips it.

AXTreeSerializer uses an interface called AXTreeSource to enable it to walk
any tree-like object without being tightly coupled to Blink or any other
specific tree. We use an implementation BlinkAXTreeSource that maps all
of the tree-walking and serialization calls into calls to Blink's
AXObject class.

This figure shows the overall system diagram covered so far:

![Inside Blink, which is inside the Render process, a Node and CSS both
influence a LayoutObject; the Node and the LayoutObject both influence the
AXObject. The AXObject communicates with RenderAccessibilityImpl, which is
outside of Blink but in the Render process. RenderAccessibilityImpl uses
AXTreeSerializer to send an update to the AXTree that lives in
BrowserAccessibilityManager in the Browser process, which is the
cached accessibility tree. Assistive technology then accesses this
cached accessibility tree using platform accessibility APIs.](
figures/multi_process_ax.png)

## We must go deeper

In the next section we'll explore some of the details that were
glossed over, including:

* Abstracting platform-specific APIs
* Relative coordinates
* Text bounding boxes
* Hit testing
* Views and other non-web custom-drawn UI

See [How Chrome Accessibility Works, Part 3](how_a11y_works_3.md)


