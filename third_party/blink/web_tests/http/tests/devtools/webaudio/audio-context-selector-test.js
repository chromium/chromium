// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as WebAudioModule from 'devtools/panels/web_audio/web_audio.js';

(async function() {
  TestRunner.addResult(`Tests the AudioContextSelector.`);

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
      /** @type {!WebAudioModule.AudioContextSelector.AudioContextSelector} */ selector) {
    TestRunner.addResult(`
Number of contexts (items): ${selector.items.length}
Title: ${selector.toolbarItem().title}}
Selected Context: ${JSON.stringify(selector.selectedContext(), null, 3)}
`);
  }

  TestRunner.runAsyncTestSuite([
    async function testStartsEmpty() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      dumpSelectorState(selector);
    },

    async function testSelectsCreatedContext() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});

      dumpSelectorState(selector);
    },

    async function testResetClearsList() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.reset();

      dumpSelectorState(selector);
    },

    async function testReSelectsCreatedContextAfterChange() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextChanged({data: context1});

      dumpSelectorState(selector);
    },

    async function testFirstCreatedContextStaysSelected() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});

      dumpSelectorState(selector);
    },

    async function testChangingContextDoesNotChangeSelection() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});
      selector.contextChanged({data: context2});

      dumpSelectorState(selector);
    },

    async function testSelectedContextBecomesSelected() {
      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();

      selector.contextCreated({data: context1});
      selector.contextCreated({data: context2});
      selector.itemSelected(context2);

      dumpSelectorState(selector);
    },

    async function testOnListItemReplacedCalled() {
      function dumpItemCount() {
        TestRunner.addResult(
            `onListItemReplaced called with contexts (items) count: ${
                this.items.length}`);
      }

      TestRunner.addSniffer(
          WebAudioModule.AudioContextSelector.AudioContextSelector.prototype, 'onListItemReplaced',
          dumpItemCount);

      const selector = new WebAudioModule.AudioContextSelector.AudioContextSelector();
      selector.contextCreated({data: context1});
      selector.contextChanged({data: context1});

      dumpSelectorState(selector);
    },
  ]);
})();