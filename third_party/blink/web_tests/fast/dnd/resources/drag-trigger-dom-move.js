'use strict';

/** Moves the mouse to the center of |element|. */
const mouseMoveToCenter = element => {
  const clientRect = element.getBoundingClientRect();
  const centerX = (clientRect.left + clientRect.right) / 2;
  const centerY = (clientRect.top + clientRect.bottom) / 2;
  if (window.eventSender)
    eventSender.mouseMoveTo(centerX, centerY);
};

/**
 * Recursively loads content into a series of nested iframes.
 * Returns a Promise that resolves with the HTMLDocument of the innermost frame.
 */
const loadNestedFrames = async domRoot => {
  const frame = domRoot.querySelector('iframe');
  if (!frame)
    return domRoot;

  const htmlSourceId = frame.getAttribute('data-source');
  frame.srcdoc = document.getElementById(htmlSourceId).textContent;
  const frameLoaded = new Promise(resolve => { frame.onload = resolve; });
  await frameLoaded;

  return await loadNestedFrames(frame.contentDocument);
};

/** Retrieves an element with id in an arbitrarily deep nesting of iframes. */
const getElementByIdAcrossIframes = (domRoot, id) => {
  if (!domRoot)
    return null;

  const dragBox = domRoot.getElementById(id);
  if (dragBox)
    return dragBox;

  return getElementByIdAcrossIframes(
      domRoot.querySelector('iframe').contentDocument, id);
};

const loadIframe = async () => {
  await loadNestedFrames(document);
  return document.getElementById('outer-iframe');
};

const loadImage = async () => {
  const image = document.createElement('img');
  image.src = '../resources/greenbox.png';

  const imageLoaded = new Promise(resolve => {
    image.onload = resolve(image);
    document.getElementById('moved-item-source').appendChild(image);
  });
  return await imageLoaded;
};

const loadMovedItem = async loadItem => {
  if (loadItem.includes('iframe'))
    return await loadIframe();
  else if (loadItem.includes('image'))
    return await loadImage();
};

/**
 * Test if moving an element (iframe or image) will cancel dragging by
 *     resetting drag source.
 * testCase: a testCase to test, containing a specified type to load and
 *     whether or not dragend is expected to fire, as well as the action
 *     to attempt.
 */
const dragDomMoveTest = testCase => {
  promise_test(async t => {
    document.querySelector('#test-description').textContent =
        JSON.stringify(testCase);

    const gotEvent = {
      dragStart: false,
      dragOver: false,
      drop: false,
      dragEnd: false,
    };

    let movedItem = await loadMovedItem(testCase.load);
    const dragBox = getElementByIdAcrossIframes(document, 'drag-box');
    const dropBox = document.getElementById('drop-box');
    const doneButton = document.createElement('button');

    dragBox.ondragstart = t.step_func(e => {
      gotEvent.dragStart = true;
      e.dataTransfer.setData('text/plain', 'Needed to work in Firefox');
    });
    dropBox.ondragover = t.step_func(e => {
      gotEvent.dragOver = true;
      e.preventDefault();
    });

    const dndTest = new Promise(resolve => {
      dragBox.ondragend = t.step_func(e => {
        gotEvent.dragEnd = true;
        return resolve();
      });
      dropBox.ondrop = t.step_func(async e => {
        gotEvent.drop = true;
        e.preventDefault();

        const movedItemDestination =
            document.getElementById('moved-item-destination');
        const movedItemSource =
            document.getElementById('moved-item-source');

        // Test whether dragging away or detaching movedItem
        // will disable dragging.
        if (testCase.action == 'removeChild')
          movedItem = movedItem.parentNode.removeChild(movedItem);
        else if (testCase.action == 'appendChild')
          movedItemDestination.appendChild(movedItem);
        else
          return reject('Error: Invalid testCase.action. Please make sure the testCase is spelled correctly');

        // Click to resolve test as backup in case dragend never triggers to
        // end the test.
        setTimeout(() => {
          const clickEvent = new Event('click');
          doneButton.dispatchEvent(clickEvent);
        }, 100);

        // Reset iframe location to teardown and prep for next test.
        if (testCase.load.includes('iframe')) {
          const movedItemLoaded = new Promise(resolve => {
            movedItem.onload = resolve;
            setTimeout(() => { movedItemSource.appendChild(movedItem); }, 100);
          });

          await movedItemLoaded;
        }
      });

      doneButton.onclick = t.step_func(() => {
        return resolve();
      })

      // Do drag and drop.
      if (window.eventSender) {
        mouseMoveToCenter(dragBox);
        eventSender.mouseDown();
        setTimeout(() => {
          mouseMoveToCenter(dropBox);
          eventSender.mouseUp();
        }, 100);
      }
    });
    await dndTest;

    assert_true(gotEvent.dragStart,
        'drag-box should have gotten a dragstart event');
    assert_true(gotEvent.dragOver,
        'drop-box should have gotten a dragover event');
    assert_true(gotEvent.drop,
        'drop-box should have gotten a drop event');
    assert_equals(gotEvent.dragEnd, testCase.expectDragEnd,
        'drag-box should have gotten a dragEnd event');
  }, `tested with input: ${testCase.load}, ${testCase.action}`);
};

const dragDomMoveTests = testCases => {
  for (let testCase of testCases)
    dragDomMoveTest(testCase);

  promise_test(() => {
    return Promise.resolve().then(() => {
      document.querySelector('#test-description').textContent = 'done';
    });
  }, 'all tests complete');
}
