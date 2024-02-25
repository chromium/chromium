// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Network from 'devtools/panels/network/network.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Application from 'devtools/panels/application/application.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests object revelation in the UI.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('resources');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      <div id="targetnode">
      </div>
      <span id="toremove"></span>
      <span id="containertoremove">
        <span id="child"></span>
      </span>
    `);
  await TestRunner.evaluateInPagePromise(`
      function loadResource(url)
      {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", url, false);
          xhr.send();
      }
  `);
  await TestRunner.addScriptTag('resources/bar.js');

  var divNode;
  var spanNode;
  var childNode;
  var container;
  var resource;
  var uiLocation;
  var requestWithResource;
  var requestWithoutResource;

  TestRunner.runTestSuite([
    async function init(next) {
      installHooks();

      TestRunner.resourceTreeModel.forAllResources(function(r) {
        if (r.url.indexOf('bar.js') !== -1) {
          resource = r;
          return true;
        }
      });
      uiLocation = Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL(resource.url).uiLocation(2, 1);

      divNode = await ElementsTestRunner.nodeWithIdPromise('targetnode');
      spanNode = await ElementsTestRunner.nodeWithIdPromise('toremove');
      childNode = await ElementsTestRunner.nodeWithIdPromise('child');
      container = await ElementsTestRunner.nodeWithIdPromise('containertoremove');

      NetworkTestRunner.recordNetwork();
      var url = TestRunner.url('bar.js');
      TestRunner.evaluateInPage(`loadResource('${url}')`, firstXhrCallback);

      function firstXhrCallback() {
        requestWithResource =
            NetworkTestRunner.networkLog().requestForURL(resource.url);
        TestRunner.evaluateInPage('loadResource(\'missing.js\')', secondXhrCallback);
      }

      function secondXhrCallback() {
        var requests = NetworkTestRunner.networkRequests();
        for (var i = 0; i < requests.length; ++i) {
          if (requests[i].url().indexOf('missing.js') !== -1) {
            requestWithoutResource = requests[i];
            break;
          }
        }
        next();
      }
    },

    function revealNode(next) {
      Common.Revealer.reveal(divNode).then(next);
    },

    function revealRemovedNode(next) {
      spanNode.removeNode(function() {
        Common.Revealer.reveal(spanNode).then(() => {
          TestRunner.addResult('Removed node revealed');
        }, () => {
          TestRunner.addResult('Removed node not revealed');
        }).finally(next);
      });
    },

    function revealNodeWithRemovedParent(next) {
      // Note: we remove the container, and then check that the child also can
      // not be revealed since it is also detached.
      container.parentNode.removeNode(function() {
        Common.Revealer.reveal(childNode).then(() => {
          TestRunner.addResult('Node with removed parent revealed');
        }, () => {
          TestRunner.addResult('Node with removed parent not revealed');
        }).finally(next);
      });
    },

    function revealUILocation(next) {
      Common.Revealer.reveal(uiLocation).then(next);
    },

    function revealResource(next) {
      Common.Revealer.reveal(resource).then(next);
    },

    function revealRequestWithResource(next) {
      Common.Revealer.reveal(requestWithResource).then(next);
    },

    function revealRequestWithoutResource(next) {
      Common.Revealer.reveal(requestWithoutResource).then(next);
    }
  ]);

  function installHooks() {
    TestRunner.addSniffer(ElementsModule.ElementsPanel.ElementsPanel.prototype, 'revealAndSelectNode', nodeRevealed, true);
    TestRunner.addSniffer(SourcesModule.SourcesPanel.SourcesPanel.prototype, 'showUILocation', uiLocationRevealed, true);
    TestRunner.addSniffer(Application.ApplicationPanelSidebar.ApplicationPanelSidebar.prototype, 'showResource', resourceRevealed, true);
    TestRunner.addSniffer(Network.NetworkPanel.NetworkPanel.prototype, 'revealAndHighlightRequest', revealed, true);
  }

  function nodeRevealed(node) {
    TestRunner.addResult('Node revealed in the Elements panel');
  }

  function uiLocationRevealed(uiLocation) {
    TestRunner.addResult(
        'UILocation ' + uiLocation.uiSourceCode.name() + ':' + uiLocation.lineNumber + ':' + uiLocation.columnNumber +
        ' revealed in the Sources panel');
  }

  function resourceRevealed(resource, lineNumber) {
    TestRunner.addResult('Resource ' + resource.displayName + ' revealed in the Resources panel');
  }

  function revealed(request) {
    TestRunner.addResult(
        'Request ' + new Common.ParsedURL.ParsedURL(request.url()).lastPathComponent + ' revealed in the Network panel');
  }
})();
