// META: script=helpers.js
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
'use strict';

// Prefix each test case with an indicator so we know what context they are run in
// if they are used in multiple iframes.
let testPrefix = "top-level-context";

// Keep track of if we run these tests in a nested context, we don't want to
// recurse forever.
let topLevelDocument = true;

// Check if we were called with a query string of allowed=false. This would
// indicate we expect the access to be denied.
let queryParams = window.location.search.substring(1).split("&");
queryParams.forEach(function (param, index) {
  if (param.toLowerCase() == "rootdocument=false") {
    topLevelDocument = false;
  } else if (param.split("=")[0].toLowerCase() == "testcase") {
    testPrefix = param.split("=")[1];
  }
});

// Common tests to run in all frames.
test(() => {
  assert_not_equals(document.requestStorageAccess, undefined);
}, "[" + testPrefix + "] document.requestStorageAccess() should be supported on the document interface");

promise_test(t => {
  let promise = document.requestStorageAccess();
  let description = "document.requestStorageAccess() call without user gesture";
  return promise.then(t.unreached_func("Should have rejected: " + description)).catch(function(e) {
    assert_equals(undefined, e, description);
  });
}, "[" + testPrefix + "] document.requestStorageAccess() should be rejected by default with no user gesture");

// Logic to load test cases within combinations of iFrames.
if (topLevelDocument) {
  // This specific test will run only as a top level test (not as a worker).
  // Specific requestStorageAccess() scenarios will be tested within the context
  // of various iFrames
  promise_test(async t => {
    let promise = RunRequestStorageAccessInDetachedFrame();
    let description = "document.requestStorageAccess() call in a detached frame";
    return promise.then(t.unreached_func("Should have rejected: " + description)).catch(function (e) {

      assert_equals(e.name, 'SecurityError', description);
    });
  }, "[non-fully-active] document.requestStorageAccess() should not resolve when run in a detached frame");

  promise_test(async t => {
    let promise = RunRequestStorageAccessViaDomParser();
    let description = "document.requestStorageAccess() in a detached DOMParser result";
    return promise.then(t.unreached_func("Should have rejected: " + description)).catch(function (e) {
      assert_equals(e.name, 'SecurityError', description);
    });
  }, "[non-fully-active] document.requestStorageAccess() should not resolve when run in a detached DOMParser document");

  // Create a test with a single-child same-origin iframe.
  let sameOriginFramePromise = RunTestsInIFrame(
      'resources/requestStorageAccess-iframe.html?testCase=same-origin-frame&rootdocument=false');

  // Create a test with a single-child cross-origin iframe.
  let crossOriginFramePromise = RunTestsInIFrame(
      'http://{{domains[www]}}:{{ports[http][0]}}/storage-access-api/resources/requestStorageAccess-iframe.html?testCase=cross-origin-frame&rootdocument=false');

  // Validate the nested-iframe scenario where the same-origin frame
  // containing the tests is not the first child.
  let nestedSameOriginFramePromise = RunTestsInNestedIFrame(
      'resources/requestStorageAccess-iframe.html?testCase=nested-same-origin-frame&rootdocument=false');

  // Validate the nested-iframe scenario where the cross-origin frame
  // containing the tests is not the first child.
  let nestedCrossOriginFramePromise = RunTestsInNestedIFrame(
      'http://{{domains[www]}}:{{ports[http][0]}}/storage-access-api/resources/requestStorageAccess-iframe.html?testCase=nested-cross-origin-frame&rootdocument=false');

  // Because the iframe tests expect no user activation, and because they
  // load asynchronously, we want to first run those tests before simulating
  // clicks on the page.
  Promise
      .all([
        sameOriginFramePromise,
        crossOriginFramePromise,
        nestedSameOriginFramePromise,
        nestedCrossOriginFramePromise,
      ])
      .then(x => {
        promise_test(
            async t => {
              await test_driver.set_permission(
                  {name: 'storage-access'}, 'granted');

              var access_promise;
              let testMethod = function() {
                access_promise = document.requestStorageAccess();
              };
              await ClickButtonWithGesture(testMethod);

              return access_promise;
            },
            '[' + testPrefix +
                '] document.requestStorageAccess() should be resolved when called properly with a user gesture');
      });
}
