// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {SDKTestRunner} from 'sdk_test_runner';

import * as Bindings from 'devtools/models/bindings/bindings.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests scripts panel file selectors.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addIframe(
      'resources/post-message-listener.html', {name: 'childframe'});

  Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().resetForTest(TestRunner.mainTarget);
  Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().resourceMapping.resetForTest(TestRunner.mainTarget);

  var subframe = TestRunner.mainFrame().childFrames[0];

  var sourcesNavigatorView = new Sources.SourcesNavigator.NetworkNavigatorView();
  sourcesNavigatorView.show(UI.InspectorView.InspectorView.instance().element);
  var contentScriptsNavigatorView = new Sources.SourcesNavigator.ContentScriptsNavigatorView();
  contentScriptsNavigatorView.show(UI.InspectorView.InspectorView.instance().element);

  var uiSourceCodes = [];
  async function addUISourceCode(url, isContentScript, frame) {
    if (isContentScript) {
      var uiSourceCode =
          await SourcesTestRunner.addScriptUISourceCode(url, '', true, 42);
      uiSourceCodes.push(uiSourceCode);
      return;
    }
    TestRunner.addScriptForFrame(url, '', frame || TestRunner.mainFrame());
    var uiSourceCode = await waitForUISourceCodeAdded(url);
    uiSourceCodes.push(uiSourceCode);
  }

  async function addUISourceCode2(url) {
    TestRunner.evaluateInPageAnonymously(`
      window.workers = window.workers || [];
      window.workers.push(new Worker('${url}'));
    `);
    var uiSourceCode = await waitForUISourceCodeAdded(url);
    uiSourceCodes.push(uiSourceCode);
  }

  function waitForUISourceCodeAdded(url) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    Workspace.Workspace.WorkspaceImpl.instance().addEventListener(
        Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
    return promise;

    function uiSourceCodeAdded(event) {
      if (event.data.url() !== url)
        return;
      Workspace.Workspace.WorkspaceImpl.instance().removeEventListener(
          Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
      fulfill(event.data);
    }
  }

  function revealUISourceCode(uiSourceCode) {
    sourcesNavigatorView.revealUISourceCode(uiSourceCode);
    contentScriptsNavigatorView.revealUISourceCode(uiSourceCode);
  }

  var rootURL = 'http://localhost:8080/LayoutTests/inspector/debugger/';

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding first resource:');
  await addUISourceCode(rootURL + 'foo/bar/script.js', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding second resource:');
  await addUISourceCode(rootURL + 'foo/bar/script.js?a=2', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding resources into another frame:');
  await addUISourceCode(rootURL + 'foo/bar/script.js?a=1', false, subframe);

  await addUISourceCode(rootURL + 'foo/baz/script.js', false, subframe);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding resources into another target:');
  await addUISourceCode2(TestRunner.url('resources/script1.js?a=3'));
  await addUISourceCode2(TestRunner.url('resources/script2.js'));
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Adding content scripts and some random resources:');
  await addUISourceCode(rootURL + 'foo/bar/contentScript2.js?a=1', true);
  await addUISourceCode(rootURL + 'foo/bar/contentScript.js?a=2', true);
  await addUISourceCode(rootURL + 'foo/bar/contentScript.js?a=1', true);
  await addUISourceCode('http://example.com/', false);
  await addUISourceCode('http://example.com/?a=b', false);
  await addUISourceCode(
      'http://example.com/the%2fdir/foo?bar=100&baz=a%20%2fb', false);
  // Verify that adding invalid URL does not throw exception.
  await addUISourceCode(
      'http://example.com/the%2fdir/foo?bar=100%&baz=a%20%2fb', false);
  await addUISourceCode(
      'http://example.com/path%20with%20spaces/white%20space.html', false);

  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
  SourcesTestRunner.dumpNavigatorViewInAllModes(contentScriptsNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Revealing first resource:');
  revealUISourceCode(uiSourceCodes[0]);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  // Here we keep http://localhost:8080/LayoutTests/inspector/debugger2/ folder
  // collapsed while adding resources into it.
  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult(
      'Adding some resources to change the way debugger folder looks like, first:');
  var rootURL2 = 'http://localhost:8080/LayoutTests/inspector/debugger2/';
  await addUISourceCode(rootURL2 + 'foo/bar/script.js', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Second:');
  await addUISourceCode(rootURL2 + 'foo/bar/script.js?a=2', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Others:');
  await addUISourceCode(rootURL2 + 'foo/bar/script.js?a=1', false);
  await addUISourceCode(rootURL2 + 'foo/baz/script.js', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  var rootURL3 = 'http://localhost:8080/LayoutTests/inspector/debugger3/';
  await addUISourceCode(
      rootURL3 + 'hasOwnProperty/__proto__/constructor/foo.js', false);
  await addUISourceCode(rootURL3 + 'hasOwnProperty/__proto__/foo.js', false);
  await addUISourceCode(rootURL3 + 'hasOwnProperty/foo.js', false);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Revealing all resources:');
  for (var i = 0; i < uiSourceCodes.length; ++i)
    revealUISourceCode(uiSourceCodes[i]);
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
  SourcesTestRunner.dumpNavigatorViewInAllModes(contentScriptsNavigatorView);

  TestRunner.addResult('\n\n================================================');
  TestRunner.addResult('Removing all resources:');
  for (const target of SDK.TargetManager.TargetManager.instance().targets()) {
    if (target !== TestRunner.mainTarget)
      Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().resetForTest(target);
  }
  SourcesTestRunner.dumpNavigatorViewInAllModes(sourcesNavigatorView);
  SourcesTestRunner.dumpNavigatorViewInAllModes(contentScriptsNavigatorView);

  TestRunner.completeTest();
})();
