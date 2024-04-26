// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_dom_observer.ts.
 */

import {replacementNodeDecorationId} from '//ios/web/annotations/resources/text_decoration.js';
import {CountedIntersectionObserver, TextDOMObserver} from '//ios/web/annotations/resources/text_dom_observer.js';
import {HTMLElementWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {expectEq, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextDOMObserver extends TestSuite implements
    CountedIntersectionObserver {
  corrupted: Node[] = [];

  // Mark: TextDecorationNodeRemovedConsumer
  decorationNodeRemovedConsumer = (node: Node): void => {
    this.corrupted.push(node);
  };

  // Mark: CountedIntersectionObserver
  observed = new Map<string, number>();

  observe(node: Node): void {
    const element = node.parentElement;
    if (element) {
      this.observed.set(element.id, (this.observed.get(element.id) || 0) + 1);
    }
  }

  unobserve(node: Node): void {
    const element = node.parentElement!;
    if (element) {
      this.observed.set(element.id, (this.observed.get(element.id) || 0) - 1);
    }
  }

  // Mark: Tests

  observer = new TextDOMObserver(
      document.documentElement, this, this.decorationNodeRemovedConsumer);

  override setUp(): void {
    this.observed.clear();
    this.corrupted.length = 0;
    this.observer.start();
  }

  override tearDown(): void {
    this.observer.stop();
  }

  // TODO(crbug.com/40936184): add test for shadowRoot.

  // Tests the observer works correctly when nodes are added or removed and when
  // text is mutated.
  testTextDOMObserverFlow() {
    load(
        '<div id="d0">' +
        '<div id="d1">Hello</div>' +
        '<div id="d2">to <span id="b0">this</span> nice</div>' +
        '<div id="d3">World</div>' +
        '</div>');
    this.observer.updateForTesting();
    expectEq(this.observed.size, 4);

    // Each observed entry should hold the number of text nodes under it.
    expectEq(this.observed.get('d1'), 1);
    expectEq(this.observed.get('d2'), 2);
    expectEq(this.observed.get('d3'), 1);
    expectEq(this.observed.get('b0'), 1);

    // Add a node.
    const parent = document.querySelector('#d3');
    const node = document.createElement('span');
    node.id = 's0';
    node.textContent = '!';
    parent?.appendChild(node);
    this.observer.updateForTesting();
    expectEq(this.observed.get('d3'), 1);
    expectEq(this.observed.get('s0'), 1);

    // Remove a node.
    document.querySelector('#d1')?.remove();
    this.observer.updateForTesting();
    expectEq(this.observed.get('d1'), 0);

    document.querySelector('#d2')?.remove();
    this.observer.updateForTesting();
    expectEq(this.observed.get('b0'), 0);
    expectEq(this.observed.get('d2'), 0);

    // Change a node.
    document.querySelector('#d3')!.textContent = 'Bye Bye!';
    this.observer.updateForTesting();
    expectEq(this.observed.get('d3'), 2);
  }

  // Tests the NodeRemovedConsumer is called when 3p corrupts an annotation.
  testTextDecorationNodeRemovedConsumer() {
    load(
        '<div id="d0">' +
        '<div id="d1">Hello</div>' +
        '<CHROME_ANNOTATION id="d2">to the</CHROME_ANNOTATION>' +
        '<div id="d3">World</div>' +
        '</div>');
    this.observer.updateForTesting();
    expectEq(this.observed.size, 2);

    // Make annotation 'decorated'.
    let annotation =
        document.querySelector('#d2') as HTMLElementWithSymbolIndex;
    annotation[replacementNodeDecorationId] = 'd2';

    // Now have 3p remove it without going through decorator.
    annotation.remove();
    this.observer.updateForTesting();

    // Expect it to be reported as corrupted.
    expectEq(this.corrupted.length, 1);
    expectEq(this.corrupted[0], annotation);
  }
}

export {TestTextDOMObserver}
