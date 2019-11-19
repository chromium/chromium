// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the AudioContextSelector.`);
  await TestRunner.loadModule('web_audio');

  /** @type {!Protocol.WebAudio.BaseAudioContext} */
  const context1 = {
    contextId: '924c4ee4-4cae-4e62-b4c6-71603edc39fd',
    contextType: 'realtime'
  };
  /** @type {!Protocol.WebAudio.BaseAudioContext} */
  const context2 = {
    contextId: '78a3e94e-4968-4bf6-8905-325109695c9e',
    contextType: 'realtime'
  };

  function dumpSelectorState(
      /** @type {!WebAudio.AudioContextSelector} */ selector) {
    TestRunner.addResult(`
Number of contexts (items): ${selector._items.length}
Title: ${selector.toolbarItem()._title}}
Selected Context: ${JSON.stringify(selector.selectedContext(), null, 3)}
`);
  }

  TestRunner.runAsyncTestSuite([
    async function testStartsEmpty() {
      const selector = new WebAudio.AudioContextSelector();

      dumpSelectorState(selector);
    },

    async function testSelectsCreatedContext() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});

      dumpSelectorState(selector);
    },

    async function testResetClearsList() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.reset();

      dumpSelectorState(selector);
    },

    async function testReSelectsCreatedContextAfterChange() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextChanged({data: context1});

      dumpSelectorState(selector);
    },

    async function testFirstCreatedContextStaysSelected() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});

      dumpSelectorState(selector);
    },

    async function testChangingContextDoesNotChangeSelection() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});
      selector.contextChanged({data: context2});

      dumpSelectorState(selector);
    },

    async function testSelectedContextBecomesSelected() {
      const selector = new WebAudio.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});
      selector.itemSelected(context2);

      dumpSelectorState(selector);
    },

    async function testOnListItemReplacedCalled() {
      function dumpItemCount() {
        TestRunner.addResult(
            `_onListItemReplaced called with contexts (items) count: ${
                this._items.length}`);
      }

      TestRunner.addSniffer(
          WebAudio.AudioContextSelector.prototype, '_onListItemReplaced',
          dumpItemCount);

      const selector = new WebAudio.AudioContextSelector();
      selector.contextCreated({data: context1});
      selector.contextChanged({data: context1});

      dumpSelectorState(selector);
    },
  ]);
})();