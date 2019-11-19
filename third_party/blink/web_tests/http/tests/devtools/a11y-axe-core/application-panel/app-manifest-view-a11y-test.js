// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');

  TestRunner.addResult('Tests accessibility of AppManifestView on application panel.');
  const manifest = `{
    "name": "TestManifest",
    "short_name": "TestManifest",
    "display": "standalone",
    "start_url": ".",
    "background_color": "#fff123",
    "theme_color": "#123fff",
    "description": "A test manifest.",
    "icons": [{
      "src": "icon.png",
      "sizes": "256x256",
      "type": "image/png"
    }]
  }`;

  await TestRunner.showPanel('resources');
  const manifestView = UI.panels.resources.visibleView;
  await manifestView._renderManifest('test_manifest', manifest, [], []);
  await AxeCoreTestRunner.runValidation(manifestView.contentElement);
  TestRunner.completeTest();
})();
