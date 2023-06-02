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
  if (buferredFinished) {
    testFinished();
    bufferedFinished = false;
  }
}

function shouldComputedColorOfElementBeEqualToRGBString(element, expectedColor)
{
  var elementName = "#" + element.id || element.tagName;
  var actualColor = window.getComputedStyle(element, null).color;
  if (actualColor === expectedColor)
    log("PASS " + elementName + " color was " + expectedColor + ".");
  else
    log("FAIL " + elementName + " color should be " + expectedColor + ". Was " + actualColor + ".");
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
