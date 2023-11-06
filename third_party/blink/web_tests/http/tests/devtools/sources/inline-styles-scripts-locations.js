// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Bindings from 'devtools/models/bindings/bindings.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Bindings should only generate locations for an inline script (style) if the location is inside of the inline script (style).\n`);
  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('../bindings/resources/inline-style.html');
  const source = await TestRunner.waitForUISourceCode('inline-style.html', Workspace.Workspace.projectTypes.Network);
  const { content } = await source.requestContent();
  TestRunner.addResult(`Content:\n${content}`);
  const sourceText = new TextUtils.Text.Text(content);

  await dumpLocations("css", sourceText.lineCount(), source);
  await dumpLocations("script", sourceText.lineCount(), source);

  async function dumpLocations(type, lineCount, source) {
    TestRunner.addResult(`Scanning ${lineCount} lines for ${type} locations. Note that location line/column numbers are zero-based.`);
    for (let line = 0; line < lineCount; ++line) {
      const rawLocations = await getLocations(line);
      if (rawLocations.length) {
        const results = rawLocations.map(async loc => `${loc.lineNumber}:${loc.columnNumber} (${await checkValidity(loc)})`);
        const rawLocationsString = (await Promise.all(results)).join('  ');
        TestRunner.addResult(`uiLocation ${line}:0 resolves to: ${rawLocationsString}`)
      }
    }
    async function getLocations(line) {
      if (type === "css")
        return Bindings.CSSWorkspaceBinding.CSSWorkspaceBinding.instance().uiLocationToRawLocations(source.uiLocation(line, 0));
      if (type === "script")
        return await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(source, line, 0);
      return null;
    }
    async function checkValidity(location) {
      if (location instanceof SDK.CSSModel.CSSLocation) {
        const h = location.header();
        if (!h) return "invalid css header";
        if (!h.containsLocation(location.lineNumber, location.columnNumber))
          return `not in css range ${h.startLine}:${h.startColumn}-${h.endLine}:${h.endColumn}`;
        return "css";
      }
      if (location instanceof SDK.DebuggerModel.Location) {
        const s = location.script();
        if (!s) return "invalid script";
        if (!s.containsLocation(location.lineNumber, location.columnNumber))
          return `not in script range ${s.lineOffset}:${s.columnOffset}-${s.endLine}:${s.endColumn})`;
        return "script";
      }
      return "invalid (wrong instance)"
    }
  }


  TestRunner.completeTest();
})();
