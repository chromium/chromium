// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_decorator.ts.
 */

import {TextAnnotationList, TextViewportAnnotation} from '//ios/web/annotations/resources/text_annotation_list.js';
import {TextDecorator} from '//ios/web/annotations/resources/text_decorator.js';
import {TextChunk, TextSection} from '//ios/web/annotations/resources/text_extractor.js';
import {TextStyler} from '//ios/web/annotations/resources/text_styler.js';
import {expectEq, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class InertTextStyler extends TextStyler {
  override style(_parentNode: Node, _element: HTMLElement, _type: string):
      void {}
}

class TestTextDecorator extends TestSuite {
  // Mark: Utils

  annotation(
      start: number, end: number, text: string, type?: string,
      data?: string): TextViewportAnnotation {
    return {start, end, text, type: type ?? 'WORD', data: data ?? '#' + text};
  }

  // Mark:  tests

  // Test case 3 (see text_decorator.ts), the normal flow without collision with
  // in-flight chunks or mutation by external 3p.
  // All annotations are limited to one element
  testTextDecoratorFlowInElement() {
    const html = '<div id="d1">abcde</div>' +
        '<div id="d2">fghij</div>' +
        '<div id="d3">klmno</div>' +
        '<div id="d4">pqrst</div>' +
        '<div id="d5">uvwxy</div>';
    load(html);
    const d1 = document.querySelector('#d1')!;
    const d2 = document.querySelector('#d2')!;
    const d3 = document.querySelector('#d3')!;
    const d4 = document.querySelector('#d4')!;
    const d5 = document.querySelector('#d5')!;

    // Let's pretend the extracted text is:
    //  'de fghij klmno pqrst uvw'
    //             111111111122222
    //   0123456789012345678901234
    // with a prefix = 'de fg' and suffix is 't uvw'. Offset in first node is 3
    // and the visible range of 'h...t' is [5, 20).
    const sections = [
      new TextSection(d1.childNodes[0] as Text, 0),  // first section
      new TextSection(d2.childNodes[0] as Text, 3),
      new TextSection(d3.childNodes[0] as Text, 9),
      new TextSection(d4.childNodes[0] as Text, 15),
      new TextSection(d5.childNodes[0] as Text, 21),
    ];
    const chunk = new TextChunk(3, 5, 20);
    chunk.add(sections, 'de fghij klmno pqrst uvw');

    // Let's pretend the annotation are 'e f', 'ghi', 'mno p', 'r', 't'  and
    // 'vw', where 'e f' and 'vw'are outside the visible range.
    const list = new TextAnnotationList(
        [
          this.annotation(1, 4, 'e f'),  // Should be ignored
          this.annotation(4, 7, 'ghi'),
          this.annotation(11, 16, 'mno p'),  // Across elements, ignored
          this.annotation(17, 18, 'r'), this.annotation(19, 20, 't'),
          this.annotation(22, 24, 'vw'),  // Should be ignored
        ],
        chunk.visibleStart, chunk.visibleEnd);

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);

    decorator.decorateChunk(chunk, list);

    expectEq(3, list.successes);
    expectEq(3, list.failures);
    expectEq(3, list.cancelled.length);

    const decoratedHTML = '<div id="d1">abcde</div>' +
        '<div id="d2">f<chrome_annotation>ghi</chrome_annotation>j</div>' +
        '<div id="d3">klmno</div>' +
        '<div id="d4">pq' +
        '<chrome_annotation>r</chrome_annotation>s' +
        '<chrome_annotation>t</chrome_annotation></div>' +
        '<div id="d5">uvwxy</div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    decorator.removeAllDecorations();
    expectEq(html, document.body.innerHTML);
  }

  // Test case 3 (see text_decorator.ts), the normal flow without collision with
  // in-flight chunks or mutation by external 3p.
  // All annotations are limited to one element
  testTextDecoratorFlowAcrossElement() {
    const html = '<div id="d1">abcde</div>' +
        '<div id="d2">fghij</div>' +
        '<div id="d3">klmno</div>' +
        '<div id="d4">pqrst</div>' +
        '<div id="d5">uvwxy</div>';
    load(html);
    const d1 = document.querySelector('#d1')!;
    const d2 = document.querySelector('#d2')!;
    const d3 = document.querySelector('#d3')!;
    const d4 = document.querySelector('#d4')!;
    const d5 = document.querySelector('#d5')!;

    // Let's pretend the extracted text is:
    //  'de fghij klmno pqrst uvw'
    //             111111111122222
    //   0123456789012345678901234
    // with a prefix = 'de fg' and suffix is 't uvw'. Offset in first node is 3
    // and the visible range of 'h...t' is [5, 20).
    const sections = [
      new TextSection(d1.childNodes[0] as Text, 0),  // first section
      new TextSection(d2.childNodes[0] as Text, 3),
      new TextSection(d3.childNodes[0] as Text, 9),
      new TextSection(d4.childNodes[0] as Text, 15),
      new TextSection(d5.childNodes[0] as Text, 21),
    ];
    const chunk = new TextChunk(3, 5, 20);
    chunk.add(sections, 'de fghij klmno pqrst uvw');

    // Let's pretend the annotation are 'e f', 'ghi', 'mno p', 'r', 't'  and
    // 'vw', where 'e f' and 'vw'are outside the visible range.
    const list = new TextAnnotationList(
        [
          this.annotation(1, 4, 'e f', 'address'),  // Should be ignored
          this.annotation(4, 7, 'ghi', 'address'),
          this.annotation(
              11, 16, 'mno p', 'address'),  // Across elements, ignored
          this.annotation(17, 18, 'r', 'address'),
          this.annotation(19, 20, 't', 'address'),
          this.annotation(22, 24, 'vw', 'address'),  // Should be ignored
        ],
        chunk.visibleStart, chunk.visibleEnd);

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);

    decorator.decorateChunk(chunk, list);

    expectEq(4, list.successes);
    expectEq(2, list.failures);
    expectEq(2, list.cancelled.length);

    const decoratedHTML = '<div id="d1">abcde</div>' +
        '<div id="d2">f<chrome_annotation>ghi</chrome_annotation>j</div>' +
        '<div id="d3">kl<chrome_annotation>mno</chrome_annotation></div>' +
        '<div id="d4"><chrome_annotation>p</chrome_annotation>q' +
        '<chrome_annotation>r</chrome_annotation>s' +
        '<chrome_annotation>t</chrome_annotation></div>' +
        '<div id="d5">uvwxy</div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    decorator.removeAllDecorations();
    expectEq(html, document.body.innerHTML);
  }

  testTextDecoratorEdgeCases() {
    const html = '<div id="d1">abcde</div>' +
        '<div id="d2">fghij</div>' +
        '<div id="d3">klmno</div>' +
        '<div id="d4">pqrst</div>' +
        '<div id="d5">uvwxy</div>';
    load(html);
    const d1 = document.querySelector('#d1')!;
    const d2 = document.querySelector('#d2')!;
    const d3 = document.querySelector('#d3')!;
    const d4 = document.querySelector('#d4')!;
    const d5 = document.querySelector('#d5')!;

    const sections = [
      new TextSection(d1.childNodes[0] as Text, 0),
      new TextSection(d2.childNodes[0] as Text, 6),
      new TextSection(d3.childNodes[0] as Text, 12),
      new TextSection(d4.childNodes[0] as Text, 18),
      new TextSection(d5.childNodes[0] as Text, 24),
    ];
    const chunk = new TextChunk(0, 0, 29);
    //                             11111111112222222222
    //                   012345678901234567890123456789
    chunk.add(sections, 'abcde fghij klmno pqrst uvwxy');

    const list = new TextAnnotationList([
      this.annotation(
          3, 14, 'de fghij kl', 'address'),     // spans over a full node.
      this.annotation(17, 20, 'zzz', 'other'),  // Text doesn't match.
      this.annotation(27, 28, 'x', 'other'),    // Side by side with 'y'
      this.annotation(28, 29, 'y', 'address'),  // End of text.
    ]);

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);
    decorator.decorateChunk(chunk, list);

    expectEq(3, list.successes, 'successes: ');
    expectEq(1, list.failures, 'failures: ');
    expectEq(1, list.cancelled.length);

    const decoratedHTML =
        '<div id="d1">abc<chrome_annotation>de</chrome_annotation></div>' +
        '<div id="d2"><chrome_annotation>fghij</chrome_annotation></div>' +
        '<div id="d3"><chrome_annotation>kl</chrome_annotation>mno</div>' +
        '<div id="d4">pqrst</div>' +
        '<div id="d5">uvw<chrome_annotation>x</chrome_annotation>' +
        '<chrome_annotation>y</chrome_annotation></div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    // Remove by type 'address', then 'other',
    decorator.removeDecorationsOfType('address');
    const partialRemoveHTML = '<div id="d1">abcde</div>' +
        '<div id="d2">fghij</div>' +
        '<div id="d3">klmno</div>' +
        '<div id="d4">pqrst</div>' +
        '<div id="d5">uvw<chrome_annotation>x</chrome_annotation>y</div>';
    expectEq(partialRemoveHTML, document.body.innerHTML);
    decorator.removeDecorationsOfType('other');
    expectEq(html, document.body.innerHTML);
    // Check that removing all, doesn't corrupt the DOM.
    decorator.removeAllDecorations();
    expectEq(html, document.body.innerHTML);
  }

  // Test case 1 and 2 (see text_decorator.ts), the normal flow with collision
  // with in-flight chunks.
  testTextDecoratorInflightCollision() {
    const html = '<div id="d1">abcdefghijklmnopqrstuvwxyz</div>';
    load(html);
    const d1 = document.querySelector('#d1')!;

    const sections = [
      new TextSection(d1.childNodes[0] as Text, 0),
    ];
    const chunk = new TextChunk(0, 0, 26);
    //                             11111111112222222
    //                   012345678901234567890123456
    chunk.add(sections, 'abcdefghijklmnopqrstuvwxyz');

    let list = new TextAnnotationList([
      this.annotation(3, 6, 'def', 'A'),  // first annotation.
    ]);

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);
    decorator.decorateChunk(chunk, list);
    expectEq(1, list.successes, 'successes 1: ');
    expectEq(0, list.failures, 'failures 1: ');
    expectEq(0, list.cancelled.length);

    const decoratedHTML = '<div id="d1">abc' +
        '<chrome_annotation>def</chrome_annotation>' +
        'ghijklmnopqrstuvwxyz</div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    // Now pretend another in-flight chunk has an annotation on the same node
    // which is now replaced. Let's reuse chunk since it is unmutable.
    list = new TextAnnotationList([
      this.annotation(20, 23, 'uvw', 'A'),  // second annotation.
    ]);
    decorator.decorateChunk(chunk, list);
    expectEq(1, list.successes, 'successes 2: ');
    expectEq(0, list.failures, 'failures 2: ');
    expectEq(0, list.cancelled.length);

    const redecoratedHTML = '<div id="d1">abc' +
        '<chrome_annotation>def</chrome_annotation>' +
        'ghijklmnopqrst' +
        '<chrome_annotation>uvw</chrome_annotation>' +
        'xyz</div>';
    expectEq(redecoratedHTML, document.body.innerHTML);

    decorator.removeAllDecorations();
    expectEq(html, document.body.innerHTML);
  }

  // Test case 2 (see text_decorator.ts) with two div second annotation at edge
  // (get new child node)
  testTextDecoratorCollision() {
    const html = '<div id="d1">abcdefghijklm</div>' +
        '<div id="d2">nopqrstuvwxyz</div>';
    load(html);
    const d1 = document.querySelector('#d1')!;
    const d2 = document.querySelector('#d2')!;

    let sections = [
      new TextSection(d1.childNodes[0] as Text, 0),
    ];
    let chunk = new TextChunk(0, 0, 26);
    //                             11111111112222222
    //                   012345678901234567890123456
    chunk.add(sections, 'abcdefghijklmnopqrstuvwxyz');

    let list = new TextAnnotationList([
      this.annotation(3, 6, 'def', 'A'),  // first annotation.
    ]);

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);
    decorator.decorateChunk(chunk, list);
    expectEq(1, list.successes, 'successes 1: ');
    expectEq(0, list.failures, 'failures 1: ');
    expectEq(0, list.cancelled.length);

    const decoratedHTML = '<div id="d1">abc' +
        '<chrome_annotation>def</chrome_annotation>' +
        'ghijklm</div>' +
        '<div id="d2">nopqrstuvwxyz</div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    // Now pretend another chunk has an annotation partly on a replacement node.
    sections = [
      new TextSection(d1.childNodes[2] as Text, 0),
      new TextSection(d2.childNodes[0] as Text, 7),
    ];
    chunk = new TextChunk(0, 0, 20);
    //                             11111111112
    //                   012345678901234567890
    chunk.add(sections, 'ghijklmnopqrstuvwxyz');
    list = new TextAnnotationList([
      this.annotation(5, 9, 'lmno', 'address'),  // second annotation.
    ]);
    decorator.decorateChunk(chunk, list);
    expectEq(1, list.successes, 'successes 2: ');
    expectEq(0, list.failures, 'failures 2: ');
    expectEq(0, list.cancelled.length);

    const redecoratedHTML = '<div id="d1">abc' +
        '<chrome_annotation>def</chrome_annotation>' +
        'ghijk' +
        '<chrome_annotation>lm</chrome_annotation></div>' +
        '<div id="d2">' +
        '<chrome_annotation>no</chrome_annotation>' +
        'pqrstuvwxyz</div>';
    expectEq(redecoratedHTML, document.body.innerHTML);

    decorator.removeAllDecorations();
    expectEq(html, document.body.innerHTML);
  }

  testTextDecoratorDeadNode() {
    const html = '<div id="d1">abcdefghijklm</div>' +
        '<div id="d2">nopqrstuvwxyz</div>';
    load(html);
    let d1: Node|null = document.querySelector('#d1')!;
    const d2 = document.querySelector('#d2')!;

    const sections = [
      new TextSection(d1!.childNodes[0] as Text, 0),
      new TextSection(d2.childNodes[0] as Text, 13),
    ];
    const chunk = new TextChunk(0, 0, 26);
    //                             11111111112222222
    //                   012345678901234567890123456
    chunk.add(sections, 'abcdefghijklmnopqrstuvwxyz');

    const list = new TextAnnotationList([
      this.annotation(11, 15, 'lmno', 'address'),
    ]);

    // Remove from DOM
    (d1 as HTMLElement).remove();
    d1 = null;

    const styler = new InertTextStyler();
    const decorator = new TextDecorator(styler);
    decorator.decorateChunk(chunk, list);
    expectEq(1, list.successes, 'successes 1: ');
    expectEq(0, list.failures, 'failures 1: ');
    expectEq(0, list.cancelled.length);

    const decoratedHTML = '<div id="d2">' +
        '<chrome_annotation>no</chrome_annotation>' +
        'pqrstuvwxyz</div>';
    expectEq(decoratedHTML, document.body.innerHTML);

    decorator.removeAllDecorations();

    const newHTML = '<div id="d2">nopqrstuvwxyz</div>';
    expectEq(newHTML, document.body.innerHTML);
  }
}

export {TestTextDecorator}
