// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUITsMojoTestCache} from './web_ui_ts_test.test-mojom-webui.js';

const TEST_DATA: Array<{url: string, contents: string}> = [
  { url: 'https://google.com/', contents: 'i am in fact feeling lucky' },
  { url: 'https://youtube.com/', contents: 'probably cat videos?' },
  { url: 'https://example.com/', contents: 'internets wow' },
];

async function doTest(): Promise<boolean> {
  const cache = WebUITsMojoTestCache.getRemote();
  for (const entry of TEST_DATA) {
    cache.put({ url: entry.url }, entry.contents);
  }

  const {items} = await cache.getAll();
  if (items.length != TEST_DATA.length) {
    return false;
  }

  const entries: {[key: string]: string } = {};
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

type WindowWithDomAutomationController = Window & {
  domAutomationController: {
    send: (success: boolean) => void;
  }
};

async function runTest() {
  (window as unknown as WindowWithDomAutomationController)
      .domAutomationController.send(await doTest());
}

Object.assign(window, {runTest});
