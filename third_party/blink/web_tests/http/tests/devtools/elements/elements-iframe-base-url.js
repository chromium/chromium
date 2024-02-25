// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that nodes have correct baseURL, documentURL.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
    `);

  await TestRunner.addIframe(`resources/elements-empty-iframe.html`);

  ElementsTestRunner.expandElementsTree(step1);

  async function step1() {
    const docs = ElementsTestRunner.getDocumentElements();
    for (const doc of docs) {
      if (doc.parentNode)
        TestRunner.addResult(`${doc.nodeName()} has parent ${doc.parentNode.nodeName()}.`);
      else
        TestRunner.addResult(`${doc.nodeName()} has no parent.`);
      TestRunner.addResult(`baseURL    : ${doc.baseURL}\ndocumentURL: ${doc.documentURL}\n`);
    }
    TestRunner.completeTest();
  }
})();
