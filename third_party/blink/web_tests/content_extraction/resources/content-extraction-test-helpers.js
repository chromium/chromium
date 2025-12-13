// This boilerplate runs automatically when the script is included.
if (window.testRunner) {
  // 1. Tell the harness this is a text test.
  testRunner.dumpAsText();
  // 2. Tell the harness to wait for a signal before finishing.
  testRunner.waitUntilDone();
}

/**
 *  Dumps the content node tree in a format suitable for testing.
 *  This function is used to output the structure and properties of the content
 *  node tree for verification in tests.
 *  It returns a string representation of the content node tree.
 *  @param {Node} node The root node of the content node tree to be dumped.
 *  @returns {string} A string representation of the content node tree.
 */
function dumpContentNodeTree(node) {
  if (!window.internals) {
    return 'No test internals available.';
  }
  return internals.dumpContentNodeTree(node);
}

/***
 * Dumps the content node in a format suitable for testing.
 * This function is used to output the content node's structure and properties
 * for verification in tests.
 * It returns a string representation of the content node.
 * @param {Node} node The content node to be dumped.
 * @returns {string} A string representation of the content node.
 */
function dumpContentNode(node) {
  if (!window.internals) {
    return 'No test internals available.';
  }
  return internals.dumpContentNode(node);
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
