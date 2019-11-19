# DOM

[Rendered](https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/dom/README.md)

Author: hayato@chromium.org

The `renderer/core/dom` directory contains the implementation of [DOM].

[dom]: https://dom.spec.whatwg.org/
[dom standard]: https://dom.spec.whatwg.org/

Basically, this directory should contain only a file which is related to [DOM
Standard]. However, for historical reasons, `renderer/core/dom` directory has
been used as if it were _misc_ directory. As a result, unfortunately, this
directory contains a lot of files which are not directly related to DOM.

Please don't add unrelated files to this directory any more. We are trying to
organize the files so that developers wouldn't get confused at seeing this
directory.

- See the
  [spreadsheet](https://docs.google.com/spreadsheets/d/1OydPU6r8CTj8HC4D9_gVkriJETu1Egcw2RlajYcw3FM/edit?usp=sharing),
  as a rough plan to organize Source/core/dom files.

  The classification in the spreadsheet might be wrong. Please update the
  spreadsheet, and move files if you can, if you know more appropriate places
  for each file.

- See [crbug.com/738794](http://crbug.com/738794) for tracking our efforts.

# Node and Node Tree

In this README, we draw a tree in left-to-right direction in _ascii-art_
notation. `A` is the root of the tree.

```text
A
├───B
├───C
│   ├───D
│   └───E
└───F
```

`Node` is a base class of all kinds of nodes in a node tree. Each `Node` has
following 3 pointers (but not limited to):

- `parent_or_shadow_host_node_`: Points to the parent (or the shadow host if it
  is a shadow root; explained later)
- `previous_`: Points to the previous sibling
- `next_`: Points to the next sibling

`ContainerNode`, from which `Element` extends, has additional pointers for its
child:

- `first_child_`: The meaning is obvious.
- `last_child_`: Nit.

That means:

- Siblings are stored as a linked list. It takes O(N) to access a parent's n-th
  child.
- Parent can't tell how many children it has in O(1).

![next sibling and previous sibling](https://hayato.io/2017/dom/next-sibling.svg)

Further info:

- `Node`, `ContainerNode`

# C++11 range-based for loops for traversing a tree

You can traverse a tree manually:

```c++
// In C++

// Traverse a children.
for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
  ...
}

// ...

// Traverse nodes in tree order, depth-first traversal.
void foo(const Node& node) {
  ...
  for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
    foo(*child);  // Recursively
  }
}
```

Tree order is:

![tree order](https://hayato.io/2017/dom/tree-order.svg)

However, traversing a tree in this way might be error-prone. Instead, you can
use `NodeTraversal` and `ElementTraversal`. They provides a C++11's range-based
for loops, such as:

```c++
// In C++
for (Node& child : NodeTraversal::childrenOf(parent) {
  ...
}
```

e.g. Given a parent _A_, this traverses _B_, _C_, and _F_ in this order.

```c++
// In C++
for (Node& node : NodeTraversal::startsAt(root)) {
  ...
}
```

e.g. Given the root _A_, this traverses _A_, _B_, _C_, _D_, _E_, and _F_ in this
order.There are several other useful range-based for loops for each purpose. The
cost of using range-based for loops is zero because everything can be inlined.

Further info:

- `NodeTraversal` and `ElementTraversal` (more type-safe version)
- The [CL](https://codereview.chromium.org/642973003), which introduced these
  range-based for loops.

# Shadow Tree

A **shadow tree** is a node tree whose root is a `ShadowRoot`. From web
developer's perspective, a shadow root can be created by calling
`element.attachShadow{ ... }` API. The _element_ here is called a **shadow
host**, or just a **host** if the context is clear.

- A shadow root is always attached to another node tree through its host. A
  shadow tree is therefore never alone.
- The node tree of a shadow root’s host is sometimes referred to as the **light
  tree**.

![shadow tree](https://hayato.io/2017/dom/shadow-tree.svg)

For example, given the example node tree:

```text
A
├───B
├───C
│   ├───D
│   └───E
└───F
```

Web developers can create a shadow root, and manipulate the shadow tree in the
following way:

```javascript
// In JavaScript
const b = document.querySelector('#B');
const shadowRoot = b.attachShadow({ mode: 'open' });
const sb = document.createElement('div');
shadowRoot.appendChild(sb);
```

The resulting shadow tree would be:

```text
shadowRoot
└── sb
```

The _shadowRoot_ has one child, _sb_. This shadow tree is being _attached_ to B:

```text
A
└── B
    ├──/shadowRoot
    │   └── sb
    ├── C
    │   ├── D
    │   └── E
    └── F
```

In this README, a notation (`──/`) is used to represent a
_shadowhost-shadowroot_ relationship, in a **composed tree**. A composed tree
will be explained later. A _shadowhost-shadowroot_ is 1:1 relationship.

Though a shadow root has always a corresponding shadow host element, a light
tree and a shadow tree should be considered separately, from a node tree's
perspective. (`──/`) is _NOT_ a parent-child relationship in a node tree.

For example, even though _B_ _hosts_ the shadow tree, _shadowRoot_ is not
considered as a _child_ of _B_. The means the following traversal:

```c++
// In C++
for (Node& node : NodeTraversal::startsAt(A)) {
  ...
}
```

traverses only _A_, _B_, _C_, _D_, _E_ and _F_ nodes. It never visits
_shadowRoot_ nor _sb_. NodeTraversal never cross a shadow boundary, `──/`.

Further info:

- `ShadowRoot`
- `Element#attachShadow`

# TreeScope

`Document` and `ShadowRoot` are always the root of a node tree. Both`Document`
and `ShadowRoot` implements `TreeScope`.

`TreeScope` maintains a lot of information about the underlying tree for
efficiency. For example, TreeScope has a _id-to-element_ mapping, as
[`TreeOrderedMap`](./tree_ordered_map.h), so that `querySelector('#foo')` can
find an element whose id attribute is "foo" in O(1). In other words,
`root.querySelector('#foo')` can be slow if that is used in a node tree whose
root is not `TreeScope`.

Each `Node` has `tree_scope_` pointer, which points to:

- The root node: if the node's root is either Document or ShadowRoot.
- [owner document](https://dom.spec.whatwg.org/#concept-node-documentOwnerDocument),
  otherwise.

The means `tree_scope_` pointer is always non-null (except for while in a DOM
mutation), but it doesn't always point to the node's root.

Since each node doesn't have a pointer which _always_ points to the root,
`Node::getRootNode(...)` may take O(N) if the node is neither in a document tree
nor in a shadow tree. If the node is in TreeScope (`Node#IsInTreeScope()` can
tell it), we can get the root in O(1).

Each node has flags, which is updated in DOM mutation, so that we can tell
whether the node is in a document tree, in a shadow tree, or in none of them, by
using `Node::IsInDocumentTree()` and/or `Node::IsInShadowTree()`.

If you want to add new features to `Document`, `Document` might be a wrong place
to add. Instead, please consider to add functionality to `TreeScope`. We want to
treat a document tree and a shadow tree equally as much as possible.

## Example

```text
document
└── a1
    ├──/shadowRoot1
    │   └── s1
    └── a2
        └── a3

document-fragment
└── b1
    ├──/shadowRoot2
    │   └── t2
    └── b2
        └── b3
```

- Here, there are 4 node trees; The root node of each tree is _document_,
  _shadowRoot1_, _document-fragment_, and _shadowRoot2_.
- Suppose that each node is created by `document.createElement(...)` (except for
  Document and ShadowRoot). That means each node's **owner document** is
  _document_.

| node              | node's root              | node's `_tree_scope` points to: |
| ----------------- | ------------------------ | ------------------------------- |
| document          | document (self)          | document (self)                 |
| a1                | document                 | document                        |
| a2                | document                 | document                        |
| a3                | document                 | document                        |
| shadowRoot1       | shadowRoot1 (self)       | shadowRoot1 (self)              |
| s1                | shadowRoot1              | shadowRoot1                     |
| document-fragment | document-fragment (self) | document                        |
| b1                | document-fragment        | document                        |
| b2                | document-fragment        | document                        |
| b3                | document-fragment        | document                        |
| shadowRoot2       | shadowRoot2 (self)       | shadowRoot2 (self)              |
| t1                | shadowRoot2              | shadowRoot2                     |

Further Info:

- [`tree_scope.h`](./tree_scope.h), [`tree_scope.cc`](./tree_scope.cc)
- `Node#GetTreeScope()`, `Node#ContainingTreeScope()`, `Node#IsInTreeScope()`

# Composed Tree (a tree of node trees)

In the previous picture, you might think that more than one node trees, a
document tree and a shadow tree, were _connected_ to each other. That is _true_
in some sense. We call this _super tree_ as _composed tree_, which is a _tree of
trees_.

![super tree](https://hayato.io/2017/dom/super-tree.svg)

The following is a complex example:

```text
document
├── a1 (host)
│   ├──/shadowRoot1
│   │   └── b1
│   └── a2 (host)
│       ├──/shadowRoot2
│       │   ├── c1
│       │   │   ├── c2
│       │   │   └── c3
│       │   └── c4
│       ├── a3
│       └── a4
└── a5
    └── a6 (host)
        └──/shadowRoot3
            └── d1
                ├── d2
                ├── d3 (host)
                │   └──/shadowRoot4
                │       ├── e1
                │       └── e2
                └── d4 (host)
                    └──/shadowRoot5
                        ├── f1
                        └── f2
```

If you see this carefully, you can notice that this _composed tree_ is composed
of 6 node trees; 1 document tree and 5 shadow trees:

- document tree

  ```text
  document
  ├── a1 (host)
  │   └── a2 (host)
  │       ├── a3
  │       └── a4
  └── a5
      └── a6 (host)
  ```

- shadow tree 1

  ```text
  shadowRoot1
  └── b1
  ```

- shadow tree 2

  ```text
  shadowRoot2
  ├── c1
  │   ├── c2
  │   └── c3
  └── c4
  ```

- shadow tree 3

  ```text
  shadowRoot3
  └── d1
      ├── d2
      ├── d3 (host)
      └── d4 (host)
  ```

- shadow tree 4

  ```text
  shadowRoot4
  ├── e1
  └── e2
  ```

- shadow tree 5

  ```text
  shadowRoot5
  ├── f1
  └── f2
  ```

If we consider each _node tree_ as _node_ of a _super-tree_, we can draw a
super-tree as such:

```text
document
├── shadowRoot1
├── shadowRoot2
└── shadowRoot3
    ├── shadowRoot4
    └── shadowRoot5
```

Here, a root node is used as a representative of each node tree; A root node and
a node tree itself can be sometimes exchangeable in explanations.

We call this kind of a _super-tree_ (_a tree of node trees_) a **composed
tree**. The concept of a _composed tree_ is very useful to understand how Shadow
DOM's encapsulation works.

[DOM Standard] defines the following terminologies:

- [shadow-including tree order](https://dom.spec.whatwg.org/#concept-shadow-including-tree-order)
- [shadow-including root](https://dom.spec.whatwg.org/#concept-shadow-including-root)
- [shadow-including descendant](https://dom.spec.whatwg.org/#concept-shadow-including-descendant)
- [shadow-including inclusive descendant](https://dom.spec.whatwg.org/#concept-shadow-including-inclusive-descendant)
- [shadow-including ancestor](https://dom.spec.whatwg.org/#concept-shadow-including-ancestor)
- [shadow-including inclusive ancestor](https://dom.spec.whatwg.org/#concept-shadow-including-inclusive-ancestor)
- [closed-shadow-hidden](https://dom.spec.whatwg.org/#concept-closed-shadow-hidden)

For example,

- _d1_'s _shadow-including ancestor nodes_ are _shadowRoot3_, _a6_, _a5_, and
  _document_
- _d1_'s _shadow-including descendant nodes_ are _d2_, _d3_, _shadowRoot4_,
  _e1_, _e2_, _d4_, _shadowRoot5_, _f1_, and _f2_.

To honor Shadow DOM's encapsulation, we have a concept of _visibility
relationship_ between two nodes.

In the following table, "`-`" means that "node _A_ is _visible_ from node _B_".

| _A_ \ _B_ | document | a1     | a2     | b1     | c1     | d1     | d2     | e1     | f1     |
| --------- | -------- | ------ | ------ | ------ | ------ | ------ | ------ | ------ | ------ |
| document  | -        | -      | -      | -      | -      | -      | -      | -      | -      |
| a1        | -        | -      | -      | -      | -      | -      | -      | -      | -      |
| a2        | -        | -      | -      | -      | -      | -      | -      | -      | -      |
| b1        | hidden   | hidden | hidden | -      | hidden | hidden | hidden | hidden | hidden |
| c1        | hidden   | hidden | hidden | hidden | -      | hidden | hidden | hidden | hidden |
| d1        | hidden   | hidden | hidden | hidden | hidden | -      | -      | -      | -      |
| d2        | hidden   | hidden | hidden | hidden | hidden | -      | -      | -      | -      |
| e1        | hidden   | hidden | hidden | hidden | hidden | hidden | hidden | -      | hidden |
| f1        | hidden   | hidden | hidden | hidden | hidden | hidden | hidden | hidden | -      |

For example, _document_ is _visible_ from any nodes.

To understand _visibility relationship_ easily, here is a rule of thumb:

- If node _B_ can reach node _A_ by traversing an _edge_ (in the first picture
  of this section), recursively, _A_ is visible from _B_.
- However, an _edge_ of (`──/`) ( _shadowhost-shadowroot_ relationship) is
  one-directional:
  - From a shadow root to the shadow host -> Okay
  - From a shadow host to the shadow root -> Forbidden

In other words, a node in an _inner tree_ can see a node in an _outer tree_ in a
composed tree, but the opposite is not true.

We have designed (or re-designed) a bunch of Web-facing APIs to honor this basic
principle. If you add a new API to the web platform and Blink, please consider
this rule and don't _leak_ a node which should be hidden to web developers.

Warning: Unfortunately, a _composed tree_ had a different meaning in the past;
it was used to specify a _flat tree_ (which will be explained later). If you
find a wrong usage of a composed tree in Blink, please fix it.

Further Info:

- `TreeScope::ParentTreeScope()`
- `Node::IsConnected()`
- DOM Standard: [connected](https://dom.spec.whatwg.org/#connected)
- DOM Standard: [retarget](https://dom.spec.whatwg.org/#retarget)

# Flat tree

A composed tree itself can't be rendered _as is_. From the rendering's
perspective, Blink has to construct a _layout tree_, which would be used as an
input to the _paint phase_. A layout tree is a tree whose node is
`LayoutObject`, which points to `Node` in a node tree, plus additional
calculated layout information.

Before the Web Platform got Shadow DOM, the structure of a layout tree is almost
_similar_ to the structure of a document tree; where only one node tree,
_document tree_, is being involved there.

Since the Web Platform got Shadow DOM, we now have a composed tree which is
composed of multiple node trees, instead of a single node tree. That means We
have to _flatten_ the composed tree to the one node tree, called a _flat tree_,
from which a layout tree is constructed.

![flat tree](https://hayato.io/2017/dom/flat-tree.svg)

For example, given the following composed tree,

```text
document
├── a1 (host)
│   ├──/shadowRoot1
│   │   └── b1
│   └── a2 (host)
│       ├──/shadowRoot2
│       │   ├── c1
│       │   │   ├── c2
│       │   │   └── c3
│       │   └── c4
│       ├── a3
│       └── a4
└── a5
    └── a6 (host)
        └──/shadowRoot3
            └── d1
                ├── d2
                ├── d3 (host)
                │   └──/shadowRoot4
                │       ├── e1
                │       └── e2
                └── d4 (host)
                    └──/shadowRoot5
                        ├── f1
                        └── f2
```

This composed tree would be flattened into the following _flat tree_ (assuming
there are not `<slot>` elements there):

```text
document
├── a1 (host)
│   └── b1
└── a5
    └── a6 (host)
        └── d1
            ├── d2
            ├── d3 (host)
            │   ├── e1
            │   └── e2
            └── d4 (host)
                ├── f1
                └── f2
```

We can't explain the exact algorithm how to flatten a composed tree into a flat
tree until I explain the concept of _slots_ and _node distribution_ If we are
ignoring the effect of `<slot>`, we can have the following simple definition. A
flat tree can be defined as:

- A root of a flat tree: _document_
- Given node _A_ which is in a flat tree, its children are defined, recursively,
  as follows:
  - If _A_ is a shadow host, its shadow root's children
  - Otherwise, _A_'s children

# Slots and node assignments

Please see this
[nice article](https://developers.google.com/web/fundamentals/web-components/shadowdom)
how `<slot>` elements work in general.

> _Slots_ are placeholders inside your component that users can fill with their
> own markup.

Here, I'll show some examples.

## Example 1

Given the following composed tree and slot assignments,

Composed tree:

```text
A
├──/shadowRoot1
│   ├── slot1
│   └── slot2
├── B
└── C
```

Slot Assignments:

| slot  | slot's assigned nodes |
| ----- | --------------------- |
| slot1 | [C]                   |
| slot2 | [B]                   |

The flat tree would be:

```text
A
├── slot1
│   └── C
└── slot2
    └── B
```

## Example 2

More complex example is here.

Composed tree:

```text
A
├──/shadowRoot1
│   ├── B
│   │   └── slot1
│   ├── slot2
│   │   └── C
│   ├── D
│   └── slot3
│       ├── E
│       └── F
├── G
├── H
├── I
└── J
```

Slot Assignments:

| slot  | slot's assigned nodes    |
| ----- | ------------------------ |
| slot1 | [H]                      |
| slot2 | [G, I]                   |
| slot3 | [] (nothing is assigned) |

The flat tree would be:

```text
A
├── B
│   └── slot1
│       └── H
├── slot2
│   ├── G
│   └── I
├── D
└── slot3
    ├── E
    └── F
```

- `slot2`'s child, `C`, is not shown in this flat tree because `slot2` has
  non-empty assigned nodes, `[G, I]`, which are used as `slot2`'s children in
  the flat tree.
- If a slots doesn't have any assigned nodes, the slot's children are used as
  _fallback contents_ in the flat tree. e.g. `slot3`s children in the flat tree
  are `E` and `F`.
- If a host's child node is assigned to nowhere, the child is not used. e.g. `J`

## Example 3

A slot itself can be assigned to another slot.

For example, if we attach a shadow root to `B`, and put a `<slot>`, `slot4`,
inside of the shadow tree.

```text
A
├──/shadowRoot1
│   ├── B
│   │   ├──/shadowRoot2
│   │   │   └── K
│   │   │       └── slot4
│   │   └── slot1
│   ├── slot2
│   │   └── C
│   ├── D
│   └── slot3
│       ├── E
│       └── F
├── G
├── H
├── I
└── J
```

| slot  | slot's assigned nodes    |
| ----- | ------------------------ |
| slot1 | [H]                      |
| slot2 | [G, I]                   |
| slot3 | [] (nothing is assigned) |
| slot4 | [slot1]                  |

The flat tree would be:

```text
A
├── B
│   └── K
│       └── slot4
│           └── slot1
│               └── H
├── slot2
│   ├── G
│   └── I
├── D
└── slot3
    ├── E
    └── F
```

# Slot Assignment Recalc

Please see
[Incremental Shadow DOM](https://docs.google.com/document/d/1R9J8CVaSub_nbaVQwwm3NjCoZye4feJ7ft7tVe5QerM/edit?usp=sharing)
to know how assignments are recalc-ed.

# FlatTreeTraversal

Blink doesn't store nor maintain a flat tree data structure in the memory.
Instead, Blink provides a utility class,
[`FlatTreeTraversal`](./flat_tree_traversal.h), which traverses a composed tree
_in a flat tree order_.

e.g. in the above example 3,

- `FlatTreeTraversal::firstChild(slot1)` returns `H`
- `FlatTreeTraversal::parent(H)` returns `slot1`
- `FlatTreeTraversal::nextSibling(G)` returns `I`
- `FlatTreeTraversal::previousSibling(I)` returns `G`

The APIs which `FlatTreeTraversal` provides are very similar to ones other
traversal utility classes provide, such as `NodeTraversal` and
`ElementTraversal`.

## Warning

For historical reasons, Blink still supports Shadow DOM v0, where the different
node distribution mechanism is still used. To support v0, you need to call
`Node::UpdateDistributionForFlatTreeTraversal` before calling any function of
`FlatTreeTraversal`.

If you use `FlatTreeTraversal` without updating distribution, you would hit
DCHECK. :(

Since `Node::UpdateDistributionForFlatTreeTraversal` can take O(N) in the worst
case (_even if the distribution flag is clean!_), you should be careful not to
call it in hot code paths. If you are not sure, please contact
dom-dev@chromium.org, or add masonfreed@chromium.org to reviewers.

Once Blink removes Shadow DOM v0 in the future, you don't need to call
`Node::UpdateDistributionForFlatTreeTraversal` before using `FlatTreeTraversal`
beforehand in most cases, however, that wouldn't happen soon.

# Event path and Event Retargeting

<!-- Old doc: https://www.w3.org/TR/2014/WD-shadow-dom-20140617/ -->

[DOM Standard] defines how an event should be dispatched
[here](https://dom.spec.whatwg.org/#concept-event-dispatch), including how
[event path](https://dom.spec.whatwg.org/#event-path) should be calculated,
however, I wouldn't be surprised if the steps described there might look a kind
of cryptogram to you.

In this README, I'll explain how an event is dispatched and how its event path
is calculated briefly by using some relatively-understandable examples.

Basically, an event is dispatched across shadow trees.

![event dispatch](https://hayato.io/2017/dom/event-dispatch.svg)

Let me show more complex example composed tree, involving a slot:

```text
A
└── B
    ├──/shadowroot-C
    │   └── D
    │       ├──/shadowroot-E
    │       │   └── F
    │       │       └── slot-G
    │       └── H
    │           └── I
    │               ├──/shadowroot-J
    │               │   └── K
    │               │       ├──/shadowroot-L
    │               │       │   └── M
    │               │       │       ├──/shadowroot-N
    │               │       │       │   └── slot-O
    │               │       │       └── slot-P
    │               │       └── Q
    │               │           └── slot-R
    │               └── slot-S
    └── T
        └── U
```

Slot Assignments:

| slot   | slot's assigned nodes |
| ------ | --------------------- |
| slot-G | [H]                   |
| slot-O | [slot-P]              |
| slot-P | [Q]                   |
| slot-R | [slot-S]              |
| slot-S | [T]                   |

Given that, suppose that an event is fired on `U`, an event path would be (in
reverse order):

```text
[U => T => slot-S => slot-R => Q => slot-P => slot-O => shadowroot-N => M
=> shadowroot-L => K => shadowroot-J => I => H => slot-G => F => shadowroot-E
=> D => shadowroot-C => B => A]
```

Roughly speaking, an event's _parent_ (the next node in event path) is
calculated as follows:

- If a node is assigned to a slot, the _parent_ is the node's asigned slot.
- If a node is a shadow root, the _parent_ is its shadow host.
- In other cases, the _parent_ is node's parent.

In the above case, `event.target`, `U`, doesn't change in its lifetime because
`U` can be _seen_ from every nodes there. However, if an event is fired on node
`Q`, for example, `event.target` would be adjusted for some nodes in event path
to honer encapsulation. That is called _event re-targeting_.

Here is an event path for an event which is fired on `Q` :

| event.currenttarget | (re-targeted) event.target |
| ------------------- | -------------------------- |
| Q                   | Q                          |
| slot-P              | Q                          |
| slot-O              | Q                          |
| shadowroot-N        | Q                          |
| M                   | Q                          |
| shadowroot-L        | Q                          |
| K                   | Q                          |
| shadowroot-J        | Q                          |
| I                   | I                          |
| H                   | I                          |
| slot-G              | I                          |
| F                   | I                          |
| shadowroot-E        | I                          |
| D                   | I                          |
| shadowroot-C        | I                          |
| B                   | B                          |
| A                   | B                          |

# Design goal of event path calculation

TODO(hayato): Explain.

# Composed events

TODO(hayato): Explain.

# Event path and related targets

TODO(hayato): Explain.

# DOM mutations

TODO(hayato): Explain.

# Related flags

TODO(hayato): Explain.
