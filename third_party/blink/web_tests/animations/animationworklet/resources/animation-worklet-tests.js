function runInAnimationWorklet(code) {
  return CSS.animationWorklet.addModule(
    URL.createObjectURL(new Blob([code], {type: 'text/javascript'}))
  );
}

function waitForAnimationFrames(count, callback) {
  function rafCallback() {
    if (count <= 0) {
      callback();
    } else {
      count -= 1;
      window.requestAnimationFrame(rafCallback);
    }
  }
  rafCallback();
}

// Wait for two main thread frames to guarantee that compositor has produced
// at least one frame.
function waitTwoAnimationFrames(callback) {
  waitForAnimationFrames(2, callback);
}

function waitForAsyncAnimationFrame() {
  return new Promise(waitTwoAnimationFrames);
}

async function waitForDocumentTimelineAdvance() {
  const timeAtStart = document.timeline.currentTime;
  do {
    await new Promise(window.requestAnimationFrame);
  } while (timeAtStart === document.timeline.currentTime)
}

async function waitForAnimationFrameWithCondition(condition) {
  do {
    await new Promise(window.requestAnimationFrame);
  } while (!condition())
}

// Load test cases in worklet context in sequence and wait until they are resolved.
function runTests(testcases) {
  if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
  }

  const runInSequence = testcases.reduce((chain, testcase) => {
    return chain.then( _ => {
        return runInAnimationWorklet(testcase);
    });
  }, Promise.resolve());

  runInSequence.then(_ => {
    // Wait to guarantee that compositor frame is produced before finishing.
    // This ensure we see at least one |animate| call in the animation worklet.
    waitTwoAnimationFrames(_ => {
      if (window.testRunner)
        testRunner.notifyDone();
     });
  });
}

// Treat every <script type=text/worklet> as a test case and run then in the
// animation worklet context.
function runAnimationWorkletTests() {
  const testcases = Array.prototype.map.call(document.querySelectorAll('script[type$=worklet]'), ($el) => {
    return $el.textContent;
  });

  runTests(testcases);
}
