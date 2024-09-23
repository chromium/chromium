// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Monitor and extract visible text on the page and pass it on to
 * the annotations manager.
 */

import {CountedIntersectionObserver} from '//ios/web/annotations/resources/text_dom_observer.js';
import {ElementWithSymbolIndex, HTMLElementWithSymbolIndex, isValidNode, NodeWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {IdleTaskTracker} from '//ios/web/annotations/resources/text_tasks.js';

// Delay before starting text extraction.
const EXTRACTION_TIMEOUT_MS = 300;

// Using Symbol as property key ensure the data doesn't show up in
// property key lists.

// Tagged on an `Element` that is is visible according to IntersectionObserver.
const visibleElement = Symbol('visibleElement');

// Tagged on parent chain of every element with `visibleElement`. It maintains
// a count of how many descendants are visible and is used to avoid going
// down uselessly a branch of the DOM that is 100% not visible when extracting
// text.
const visibleDescendantCount = Symbol('visibleDescendantCount');

// Tagged on an text `Node` that contribute to their parent element
// `observedTextNodeCount`.
const observedNode = Symbol('observedNode');

// Tagged on an `Element` that is is visible according to IntersectionObserver.
// The attached value contains the number of text nodes child that have
// requested observation.
const observedTextNodeCount = Symbol('observedTextNodeCount');

// Interface to used parts of `IntersectionObserver`. Can be mocked easily.
class InternalIntersectionObserver {
  constructor(
      _callback: IntersectionObserverCallback,
      _options?: IntersectionObserverInit) {}
  disconnect(): void {}
  observe(_target: Element): void {}
  unobserve(_target: Element): void {}
}

// Real time `IntersectionObserverInterface` based on `IntersectionObserver`.
class LiveIntersectionObserver implements InternalIntersectionObserver {
  private observer: IntersectionObserver;

  constructor(
      callback: IntersectionObserverCallback,
      options?: IntersectionObserverInit) {
    this.observer = new IntersectionObserver(callback, options);
  }
  disconnect(): void {
    this.observer.disconnect();
  }
  observe(target: Element): void {
    this.observer.observe(target);
  }
  unobserve(target: Element): void {
    this.observer.unobserve(target);
  }
}

// Interface for objects wanting to visit the visible part of the DOM.
interface TextNodeVisitor {
  // Called when starting the visit.
  begin(): void;
  // Called for visible text `textNode` with `textContent` not null.
  visibleTextNode(textNode: Text): void;
  // Called for invisible `node` between `visibleTextNode`s.
  invisibleNode(node: Node): void;
  // Called before entering `node` subtree.
  enterVisibleNode(node: Node): void;
  // Called after leaving `node` subtree.
  leaveVisibleNode(node: Node): void;
  // Called when ending the visit.
  end(): void;
}

class TextIntersectionObserver implements CountedIntersectionObserver {
  private intersectionOptions = {
    // Monitor viewport.
    root: null,
    // Make intersect window bigger to prepare potential incoming (scrolling)
    // intents.
    rootMargin: '100px',
    // Catch any node partially in (extended) viewport.
    threshold: 0,
  };

  // IntersectionObserver can only observe `Element` objects, not `Node`s
  // like text nodes. To cope with that, we observe the text nodes parent, and
  // when they are called visible, we take for granted all their text nodes are
  // too (which is probably not true all the time, but there's no fast and
  // obvious solution).
  private observer: InternalIntersectionObserver|null = null;

  constructor(
      public root: Element, public visitor: TextNodeVisitor,
      private idleTaskTracker: IdleTaskTracker,
      private observerClass:
          typeof InternalIntersectionObserver = LiveIntersectionObserver,
      private visitAfterDelayMs = EXTRACTION_TIMEOUT_MS) {}

  // Cleanup visibility tags.
  private cleanup(): void {
    const traverseVisible = (node: NodeWithSymbolIndex) => {
      if (!isValidNode(node)) {
        return;
      }
      if (node instanceof Element && node.shadowRoot &&
          node.shadowRoot !== node as Node) {
        traverseVisible(node.shadowRoot);
      } else if (node.hasChildNodes()) {
        for (const childNode of node.childNodes as
             NodeListOf<NodeWithSymbolIndex>) {
          if (childNode[visibleDescendantCount] || childNode[visibleElement]) {
            traverseVisible(childNode);
          }
        }
        if (node[visibleElement] && node instanceof Element) {
          this.untagVisibleElement(node);
        }
      }
    };
    traverseVisible(this.root);
  }

  // Extracts visible text that hasn't been processed.
  // Releases intersection observation, so this will not trigger again and be
  // extracted only once. Unless node's text is mutated and the domObserver
  // re-adds it to the intersection observer.
  private visit(visitor: TextNodeVisitor): void {
    // DFS traversal to locate visible elements, in rendering order.
    const traverseVisible = (node: NodeWithSymbolIndex) => {
      if (!isValidNode(node)) {
        return;
      }
      if (node instanceof Element && node.shadowRoot &&
          node.shadowRoot !== node as Node) {
        traverseVisible(node.shadowRoot);
      } else if (node.hasChildNodes()) {
        const visible = node[visibleElement];
        for (const childNode of node.childNodes as
             NodeListOf<NodeWithSymbolIndex>) {
          if (visible && childNode.nodeType === Node.TEXT_NODE) {
            visitor.visibleTextNode(childNode as Text);
            this.unobserve(childNode);
          } else if (
              childNode[visibleDescendantCount] || childNode[visibleElement]) {
            visitor.enterVisibleNode(childNode);
            traverseVisible(childNode);
            visitor.leaveVisibleNode(childNode);
          } else {
            visitor.invisibleNode(childNode);
          }
        }
        if (visible && node instanceof Element) {
          this.untagVisibleElement(node);
        }
      }
    };
    visitor.begin();
    traverseVisible(this.root);
    visitor.end();
  }

  // Singleton function for text extraction. Needs to be a singleton for the
  // `idleTaskTracker` to replace an already scheduled extraction.
  private textExtractionTask = () => {
    this.visit(this.visitor);
  };

  // `IntersectionObserver` used to tag visibility of elements.
  private intersectionCallback: IntersectionObserverCallback = (entries) => {
    let updateNeeded = false;
    entries.forEach((entry) => {
      if (entry.isIntersecting) {
        this.tagVisibleElement(entry.target);
        updateNeeded = true;
      } else {
        this.untagVisibleElement(entry.target);
      }
    });
    if (updateNeeded) {
      this.idleTaskTracker.schedule(
          this.textExtractionTask, this.visitAfterDelayMs);
    }
  };

  // Tags given `element` with `visibleElement` symbol and updates the parent
  // chain of `visibleDescendantCount` tags.
  private tagVisibleElement(element: Element): void {
    let item: ElementWithSymbolIndex|null = element as ElementWithSymbolIndex;
    let parent: ElementWithSymbolIndex|null;
    if (item[visibleElement]) {
      return;
    }
    item[visibleElement] = true;
    while (item !== null && item !== this.root) {
      if (item instanceof ShadowRoot) {
        parent = item.host as ElementWithSymbolIndex;
      } else {
        parent = item.parentElement;
      }
      if (parent) {
        parent[visibleDescendantCount] =
            (parent[visibleDescendantCount] ?? 0) + 1;
      }
      item = parent;
    }
  }

  // Untags given `element` `visibleElement` symbol and updates the parent chain
  // of `visibleDescendantCount` tags.
  private untagVisibleElement(element: Element): void {
    let item: ElementWithSymbolIndex|null = element as ElementWithSymbolIndex;
    let parent: ElementWithSymbolIndex|null;
    if (!item[visibleElement]) {
      // It happens...
      return;
    }
    delete item[visibleElement];
    while (item !== null && item !== this.root) {
      if (item instanceof ShadowRoot) {
        parent = item.host as ElementWithSymbolIndex;
      } else {
        parent = item.parentElement;
      }
      if (parent) {
        if (parent[visibleDescendantCount] > 1) {
          parent[visibleDescendantCount] = parent[visibleDescendantCount] - 1;
        } else {
          delete parent[visibleDescendantCount];
        }
      }
      item = parent;
    }
  }

  // Mark: CountedIntersectionObserver

  observe(node: NodeWithSymbolIndex): void {
    // Already observed and counted.
    if (node[observedNode]) {
      return;
    }

    const element = node.parentElement as HTMLElementWithSymbolIndex;
    if (!element || !isValidNode(element)) {
      return;
    }

    node[observedNode] = true;

    let count = element[observedTextNodeCount] ?? 0;
    if (count === 0) {
      this.observer?.observe(element);
    }
    element[observedTextNodeCount] = count + 1;
  }

  unobserve(node: NodeWithSymbolIndex): void {
    // Not observed and counted.
    if (!node[observedNode]) {
      return;
    }

    delete node[observedNode];

    const element = node.parentElement as HTMLElementWithSymbolIndex;
    if (!element || !isValidNode(element)) {
      return;
    }
    let count = element[observedTextNodeCount] ?? 0;
    if (count === 1) {
      this.observer?.unobserve(element);
      delete element[observedTextNodeCount];
    } else {
      element[observedTextNodeCount] = count - 1;
    }
  }

  // Mark: Public API

  // Starts the intersection observer.
  start(): void {
    this.observer = new this.observerClass(
        this.intersectionCallback, this.intersectionOptions);
  }

  // Stops the intersection observer.
  stop(): void {
    this.cleanup();
    this.observer?.disconnect();
    this.observer = null;
  }
}

export {
  EXTRACTION_TIMEOUT_MS,
  visibleElement,
  visibleDescendantCount,
  observedNode,
  observedTextNodeCount,
  TextIntersectionObserver,
  TextNodeVisitor,
  InternalIntersectionObserver,
  LiveIntersectionObserver
}
