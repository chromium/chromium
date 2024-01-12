// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_annotation_list.ts.
 */

import {TextAnnotationList, TextViewportAnnotation} from '//ios/web/annotations/resources/text_annotation_list.js';
import {expectEq, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextAnnotationList extends TestSuite {
  annotation(start: number, end: number): TextViewportAnnotation {
    return {start, end, text: '', type: 'EMAIL', data: '#' + start};
  }

  // Tests that annotation are sorted properly and that, for overlapping
  // annotations, only one survives.
  testTextAnnotationListSortAndOverlap() {
    const list = new TextAnnotationList([
      this.annotation(10, 20),  // Should be removed
      this.annotation(5, 15),   // Should be first
      this.annotation(50, 60),  // Should be third and last
      this.annotation(35, 45),  // Should be removed
      this.annotation(30, 40),  // Should be second
    ]);
    expectEq(false, list.done);
    expectEq(list.currentAnnotation!.start, 5);
    list.next();
    expectEq(false, list.done);
    expectEq(list.currentAnnotation!.start, 30);
    list.next();
    expectEq(false, list.done);
    expectEq(list.currentAnnotation!.start, 50);
    list.next();
    expectEq(true, list.done);

    expectEq(3, list.successes);
    expectEq(2, list.failures);
    expectEq(2, list.cancelled.length);
    expectEq('#10', list.cancelled[0]!.data);
    expectEq('#35', list.cancelled[1]!.data);
  }

  // Tests that annotation are clipped properly.
  testTextAnnotationListClip() {
    const list = new TextAnnotationList(
        [
          this.annotation(10, 20),
          this.annotation(30, 40),
          this.annotation(50, 60),
          this.annotation(70, 80),
          this.annotation(90, 100),
        ],
        35, 75);
    expectEq(list.currentAnnotation!.start, 30);
    list.next();
    expectEq(list.currentAnnotation!.start, 50);
    list.next();
    expectEq(list.currentAnnotation!.start, 70);
    list.next();
    expectEq(true, list.done);

    expectEq(3, list.successes);
    expectEq(2, list.failures);
    expectEq(2, list.cancelled.length);
    expectEq('#10', list.cancelled[0]!.data);
    expectEq('#90', list.cancelled[1]!.data);
  }

  // Tests `next`, `skip` and `fail` cursor movers.
  testTextAnnotationListSkipFail() {
    const list = new TextAnnotationList([
      this.annotation(10, 20),
      this.annotation(30, 40),
      this.annotation(50, 60),
      this.annotation(70, 80),
      this.annotation(90, 100),
    ]);
    expectEq(list.currentAnnotation!.start, 10);
    list.next();
    expectEq(list.currentAnnotation!.start, 30);
    list.skip();
    expectEq(list.currentAnnotation!.start, 50);
    list.next();
    expectEq(list.currentAnnotation!.start, 70);
    list.fail();
    expectEq(true, list.done);

    expectEq(2, list.successes);
    expectEq(3, list.failures);
    expectEq(3, list.cancelled.length);
    expectEq('#30', list.cancelled[0]!.data);
    expectEq('#70', list.cancelled[1]!.data);
    expectEq('#90', list.cancelled[2]!.data);
  }
}

export {TestTextAnnotationList}
