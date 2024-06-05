// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests UISourceCode class.\n`);
  await TestRunner.showPanel('sources');

  var MockProject = class extends Workspace.Workspace.ProjectStore {
    requestFileContent(uri) {
      TestRunner.addResult('Content is requested from SourceCodeProvider.');
      return new Promise(resolve => {
        setTimeout(() => resolve(new TextUtils.ContentData.ContentData('var x = 0;', false, 'text/javascript')));
      });
    }

    mimeType() {
      return 'text/javascript';
    }

    isServiceProject() {
      return false;
    }

    type() {
      return Workspace.Workspace.projectTypes.Debugger;
    }

    url() {
      return 'mock://debugger-ui/';
    }
  };

  TestRunner.runTestSuite([function testUISourceCode(next) {
    var uiSourceCode = new Workspace.UISourceCode.UISourceCode(new MockProject(), 'url', Common.ResourceType.resourceTypes.Script);
    function didRequestContent(callNumber, { text }) {
      TestRunner.addResult('Callback ' + callNumber + ' is invoked.');
      TestRunner.assertEquals('text/javascript', uiSourceCode.mimeType());
      TestRunner.assertEquals('var x = 0;', text);

      if (callNumber === 3) {
        // Check that sourceCodeProvider.requestContent won't be called anymore.
        uiSourceCode.requestContentData().then(function({ text }) {
          TestRunner.assertEquals('text/javascript', uiSourceCode.mimeType());
          TestRunner.assertEquals('var x = 0;', text);
          next();
        });
      }
    }
    // Check that all callbacks will be invoked.
    uiSourceCode.requestContentData().then(didRequestContent.bind(null, 1));
    uiSourceCode.requestContentData().then(didRequestContent.bind(null, 2));
    uiSourceCode.requestContentData().then(didRequestContent.bind(null, 3));
  }]);
})();
