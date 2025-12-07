# Blink Accessibility

This document provides a high-level overview of the accessibility (AX) support codebase within the Blink rendering engine. It complements the broader Chromium accessibility documentation by detailing how web content is made accessible from the renderer process, before being serialized and sent to the browser process for consumption by native accessibility APIs. For a full architectural understanding, refer to the "Further Reading" section at the end of this document.

## Blink's Role in Chromium Accessibility

In Chromium's multi-process architecture, Blink is responsible for constructing and updating the accessibility tree from the web page it renders. This accessibility tree is a hierarchy of [`AXObject`](ax_object.h)s. It then serializes this information and sends it across the process boundary to the browser process.

The core components include:

* [`AXObject`](ax_object.h): This is the fundamental abstract class representing an accessible object in Blink's accessibility tree.
* [`AXObjectCacheImpl`](ax_object_cache_impl.h): This central class manages the accessibility tree within Blink. It is responsible for caching `AXObject`s corresponding to [`Node`](../../core/dom/node.h)s or [`LayoutObject`](../../core/layout/layout_object.h)s and relaying accessibility events from Blink to the embedding content layer. It queues work via [`DeferTreeUpdate()`](ax_object_cache_impl.cc) and processes updates when layout is clean.
*   [`WebAXObject`](../../../../public/web/web_ax_object.h): This acts as a public API wrapper around `AXObject`s.

## How Blink Processes and Serializes Accessibility Updates

Blink processes accessibility updates as part of the [`DocumentLifecycle`](../../core/dom/document_lifecycle.h), which aligns with the display's refresh rate. Accessibility processing runs in parallel with GPU rendering, after the lifecycle is complete. The [`AXObjectCache::CommitAXUpdates()`](ax_object_cache_impl.cc) method is called to process queued tree updates, ensure the tree is updated, and then "freeze" and serialize it.

When changes occur in the web content (e.g., DOM manipulations, style changes, or user input), these changes are eventually reflected in the accessibility tree. Blink's central class for managing accessibility events is [`AXObjectCacheImpl`](ax_object_cache_impl.h). This class contains numerous "`Handle[Foo]`" methods (e.g., [`HandleAriaExpandedChanged`](ax_object_cache_impl.cc), [`HandleValueChanged`](ax_object_cache_impl.cc)), which are called throughout Blink via the public [`AXObjectCache`](ax_object_cache.h) interface to notify the cache that its tree may need updating.

Instead of immediate processing, most accessibility changes are queued up or "deferred". This batching is crucial for performance. These queued updates are then processed at a "safe time", specifically when the document's rendering lifecycle is "clean" or "complete". Querying certain accessibility properties when the document is in a "dirty" layout state could lead to inconsistent results or even crashes.

This is where the "`FooWithCleanLayout`" methods come into play (e.g., [`ChildrenChangedWithCleanLayout`](ax_object_cache_impl.cc), [`MarkAXObjectDirtyWithCleanLayout`](ax_object_cache_impl.cc), [`TextChangedWithCleanLayout`](ax_object_cache_impl.cc)). These methods are callbacks that are explicitly designed to execute only when the layout is clean. When [`AXObjectCacheImpl::CommitAXUpdates()`](ax_object_cache_impl.cc) is called – a process that runs in parallel with GPU rendering after layout is complete – it processes these queued updates. During this "Process deferred updates" stage of the accessibility lifecycle, the [`AXObject`](ax_object.h)s' cached values and tree structure are updated, and objects and events are queued for serialization.

Once the accessibility tree is updated, its relevant information is serialized for communication with the browser process. This process involves:

* Calling [`AXObject::Serialize()`](ax_object.cc) on the `AXObject`s in the renderer process. This method populates a [`ui::AXNodeData`](../../../../../../ui/accessibility/ax_node_data.h) structure.
* The system operates on a "push" model; changes in the renderer's accessibility tree are actively pushed to a cached accessibility tree in the browser process. This approach makes handling operating system accessibility API calls very fast in the browser process, as the tree resides in the browser's memory space.
* The [`AXTreeSerializer`](../../../../../../ui/accessibility/ax_tree_serializer.h) class is used to walk the tree and generate [`AXTreeUpdates`](../../../../../../ui/accessibility/ax_tree_update.h) (which contain `AXNodeData`), which are then sent to the browser process via Mojo messages.
* The browser then consumes the received data, incrementally updating the remote [`AXTree`](../../../../../../ui/accessibility/ax_tree.h) via [`AXTree::Unserialize()`](../../../../../../ui/accessibility/ax_tree.cc).

The entire process is governed by an "Accessibility lifecycle". Key stages include:

1. **Defer tree updates**: Listening for DOM and layout changes and queuing [`TreeUpdateReason`](ax_object_cache_impl.cc) objects for later processing.
2. **Process deferred updates**: Processing these queued updates when layout is clean, updating the tree structure and `AXObject` cached values.
3. **Finalize tree**: Ensuring the tree structure and properties are updated before serialization.
4. **Freeze & Serialize**: Preparing the [`AXTreeUpdate`](../../../../../../ui/accessibility/ax_tree_update.h) list, during which no `AXObject` creation or cached value updates are allowed to ensure consistency.

This architecture, while effective for AT communication, incurs significant costs in terms of UI jank, performance, and memory usage. Projects are underway to address these issues, including improving [`AXMode`](../../../../../../ui/accessibility/ax_mode.h) usage to only compute and cache what's needed, reducing serializations, and optimizing data structures.

## What's an AXObject?

An [`AXObject`](ax_object.h) provides methods to determine roles, properties, and navigate the accessibility tree. Some of the more useful members are:

* The Unique Identifier (AXID) available via [`AXObjectID()`](ax_object.h): Each `AXObject` has a unique integer ID within its process. This ID will match the DOM `NodeID` if the `AXObject` corresponds to a DOM node; otherwise, a negative ID is generated. Almost all `AXObject`s are backed by a DOM node.

* Structural Properties: `AXObject`s define their role, parent, and children relationships, forming the hierarchy of the accessibility tree. See [`RoleValue()`](ax_object.h), [`ParentObjectIncludedInTree()`](ax_object.h) and [`ChildrenIncludingIgnored()`](ax_object.h)

* Cached Values (State): To optimize performance, `AXObject`s cache certain properties. These cached "values" are typically those that:

  * Require layout computation but might be needed at any time.
  * Are expensive to compute but are used repeatedly.
  * Are inherited from their parent `AXObject`. Examples of specific cached values include `cached_is_ignored_`, `cached_is_aria_hidden_`, `cached_can_set_focus_attribute_`, and `cached_local_bounding_box_`. When a node is marked as "dirty" (indicating a change), its cached values are invalidated and updated during the "clean layout" phase of the accessibility lifecycle. Changes to inherited values can also trigger a recursive refresh of cached values for a subtree.

* Behavioral Properties: `AXObject`s include property getters and the [`Serialize()`](ax_object.cc) method which is used to gather data into the [`AXNodeData`](../../../../../../ui/accessibility/ax_node_data.h) structure for serialization.

* Actions: `AXObject`s support various actions that can be performed on the object, which are represented by the [`ui::AXActionData`](../../../../../../ui/accessibility/ax_action_data.h) struct.

* Utility Methods: `AXObject`s provide useful methods such as [`IsDetached()`](ax_object.h) (to check if it's no longer connected to the tree), [`AXObjectCache()`](ax_object.h) (to access its associated cache), and [`IsIgnored()`](ax_object.h) (to determine if it's ignored by accessibility features).

### Subclasses of AXObject

`AXObject` utilizes concrete subclasses:

* [`AXNodeObject`](ax_node_object.h): This is a primary subclass that wraps a DOM [`Node`](../../core/dom/node.h) or a [`LayoutObject`](../../core/layout/layout_object.h) (or both). It stores references to these underlying web platform objects in its `node_` and `layout_object_` members, respectively. `AXNodeObject` also contains specific-case logic for various types of Nodes.
  * Some `AXNodeObject`s such as descendants of pseudo elements have only a layout object and do not correspond to actual DOM nodes.
  * Some `AXNodeObject`s such as those styled with display:none or display:contents have a `Node`, but no `LayoutObject`.
* [`AXInlineTextBox`](ax_inline_text_box.h): This subclass specifically represents a line of text within the accessibility tree. `AXInlineTextBox` objects are leaf nodes, meaning they do not have children, and they encapsulate an [`AbstractInlineTextBox`](ax_inline_text_box.cc) or an [`AXBlockFlowIterator::FragmentIndex`](ax_inline_text_box.cc).
* Other, more specialized subclasses exist (e.g., [`AXImageMapLink`](ax_image_map_link.h), [`AXSlider`](ax_slider.h)), though many have been removed to reduce code complexity and fragility.

### Serialization of AXObject Data

Once `AXObject`s are constructed and updated, their relevant information is gathered for transmission across the process boundary to the browser process. This involves calling [`AXObject::Serialize()`](ax_object.cc) which populates a [`ui::AXNodeData`](../../../../../../ui/accessibility/ax_node_data.h) structure. The `AXNodeData` structure is designed to be compact and sparse, storing attributes like role, ID, and other properties in attribute arrays (e.g., `string_attributes`, `int_attributes`), rather than allocating space for every possible attribute on every node. During the "Freeze & Serialize" stage of the accessibility lifecycle, the tree is temporarily frozen to prevent modifications, and `const AXObject*` instances are fed to the serializer to generate these `AXNodeData` updates. The collection of updates created in [`AXObjectCacheImpl::GetUpdatesAndEventsForSerialization`](ax_object_cache_impl.cc) is then serialized into a Mojo message by [`RenderAccessibilityImpl::SendAccessibilitySerialization`](../../../../content/browser/renderer_host/render_accessibility_impl.cc) for transmission.

## Ignored and Included Objects

The concepts of "ignored" and "included" objects are fundamental to how the accessibility tree is constructed and exposed to assistive technologies (ATs).

### 1\. Ignored Objects

An "ignored" accessibility object is one that will not be exposed in platform accessibility APIs to assistive technologies. This means that, from the perspective of a screen reader or other AT, the object effectively does not exist in the accessibility tree.

Objects are marked as ignored for various reasons, often to prevent redundant information, remove visually hidden elements, or simplify the accessibility tree by removing "uninteresting" content. The ignored state is computed by [`AXObject::ComputeIsIgnored()`](ax_object.cc) or [`AXTree::ComputeNodeIsIgnored()`](../../../../../../ui/accessibility/ax_tree.cc).

Common reasons why an object might be ignored include:

* **Explicit State/Role**: If an element has an ARIA presentational role (`role="none"`).
* **CSS Hiding**: Elements hidden by CSS properties like `display: none` or `visibility: hidden` (determined by [`IsHiddenViaStyle()`](ax_object.cc)).
* **ARIA Hiding**: Elements with `aria-hidden=true` (via [`IsAriaHidden()`](ax_object.cc)), which hides the entire subtree.
* **Inertness**: Elements marked with the `inert` attribute/`interactivity` property or considered inert due to their context (via [`IsInert()`](ax_object.cc)).
* **Uninteresting Content**: Whitespace nodes, `<span>` tags without additional ARIA information, or `<label>` elements that are already used to name a control and would cause redundancy. The `<html>` and `<body>` elements are also typically ignored.
* **Layout/Structure**: SVG `symbol` elements (which are graphical templates), or specific layout objects like a 1x1 pixel canvas.

**Important Exception**: A **focused node is never ignored**, even if it has properties (like `aria-hidden` or `display: none`) that would otherwise cause it to be ignored. This ensures that users of assistive software can always interact with the currently focused element.

### 2\. Included Objects

An object is "included" in the accessibility tree if it is part of the internal structure that is processed and serialized by Blink's accessibility engine, even if it is marked as ignored from the perspective of platform APIs. The purpose of including ignored objects is to retain necessary information for internal computations, tree consistency, and other browser-side accessibility logic.

The [`AXObject`](ax_object.h) class, and its concrete subclass [`AXNodeObject`](ax_node_object.h), manage this state using cached values like `cached_is_ignored_` and `cached_is_ignored_but_included_in_tree_`.

### 3\. Ignored but Included Objects

This is a specific category of objects that are not exposed to platform accessibility APIs (ignored) but are still present in the internal accessibility tree (included). This allows for a more accurate and robust internal representation of the page, even for elements that are hidden from the user.

Reasons why an ignored object might still be included in the tree:

* **ARIA-owned Objects**: Objects that are "owned" by another element via `aria-owns` are always included, regardless of their own ignored state, because they need to be children of the owning element in the accessibility tree.
* **Label and Description Calculation**: Nodes that are used to compute the accessible name or description of other elements (determined by [`IsUsedForLabelOrDescription()`](ax_object.cc)) are included. This is crucial for accurate name calculation, even if the label itself is visually hidden or otherwise ignored.
  * For instance, `<label>` elements are often ignored to prevent duplicate speech but are included for their role in naming controls. Similarly, children of `<label>` and `<map>` elements are included for accessibility calculation.
  * Elements referenced by `aria-labelledby` or `aria-describedby` relations, especially if hidden, are included to allow their subtrees to contribute to the name/description.
* **Internal Bookkeeping and Consistency**:
  * If a node's "flat tree parent" (as determined by [`LayoutTreeBuilderTraversal`](../../core/dom/layout_tree_builder_traversal.h)) is different from its DOM parent, it must be included. This handles complex DOM structures, like those involving shadow DOM slotting, where direct DOM traversal might be unsafe or insufficient.
  * Elements in media controls (e.g., `<video>` or `<audio>`) are kept in the tree, even if ignored, because their ignored state can change dynamically without a layout update, and the serializer needs them to be present.
  * Menu elements are included even if hidden to facilitate event generation when they open.
  * Table-related elements (like `<table>`, `<tbody>`, `<tr>`, `<td>`) are kept included because their role and ignored status can be highly dependent on their complex ancestry.
* **Specific HTML Elements**:
  * The `<html>` element is included, even though it's typically ignored.
  * Ruby annotation (`<rt>`) elements are included to help calculate accessible descriptions for ruby text.
  * Line breaking objects (`<br>`) are included if visible, as they are necessary for detecting paragraph edges in text navigation.
* **Pseudo-elements**: All pseudo-element content (e.g., `::before`, `::after`, `::marker`) and their parents are included to ensure all visible descendant pseudo-content is reached and to assist in name computation.

### Summary of Ignored vs Included

## Additional Considerations

### Performance and Jank

Accessibility processing can significantly contribute to jank (dropped frames) if it takes too long within the document lifecycle. For instance, a subtree changing from `display:none` to `display:block` can cause every descendant to be rebuilt and serialized, leading to jank.

To mitigate these issues, Blink's accessibility implementation focuses on:

* Controlling Data Flow with [`AXMode`](../../../../../../ui/accessibility/ax_mode.h): The [`ui::AXMode`](../../../../../../ui/accessibility/ax_mode.h) flags dictate how much accessibility data Blink is instructed to compute and serialize. More detailed modes, such as [`kExtendedProperties`](../../../../../../ui/accessibility/ax_mode.h),  [`kInlineTextBoxes`](../../../../../../ui/accessibility/ax_mode.h), and [`kScreenReader`](../../../../../../ui/accessibility/ax_mode.h), are expensive and can be "overused". A long term goal is to start with minimal `AXMode`, activating more detailed modes only when necessary.
* Sparse Data Representation: The accessibility node data ([`ui::AXNodeData`](../../../../../../ui/accessibility/ax_node_data.h)) is designed to be compact and sparse, avoiding allocation for every possible attribute on every node to reduce memory footprint.

### Frame Handling

For embedded web pages (iframes), Blink will build a separate independent accessibility tree for each frame, regardless of whether it's in the same process or a different one. These individual frame trees are then serialized in [`AXObject::SerializeChildTreeID()`](ax_object.cc) and sent to the browser process, where they are cached and composed into a single virtual accessibility tree.

### Standards Support

Blink implements the Web Platform Accessibility Mapping (AAM) standards, including [CORE-AAM](https://w3c.github.io/aria/core-aam/) (for ARIA), [HTML-AAM](https://w3c.github.io/aria/html-aam/) (includes CSS), [SVG-AAM](https://w3.org/TR/svg-aam-1.0/), and [MathML-AAM](https://w3c.github.io/mathml-aam/), which define how markup maps to platform accessibility APIs. This includes handling specific HTML elements, ARIA attributes, and complex interactions like custom elements and shadow DOM.

## Testing and Debugging

Blink's accessibility functionality is validated through several testing methodologies:

* Blink Accessibility Web Tests: These tests verify the basic computation of the accessibility tree within the Blink renderer. They do not, however, test the serialization/deserialization process or the final mappings to platform APIs.
* Dump Accessibility Tests: These are a type of [`content_browsertest`](../../../../../../content/public/test/browser_test.h) that are crucial for validating the accurate serialization and deserialization of the accessibility tree data from Blink and its correct mapping to platform-specific APIs (Windows "uia" and "ia2", Mac "mac", Linux "auralinux", Android "android", and the internal "blink" format). These tests are text-based and easy to rebaseline.
* Automated AT Output Testing: While this is a broader Chromium project (often focused on the browser-side), Blink's correct serialization is a prerequisite for validating the output of screen readers like NVDA.

For debugging Blink's accessibility tree, developers can use:

* `chrome://accessibility` to view the full AX tree.
* DevTools to inspect the accessibility tree.
* Internal logging functions like [`LOG(ERROR) << ax_object`](../../../../../../base/logging.h)

## Further Reading

For a more comprehensive and detailed understanding of Chromium's overall accessibility architecture, internal data structures, and interactions between components, please refer to the following documents in the Chromium repository:

* ["Chromium Accessibility: Architecture and Concepts"](../../../../../../docs/accessibility/architecture_and_concepts.md)
* ["How Chrome Accessibility Works - Part 1"](../../../../../../docs/accessibility/how_accessibility_works_1.md)
* ["How Chrome Accessibility Works - Part 2"](../../../../../../docs/accessibility/how_accessibility_works_2.md)
* ["How Chrome Accessibility Works - Part 3"](../../../../../../docs/accessibility/how_accessibility_works_3.md)


