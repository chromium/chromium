// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function debug(message) {
  var span = document.createElement("span");
  span.appendChild(document.createTextNode(message));
  span.appendChild(document.createElement("br"));
  document.getElementById('status').appendChild(span);
}

function done(message) {
  if (document.location.hash == '#fail')
    return;
  if (message)
    debug('PASS: ' + message);
  else
    debug('PASS');
  document.location.hash = '#pass';
}

function fail(message) {
  debug('FAILED: ' + message);
  document.location.hash = '#fail';
}

function getLog() {
  return "" + document.getElementById('status').innerHTML;
}

// The following functions are based on
// blink/web_tests/resources/js-test.js
// so that the tests will look similar to the existing web tests.
function stringify(v) {
  if (v === 0 && 1/v < 0)
    return "-0";
  else return "" + v;
}

function areArraysEqual(a, b) {
  try {
    if (a.length !== b.length)
      return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] !== b[i])
        return false;
    }
  } catch (ex) {
    return false;
  }
  return true;
}

function isResultCorrect(_actual, _expected)
{
  if (_expected === 0)
    return _actual === _expected && (1/_actual) === (1/_expected);
  if (_actual === _expected)
    return true;
  if (typeof(_expected) == "number" && isNaN(_expected))
    return typeof(_actual) == "number" && isNaN(_actual);
  if (Object.prototype.toString.call(_expected) ==
      Object.prototype.toString.call([]))
    return areArraysEqual(_actual, _expected);
  return false;
}

function shouldBe(_a, _b)
{
  if (typeof _a != "string" || typeof _b != "string")
    debug("WARN: shouldBe() expects string arguments");
  var exception;
  var _av;
  try {
    _av = eval(_a);
  } catch (e) {
    exception = e;
  }
  var _bv = eval(_b);

  if (exception)
    fail(_a + " should be " + _bv + ". Threw exception " + exception);
  else if (isResultCorrect(_av, _bv))
    debug(_a + " is " + _b);
  else if (typeof(_av) == typeof(_bv))
    fail(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
  else
    fail(_a + " should be " + _bv + " (of type " + typeof _bv + "). " +
         "Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }
function shouldBeEqualToString(a, b) {
  var unevaledString = '"' + b.replace(/\\/g, "\\\\").replace(/"/g, "\"") + '"';
  shouldBe(a, unevaledString);
}

if (typeof String.prototype.startsWith !== 'function') {
  String.prototype.startsWith = function (str) {
    return this.indexOf(str) === 0;
  };
}