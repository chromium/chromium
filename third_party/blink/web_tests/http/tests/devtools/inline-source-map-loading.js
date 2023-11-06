// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verify that inline sourcemap has proper URL and compiledURL.\n`);


  var client = {};
  var sourceMap = {
    'file': 'compiled.js',
    'mappings': 'AAASA,QAAAA,IAAG,CAACC,CAAD,CAAaC,CAAb,CACZ,CACI,MAAOD,EAAP,CAAoBC,CADxB,CAIA,IAAIC,OAAS;',
    'sources': ['source.js'],
    'sourcesContent': ['<source content>']
  };
  var sourceMapURL = 'data:application/json;base64,' + btoa(JSON.stringify(sourceMap));
  var scriptSource = '\n//# sourceMappingURL=' + sourceMapURL + '\n';
  TestRunner.evaluateInPage(scriptSource);
  TestRunner.debuggerModel.sourceMapManager().addEventListener(
      SDK.SourceMapManager.Events.SourceMapAttached, onSourceMap);

  function onSourceMap(event) {
    var sourceMap = event.data.sourceMap;
    TestRunner.addResult('SourceMap Loaded:');
    TestRunner.addResult('url: ' + sourceMap.url());
    TestRunner.addResult('compiledURL: ' + sourceMap.compiledURL());
    TestRunner.completeTest();
  }
})();
