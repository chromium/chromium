// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Smoke test for basic FlameChart functionality.\n`);

  await TestRunner.loadLegacyModule("perf_ui");

   class FlameChartProvider {
    constructor(entries, groups, defaults) {
        this._entries = entries;
        this._defaults = defaults || {};

        var entryLevels = entries.map(e => e.level);
        var entryTotalTimes = entries.map(e => e.end - e.start);
        var entryStartTimes = entries.map(e => e.start);
        this._data = new PerfUI.FlameChart.TimelineData(entryLevels, entryTotalTimes, entryStartTimes, groups);
    }

    entryTitle(index) {
        return this._entries[index].title || this._defaults.title || "";
    }

    entryColor(index) {
        return this._entries[index].entryColor || this._defaults.entryColor || "green";
    }

    textColor(index) {
        return this._entries[index].textColor || this._defaults.textColor || "blue";
    }

    entryFont(index) {
        return this._entries[index].entryFont || this._defaults.entryFont || "12px";
    }

    timelineData() {
        return this._data;
    }

    minimumBoundary() {
        return this._data.entryStartTimes.reduce((a, b) => Math.min(a, b));
    }

    totalTime() {
        return this._data.entryStartTimes.reduce((a, b) => Math.min(a, b)) - this.minimumBoundary();
    }

    maxStackDepth() {
        return this._data.entryLevels.reduce((a, b) => Math.max(a, b));
    }

    prepareHighlightedEntryInfo(index) {
        return null;
    }

    camJumpToEntry(index) {
        return false;
    }

    forceDecoration(index) {
        return false;
    }

    decorateEntry(entryIndex, context, text, barX, barY, barWidth, barHeight, unclippedBarX, timeToPixels) {
    }

    highlightEntry(entryIndex) {
    }

    navStartTimes() {
        return [];
    }
  }

  var entries = [
    {start: 1000, end: 5000, level: 0, title: 'AAAAAAAAAAAAAAAAAAAAAA'},
    {start: 2000, end: 3000, level: 1, title: 'bbbb'},
    {start: 2000, end: 3000, level: 2, title: 'cccc'},
    {start: 4000, end: 5000, level: 1, title: 'dddd'},
    {start: 6000, end: 7000, level: 0, title: 'eeee'},
  ];

  try {
    var provider = new FlameChartProvider(entries, null);
    var flameChart = new PerfUI.FlameChart(provider, new PerfUI.FlameChartDelegate());
    flameChart.update();
    TestRunner.addResult('PASSED');
  } catch (e) {
    TestRunner.addResult('Failed: ' + e.stack);
  }
  TestRunner.completeTest();
})();
