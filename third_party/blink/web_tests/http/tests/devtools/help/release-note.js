// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('help');

  TestRunner.addResult(`Test release note\n`);

  Help.releaseNoteText = [
    {
      version: 99,
      header: 'Highlights from Chrome 100 update',
      highlights: [
        {
          title: 'Improved Performance and Memory panels',
          subtitle: '',
          link: 'https://developers.google.com/web/tools/chrome-devtools/',
        },
        {
          title: 'Edit cookies directly from the Application panel',
          subtitle: '',
          link: 'https://developers.google.com/web/tools/chrome-devtools/',
        },
      ],
      link: 'https://developers.google.com/web/tools/chrome-devtools/',
    },
  ];

  TestRunner.addSniffer(UI.viewManager, 'showView', onShowView);

  TestRunner.addResult('Last release note version seen:');
  Help._releaseNoteVersionSetting.set(1);
  TestRunner.addResult(Help._releaseNoteVersionSetting.get() + '\n');
  Help._showReleaseNoteIfNeeded();

  function onShowView(viewId, isUserGesture, viewPromise) {
    viewPromise.then(() => {
      var releaseNoteView = UI.viewManager.view('release-note');
      var releaseNoteElement = releaseNoteView[UI.View.widgetSymbol].contentElement;
      TestRunner.addResult('Dumping release note text:');
      TestRunner.addResult(releaseNoteElement.innerText);
      TestRunner.addResult('Last version of release note seen should be updated:');
      TestRunner.addResult(Help._releaseNoteVersionSetting.get() + '\n');

      TestRunner.addSniffer(UI.InspectorView.prototype, 'closeDrawerTab', onClose);
      TestRunner.addResult('Click on hide button');
      var closeButton = releaseNoteElement.querySelector('.close-release-note');
      closeButton.click();
    });
  }

  function onClose(view) {
    TestRunner.addResult(`Hiding view: ${view}`);
    TestRunner.completeTest();
  }
})();
