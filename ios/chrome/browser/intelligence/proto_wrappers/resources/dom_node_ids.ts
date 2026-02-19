// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Library for generating and caching unique IDs for DOM nodes.
 */

declare global {
  interface Window {
    __gCrWebDomNodeIdMap?: WeakMap<Node, number>;
    __gCrWebNextDomNodeId?: number;
  }
}

// TODO(crbug.com/484985334): Look into using this for generating the IDs for
// autofill. Both could use the same pool of IDs.
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
  const nodeWindow =
      (node instanceof Document ? node : node.ownerDocument)?.defaultView;
  if (!nodeWindow) {
    return null;
  }

  // Initialize the map and counter if they don't exist.
  if (!nodeWindow.__gCrWebDomNodeIdMap) {
    nodeWindow.__gCrWebDomNodeIdMap = new WeakMap<Node, number>();
    nodeWindow.__gCrWebNextDomNodeId = 1;
  }

  let id = nodeWindow.__gCrWebDomNodeIdMap.get(node);
  if (id === undefined) {
    // Assign an ID to the node.
    id = nodeWindow.__gCrWebNextDomNodeId!;
    nodeWindow.__gCrWebNextDomNodeId = id + 1;
    nodeWindow.__gCrWebDomNodeIdMap.set(node, id);
  }
  return id;
}

/**
 * Gets the ID for a node if it already exists.
 * Does NOT create a new ID if one is missing.
 *
 * @param node The node to get the ID for.
 * @return The ID for the node (>= 1), or null if it doesn't exist.
 */
export function getNodeId(node: Node): number|null {
  const nodeWindow =
      (node instanceof Document ? node : node.ownerDocument)?.defaultView;
  if (!nodeWindow) {
    return null;
  }

  if (!nodeWindow.__gCrWebDomNodeIdMap) {
    return null;
  }

  return nodeWindow.__gCrWebDomNodeIdMap.get(node) ?? null;
}
