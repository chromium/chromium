// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DOM observer for annotation changes.
 */

import {isDecorationNode} from '//ios/web/annotations/resources/text_decoration.js';
import {isValidNode} from '//ios/web/annotations/resources/text_dom_utils.js';

// Consumer of decoration `Node` removed callback.
type TextDecorationNodeRemovedConsumer = {
  (node: Node): void;
};

// Interface for an `IntersectionObserver` that should count `observe` and
// `unobserve` call and actually `unobserve` only when the count reaches 0.
// The `TextDOMObserver` will call it for every text node and doesn't
// care about keeping track of observations state.
interface CountedIntersectionObserver {
  observe(node: Node): void;
  unobserve(node: Node): void;
}

// Class for a DOM `MutationObserver` that handles passing on to a
// `CountedIntersectionObserver` the text nodes that should be observed
// (or unobserved) for viewport intersection.
class TextDOMObserver {
  constructor(
      public root: Element,
      private intersectionObserver: CountedIntersectionObserver,
      private decorationNodeRemoved: TextDecorationNodeRemovedConsumer) {}

  // Mutation observer for handling added and removed nodes and text mutation.
  private mutationCallback = (mutationList: MutationRecord[]) => {
    for (const mutation of mutationList) {
      if (mutation.type === 'childList') {
        // Avoid observing again if this is triggered by decorating.
        for (const node of mutation.addedNodes) {
          if (!isDecorationNode(node)) {
            this.observeNodes(node);
          }
        }
        for (const node of mutation.removedNodes) {
          this.unobserveNodes(node);
          // This wasn't removed by the decorator, there's corruption.
          if (isDecorationNode(node) && node.nodeName === 'CHROME_ANNOTATION') {
            this.decorationNodeRemoved(node);
          }
        }
      } else if (mutation.type === 'characterData') {
        // Since it was probably handled and unobserved, let's observe it
        // again with its new value. The IntersectionObserver will trigger
        // right away if the `mutation.target`'s parent is visible.
        this.observeNodes(mutation.target);
      }
    }
  };

  private mutationObserver = new MutationObserver(this.mutationCallback);

  // Starts at given `node` and traverses all of its descendants, registering
  // text nodes with the IntersectionObserver.
  private observeNodes(node: Node): void {
    // Observe only nodes with text.
    if (node.nodeType === Node.TEXT_NODE) {
      this.intersectionObserver.observe(node);
    }
    if (node instanceof Element && isValidNode(node)) {
      if (node.shadowRoot && node.shadowRoot !== node as Node) {
        this.observeNodes(node.shadowRoot);
      } else if (node.hasChildNodes()) {
        for (const childNode of node.childNodes) {
          this.observeNodes(childNode);
        }
      }
    }
  }

  // Starts at given `node` and traverses all of its descendants, unregistering
  // text nodes with the IntersectionObserver.
  private unobserveNodes(node: Node): void {
    // Only nodes with text are observed.
    if (node.nodeType === Node.TEXT_NODE) {
      this.intersectionObserver.unobserve(node);
    }
    if (node instanceof Element && isValidNode(node)) {
      if (node.shadowRoot && node.shadowRoot !== node as Node) {
        this.unobserveNodes(node.shadowRoot);
      } else if (node.hasChildNodes()) {
        for (const childNode of node.childNodes) {
          this.unobserveNodes(childNode);
        }
      }
    }
  }

  // Starts the DOM observer. Scans the tree under `root` that is already loaded
  // then observes for further mutations.
  start(): void {
    this.observeNodes(this.root);
    // Only monitor DOM `element` changes and text nodes mutation.
    this.mutationObserver.observe(this.root, {
      attributes: false,
      childList: true,
      characterData: true,
      subtree: true,
      attributeOldValue: false,
      characterDataOldValue: false,
    });
  }

  // Stops the DOM observer. Also cleans `intersectionObserver` under `root`.
  stop(): void {
    this.unobserveNodes(this.root);
    this.mutationObserver.disconnect();
  }

  // Force updating before next js main thread cycle.
  updateForTesting(): void {
    this.mutationCallback(this.mutationObserver.takeRecords());
  }
}

export {
  CountedIntersectionObserver,
  TextDOMObserver,
  TextDecorationNodeRemovedConsumer
}
