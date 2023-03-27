// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIMojoTestCache} from './content/test/data/web_ui_test.test-mojom-webui.js';

/** @type {{send: function(*)}} */
Window.prototype.domAutomationController;

const TEST_DATA = [
  { url: 'https://google.com/', contents: 'i am in fact feeling lucky' },
  { url: 'https://youtube.com/', contents: 'probably cat videos?' },
  { url: 'https://example.com/', contents: 'internets wow' },
];

async function doTest() {
  const cache = WebUIMojoTestCache.getRemote();
  for (const entry of TEST_DATA) {
    cache.put({ url: entry.url }, entry.contents);
  }

  const {items} = await cache.getAll();
  if (items.length != TEST_DATA.length) {
    return false;
  }

  const entries = {};
  for (const item of items) {
    entries[item.url.url] = item.contents;
  }

  for (const entry of TEST_DATA) {
    if (!(entry.url in entries)) {
      return false;
    }
    if (entries[entry.url] != entry.contents) {
      return false;
    }
  }

  return true;
}

window.runTest = async function() {
  return doTest();
}
