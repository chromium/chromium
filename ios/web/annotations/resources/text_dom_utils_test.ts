// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_dom_utils.ts.
 */

import {hasNoIntentDetection, isValidNode, nextLeaf, noFormatDetectionTypes, previousLeaf} from '//ios/web/annotations/resources/text_dom_utils.js';
import {expectEq, load, loadHead, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestDomUtils extends TestSuite {
  // Builds a text string by navigating leaf nodes backward.
  iterateOnPreviousLeaf(node: Node|null, breakAtInvalid: boolean): string {
    let result = '';
    while (node) {
      result += node.textContent;
      node = previousLeaf(node, breakAtInvalid);
    }
    return result;
  }

  // Builds a text string by navigating leaf nodes forward.
  iterateOnNextLeaf(node: Node|null, breakAtInvalid: boolean): string {
    let result = '';
    while (node) {
      result += node.textContent;
      node = nextLeaf(node, breakAtInvalid);
    }
    return result;
  }

  // Tests `previousLeaf` and `nextLeaf` DOM tree navigation.
  testPreviousAndNextLeaf() {
    load(
        '<div>' +
        '<div><span id="first">1</span>2<span>3</span></div>' +
        '<div><span>4<A>5</A></span>6</div>' +
        '<div>7<span><span id="last">8</span></span></div>' +
        '</div>');
    let first: Node|null = document.querySelector('#first');
    let last: Node|null = document.querySelector('#last');
    expectEq(this.iterateOnPreviousLeaf(first, true), '1');
    expectEq(this.iterateOnPreviousLeaf(first, false), '1');
    expectEq(this.iterateOnPreviousLeaf(last, true), '876');
    expectEq(this.iterateOnPreviousLeaf(last, false), '8764321');
    expectEq(this.iterateOnNextLeaf(first, true), '1234');
    expectEq(this.iterateOnNextLeaf(first, false), '1234678');
    expectEq(this.iterateOnNextLeaf(last, true), '8');
    expectEq(this.iterateOnNextLeaf(last, false), '8');
  }

  // Tests `isValidNode` on some nodes.
  testIsValidNode() {
    load(
        '<div>' +
        '<div id="n1">valid</div>' +
        '<CHROME_ANNOTATION id="n2">invalid</CHROME_ANNOTATION>' +
        '<input id="n3">invalid</input>' +
        '<div contenteditable="true" id="n4">invalid</div>' +
        '</div>');
    for (let i = 1; i <= 4; i++) {
      let id = '#n' + i;
      let node: Node|null = document.querySelector(id);
      expectEq(node?.textContent === 'valid', isValidNode(node!), id + ': ');
    }
  }

  // Tests `hasNoIntentDetection`.
  testHasNoIntentDetection() {
    expectEq(false, hasNoIntentDetection());
    loadHead(
        '  <meta name="chrome" content="nointentdetection">' +
        '  <meta name="chrome" content="intentdetection">');
    expectEq(true, hasNoIntentDetection());
  }

  // Tests `noFormatDetectionTypes`.
  testHasNoFormatDetection() {
    expectEq(false, noFormatDetectionTypes().has('telephone'));
    loadHead('<meta name="format-detection" content="telephone=no">');
    expectEq(true, noFormatDetectionTypes().has('telephone'));

    loadHead(
        '<meta name="format-detection" ' +
        'content="telephone=no ,email= yes, date=no">');
    expectEq(true, noFormatDetectionTypes().has('telephone'));
    expectEq(false, noFormatDetectionTypes().has('email'));
    expectEq(true, noFormatDetectionTypes().has('date'));
    expectEq(false, noFormatDetectionTypes().has('address'));

    loadHead(
        '<meta name="format-detection" ' +
        'content="telephone=no ,email= yes, date=no, unit=no">' +
        '<meta name="format-detection" content="Address=NO">');
    expectEq(true, noFormatDetectionTypes().has('telephone'));
    expectEq(false, noFormatDetectionTypes().has('email'));
    expectEq(true, noFormatDetectionTypes().has('date'));
    expectEq(true, noFormatDetectionTypes().has('address'));
    expectEq(true, noFormatDetectionTypes().has('unit'));
  }
}

export {TestDomUtils}
