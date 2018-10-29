// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the tooltip for the image on hover.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  const imgURL = `data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAANcAAACuCAIAAAAqMg/rAAAAAXNSR0IArs4c6QAAAU9JREFUeNrt0jERAAAIxDDAv+dHAxNLIqHXTlLwaiTAheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSF4EJcCC7EheBCXAguxIXgQlwILsSFEuBCcCEuBBfiQnAhLgQX4kJwIS4EF+JCcCEuBBfiQnAhLgQX4kJwIS4EF+JCcCEuBBfiQnAhLgQX4kJwIS4EF+JCcCEuBBfiQnAhLoSDBZXqBFnkRyeqAAAAAElFTkSuQmCC`;
  await TestRunner.loadHTML(`
      <img id="image" src="${imgURL}">
      <img id="image-bordered" style="border: 5px solid; width: 20px; height: 20px" src="${imgURL}">
      <img id="image-box-size" style="box-sizing: border-box; border: 5px solid; width: 20px; height: 20px" src="${imgURL}">
    `);

  var treeElement;
  const node = await ElementsTestRunner.nodeWithIdPromise('image');
  const nodeWithBorder = await ElementsTestRunner.nodeWithIdPromise('image-bordered');
  const nodeWithBoxSizing = await ElementsTestRunner.nodeWithIdPromise('image-box-size');
  const treeOutline = ElementsTestRunner.firstElementsTreeOutline();
  dumpDimensions(await Components.ImagePreview.loadDimensionsForNode(node));
  dumpDimensions(await Components.ImagePreview.loadDimensionsForNode(nodeWithBorder));
  dumpDimensions(await Components.ImagePreview.loadDimensionsForNode(nodeWithBoxSizing));

  TestRunner.completeTest();

  /**
   * @param {!Object}
   */
  function dumpDimensions(dimensions) {
    const {currentSrc, ...sizes} = dimensions;
    TestRunner.addResult(JSON.stringify(sizes));
  }
})();
