// Returns a Promise for future conversion into WebDriver-backed API.
function keyDown(key, modifiers) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      eventSender.keyDown(key, modifiers);
      resolve();
    } else {
      reject();
    }
  });
}

function selectTarget(selector) {
  const target = document.querySelector(selector);
  if (target.select) {
    target.select();
  } else {
    const selection = window.getSelection();
    selection.collapse(target, 0);
    selection.extend(target, 1);
  }
}

function focusAndKeyDown(selector, key) {
  const target = document.querySelector(selector);
  return test_driver.send_keys(target, key);
}

function collapseEndAndKeyDown(selector, key, modifiers) {
  const target = document.querySelector(selector);
  window.getSelection().collapse(target.lastElementChild || target, 1);
  return keyDown(key, modifiers);
}

function selectAndKeyDown(selector, key) {
  selectTarget(selector);
  return keyDown(key);
}

function selectAndExecCommand(selector, command) {
  assert_not_equals(window.testRunner, undefined, 'This test requires testRunner.');
  selectTarget(selector);
  testRunner.execCommand(command);
}
