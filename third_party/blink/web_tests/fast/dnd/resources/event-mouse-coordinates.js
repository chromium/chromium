'use strict';

// The mouse bot will always be precise. Humans can click anywhere in the boxes.
const epsilon = (window.eventSender) ? 0 : 50;

const elementCenter = (element) => {
  const clientRect = element.getBoundingClientRect();
  const centerX = (clientRect.left + clientRect.right) / 2;
  const centerY = (clientRect.top + clientRect.bottom) / 2;
  return { x: centerX, y: centerY };
};

const mouseMoveToCenter = (element, frameOffset) => {
  const center = elementCenter(element);
  eventSender.mouseMoveTo(center.x + frameOffset.x, center.y + frameOffset.y);
};

const mouseEventCoordinates = (event) => {
  return {
    client: { x: event.clientX, y: event.clientY },
    offset: { x: event.offsetX, y: event.offsetY },
    page: { x: event.pageX, y: event.pageY },
    screen: { x: event.screenX, y: event.screenY },
  };
};

// Recursively loads content into a series of nested iframes.
// Returns a promise that resolves with the HTMLDocument of the innermost frame.
const loadNestedFrames = (domRoot) => {
  const frame = domRoot.querySelector('iframe');
  if (!frame)
    return Promise.resolve(domRoot);

  return new Promise((resolve, reject) => {
    const htmlSourceId = frame.getAttribute('data-source');
    const html = document.getElementById(htmlSourceId).textContent;
    frame.onload = () => {
      const frameDocument = frame.contentDocument;
      resolve(loadNestedFrames(frameDocument));
    };
    frame.setAttribute('srcdoc', html);
  });
};

const runDragTest = (t, params) => {
  const domRoot = params.domRoot;

  const dragged = domRoot.querySelector('.dragged');
  let dragStartCoordinates = null;
  dragged.ondragstart = (event) => {
    dragStartCoordinates = mouseEventCoordinates(event);
    event.dataTransfer.setData('text/plain', 'Needed to work in Firefox');
  };

  const dropZone = domRoot.querySelector('.dropzone');
  dropZone.ondragover = (event) => { event.preventDefault(); }

  let dropCoordinates = null;
  dropZone.ondrop = (event) => {
    // Needed to avoid navigation in Firefox.
    event.preventDefault();
    dropCoordinates = mouseEventCoordinates(event);
  }

  let dragEndCoordinates = null;
  return new Promise((resolve, reject) => {
    dragged.ondragend = (event) => {
      dragEndCoordinates = mouseEventCoordinates(event);
      resolve(true);
    }

    if (window.eventSender) {
      mouseMoveToCenter(dragged, params.frameOffset);
      eventSender.mouseDown();
      setTimeout(() => {
        mouseMoveToCenter(dropZone, params.frameOffset);
        eventSender.mouseUp();
      }, 100);
    }
  }).then(() => t.step(() => {
    assert_approx_equals(dragStartCoordinates.client.x, params.start.client.x,
        epsilon, 'clientX on the dragstart event should be in the drag me box');
    assert_approx_equals(dragStartCoordinates.client.y, params.start.client.y,
        epsilon, 'clientY on the dragstart event should be in the drag me box');
    assert_approx_equals(dragStartCoordinates.page.x, params.start.page.x,
        epsilon, 'pageX on the dragstart event should be in the drag me box');
    assert_approx_equals(dragStartCoordinates.page.y, params.start.page.y,
        epsilon, 'pageY on the dragstart event should be in the drag me box');

    assert_approx_equals(dropCoordinates.client.x, params.end.client.x, epsilon,
        'clientX on the drop event should be in the drop here box');
    assert_approx_equals(dropCoordinates.client.y, params.end.client.y, epsilon,
        'clientX on the drop event should be in the drop here box');
    assert_approx_equals(dropCoordinates.page.x, params.end.page.x, epsilon,
        'pageX on the drop event should be in the drop here box');
    assert_approx_equals(dropCoordinates.page.y, params.end.page.y, epsilon,
        'pageY on the drop event should be in the drop here box');

    assert_approx_equals(dragEndCoordinates.client.x, params.end.client.x,
        epsilon, 'clientX on the dragend event should be in the drop here box');
    assert_approx_equals(dragEndCoordinates.client.y, params.end.client.y,
        epsilon, 'clientY on the dragend event should be in the drop here box');
    assert_approx_equals(dragEndCoordinates.page.x, params.end.page.x, epsilon,
        'pageX on the dragend event should be in the drop here box');
    assert_approx_equals(dragEndCoordinates.page.y, params.end.page.y, epsilon,
        'pageY on the dragend event should be in the drop here box');

  }));
};
