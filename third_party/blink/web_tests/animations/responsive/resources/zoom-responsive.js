'use strict';

function populate(property, values) {
  var container = document.querySelector('#container');
  values.forEach(value => {
    var text = document.createElement('div');
    text.textContent = value;
    container.appendChild(text);

    var target = document.createElement('div');
    target.classList.add('target');
    container.appendChild(target);

    var keyframe = {};
    keyframe[property] = value;

    target.animate([keyframe, keyframe], 1e8);
  });
}

function zoomDuringAnimation(property, values) {
  var footer = document.createElement('div');
  document.body.appendChild(footer);
  populate(property, values);

  function waitForCompositor() {
    return footer.animate({opacity: ['1', '1']}, 1).ready;
  }

  if (window.testRunner)
    testRunner.waitUntilDone();

  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      eventSender.setPageZoomFactor(2);
      if (!window.testRunner)
        return;

      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          waitForCompositor().then(() => {
            requestAnimationFrame(() => {
              testRunner.notifyDone();
            });
          });
        });
      });
    });
  });
}

function zoomBeforeAnimation(property, values) {
  eventSender.setPageZoomFactor(2);

  var footer = document.createElement('div');
  document.body.appendChild(footer);
  populate(property, values);

  function waitForCompositor() {
    return footer.animate({opacity: ['1', '1']}, 1).ready;
  }

  if (!window.testRunner)
    return;

  testRunner.waitUntilDone();

  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      waitForCompositor().then(() => {
        requestAnimationFrame(() => {
          testRunner.notifyDone();
        });
      });
    });
  });
}
