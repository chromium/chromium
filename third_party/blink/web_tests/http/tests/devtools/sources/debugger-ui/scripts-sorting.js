// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {SDKTestRunner} from 'sdk_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  'use strict';
  TestRunner.addResult(`Tests scripts sorting in the scripts panel.\n`);
  await TestRunner.showPanel('sources');

  function createNavigatorView(constructor) {
    var navigatorView = new constructor();
    navigatorView.show(UI.InspectorView.InspectorView.instance().element);
    return navigatorView;
  }

  const sourcesNavigatorView =
      createNavigatorView(Sources.SourcesNavigator.NetworkNavigatorView);
  const contentScriptsNavigatorView =
      createNavigatorView(Sources.SourcesNavigator.ContentScriptsNavigatorView);

  var pageMock = new SDKTestRunner.PageMock('http://example.com');
  pageMock.turnIntoWorker();
  pageMock.connectAsMainTarget('mock-target-1');

  var uiSourceCodes = [];
  async function addUISourceCode(url, isContentScript) {
    pageMock.evalScript(url, '', isContentScript);
    var uiSourceCode = await TestRunner.waitForUISourceCode(url);
    uiSourceCodes.push(uiSourceCode);
  }

  function dumpScriptsList() {
    SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
    SourcesTestRunner.dumpNavigatorViewInAllModes(contentScriptsNavigatorView);

    for (var i = 0; i < uiSourceCodes.length; ++i) {
      sourcesNavigatorView.revealUISourceCode(uiSourceCodes[i]);
      contentScriptsNavigatorView.revealUISourceCode(uiSourceCodes[i]);
    }

    SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
    SourcesTestRunner.dumpNavigatorViewInAllModes(contentScriptsNavigatorView);
  }

  var scripts = [
    'block.js?block=foo', 'ga.js', 'lenta.ban?pg=4883&ifr=1', 'lenta.ban?pg=5309&ifr=1', 'top100.jcn?80674',
    '_js/production/motor.js?1308927432', 'i/xgemius.js', 'i/js/jquery-1.5.1.min.js', 'i/js/jquery.cookie.js',
    'foo/path/bar.js?file=bar/zzz.js', 'foo/path/foo.js?file=bar/aaa.js'
  ];
  for (var i = 0; i < scripts.length; ++i)
    await addUISourceCode('http://foo.com/' + scripts[i]);

  var scripts2 = ['foo/path/bar.js?file=bar/zzz.js', 'foo/path/foo.js?file=bar/aaa.js'];
  for (var i = 0; i < scripts2.length; ++i)
    await addUISourceCode('http://bar.com/' + scripts2[i]);
  await addUISourceCode('*Non*URL*path');

  var extensions = ['extension-schema://extension-name/bar.js', 'extension-schema://extension-name/folder/baz.js'];
  for (var i = 0; i < extensions.length; ++i)
    await addUISourceCode(extensions[i], true);
  await addUISourceCode('*Another*Non*URL*path', true);
  dumpScriptsList();
  TestRunner.completeTest();
})();
