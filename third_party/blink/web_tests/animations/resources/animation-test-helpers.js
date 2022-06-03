/* This is the helper script for animation tests:

Test page requirements:
- The body must contain an empty div with id "result"
- Call this function directly from the <script> inside the test page

runAnimationTest and runTransitionTest parameters:
    expected [required]: an array of arrays defining a set of CSS properties that must have given values at specific times (see below)
    callbacks [optional]: a function to be executed immediately after animation starts;
                          or, an object in the form {time: function} containing functions to be
                          called at the specified times (in seconds) during animation.
    trigger [optional]: a function to be executed just before the test starts (none by default)
    doPixelTest [optional]: whether to dump pixels during the test (false by default)
    disablePauseAnimationAPI [optional]: whether to disable the pause API and run a RAF-based test (false by default)

    Each sub-array must contain these items in this order:
    - the time in seconds at which to snapshot the CSS property
    - the id of the element on which to get the CSS property value [1]
    - the name of the CSS property to get [2]
    - the expected value for the CSS property [3]
    - the tolerance to use when comparing the effective CSS property value with its expected value

    [1] If a single string is passed, it is the id of the element to test. If an array with 2 elements is passed they
    are the ids of 2 elements, whose values are compared for equality. In this case the expected value is ignored
    but the tolerance is used in the comparison.

    If a string with a '.' is passed, this is an element in an iframe. The string before the dot is the iframe id
    and the string after the dot is the element name in that iframe.

    [2] Appending ".N" to the CSS property name (e.g. "transform.N") makes
    us only check the Nth numeric substring of the computed style.

    [3] The expected value has several supported formats. If an array is given,
    we extract numeric substrings from the computed style and compare the
    number of these as well as the values. If a number is given, we treat it as
    an array of length one. If a string is given, we compare numeric substrings
    with the computed style with the given tolerance and also the remaining
    non-numeric substrings.

*/

// Set to true to log debug information in failing tests. Note that these logs
// contain timestamps, so are non-deterministic and will introduce flakiness if
// any expected results include failures.
var ENABLE_ERROR_LOGGING = false;

function isCloseEnough(actual, expected, tolerance)
{
    return Math.abs(actual - expected) <= tolerance;
}

function roundNumber(num, decimalPlaces)
{
  return Math.round(num * Math.pow(10, decimalPlaces)) / Math.pow(10, decimalPlaces);
}

function checkExpectedValue(expected, index)
{
    log('Checking expectation: ' + JSON.stringify(expected[index]));
    var time = expected[index][0];
    var elementId = expected[index][1];
    var specifiedProperty = expected[index][2];
    var expectedValue = expected[index][3];
    var tolerance = expected[index][4];
    var postCompletionCallback = expected[index][5];

    var expectedIndex = specifiedProperty.split(".")[1];
    var property = specifiedProperty.split(".")[0];

    // Check for a pair of element Ids
    if (typeof elementId != "string") {
        if (elementId.length != 2)
            return;

        var elementId2 = elementId[1];
        elementId = elementId[0];

        var computedValue = getPropertyValue(property, elementId);
        var computedValue2 = getPropertyValue(property, elementId2);

        if (comparePropertyValue(computedValue, computedValue2, tolerance, expectedIndex))
            result += "PASS - \"" + specifiedProperty + "\" property for \"" + elementId + "\" and \"" + elementId2 +
                            "\" elements at " + time + "s are close enough to each other" + "<br>";
        else
            result += "FAIL - \"" + specifiedProperty + "\" property for \"" + elementId + "\" and \"" + elementId2 +
                            "\" elements at " + time + "s saw: \"" + computedValue + "\" and \"" + computedValue2 +
                                            "\" which are not close enough to each other" + "<br>";
    } else {
        var elementName = elementId;
        var array = elementId.split('.');
        var iframeId;
        if (array.length == 2) {
            iframeId = array[0];
            elementId = array[1];
        }

        var computedValue = getPropertyValue(property, elementId, iframeId);

        if (comparePropertyValue(computedValue, expectedValue, tolerance, expectedIndex))
            result += "PASS - \"" + specifiedProperty + "\" property for \"" + elementName + "\" element at " + time +
                            "s saw something close to: " + expectedValue + "<br>";
        else
            result += "FAIL - \"" + specifiedProperty + "\" property for \"" + elementName + "\" element at " + time +
                            "s expected: " + expectedValue + " but saw: " + computedValue + "<br>";
    }

    if (postCompletionCallback)
      result += postCompletionCallback();
}

function getPropertyValue(property, elementId, iframeId)
{
    var context = iframeId ? document.getElementById(iframeId).contentDocument : document;
    var element = context.getElementById(elementId);
    return window.getComputedStyle(element)[property];
}

// splitValue("calc(12.5px + 10%)") -> [["calc(", "px + ", "%)"], [12.5, 10]]
function splitValue(value)
{
    var substrings = value.split(/(-?\d+(?:\.\d+)?(?:e-?\d+)?)/g);
    var strings = [];
    var numbers = [];
    for (var i = 0; i < substrings.length; i++) {
        if (i % 2 == 0)
            strings.push(substrings[i]);
        else
            numbers.push(parseFloat(substrings[i]));
    }
    return [strings, numbers];
}

function comparePropertyValue(computedValue, expectedValue, tolerance, expectedIndex)
{
    computedValue = splitValue(computedValue);
    var computedStrings = computedValue[0];
    var computedNumbers = computedValue[1];

    var expectedStrings, expectedNumbers;

    if (expectedIndex !== undefined)
        return isCloseEnough(computedNumbers[expectedIndex], expectedValue, tolerance);

    if (typeof expectedValue === "string") {
        expectedValue = splitValue(expectedValue);

        expectedStrings = expectedValue[0];
        if (computedStrings.length !== expectedStrings.length)
            return false;
        for (var i = 0; i < expectedStrings.length; i++)
            if (expectedStrings[i] !== computedStrings[i])
                return false;

        expectedNumbers = expectedValue[1];
    } else if (typeof expectedValue === "number") {
        expectedNumbers = [expectedValue];
    } else {
        expectedNumbers = expectedValue;
    }

    if (computedNumbers.length !== expectedNumbers.length)
        return false;
    for (var i = 0; i < expectedNumbers.length; i++)
        if (!isCloseEnough(computedNumbers[i], expectedNumbers[i], tolerance))
            return false;

    return true;
}

function waitForCompositor() {
    return document.body.animate({opacity: [1, 1]}, 1).finished;
}

function endTest()
{
    log('Ending test');
    var resultElement = useResultElement ? document.getElementById('result') : document.documentElement;
    if (ENABLE_ERROR_LOGGING && result.indexOf('FAIL') >= 0)
        result += '<br>Log:<br>' + logMessages.join('<br>');
    resultElement.innerHTML = result;

    if (window.testRunner) {
        waitForCompositor().then(() => {
            testRunner.notifyDone();
        });
    }
}

function runChecksWithRAF(checks)
{
    var finished = true;
    var time = performance.now() - animStartTime;

    log('RAF callback, animation time: ' + time);
    for (var k in checks) {
        var checkTime = Number(k);
        if (checkTime > time) {
            finished = false;
            break;
        }
        log('Running checks for time: ' + checkTime + ', delay: ' + (time - checkTime));
        checks[k].forEach(function(check) { check(); });
        delete checks[k];
    }

    if (finished)
        endTest();
    else
        requestAnimationFrame(runChecksWithRAF.bind(null, checks));
}

function runChecksWithPauseAPI(checks) {
    for (var k in checks) {
        var timeMs = Number(k);
        log('Pausing at time: ' + timeMs + ', current animations: ' + document.getAnimations().length);
        internals.pauseAnimations(timeMs / 1000);
        checks[k].forEach(function(check) { check(); });
    }
    requestAnimationFrame(function() {
      requestAnimationFrame(function() {
        endTest();
      });
    });
}

function startTest(checks)
{
    if (hasPauseAnimationAPI)
        runChecksWithPauseAPI(checks);
    else {
        result += 'Warning this test is running in real-time and may be flaky.<br>';
        runChecksWithRAF(checks);
    }
}

var logMessages = [];
var useResultElement = false;
var result = "";
var hasPauseAnimationAPI;
var animStartTime;
var isTransitionsTest = false;

function log(message)
{
    logMessages.push(performance.now() + ' - ' + message);
}

function waitForAnimationsToStart(callback)
{
    if (document.getAnimations().length > 0) {
        callback();
    } else {
        requestAnimationFrame(waitForAnimationsToStart.bind(this, callback));
    }
}

function convertExpectationsToChecks(expected, callbacks) {
  var checks = {};

  if (typeof callbacks == 'function') {
    checks[0] = [callbacks];
  } else for (var time in callbacks) {
      timeMs = Math.round(time * 1000);
      checks[timeMs] = [callbacks[time]];
  }

  for (var i = 0; i < expected.length; i++) {
      var expectation = expected[i];
      var timeMs = Math.round(expectation[0] * 1000);
      if (!checks[timeMs])
          checks[timeMs] = [];
      checks[timeMs].push(checkExpectedValue.bind(null, expected, i));
  }

  return checks;
}

// FIXME: disablePauseAnimationAPI and doPixelTest
function runAnimationTest(expected, callbacks, trigger, disablePauseAnimationAPI, doPixelTest, startTestImmediately)
{
    log('runAnimationTest');
    if (!expected)
        throw "Expected results are missing!";

    if (!document.getAnimations)
        throw "Animation tests require document.getAnimations";

    hasPauseAnimationAPI = 'internals' in window;
    if (disablePauseAnimationAPI)
        hasPauseAnimationAPI = false;

    var checks = convertExpectationsToChecks(expected, callbacks);

    var doPixelTest = Boolean(doPixelTest);
    useResultElement = doPixelTest;

    if (window.testRunner) {
        if (!doPixelTest)
            testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    var started = false;

    function begin() {
        if (!started) {
            log('First ' + event + ' event fired');
            started = true;
            if (trigger) {
                trigger();
                document.documentElement.offsetTop
            }
            waitForAnimationsToStart(function() {
                log('Finished waiting for animations to start');
                animStartTime = performance.now();
                startTest(checks);
            });
        }
    }

    var startTestImmediately = Boolean(startTestImmediately);
    if (startTestImmediately) {
        begin();
    } else {
        var target = isTransitionsTest ? window : document;
        var event = isTransitionsTest ? 'load' : 'webkitAnimationStart';
        target.addEventListener(event, begin, false);
    }
}

function runTransitionTest(expected, trigger, callbacks, doPixelTest, disablePauseAnimationAPI) {
    isTransitionsTest = true;
    runAnimationTest(expected, callbacks, trigger, disablePauseAnimationAPI, doPixelTest);
}

/*
 * Creates one or more animations that are effectively frozen at 50% progress
 * due to the duration of the animation and timing function. The test finishes
 * once all ready promises are resolved and property values have been validated.
 *
 * @param {array<{ string: id, string: keyframes}>} animation_list
 * @param {string} reference Id of reference element.
 * @param {string} property  Name of the property, which may be longhand or
 *     shorthand.
 * @param {number} tolerance Numerical tolerance. For non-numerical property
 *     values, the tolerance is applied to each functional argument or list
 *     member within the expression.
 */
function runAnimationMidpointTest(animation_list, reference, property,
                                  tolerance) {
  const duration = 100000;
  const promises = [];
  useResultElement = true;
  animation_list.forEach(options => {
    const element = document.getElementById(options.id);
    element.style.animationName = options.keyframes;
    element.style.animationDuration = `${duration}s`;
    element.style.animationDelay = `${-duration/2}s`;
    // This timing function has a zero slope at the midpoint.
    element.style.animationTimingFunction = 'cubic-bezier(0, 1, 1, 0)';
    // The getAnimations call forces a style update, which in turn will create
    // the CSS animation.
    const animation = element.getAnimations()[0];
    // By waiting on the ready promise, we ensure that the client agent and the
    // animation are in sync.
    promises.push(animation.ready);
  });
  Promise.all(promises).then(() => {
    animation_list.forEach(options => {
        const element = options.id;
        const computedValue = getPropertyValue(property, element);
        const expectedValue = getPropertyValue(property, reference);
        if (comparePropertyValue(computedValue, expectedValue, tolerance)) {
            result += "PASS - \"" + property + "\" property for \"" +
                      element + "\" and \"" + reference +
                      "\" elements are close enough to each other" + "<br>";
        } else {
            result += "FAIL - \"" + property + "\" property for \"" +
                      element  + "\" and \"" + reference +
                      "\" elements saw: \"" + computedValue +
                      "\" and \"" + expectedValue +
                      "\" which are not close enough to each other" + "<br>";
        }
    });
    endTest();
  });
  if (window.testRunner)
    testRunner.waitUntilDone();
}
