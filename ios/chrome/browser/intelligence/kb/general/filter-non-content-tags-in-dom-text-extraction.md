---
name: ios/filter-non-content-tags-in-dom-text-extraction
description: When extracting DOM text where innerText is unsupported, traverse nodes directly to filter non-content tags rather than calling cloneNode(true)
rubric: correctness
severity: should_fix
---

**Rule.** When extracting user-visible text from DOM subtrees in injected
TypeScript scripts where `innerText` is unavailable or unsuitable (such as SVG
subtrees in WebKit), do not use `cloneNode(true)` to duplicate the subtree for
sanitization, and do not read raw `textContent` without filtering. Traverse
existing nodes in place (such as via `document.createTreeWalker`) and reject
non-rendered, metadata, and script elements (e.g. `<script>`, `<style>`,
`<metadata>`, `<title>`, `<desc>`).

**Why.** Deep cloning DOM subtrees duplicates all element, attribute, and text
nodes in memory, causing memory bloat and garbage collection pressure on large
DOM subtrees under iOS memory limits. Furthermore, reading `.textContent`
directly concatenates descendant text indiscriminately, leaking raw scripts,
styles, and metadata into extracted user-facing text.

## Examples

Bad:

```cpp
// Cloning DOM subtrees causes memory bloat, and reading raw textContent leaks script/style tags.
const clone = domNode.cloneNode(true) as Element;
const hiddenElements = clone.querySelectorAll('title, desc');
hiddenElements.forEach(e => e.remove());
const innerText = clone.textContent || '';
```

Good:

```cpp
// Traverse existing nodes directly with TreeWalker without cloning the DOM subtree.
const filter: NodeFilter = {
  acceptNode(node: Node) {
    if (node.nodeType === Node.ELEMENT_NODE) {
      const tag = (node as Element).tagName.toLowerCase();
      if (['script', 'style', 'metadata', 'title', 'desc'].includes(tag)) {
        return NodeFilter.FILTER_REJECT;
      }
      return NodeFilter.FILTER_SKIP;
    }
    return node.nodeType === Node.TEXT_NODE ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_SKIP;
  },
};
const walker = document.createTreeWalker(domNode, NodeFilter.SHOW_ALL, filter);
```
