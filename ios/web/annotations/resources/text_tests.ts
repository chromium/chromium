// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Entry point for all tests.
 */

import {TestTextAnnotationList} from '//ios/web/annotations/resources/text_annotation_list_test.js';
import {TestTextClick} from '//ios/web/annotations/resources/text_click_test.js';
import {TestTextDecoration} from '//ios/web/annotations/resources/text_decoration_test.js';
import {TestTextDecorator} from '//ios/web/annotations/resources/text_decorator_test.js';
import {TestTextDOMObserver} from '//ios/web/annotations/resources/text_dom_observer_test.js';
import {TestDomUtils} from '//ios/web/annotations/resources/text_dom_utils_test.js';
import {TestTextExtractor} from '//ios/web/annotations/resources/text_extractor_test.js';
import {TestTextIntersectionObserver} from '//ios/web/annotations/resources/text_intersection_observer_test.js';
import {TestTextTasks} from '//ios/web/annotations/resources/text_tasks_test.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

function testAll() {
  return [
    ...new TestDomUtils().run(),
    ...new TestTextTasks().run(),
    ...new TestTextDOMObserver().run(),
    ...new TestTextIntersectionObserver().run(),
    ...new TestTextExtractor().run(),
    ...new TestTextDecorator().run(),
    ...new TestTextClick().run(),
    ...new TestTextAnnotationList().run(),
    ...new TestTextDecoration().run(),
  ];
}

gCrWeb.textTests = {
  testAll,
};
