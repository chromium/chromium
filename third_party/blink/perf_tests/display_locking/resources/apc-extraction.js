// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Measures the cost of annotated page content (APC) extraction on a large
// spreadsheet-like page with 20k rows x 20 columns (400k cells). The rows are
// split into 100 tables; all tables but the first are wrapped in a
// `div.waffle-chunk`. Tests can style `.waffle-chunk` with
// `content-visibility: auto` so that off-screen chunks skip style and layout
// during rendering.
//
// Each iteration rebuilds the sheet, waits for rendering to update style and
// layout, and then measures one extraction via
// internals.extractPageContent(document). The DOM is rebuilt so every iteration
// measures the first extraction after the content was rendered.
//
// Requires content_shell with --expose-internals-for-testing.

(function() {

const kTables = 100;
const kRowsPerTable = 200;
const kColumns = 20;

const kLoremWords = [
  'lorem',      'ipsum',        'dolor',
  'sit',        'amet',         'consectetur',
  'adipiscing', 'elit',         'sed',
  'do',         'eiusmod',      'tempor',
  'incididunt', 'ut',           'labore',
  'et',         'dolore',       'magna',
  'aliqua',     'enim',         'ad',
  'minim',      'veniam',       'quis',
  'nostrud',    'exercitation', 'ullamco',
  'laboris',    'nisi',         'aliquip',
  'ex',         'ea',           'commodo',
  'consequat',  'duis',         'aute',
  'irure',      'in',           'reprehenderit',
  'voluptate',  'velit',        'esse',
  'cillum',     'fugiat',       'nulla',
  'pariatur',   'excepteur',    'sint',
  'occaecat',   'cupidatat'
];

function word(i) {
  return kLoremWords[i % kLoremWords.length];
}

function cellText(row, column) {
  const index = row * 7 + column * 13;
  return word(index) + ' ' + word(index + 1);
}

function rowHtml(row) {
  let html = '<tr><th><div>' + (row + 1) + '</div></th>';
  for (let column = 0; column < kColumns; ++column) {
    const text = row == 0 ? word(column) : cellText(row, column);
    html += '<td class="' + (row == 0 ? 's0' : 's1') + '">' + text + '</td>';
  }
  return html + '</tr>';
}

function tableHtml(table) {
  let html = '<table class="waffle grid-minimized" cellspacing="0" ' +
      'cellpadding="0">';
  if (table == 0) {
    html += '<thead><tr><th class="row-header"></th>';
    for (let column = 0; column < kColumns; ++column) {
      html += '<th style="width:120px;">' + String.fromCharCode(65 + column) +
          '</th>';
    }
    html += '</tr></thead>';
  } else {
    html += '<colgroup><col class="row-header">';
    for (let column = 0; column < kColumns; ++column)
      html += '<col style="width:121px;">';
    html += '</colgroup>';
  }
  html += '<tbody>';
  for (let row = table * kRowsPerTable; row < (table + 1) * kRowsPerTable;
       ++row) {
    html += rowHtml(row);
  }
  return html + '</tbody></table>';
}

function sheetHtml() {
  let html = '<div class="ritz grid-container" dir="ltr">' + tableHtml(0);
  for (let table = 1; table < kTables; ++table)
    html += '<div class="waffle-chunk">' + tableHtml(table) + '</div>';
  return html + '</div>';
}

// Resolves after rendering has run style and layout for the current DOM, with
// no further lifecycle update pending. Waits two frames since the first frame
// after a `content-visibility: auto` element is inserted determines which
// elements are on screen, and the next one renders those.
function waitForRenderingUpdate() {
  return new Promise(resolve => {
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        setTimeout(resolve, 0);
      });
    });
  });
}

window.runAPCExtractionTest = function(description) {
  if (!window.internals || !internals.extractPageContent) {
    PerfTestRunner.logFatalError(
        'internals.extractPageContent is unavailable. ' +
        'Run content_shell with --expose-internals-for-testing.');
    return;
  }

  const viewport = document.getElementById('sheets-viewport');
  const html = sheetHtml();
  let isDone = false;

  async function runIteration() {
    viewport.innerHTML = '';
    PerfTestRunner.gc();
    viewport.innerHTML = html;
    await waitForRenderingUpdate();

    PerfTestRunner.addRunTestStartMarker();
    const startTime = PerfTestRunner.now();
    internals.extractPageContent(document);
    const elapsed = PerfTestRunner.now() - startTime;
    PerfTestRunner.addRunTestEndMarker();
    PerfTestRunner.measureValueAsync(elapsed);

    if (!isDone)
      setTimeout(runIteration, 0);
  }

  PerfTestRunner.startMeasureValuesAsync({
    description,
    unit: 'ms',
    warmUpCount: 1,
    iterationCount: 5,
    doNotMeasureMemoryUsage: true,
    done: () => {
      isDone = true;
    },
    run: runIteration,
  });
};
})();
