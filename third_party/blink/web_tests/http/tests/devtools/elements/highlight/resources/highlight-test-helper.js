// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

function sortHighlightObject(obj) {
  if (Array.isArray(obj)) {
    return obj.map(sortHighlightObject);
  } else if (obj !== null && typeof obj === 'object') {
    const sortedObj = {};
    for (const key of Object.keys(obj)) {
      if (key === 'areaNames' && obj[key] !== null && typeof obj[key] === 'object') {
        const sortedAreas = {};
        for (const areaName of Object.keys(obj[key]).sort()) {
          sortedAreas[areaName] = obj[key][areaName];
        }
        sortedObj[key] = sortedAreas;
      } else if ((key === 'rowLineNameOffsets' || key === 'columnLineNameOffsets') && Array.isArray(obj[key])) {
        const sortedLines = obj[key].map(sortHighlightObject);
        sortedLines.sort((a, b) => {
          if (a.name !== b.name) return a.name.localeCompare(b.name);
          if (a.x !== b.x) return a.x - b.x;
          return a.y - b.y;
        });
        sortedObj[key] = sortedLines;
      } else {
        sortedObj[key] = sortHighlightObject(obj[key]);
      }
    }
    return sortedObj;
  }
  return obj;
}

export async function dumpStableInspectorHighlightJSON(id) {
  const originalAddResult = TestRunner.addResult;
  TestRunner.addResult = function(text) {
    if (text.startsWith(id)) {
      const jsonStr = text.substring(id.length);
      try {
        const parsed = JSON.parse(jsonStr);
        const sorted = sortHighlightObject(parsed);
        originalAddResult.call(TestRunner, id + JSON.stringify(sorted, null, 2));
        return;
      } catch (e) {
        // Fallback if not valid JSON
      }
    }
    originalAddResult.call(TestRunner, text);
  };

  try {
    await new Promise(resolve => ElementsTestRunner.dumpInspectorHighlightJSON(id, resolve));
  } finally {
    TestRunner.addResult = originalAddResult;
  }
}

export async function dumpStableInspectorGridHighlightsJSON(idValues) {
  const originalAddResult = TestRunner.addResult;
  TestRunner.addResult = function(text) {
    try {
      const parsed = JSON.parse(text);
      const sorted = sortHighlightObject(parsed);
      originalAddResult.call(TestRunner, JSON.stringify(sorted, null, 2));
      return;
    } catch (e) {
      // Fallback
    }
    originalAddResult.call(TestRunner, text);
  };

  try {
    await new Promise(resolve => ElementsTestRunner.dumpInspectorGridHighlightsJSON(idValues, resolve));
  } finally {
    TestRunner.addResult = originalAddResult;
  }
}
