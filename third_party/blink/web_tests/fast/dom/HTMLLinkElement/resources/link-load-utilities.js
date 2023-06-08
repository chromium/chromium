if (window.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
}

var haveBuffer = false;
var bufferedOutput = [];
var bufferedFinished = false;

function ensureBuffer() {
  if (haveBuffer)
    return;

  haveBuffer = true;
  window.addEventListener("load", flushBuffer);
}

function flushBuffer() {
  haveBuffer = false;
  for (let line of bufferedOutput) {
    log(line);
  }
  bufferedOutput = [];
  if (bufferedFinished) {
    testFinished();
    bufferedFinished = false;
  }
}

function shouldComputedColorOfElementByIdBeEqualToRGBStringAndTestFinished(element_id, expectedColor)
{
  let element = document.getElementById(element_id);
  if (!element) {
    if (document.readyState == "complete") {
      log(`FAIL unable to find element with ID "${element_id}".`);
      testFinished();
      return;
    }
    window.addEventListener("load", () => shouldComputedColorOfElementByIdBeEqualToRGBStringAndTestFinished(element_id, expectedColor));
    return;
  }
  var elementName = "#" + element_id;
  var actualColor = window.getComputedStyle(element, null).color;
  if (actualColor === expectedColor)
    log("PASS " + elementName + " color was " + expectedColor + ".");
  else
    log("FAIL " + elementName + " color should be " + expectedColor + ". Was " + actualColor + ".");
  testFinished();
}

function createLinkElementWithStylesheet(stylesheetURL)
{
  var link = document.createElement("link");
  link.rel = "stylesheet";
  link.href = stylesheetURL;
  return link;
}

function createStyleElementWithString(stylesheetData)
{
  var style = document.createElement("style");
  style.textContent = stylesheetData;
  return style;
}

function log(message)
{
  let console = document.getElementById("console");
  if (!console) {
    ensureBuffer();
    bufferedOutput.push(message);
    return;
  }
  if (haveBuffer) {
    flushBuffer();
  }
  console.appendChild(document.createTextNode(message + "\n"));
}

function testPassed(message)
{
  log("PASS " + message);
}

function testFailed(message)
{
  log("FAIL " + message);
}

function testPassedAndNotifyDone(message)
{
  testPassed(message);
  testFinished();
}

function testFailedAndNotifyDone(message)
{
  testFailed(message);
  testFinished();
}

function testFinished()
{
  if (haveBuffer) {
    bufferedFinished = true;
    return;
  }
  if (window.testRunner)
    testRunner.notifyDone();
}
