'use strict';

function processQueryParams() {
  const queryParams = new URL(window.location).searchParams;
  return {
    expectAccessAllowed: queryParams.get("allowed") != "false",
    topLevelDocument: queryParams.get("rootdocument") != "false",
    testPrefix: queryParams.get("testCase") || "top-level-context",
  };
}

function CreateFrameAndRunTests(setUpFrame) {
  const frame = document.createElement('iframe');
  const promise = new Promise((resolve, reject) => {
    frame.onload = resolve;
    frame.onerror = reject;
  });

  setUpFrame(frame);

  fetch_tests_from_window(frame.contentWindow);
  return promise;
}

function RunTestsInIFrame(sourceURL) {
  return CreateFrameAndRunTests((frame) => {
    frame.src = sourceURL;
    document.body.appendChild(frame);
  });
}

function RunTestsInNestedIFrame(sourceURL) {
  return CreateFrameAndRunTests((frame) => {
    document.body.appendChild(frame);
    frame.contentDocument.write(`
      <script src="/resources/testharness.js"></script>
      <script src="helpers.js"></script>
      <body>
      <script>
        RunTestsInIFrame("${sourceURL}");
      </script>
    `);
    frame.contentDocument.close();
  });
}

function RunRequestStorageAccessInDetachedFrame() {
  const frame = document.createElement('iframe');
  document.body.append(frame);
  const inner_doc = frame.contentDocument;
  frame.remove();
  return inner_doc.requestStorageAccess();
}

function RunRequestStorageAccessViaDomParser() {
  const parser = new DOMParser();
  const doc = parser.parseFromString('<html></html>', 'text/html');
  return doc.requestStorageAccess();
}

async function RunCallbackWithGesture(buttonId, callback) {
  // Append some formatting and information so non WebDriver instances can complete this test too.
  const info = document.createElement('p');
  info.innerText = "This test case requires user-interaction and TestDriver. If you're running it manually please click the 'Request Access' button below exactly once.";
  document.body.appendChild(info);

  const button = document.createElement('button');
  button.innerText = "Request Access";
  button.id = buttonId;
  button.style = "background-color:#FF0000;"

  // Insert the button and use test driver to click the button with a gesture.
  document.body.appendChild(button);

  const promise = new Promise((resolve, reject) => {
    const wrappedCallback = () => {
      callback().then(resolve, reject);
    };

    // In automated tests, we call the callback via test_driver.
    test_driver.bless('run callback with user interaction', wrappedCallback);

    // But we still allow the button to trigger the callback, for use on
    // https://wpt.live.
    button.addEventListener('click', e => {
      wrappedCallback();
      button.style = "background-color:#00FF00;"
    }, {once: true});
  });

  return {promise};
}
