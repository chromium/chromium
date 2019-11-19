'use strict';

const mouseMoveToCenter = element => {
  const clientRect = element.getBoundingClientRect();
  const centerX = (clientRect.left + clientRect.right) / 2;
  const centerY = (clientRect.top + clientRect.bottom) / 2;
  eventSender.mouseMoveTo(centerX, centerY);
};

const dropEffectTest = testCase => {
  let gotDrop = false;
  promise_test(t => new Promise((resolve, reject) => {
    document.querySelector('#test-description').textContent =
        JSON.stringify(testCase);

    const dragged = document.querySelector('.dragged');
    if (dragged && !dragged.classList.contains('no-ondragstart')) {
      dragged.ondragstart = t.step_func(event => {
        event.dataTransfer.setData('text/plain', 'Needed to work in Firefox');
        if ('allowed' in testCase)
          event.dataTransfer.effectAllowed = testCase.allowed;
      });
    }

    const dropZone = document.querySelector('.dropzone');
    dropZone.ondragover = t.step_func(event => {
      event.preventDefault();
      if ('drop' in testCase)
        event.dataTransfer.dropEffect = testCase.drop;
    });
    dropZone.ondrop = t.step_func(event => {
      event.preventDefault();
      gotDrop = true;
    });

    const doneButton = document.querySelector('.done');

    if (dragged) {
      dragged.ondragend = t.step_func(event => {
        resolve(event.dataTransfer.dropEffect);
      });
    } else {
      doneButton.onclick = t.step_func(() => {
        resolve('none');
      });
    }

    if (window.eventSender) {
      if (dragged) {
        mouseMoveToCenter(dragged);
        if ('keyPressed' in testCase)
          eventSender.keyDown(testCase.keyPressed);
        eventSender.mouseDown();
      } else {
        eventSender.mouseMoveTo(0, 0);
        eventSender.beginDragWithFiles(['resources/dragged-file.txt']);
      }
      setTimeout(() => {
        mouseMoveToCenter(dropZone);
        eventSender.mouseUp();
        //TODO(huangdarwin): Use eventSender.keyUp() here when it becomes
        //available, to mirror eventSender.keyDown()
        if (doneButton) {
          setTimeout(() => {
            const clickEvent = new Event('click');
            doneButton.dispatchEvent(clickEvent);
          }, 100);
        }
      }, 100);
    }
  }).then(dragOperation => {
    if ('operation' in testCase) {
      assert_true(gotDrop, 'drop target should have received a drop event');
      assert_equals(dragOperation, testCase.operation);
    } else {
      assert_true(
          !gotDrop, 'drop target should not have received a drop event');
      assert_equals(dragOperation, 'none');
    }
  }), `effectAllowed: ${testCase.allowed} keyPressed: ${testCase.keyPressed} dropEffect: ${testCase.drop}`);
};

const dropEffectTests = testCases => {
  for (let testCase of testCases)
    dropEffectTest(testCase);

  promise_test(t => {
    return Promise.resolve().then(() => {
      document.querySelector('#test-description').textContent = 'done';
    });
  }, 'all tests complete');
}
