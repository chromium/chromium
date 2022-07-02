// Logs (append) an HTML string to the document in a list format.
function log(str) {
  const entry = document.createElement('li');
  entry.innerHTML = str;
  logger.appendChild(entry);
  return entry;
}

// Common setup for window placement tests. Performs some basic assertions, and
// then waits for a click on the `setUpButton` element (for manual tests).
// Example usage:
//  promise_test(async setUpTest => {
//    await setUpWindowPlacement(setUpTest, setUpButton);
//    ...
//  });
async function setUpWindowPlacement(setUpTest, setUpButton) {
  assert_true(
    'getScreenDetails' in self && 'isExtended' in screen,
    `API not supported; use Chromium (not content_shell) and enable
     chrome://flags/#enable-experimental-web-platform-features`);
  if (!screen.isExtended)
    log(`WARNING: Use multiple screens for full test coverage`);
  if (window.location.href.startsWith('file'))
    log(`WARNING: Run via 'wpt serve'; file URLs lack permission support`);

  try {  // Support manual testing where test_driver is not running.
    await test_driver.set_permission({ name: 'window-placement' }, 'granted');
  } catch {
  }
  const setUpWatcher = new EventWatcher(setUpTest, setUpButton, ['click']);
  const setUpClick = setUpWatcher.wait_for('click');
  try {  // Support manual testing where test_driver is not running.
    await test_driver.click(setUpButton);
  } catch {
  }
  await setUpClick;
  setUpButton.disabled = true;
}
