// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Library for generating and caching unique IDs for DOM nodes.
 */

import {getOrCreateUniqueNodeId, hasUniqueNodeId, safeOwnerDocument} from '//components/autofill/ios/form_util/resources/unique_node_id.js';

const DOM_NODE_ID_MANAGER_SYMBOL = Symbol.for('__gCrWebDomNodeIdManager');

class DomNodeIdManager {
  private readonly domNodeReverseMap = new Map<number, WeakRef<Node>>();
  private readonly domNodeRegistry = new FinalizationRegistry<number>((id) => {
    this.domNodeReverseMap.delete(id);
  });

  /**
   * Gets the ID for a node, creating one if it doesn't exist.
   */
  getOrCreateNodeId(node: Node): number {
    const id = getOrCreateUniqueNodeId(node);
    if (!this.domNodeReverseMap.has(id)) {
      this.domNodeReverseMap.set(id, new WeakRef(node));
      this.domNodeRegistry.register(node, id);
    }
    return id;
  }

  /**
   * Gets the ID for a node if it already exists.
   */
  getNodeId(node: Node): number|null {
    if (hasUniqueNodeId(node)) {
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
