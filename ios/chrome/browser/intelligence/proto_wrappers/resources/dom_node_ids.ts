// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Library for generating and caching unique IDs for DOM nodes.
 */

const DOM_NODE_ID_MANAGER_SYMBOL = Symbol.for('__gCrWebDomNodeIdManager');
const UNIQUE_ID_SYMBOL = Symbol.for('__gCrUniqueID');

interface IndexableDocument extends Document {
  [UNIQUE_ID_SYMBOL]?: number;
}

interface IndexableElement extends Element {
  [UNIQUE_ID_SYMBOL]?: number;
}

const nodeTypeGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'nodeType')?.get;
const ownerDocumentGetter =
    Object.getOwnPropertyDescriptor(Node.prototype, 'ownerDocument')?.get;

// Returns the nodeType of the node. Directly calls the prototype getter to
// protect against DOM clobbering (e.g. `<form><input name="nodeType">`).
function safeNodeType(node: Node): number {
  return nodeTypeGetter ? nodeTypeGetter.call(node) : node.nodeType;
}

// Returns whether the node is an Element. Uses `safeNodeType` to work across
// same-origin frame realms where `instanceof Element` is false.
function isElementNode(node: Node): node is IndexableElement {
  return safeNodeType(node) === Node.ELEMENT_NODE;
}

// Returns the owner document of the node, or the node itself if it is already a
// Document. Directly calls the prototype to protect against DOM clobbering and
// checks `nodeType === Node.DOCUMENT_NODE` to work across same-origin frame
// realms where `instanceof Document` is false.
export function safeOwnerDocument(node: Node): Document|null {
  if (safeNodeType(node) === Node.DOCUMENT_NODE) {
    return node as Document;
  }
  return ownerDocumentGetter ? ownerDocumentGetter.call(node) :
                               node.ownerDocument;
}

class DomNodeIdManager {
  private readonly domNodeIdMap = new WeakMap<Node, number>();
  private readonly domNodeReverseMap = new Map<number, WeakRef<Node>>();
  private readonly domNodeRegistry = new FinalizationRegistry<number>((id) => {
    this.domNodeReverseMap.delete(id);
  });

  /**
   * Gets the ID for a node, creating one if it doesn't exist.
   */
  getOrCreateNodeId(node: Node): number {
    const existingId = this.domNodeIdMap.get(node);
    if (existingId !== undefined) {
      return existingId;
    }

    const ownerDoc = safeOwnerDocument(node) as IndexableDocument;
    ownerDoc[UNIQUE_ID_SYMBOL] ??= 1;

    // Only stamp `UNIQUE_ID_SYMBOL` on Element nodes. Non-element nodes (like
    // Document and Text nodes) also receive IDs, and setting
    // `[UNIQUE_ID_SYMBOL]` on a Document would overwrite the document counter.
    const id = isElementNode(node) ?
        (node[UNIQUE_ID_SYMBOL] ??= ownerDoc[UNIQUE_ID_SYMBOL]++) :
        ownerDoc[UNIQUE_ID_SYMBOL]++;

    this.domNodeIdMap.set(node, id);
    this.domNodeReverseMap.set(id, new WeakRef(node));
    this.domNodeRegistry.register(node, id);
    return id;
  }

  /**
   * Gets the ID for a node if it already exists.
   */
  getNodeId(node: Node): number|null {
    const id = this.domNodeIdMap.get(node);
    if (id !== undefined) {
      return id;
    }
    if (isElementNode(node) && node[UNIQUE_ID_SYMBOL] !== undefined) {
      return this.getOrCreateNodeId(node);
    }
    return null;
  }

  /**
   * Gets a node by its assigned ID, if it still exists in the DOM.
   */
  getNodeById(id: number): Node|null {
    const weakRef = this.domNodeReverseMap.get(id);
    if (!weakRef) {
      return null;
    }

    const node = weakRef.deref();
    if (!node) {
      // The node was garbage collected. Clean up the stale map entry.
      this.domNodeReverseMap.delete(id);
      return null;
    }
    if (!node.isConnected) {
      return null;
    }
    return node;
  }
}

declare global {
  interface Window {
    [DOM_NODE_ID_MANAGER_SYMBOL]?: DomNodeIdManager;
  }
}

function getManager(nodeWindow: Window): DomNodeIdManager {
  if (!nodeWindow[DOM_NODE_ID_MANAGER_SYMBOL]) {
    nodeWindow[DOM_NODE_ID_MANAGER_SYMBOL] = new DomNodeIdManager();
  }
  return nodeWindow[DOM_NODE_ID_MANAGER_SYMBOL];
}

// TODO(crbug.com/484985334): Extract and unify the shared document-scoped
// unique ID generation with Autofill (`renderer_id.ts`).
/**
 * Gets the ID for a node, creating one if it doesn't exist.
 * Uses a global map and counter stored on the window tied to the `node`.
 *
 * @param node The node to get or create an ID for.
 * @return The ID for the node, or null if it can't be obtained. Node IDs start
 *     at 1.
 */
export function getOrCreateNodeId(node: Node): number|null {
  // Get the window tied to the node.
  const nodeWindow = safeOwnerDocument(node)?.defaultView;
  if (!nodeWindow) {
    return null;
  }

  return getManager(nodeWindow).getOrCreateNodeId(node);
}

/**
 * Gets the ID for a node if it already exists.
 * Does NOT create a new ID if one is missing.
 *
 * @param node The node to get the ID for.
 * @return The ID for the node (>= 1), or null if it doesn't exist.
 */
export function getNodeId(node: Node): number|null {
  const nodeWindow = safeOwnerDocument(node)?.defaultView;
  if (!nodeWindow) {
    return null;
  }

  return getManager(nodeWindow).getNodeId(node);
}

/**
 * Gets a node by its assigned ID, if it still exists in the DOM.
 * @param id The ID to look for.
 * @param nodeWindow The window where the node resides.
 * @return The matching node or null if not found.
 */
export function getNodeById(id: number, nodeWindow: Window): Node|null {
  return getManager(nodeWindow).getNodeById(id);
}
