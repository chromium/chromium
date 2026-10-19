(function() {
// Evaluates `code` with a sourceURL, waits for `count` Log.entryAdded
// entries and prints each one with its stack trace.
async function collectViolations(testRunner, session, dp, count, code) {
  const entries = [];
  let resolveEntries;
  const done = new Promise(resolve => resolveEntries = resolve);
  dp.Log.onEntryAdded(({params}) => {
    entries.push(params.entry);
    if (entries.length === count) {
      resolveEntries();
    }
  });
  await dp.Log.enable();
  await session.evaluate(code + '\n//# sourceURL=csp-test.js');
  await done;

  entries.forEach((entry, index) => {
    testRunner.log(
        `Message[${index}]: ${entry.source} ${entry.level}: ${entry.text}`);
    testRunner.log('Stack Trace:');
    for (const frame of entry.stackTrace?.callFrames || []) {
      testRunner.log(`  url: ${frame.url}`);
      testRunner.log(`  function: ${frame.functionName}`);
      testRunner.log(`  line: ${frame.lineNumber}`);
    }
  });
}

return {collectViolations};
})()
