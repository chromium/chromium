// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* TODO(chromium:1050549)
 * once that bug is complete we can lose this test
 * as DevTools will no longer touch built-in prototypes.
 */


(async function() {
  TestRunner.addResult(
      `Tests that opening inspector front-end doesn't change methods defined by the inspected application.\n`);


  await TestRunner.loadHTML(`
    <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
    <div id="output"></div>
  `);

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
      if (!self._output)
          self._output = [];
      self._output.push(message);
    }

    function myImpl() {
      return "my value";
    }

    // Provide some custom methods.
    Object.type = myImpl;
    Object.hasProperties = myImpl;
    Object.describe = myImpl;
    Object.className = myImpl;
    String.prototype.testStringProtoFunc = myImpl;
    var originalJSONStringify = JSON.stringify;
    JSON.stringify = myImpl;

    function dumpValues()
    {
        // Check that the methods haven't changed.
        output("myImpl() => " + myImpl());
        output("Object.type === myImpl => " + (Object.type === myImpl));
        output("Object.hasProperties === myImpl => " + (Object.hasProperties === myImpl));
        output("Object.describe === myImpl => " + (Object.describe === myImpl));
        output("Object.className === myImpl => " + (Object.className === myImpl));
        output("String.prototype.testStringProtoFunc === myImpl => " + (String.prototype.testStringProtoFunc === myImpl));
        output("JSON.stringify === myImpl => " + (JSON.stringify === myImpl));
    }
  `);


  async function callback() {
    const output = await TestRunner.evaluateInPageAsync('originalJSONStringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    TestRunner.completeTest();
  }

  TestRunner.evaluateInPage('dumpValues()', callback);
})();
