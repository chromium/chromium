'use strict';

const mouseMoveToCenter = (element) => {
  const clientRect = element.getBoundingClientRect();
  const centerX = (clientRect.left + clientRect.right) / 2;
  const centerY = (clientRect.top + clientRect.bottom) / 2;
  eventSender.mouseMoveTo(centerX, centerY);
}

const runLinkDraggingTest = (t, params) => {
  let gotDragStart = false
  let dragStartEffect = null;
  let dragStartUriList = null;
  let dragStartTypes = null;
  let dragStartTarget = null;

  // Enables the Ahem font, which makes the drag image platform-independent.
  if (window.testRunner)
    document.body.setAttribute('class', 'test-runner');

  const dragged = document.querySelector('.dragged');
  dragged.ondragstart = (event) => {
    if (!gotDragStart) {
      gotDragStart = true;
      dragStartEffect = event.dataTransfer.dropEffect;
      dragStartUriList = event.dataTransfer.getData('text/uri-list');
      dragStartTypes = [].concat(event.dataTransfer.types).sort();
      dragStartTarget = event.target;
    }
  }

  let gotDrop = false;
  let dropEffect = null;
  const dropZone = document.querySelector('.dropzone');
  dropZone.ondragover = (event) => event.preventDefault();

  const testDone = document.querySelector('.done');
  return new Promise((resolve, reject) => {
    dropZone.ondrop = (event) => {
      if (!gotDrop) {
        gotDrop = true;
        dropEffect = event.dataTransfer.dropEffect;
      }
      // Some of the drags would result in the link being followed in Firefox.
      event.preventDefault();

      resolve(true);
    }

    if (window.eventSender) {
      mouseMoveToCenter(dragged);
      eventSender.mouseDown();
      setTimeout(() => {
        mouseMoveToCenter(dropZone);
        eventSender.mouseUp();
      }, 100);
    }
  }).then(() => t.step(() => {
    assert_equals(gotDragStart, true,
        'dragstart event should fire for all drags');
    if (gotDragStart) {
      assert_true(dragStartTarget.classList.contains('dragged'),
          'the dragged element should have the class "dragged"');
      assert_equals(dragStartEffect, 'none',
          'dataTransfer.dropEffect must always default to "none" in dragstart');
      assert_equals(dragStartUriList, params.dragStartUriList,
          'incorrect dataTransfer.getData("text/uri-list") in dragstart');
      assert_array_equals(dragStartTypes, params.dragStartTypes,
          'incorrect dataTransfer types in dragstart');
    }

    if (gotDrop) {
      assert_equals(dropEffect, 'none',
          'dataTransfer.dropEffect must always default to "none" in drop');
    }
  }));
}

if (window.testRunner)
  testRunner.dumpDragImage();
