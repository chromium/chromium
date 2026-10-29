---
name: blink/content_extraction/avoid-speculative-dom-node-id-allocation
description: >-
  Avoid calling DOMNodeIds::IdForNode() speculatively during indexing or
  read-only lookups; key transient maps by Member<const Node> and use
  ExistingIdForNode() for membership checks.
rubric: perf
severity: should_fix
---

**Rule.** Do not call `DOMNodeIds::IdForNode()` speculatively when indexing,
filtering, or checking whether a node is tracked. `DOMNodeIds::IdForNode(node)`
mutates a process-wide Blink weak map and allocates a new `DOMNodeId` if the
node does not already have one. Key per-traversal lookup tables (such as
hit-test z-order maps) by `Member<const Node>` in an Oilpan `HeapHashMap`, and
use `DOMNodeIds::ExistingIdForNode(node)` when checking whether a node is in a
set of tracked IDs (`interactive_dom_node_ids_`). Only call
`DOMNodeIds::IdForNode(node)` after confirming the ID will be emitted.

**Why.** Calling `DOMNodeIds::IdForNode()` on every visited or hit-tested `Node`
allocates thousands of permanent entries in Blink's global `DOMNodeIds` map for
nodes that are never emitted or whose IDs are suppressed by `node_id_allowlist`.

## Examples

Bad:

```cpp
// BUG 1: Mints a global DOMNodeId for every hit-tested node during indexing,
// even if the node is never emitted in the extracted tree!
HashMap<DOMNodeId, int32_t> node_to_rank;
for (const auto& hit_node : hit_test_results) {
  node_to_rank.insert(DOMNodeIds::IdForNode(hit_node.Get()), next_rank++);
}

// BUG 2: Mints a new DOMNodeId just to check whether the node is in a set!
bool IsTrackedNode(const Node* node) const {
  return tracked_node_ids_.Contains(DOMNodeIds::IdForNode(node));
}
```

Good:

```cpp
// 1. Key transient per-walk maps by Member<const Node>:
HeapHashMap<Member<const Node>, int32_t> node_to_rank;
for (const auto& hit_node : hit_test_results) {
  node_to_rank.insert(hit_node.Get(), next_rank++);
}

// 2. Use ExistingIdForNode() for read-only membership checks:
bool IsTrackedNode(const Node* node) const {
  return tracked_node_ids_.Contains(DOMNodeIds::ExistingIdForNode(node));
}
```
