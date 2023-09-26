// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel updates dom tree structure when shadow dom slots are reassigned.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.evaluateInPagePromise(`
      function createShadowRoot(hostId, slots)
      {
          var host = document.createElement("div");
          host.id = hostId;
          document.body.appendChild(host);

          var shadow = host.attachShadow({ mode: "open" });
          for (var i = 0; i < slots.length; i++) {
              var slot = document.createElement("slot");
              slot.id = "slot" + (i + 1);
              if (slots[i])
                  slot.setAttribute("name", slots[i]);
              shadow.appendChild(slot);
          }
      }

      function createChild(hostId, childId, tagName, slotName)
      {
          var child = document.createElement(tagName);
          child.id = childId;
          if (slotName)
              child.setAttribute("slot", slotName);
          var host = document.getElementById(hostId);
          host.appendChild(child);
      }

      function resolveElement(elementId)
      {
          var parts = elementId.split(".");
          var root = document;
          while (parts.length > 1) {
              root = root.getElementById(parts[0]).shadowRoot;
              parts.shift();
          }
          return root.getElementById(parts[0]);
      }

      function changeAttribute(elementId, name, value)
      {
          var element = resolveElement(elementId);
          if (value)
              element.setAttribute(name, value);
          else
              element.removeAttribute(name);
      }

      function removeElement(elementId)
      {
          var element = resolveElement(elementId);
          element.parentNode.removeChild(element);
      }

      function reparentElement(elementId, parentId)
      {
          var element = resolveElement(elementId);
          var parent = resolveElement(parentId);
          parent.appendChild(element);
      }

      // In order for the elements tree to reflect the changes made during this test,
      // there needs to be a step that ensures that Shadow DOM slot assignments are updated.
      // Forcing a style recalc ensures that slot assignments are updated, and that
      // the relevant DevTools CDP events are sent to the front end.
      function updateSlotAssignmentsIfNeeded() {
        getComputedStyle(document.documentElement).left;
      }
  `);

  TestRunner.runTestSuite([
    function createHost1(next) {
      evalAndDump('createShadowRoot(\'host1\', [\'slot1\', \'slot2\', \'\'])', 'host1', next);
    },

    function createChild1(next) {
      evalAndDump('createChild(\'host1\', \'child1\', \'span\', \'slot2\')', 'host1', next);
    },

    function createChild2(next) {
      evalAndDump('createChild(\'host1\', \'child2\', \'div\', \'\')', 'host1', next);
    },

    function createChild3(next) {
      evalAndDump('createChild(\'host1\', \'child3\', \'h1\', \'slot2\')', 'host1', next);
    },

    function createChild4(next) {
      evalAndDump('createChild(\'host1\', \'child4\', \'h2\', \'slot1\')', 'host1', next);
    },

    function createChild5(next) {
      evalAndDump('createChild(\'host1\', \'child5\', \'h3\', \'slot3\')', 'host1', next);
    },

    function modifyChild1(next) {
      evalAndDump('changeAttribute(\'child1\', \'slot\', \'slot1\')', 'host1', next);
    },

    function modifyChild4(next) {
      evalAndDump('changeAttribute(\'child4\', \'slot\', \'\')', 'host1', next);
    },

    function modifySlot1(next) {
      evalAndDump('changeAttribute(\'host1.slot1\', \'name\', \'slot3\')', 'host1', next);
    },

    function modifySlot2(next) {
      evalAndDump('changeAttribute(\'host1.slot2\', \'name\', \'slot1\')', 'host1', next);
    },

    function removeChild3(next) {
      evalAndDump('removeElement(\'child3\')', 'host1', next);
    },

    function removeChild1(next) {
      evalAndDump('removeElement(\'child1\')', 'host1', next);
    },

    function removeSlot1(next) {
      evalAndDump('removeElement(\'host1.slot1\')', 'host1', next);
    },

    function createHost2(next) {
      evalAndDump('createShadowRoot(\'host2\', [\'slot3\'])', 'host2', next);
    },

    function moveChild5FromHost1ToHost2(next) {
      evalAndDump('reparentElement(\'child5\', \'host2\')', 'host2', next);
    },

    function modifyChild4(next) {
      evalAndDump('changeAttribute(\'child4\', \'slot\', \'slot1\')', 'host1', next);
    },
  ]);

  function evalAndDump(code, nodeId, next) {
    TestRunner.evaluateInPage(code, ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, callback));

    function callback() {
      TestRunner.evaluateInPage('updateSlotAssignmentsIfNeeded()', ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, dump));
    }

    function dump() {
      ElementsTestRunner.dumpElementsTree(ElementsTestRunner.expandedNodeWithId(nodeId));
      next();
    }
  }
})();
