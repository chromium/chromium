// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that you can click and hover on links in attributes.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <img id="linkify-1" srcset="./1x/googlelogo_color_272x92dp.png 1x, ./2x/googlelogo_color_272x92dp.png 2x">
      <img id="linkify-2" srcset="./1x/googlelogo_color_272x92dp.png">
      <picture>
          <source id="linkify-3" media="(min-width: 650px)" srcset="./kitten-large.png">
          <source id="linkify-4" media="(min-width: 465px)" srcset="./kitten-medium.png">
          <source id="linkify-5" media="(min-width: 400px)" srcset="./kitten-large.png 2x">
          <img id="linkify-6" src="./kitten-small.png">
          <img id="linkify-7" srcset="./kitten-medium.png, ./kitten-large.png 2x">
          <img id="linkify-8" srcset="./kitten-medium.png 1x, ./kitten-large.png 2x">
          <img id="linkify-9" srcset="./kitten-medium.png 1x,./kitten-large.png 2x">
          <img id="linkify-10" srcset="data:,abc">
          <img id="linkify-11" srcset="data:,abc 1x">
          <img id="linkify-12" srcset="data:,abc 1x, data:,def 2x">
          <img id="linkify-13" srcset="data:,abc 1x,data:,def 2x">
      </picture>
      <a id="linkify-14" href="http://www.google.com">a link</a>
      <svg width="200" height="200"
        xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
        <image id="linkify-15" xlink:href="./kitten-small.png" height="200" width="200"/>
        <image id="linkify-16" href="./kitten-small.png" height="200" width="200"/>
      </svg>
    `);

  var i = 0;
  var last = 16;

  function check() {
    i++;
    if (i > last)
      return TestRunner.completeTest();

    ElementsTestRunner.selectNodeWithId('linkify-' + i, function() {
      var treeElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;

      TestRunner.addResult('\nRendered text: ' + treeElement.title.textContent);

      // Print the embedded links.
      var links = treeElement.title.querySelectorAll('.devtools-link');
      links.forEach(link => {
        var offset = 0;
        var node = treeElement.title;
        while ((node = node.traverseNextTextNode(treeElement.title)) && !node.isSelfOrDescendant(link))
          offset += node.textContent.length;
        TestRunner.addResult('Link at offset: ' + offset + ': ' + link.textContent);
      });

      check();
    });
  }
  check();
})();
