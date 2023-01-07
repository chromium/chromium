// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests history saving logic in TabbedEditorContainer.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');

  function dumpHistory(history) {
    TestRunner.addResult('  history = ' + JSON.stringify(history.serializeToObject()) + '');
  }

  function updateScrollAndSelectionAndDump(history, url, scrollLineNumber, selection) {
    history.updateScrollLineNumber(url, scrollLineNumber);
    history.updateSelectionRange(url, selection);
    dumpHistory(history);
  }

  function updateAndDump(history, urls) {
    history.update(urls);
    dumpHistory(history);
  }

  function removeAndDump(history, url) {
    history.remove(url);
    dumpHistory(history);
  }

  function url(index) {
    return 'url_' + index;
  }

  var history = new Sources.TabbedEditorContainer.History([]);

  dumpHistory(history);
  // Emulate opening of several tabs.
  updateAndDump(history, [url(1)]);
  updateAndDump(history, [url(2), url(1)]);
  updateAndDump(history, [url(3), url(2), url(1)]);
  // Emulate switching between tabs.
  updateAndDump(history, [url(2), url(3), url(1)]);
  updateAndDump(history, [url(1), url(2), url(3)]);
  // Emulate opening of several tabs from another page.
  updateAndDump(history, [url(11)]);
  updateAndDump(history, [url(12), url(11)]);
  updateAndDump(history, [url(13), url(12), url(11)]);
  // ... and switching between them.
  updateAndDump(history, [url(12), url(13), url(11)]);
  updateAndDump(history, [url(11), url(12), url(13)]);
  updateScrollAndSelectionAndDump(history, url(11), 10, new TextUtils.TextRange(15, 5, 15, 10));
  // Now close some tabs.
  removeAndDump(history, url(11));
  removeAndDump(history, url(13));
  // Now open some other instead of them.
  updateAndDump(history, [url(14), url(12)]);
  updateAndDump(history, [url(15), url(14), url(12)]);
  updateAndDump(history, [url(16), url(15), url(14), url(12)]);
  // Close all of them one by one.
  removeAndDump(history, url(16));
  removeAndDump(history, url(15));
  removeAndDump(history, url(14));
  removeAndDump(history, url(12));
  removeAndDump(history, url(1));
  removeAndDump(history, url(2));
  removeAndDump(history, url(3));

  TestRunner.completeTest();
})();
