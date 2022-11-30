// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that watch expressions expansion state is restored after update.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      var globalObject = {
          foo: {
              bar: {
                  baz: 2012
              }
          }
      };
      var windowAlias = window;
      var array = [];
      for (var i = 0; i < 300; ++i)
          array[i] = i;

      (function()
      {
          var a = 10;
          var b = 100;
          window.func = function() {return a + b;}
      }());
  `);

  var watchExpressionsPane = Sources.WatchExpressionsSidebarPane.instance();
  UI.panels.sources.sidebarPaneStack
      .showView(UI.panels.sources.watchSidebarPane)
      .then(() => {
        watchExpressionsPane.doUpdate();
        watchExpressionsPane.createWatchExpression('globalObject');
        watchExpressionsPane.createWatchExpression('windowAlias');
        watchExpressionsPane.createWatchExpression('array');
        watchExpressionsPane.createWatchExpression('func');
        watchExpressionsPane.saveExpressions();
        TestRunner.deprecatedRunAfterPendingDispatches(step2);
      });

  function step2() {
    TestRunner.addResult('Watch expressions added.');
    var expandArray = expandWatchExpression.bind(
        null, ['array', '[200 \u2026 299]', '299'], step3);
    var expandFunc = expandWatchExpression.bind(
        null, ['func', '[[FunctionLocation]]'], expandArray);
    expandWatchExpression(['globalObject', 'foo', 'bar'], expandFunc);
  }

  function step3() {
    TestRunner.addResult('Watch expressions expanded.');
    dumpWatchExpressions();
    TestRunner.reloadPage(step4);
  }

  function step4() {
    TestRunner.addResult('Watch expressions after page reload:');
    dumpWatchExpressions();
    TestRunner.completeTest();
  }

  function dumpWatchExpressions() {
    var pane = Sources.WatchExpressionsSidebarPane.instance();

    for (var i = 0; i < pane.watchExpressions.length; i++) {
      var watch = pane.watchExpressions[i];
      TestRunner.addResult(
          watch.expression() + ': ' +
          watch.treeElement().object.description);
      dumpObjectPropertiesTreeElement(
          watch.treeElement(), '  ');
    }
  }

  function dumpObjectPropertiesTreeElement(treeElement, indent) {
    if (treeElement.property && treeElement.property.name === '[[Scopes]]') return;
    if (treeElement.property)
      addResult(
          indent + treeElement.property.name + ': ' +
          treeElement.property.value.description);
    else if (typeof treeElement.title === 'string')
      addResult(indent + treeElement.title);

    for (var i = 0; i < treeElement.children().length; i++)
      dumpObjectPropertiesTreeElement(treeElement.children()[i], '  ' + indent);
  }

  function expandProperties(watchExpressionTreeElement, path, callback) {
    const treeOutline = watchExpressionTreeElement.treeOutline;
    treeOutline.addEventListener(
        UI.TreeOutline.Events.ElementAttached, elementAttached);
    watchExpressionTreeElement.expand();

    function elementAttached(event) {
      var treeElement = event.data;
      var currentName =
          treeElement.property ? treeElement.property.name : treeElement.title;
      if (currentName !== path[0])
        return;

      var childName = path.shift();
      addResult(
          'expanded ' + childName + ' ' +
          (treeElement.property ? treeElement.property.value : ''));

      if (path.length) {
        treeElement.expand();
        return;
      }

      treeOutline.removeEventListener(
          UI.TreeOutline.Events.ElementAttached, elementAttached);
      callback();
    }
  }

  function expandWatchExpression(path, callback) {
    var pane = Sources.WatchExpressionsSidebarPane.instance();
    var expression = path.shift();
    for (var i = 0; i < pane.watchExpressions.length; i++) {
      var watch = pane.watchExpressions[i];
      if (watch.expression() === expression) {
        expandProperties(watch.treeElement(), path, callback);
        break;
      }
    }
  }

  function addResult(string) {
    TestRunner.addResult(string.replace('\u2026', '..'));
  }
})();
