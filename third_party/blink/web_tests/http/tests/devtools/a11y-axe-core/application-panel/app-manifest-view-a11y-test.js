// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {

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
  const manifestView = Application.ResourcesPanel.ResourcesPanel.instance().visibleView;
  await manifestView.renderManifest('test_manifest', manifest, [], []);
  await AxeCoreTestRunner.runValidation(manifestView.contentElement);
  TestRunner.completeTest();
})();
