# Security Guidelines for the Accessibility Tree

TL;DR; The existing
[AXTreeData](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/mojom/ax_tree_data.mojom)
and
[AXTreeUpdate](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/mojom/ax_tree_update.mojom)
structures can be walked in the browser process using
[ax_tree.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree.h)
and
[ax_tree_update.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree_update.h)
when no alternatives are available. The trees describe complex documents and can
be supplied by compromised renderers. Care should be taken when processing AX
data, especially when processing updates. New features using the tree should be
hosted in a sandboxed process.

## Background

One of Chromium's key features is its multi-process architecture. This brings
numerous benefits but presents one key challenge for accessibility.

Operating system accessibility apis expect the ability to synchronously request
data about the screen presented as a tree of semantic nodes. These nodes contain
numerous attributes describing the node such as its role, display name, bounds,
and more. Such an api is at the heart of technologies used for persons with
disabilities. This includes software that reads the web page to those with
visual impairments, highlights words to those with print disabilities,
auto-scans the actionable views on a page for those with motor impairments, and
much more.

Since Chromium’s renderers hold the semantics about the web page and the browser
requires this data synchronously, Chromium developers had to come up with a way
to bridge this gap while keeping the system itself performant.

After trying various approaches, Chromium settled on a model where accessibility
trees get serialized from renderers and deserialized in a destination process
such as the browser.

Although designed for features and extensions that improve Chrome’s
accessibility, many other features make use of the trees and updates. The AX
tree provides a convenient way to snapshot embedded frames, and surface page
structure, without the features needing to walk the DOM or inject scripts into
renderers.

In Chrome, the
[rule-of-two](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md)
requires any C++ code in the browser process to only act on trustworthy data
from renderers. This is usually achieved by passing data to the browser over
mojo, which validates the data’s format and enforces that it is properly typed.
[AXTreeData](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/mojom/ax_tree_data.mojom)
and
[AXTreeUpdate](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/mojom/ax_tree_update.mojom)
(along with other AX mojom types) serve this purpose for accessibility tree
snapshots and updates. Consumers of these structures should access them via
[ax_tree.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree.h)
and
[ax_tree_update.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree_update.h)
and need not further validate the /format/ of the data they represent. Normal
web contents and javascript should not be able to generate or send invalid
snapshots or updates.

However the AX Tree is complex and its attributes fields are not validated.
Fundamentally the
[rule-of-two](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md)
exists to prevent us acting on web content with bugprone C++ code. As the AXTree
embeds and is controlled by web content we would implement it differently today.
Its current design is necessary to work with accessibility tooling on operating
systems that were not designed from the ground up to host web content.

Today, a [compromised
renderer](https://chromium.googlesource.com/chromium/src/+/master/docs/security/compromised-renderers.md)
can cause unexpected trees or updates to be sent to the browser process. Any
code performing complex processing of the trees such as comparison of snapshots,
or updating of internal data structures from updates, should guard itself
against receiving manipulated snapshots or updates. Code using ax_tree.h can
trust that the data structures are well-formed, but *cannot* trust that they
hold valid data. This includes the contents of free-form fields such as
[strings](https://source.chromium.org/search?q=file:ui%2Faccessibility%2Fmojom%2F.*mojom$%20string&ss=chromium%2Fchromium%2Fsrc:ui%2Faccessibility%2Fmojom%2F),
or the objects or types of objects referenced by a given ID. The only safe way
to process this information is in a sandboxed environment or memory-safe
language.

## Guidance

### The AX Tree is accepted for accessibility features

If your feature is doing simple things with the AX data you should be fine - it
will make security review quicker if you briefly highlight what you are doing
and why it is safe in your Security Considerations section.

The existing
[ax_tree.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree.h),
[ax_tree_update.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree_update.h)
and associated wrappers are accepted for limited uses in the browser.

Consumers *must* use the ax_tree.h methods to deserialize trees or apply updates
and not reimplement this.

Consumers should not use the mojom structure of the tree directly and instead
should rely on the deserialization & updating methods in
[ui/accessibility/](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/).

### The AX Tree carries sensitive data or tokens

Some fields in the tree represent sensitive data from the origin in a given
renderer, or tokens that can drive features in the browser, and should not be
shared into unrelated renderer processes:

* The [AXTreeId](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/ax_tree_id.h)
  is also the embedder token.
* Page content is present and is site-private user data.
* Annotations, urls, titles and metadata may be present in string arguments.
* The AX Tree has string fields that can be manipulated

### The AX tree is complex - and any complex processing could be risky

If you are doing complicated things with the structures - or if you relate more
than one snapshot, or update internal structures from snapshots or updates, then
you enter hairy territory - a compromised render has significant control over
the tree and can send malicious snapshots or updates.

You should do this processing in a sandboxed service process or the renderer,
and your security considerations section should outline how your feature deals
with malicious incoming data.

### Fields in the tree are not validated

The content of the tree is untrustworthy and may come from a compromised
renderer.

String might not have expected values or formats:

* String urls should be validated using GURL.
* Urls should *not* be assumed to refer to the renderer’s origin or its
  subframe’s origins.
* Attributes, titles, roles etc. should be sanitized if included or displayed in
  messages.
* Matching an entire string, using them as lookups in maps and so on is ok.
* Smuggling formatted data (such as JSON) in a string is *not* ok.
* Offsets into strings are untrustworthy - complex string processing should
  happen in a sandboxed process or a memory-safe language.

A compromised renderer might set or add unexpected attributes or values to
updates or snapshots, for instance:

* Rects may have negative or positive x,y coordinates.
* Offsets into text may lie out of bounds, and may be out of order
* Object ids in snapshots can be faked or duplicated.
* Updates or repeated snapshots might describe very different structures.
* Expected attributes might not be present, or extra attributes might be added.
* Attribute lists may have inconsistent lengths.
* A node with five attributes in one snapshot might have seven, or none, in a
  subsequent snapshot.
* IDs might be repeated, invalid, or self-referencing.

The AX Tree ID can sometimes come from a renderer - it is also the embedding
token of the frame - so be very careful when tracking or observing these IDs.
These tokens should not be shared between renderers and are unguessable.

## Accessibility / Security FAQs

### Accessibility actions can be used to drive the browser programmatically. Is this a vulnerability?

No. Accessibility software needs to access and control applications for the
person using Chrome. This is intended and supported behavior.

### Operating system accessibility APIs talk directly to the browser process. Is this a security concern?

No. Accessibility software needs to access and control applications at the same
level as a person using Chrome. This is intended and supported behavior.

### Accessibility transfers data from renderers to the browser process. Is this a security concern?

No. This data represents the contents and structure of the page for
accessibility tools, and other Chrome features. Data is safely passed via mojo.

The tree represents a collection of data from all renderer processes in a tab’s
frame tree, and holds sensitive content that should not be passed back down to
renderers to avoid data leaks between them. The accessibility tree is complex
and any new complex processing should happen in a sandboxed process.

### Accessibility has a large mojo surface, is this a concern?

No. This data represents the contents and structure of the page for
accessibility tools. Data is safely passed via mojo.

Like all IPC in Chrome a compromised renderer can lie about some fields.
Privileged processes accessing this data do so via the ax_tree.h APIs, and treat
any strings or offsets as untrustworthy. The tree itself is complex and features
comparing trees should do so in a sandboxed process or memory safe language. See
“Guidance” above for more discussion.

### Render frames pass accessibility data to one another e.g. hit test requests, accessibility tree ids. Are these requests allowed?

Frames should only pass data to frames that should have it. For instance the
accessibility tree id is also the frame’s embedder token, and should not be
available to unrelated frames.

### Is the accessibility API (either at the OS-level or via extensions) preferred over content script injection?

All approaches have merits. Extensions are intrinsically safe and allow for
experimentation outside of the Chromium. Content script injection is more
fragile but can access everything it needs within the injected site. The ax tree
provides an easy way to access page structure and contents without needing to
reinvent methods for transmitting the structure, or updates to the structure.
The accessibility tree is intended for accessibility surfaces and should not be
extended for other features in Chrome. The ax tree can also be queried safely
within a renderer.

### Is it considered safer to deserialize accessibility tree data within the browser or a dedicated renderer?

Both approaches have merits. If a feature needs only limited data from a
renderer then it will be better to query the DOM or use the ax tree within the
renderer, then pass information to the browser via feature-specific mojo
definitions. If a feature needs to know the full structure of the page, then it
might be best to use the ax tree (via the ax_tree.h APIs) as they provide a
semi-hardened way to access this.

### Accessibility tree attributes and fields are untrusted. What am I allowed to do with them?

A compromised renderer can supply whatever string or list attributes it wants -
even when you access these through the ax_tree.h apis - and there is no
guarantee they will have the format or contents you expect. Features in the
browser or a web ui should sanitize or validate the data (e.g. by rejecting
input containing invalid characters, or displaying fields using safe html
elements in web ui). Accessibility helpers installed in the OS should perform
their own validation.

See “Guidance” above for more discussion.
