// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_extractor.ts.
 */

import {TextChunk, TextChunkConsumer, TextExtractor} from '//ios/web/annotations/resources/text_extractor.js';
import {expectEq, expectNeq, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextExtractor extends TestSuite {
  // Mark:  TextChunkConsumer

  textChunk?: TextChunk;

  chunkConsumer: TextChunkConsumer = (chunk: TextChunk): void => {
    this.textChunk = chunk;
  };

  // Mark:  tests

  override setUp() {
    this.textChunk = undefined;
  }

  // Tests the normal flow of text extracting and prefix/suffix adding.
  testTextExtractorFlow() {
    const html = '<invisible>012</invisible>' +
        '<visible>abc</visible>' +
        '<visible>defgh</visible>' +
        '    ' +
        '<!-- Comment should be ignored -->' +
        '<visible>ijkl</visible>' +
        '<invisible>mno</invisible>' +
        '<span>pqr</span>' +
        '<span>stuv</span>' +
        '<visible>wxyz</visible>' +
        '\n' +
        '<div>345678</div>';
    load(html);

    const extractor = new TextExtractor(this.chunkConsumer, 5, '|');
    const root = document.body;

    // Simulates the visit.
    extractor.begin();
    let run = '';
    for (const childNode of root.childNodes) {
      run += ':' + childNode.nodeName;
      if (childNode.nodeType === Node.TEXT_NODE) {
        extractor.visibleTextNode(childNode as Text);
      } else if (childNode.nodeName === 'VISIBLE') {
        extractor.enterVisibleNode(childNode);
        extractor.visibleTextNode(childNode.childNodes[0] as Text);
        extractor.leaveVisibleNode(childNode);
      } else if (childNode.nodeName === 'SPAN') {
        extractor.enterVisibleNode(childNode);
        extractor.visibleTextNode(childNode.childNodes[0] as Text);
        extractor.leaveVisibleNode(childNode);
      } else if (childNode.nodeName === 'INVISIBLE') {
        extractor.invisibleNode(childNode);
      }
    }
    expectEq(true, extractor.spaced);
    extractor.end();

    expectNeq(undefined, this.textChunk, 'textChunk:');
    expectEq(
        '012 ' +       // prefix (up to 5 chars)
            'abc' +    // node text
            ' ' +      // single space
            'defgh' +  // node text
            ' ' +      // single space
            'ijkl' +   // node text
            ' ' +      // single space
            '|' +      // section break
            'pqr' +    // node text (no space after)
            'stuv' +   // node text (no space before)
            ' ' +      // single space
            'wxyz' +   // node text
            ' ' +      // space
            '34567',   // postfix (5 chars)
        this.textChunk!.text);
    expectEq(0, this.textChunk!.firstNodeOffset);

    expectEq(4, this.textChunk!.visibleStart);
    expectEq(33, this.textChunk!.visibleEnd);

    expectEq(8, this.textChunk!.sections.length);
    expectEq(0, this.textChunk!.sections[0]!.index);
    expectEq('012', this.textChunk!.sections[0]!.textNode!.textContent);
    expectEq(4, this.textChunk!.sections[1]!.index);
    expectEq('abc', this.textChunk!.sections[1]!.textNode!.textContent);
    expectEq(8, this.textChunk!.sections[2]!.index);
    expectEq('defgh', this.textChunk!.sections[2]!.textNode!.textContent);
    expectEq(14, this.textChunk!.sections[3]!.index);
    expectEq('ijkl', this.textChunk!.sections[3]!.textNode!.textContent);
    expectEq(20, this.textChunk!.sections[4]!.index);
    expectEq('pqr', this.textChunk!.sections[4]!.textNode!.textContent);
    expectEq(23, this.textChunk!.sections[5]!.index);
    expectEq('stuv', this.textChunk!.sections[5]!.textNode!.textContent);
    expectEq(28, this.textChunk!.sections[6]!.index);
    expectEq('wxyz', this.textChunk!.sections[6]!.textNode!.textContent);
    expectEq(33, this.textChunk!.sections[7]!.index);
    expectEq('345678', this.textChunk!.sections[7]!.textNode!.textContent);
  }
}

export {TestTextExtractor}
