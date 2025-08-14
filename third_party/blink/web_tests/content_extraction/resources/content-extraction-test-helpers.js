// This boilerplate runs automatically when the script is included.
if (window.testRunner) {
  // 1. Tell the harness this is a text test.
  testRunner.dumpAsText();
  // 2. Tell the harness to wait for a signal before finishing.
  testRunner.waitUntilDone();
}

/**
 * Sets the provided text as the definitive test output and signals
 * that the test is complete.
 * @param {string} resultText The text to be used for the expectation file.
 */
function finishTest(resultText) {
  if (window.testRunner) {
    // 3. Set the custom text output for the test result.
    testRunner.setCustomTextOutput(resultText);
    // 4. Signal that the test is finished.
    testRunner.notifyDone();
  }
}
