// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_intersection_observer.ts.
 */

import {HTMLElementWithSymbolIndex, NodeWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {InternalIntersectionObserver, observedNode, observedTextNodeCount, TextIntersectionObserver, TextNodeVisitor, visibleDescendantCount, visibleElement} from '//ios/web/annotations/resources/text_intersection_observer.js';
import {IdleTaskTracker} from '//ios/web/annotations/resources/text_tasks.js';
import {expectEq, expectNeq, FakeTaskTimer, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

let currentObserver: FakeIntersectionObserver|null = null;

class FakeIntersectionObserver implements InternalIntersectionObserver {
  public connected = false;
  public observed = new Set<Element>();

  constructor(
      public callback: IntersectionObserverCallback,
      _options?: IntersectionObserverInit) {
    currentObserver = this;
    this.connected = true;
  }

  disconnect(): void {
    this.connected = false;
  }
  observe(target: Element): void {
    this.observed.add(target);
  }
  unobserve(target: Element): void {
    this.observed.delete(target);
  }

  // Simulates viewport intersection hits on given `items`.
  hits(items: {target: Element, isIntersecting: boolean}[]): void {
    const entries: IntersectionObserverEntry[] = [];
    for (let item of items) {
      entries.push(new IntersectionObserverEntry({
        boundingClientRect: {},
        intersectionRatio: item.isIntersecting ? 1 : 0,
        intersectionRect: {},
        isIntersecting: item.isIntersecting,
        rootBounds: null,
        target: item.target,
        time: 0
      }));
    }
    this.callback(entries, null as unknown as IntersectionObserver);
  }
}

class TestTextIntersectionObserver extends TestSuite implements
    TextNodeVisitor {
  // Mark: TextNodeVisitor

  visibleText = '';
  invisibleNodes: Node[] = [];
  invisibleNodeNames = '';
  flowState = 'idle';

  begin() {
    this.flowState += '-begin';
  }
  visibleTextNode(textNode: Text): void {
    this.visibleText += '+' + textNode.textContent;
  }
  invisibleNode(node: Node): void {
    this.invisibleNodes.push(node);
    this.invisibleNodeNames += ':' + node.nodeName;
  }
  enterVisibleNode(node: Node): void {
    this.visibleText += '+<' + node.nodeName + '>';
  }
  leaveVisibleNode(node: Node): void {
    this.visibleText += '+</' + node.nodeName + '>';
  }
  end(): void {
    this.flowState += '-end';
  }

  // Mark: Tests

  timer = new FakeTaskTimer();
  tracker = new IdleTaskTracker(this.timer, 100, 50);
  observer = new TextIntersectionObserver(
      document.documentElement, this, this.tracker, FakeIntersectionObserver,
      50);

  override setUp(): void {
    currentObserver = null;
    this.timer.restart();
    this.visibleText = '';
    this.invisibleNodes.length = 0;
    this.invisibleNodeNames = '';
    this.flowState = 'idle';
    this.observer.start();
    expectNeq(currentObserver, null);
  }

  override tearDown(): void {
    this.observer.stop();
    currentObserver = null;
  }

  // TODO(crbug.com/40936184): add test for shadowRoot.

  // Tests the proper tagging of nodes depending on events from
  // IntersectionObserver (the fake one above). also tests that the visiting of
  // nodes after the given delay happened.
  testTextIntersectionObserverFlow() {
    load(
        '<div id="d1">Hello</div>' +
        '<div id="d2">Small</div>' +
        '<div id="d3">World</div>');
    const html = document.documentElement as HTMLElementWithSymbolIndex;
    const body = document.body as HTMLElementWithSymbolIndex;
    const d1 = document.querySelector('#d1') as HTMLElementWithSymbolIndex;
    const d2 = document.querySelector('#d2') as HTMLElementWithSymbolIndex;
    const d3 = document.querySelector('#d3') as HTMLElementWithSymbolIndex;

    this.observer.observe(d1.childNodes[0]!);
    this.observer.observe(d2.childNodes[0]!);
    this.observer.observe(d3.childNodes[0]!);
    expectEq(currentObserver?.observed.size, 3);

    expectEq(undefined, html[visibleDescendantCount]);
    expectEq(undefined, body[visibleDescendantCount]);
    expectEq(false, !!d1[visibleElement]);
    expectEq(false, !!d2[visibleElement]);
    expectEq(false, !!d3[visibleElement]);
    expectEq(1, d1[observedTextNodeCount]);
    expectEq(1, d2[observedTextNodeCount]);
    expectEq(1, d3[observedTextNodeCount]);
    expectEq(true, !!(d1.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(true, !!(d2.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(true, !!(d3.childNodes[0] as NodeWithSymbolIndex)[observedNode]);

    // Make d2 visible.
    currentObserver?.hits([{target: d2, isIntersecting: true}]);
    expectEq(1, html[visibleDescendantCount]);
    expectEq(1, body[visibleDescendantCount]);
    expectEq(false, !!d1[visibleElement]);
    expectEq(true, !!d2[visibleElement]);
    expectEq(false, !!d3[visibleElement]);
    expectEq(1, d1[observedTextNodeCount]);
    expectEq(1, d2[observedTextNodeCount]);
    expectEq(1, d3[observedTextNodeCount]);
    expectEq(true, !!(d1.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(true, !!(d2.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(true, !!(d3.childNodes[0] as NodeWithSymbolIndex)[observedNode]);

    this.timer.moveAhead(/* ms= */ 10, /* times= */ 6);  // -> 60ms total

    // Check that the visit happened.
    expectEq(this.visibleText, '+<BODY>+<DIV>+Small+</DIV>+</BODY>');
    expectEq(this.invisibleNodeNames, ':HEAD:DIV:DIV');
    expectEq(this.invisibleNodes[1], d1);
    expectEq(this.invisibleNodes[2], d3);
    expectEq(this.flowState, 'idle-begin-end');
    // d2 should not be observed anymore.
    expectEq(currentObserver?.observed.size, 2);
    expectEq(undefined, d2[observedTextNodeCount]);
    expectEq(false, !!(d2.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    // And not visible.
    expectEq(false, !!d2[visibleElement]);
    expectEq(undefined, html[visibleDescendantCount]);
    expectEq(undefined, body[visibleDescendantCount]);
  }

  // Tests the proper tagging/untagging of nodes depending on events from
  // IntersectionObserver when simulating a viewport scrolling down.
  testTextIntersectionObserverScroll() {
    load(
        '<div id="d1">Hello</div>' +
        '<div id="d2">Small</div>' +
        '<div id="d3">World</div>');
    const html = document.documentElement as HTMLElementWithSymbolIndex;
    const body = document.body as HTMLElementWithSymbolIndex;
    const d1 = document.querySelector('#d1') as HTMLElementWithSymbolIndex;
    const d2 = document.querySelector('#d2') as HTMLElementWithSymbolIndex;
    const d3 = document.querySelector('#d3') as HTMLElementWithSymbolIndex;
    this.observer.observe(d1.childNodes[0]!);
    this.observer.observe(d2.childNodes[0]!);
    this.observer.observe(d3.childNodes[0]!);
    expectEq(currentObserver?.observed.size, 3);

    // Make d1 visible.
    currentObserver?.hits([{target: d1, isIntersecting: true}]);
    this.timer.moveAhead(/* ms= */ 10, /* times= */ 6);  // -> 60ms total
    // Check that the visit happened.
    expectEq(this.visibleText, '+<BODY>+<DIV>+Hello+</DIV>+</BODY>');
    expectEq(this.invisibleNodeNames, ':HEAD:DIV:DIV');
    expectEq(this.invisibleNodes[1], d2);
    expectEq(this.invisibleNodes[2], d3);
    expectEq(this.flowState, 'idle-begin-end');

    // Make d2 visible.
    currentObserver?.hits([{target: d2, isIntersecting: true}]);
    this.timer.moveAhead(/* ms= */ 10, /* times= */ 2);  // -> 80ms total
    // But before text extraction, make it invisible.
    currentObserver?.hits([{target: d2, isIntersecting: false}]);
    this.timer.moveAhead(/* ms= */ 10, /* times= */ 6);  // -> 140ms total
    expectEq(this.visibleText, '+<BODY>+<DIV>+Hello+</DIV>+</BODY>');
    expectEq(this.flowState, 'idle-begin-end-begin-end');

    // Make d3 visible.
    currentObserver?.hits([{target: d3, isIntersecting: true}]);
    this.timer.moveAhead(/* ms= */ 10, /* times= */ 6);  // -> 200ms total
    expectEq(
        this.visibleText,
        '+<BODY>+<DIV>+Hello+</DIV>+</BODY>+<BODY>+<DIV>+World+</DIV>+</BODY>');
    expectEq(this.flowState, 'idle-begin-end-begin-end-begin-end');

    // d1 and d3 should not be observed anymore, d2 should.
    expectEq(undefined, html[visibleDescendantCount]);
    expectEq(undefined, body[visibleDescendantCount]);
    expectEq(false, !!d1[visibleElement]);
    expectEq(false, !!d2[visibleElement]);
    expectEq(false, !!d3[visibleElement]);
    expectEq(undefined, d1[observedTextNodeCount]);
    expectEq(1, d2[observedTextNodeCount]);
    expectEq(undefined, d3[observedTextNodeCount]);
    expectEq(false, !!(d1.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(true, !!(d2.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
    expectEq(false, !!(d3.childNodes[0] as NodeWithSymbolIndex)[observedNode]);
  }
}

export {TestTextIntersectionObserver}
