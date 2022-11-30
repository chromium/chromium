// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests getEventListeners() method of console command line API.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.loadHTML(`
    <p id="foo">
    </p>
    <div id="outer">
    <div id="inner">
    </div>
    </div>
    <div id="empty">
    </div>
    <button id="button" onclick="alert(1)" onmouseover="listener2()"></button>
    <button id="invalid" onclick="Invalid JavaScript"></button>
  `);

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
        if (!self._output)
            self._output = [];
        self._output.push('[page] ' + message);
    }

    function listener1()
    {
    }
    function listener2()
    {
    }

    document.getElementById("inner").addEventListener("keydown", listener1, false);
    document.getElementById("inner").addEventListener("keydown", listener2, true);
    document.getElementById("inner").addEventListener("wheel", listener2, {"passive": true});
    document.getElementById("outer").addEventListener("mousemove", listener1, false);
    document.getElementById("outer").addEventListener("mousedown", listener2, true);
    document.getElementById("outer").addEventListener("keydown", listener2, true);
    document.getElementById("outer").addEventListener("keyup", listener2, {once: true});
    window.addEventListener("popstate", listener1, false);

    function dumpObject(object, prefix)
    {
        if (!object) {
            output("FAIL: object is " + object);
            return;
        }
        prefix = prefix || "";
        var keys = Object.keys(object);
        keys.sort();
        for (var i = 0; i < keys.length; ++i) {
            var value = object[keys[i]];
            var nameWithPrefix = prefix + keys[i] + ": ";
            switch (typeof(value)) {
            case "object":
                if (value === null) {
                    output(nameWithPrefix + "null");
                    break;
                }
                output(nameWithPrefix + "{");
                dumpObject(value, prefix + "    ")
                output(prefix + "}");
                break;
            case "string":
                output(nameWithPrefix + JSON.stringify(value));
                break;
            case "function":
                var body = value.toString().replace(/[ \\n]+/gm, " ");
                body = body.replace(/; }/g, " }");
                output(nameWithPrefix + body);
                break;
            default:
                output(nameWithPrefix + String(value));
                break;
            }
        }
    }

    function runTestsInPage(getEventListeners)
    {
        output("- inner -");
        var innerElement = document.getElementById("inner");
        var innerListeners = getEventListeners(innerElement);
        dumpObject(innerListeners);
        innerElement.removeEventListener("keydown", innerListeners.keydown[0].listener, innerListeners.keydown[0].useCapture);
        innerElement.removeEventListener("wheel", innerListeners.wheel[0].listener, innerListeners.wheel[0].useCapture);
        output("- inner after a removal -");
        dumpObject(getEventListeners(innerElement));
        output("- outer -");
        dumpObject(getEventListeners(document.getElementById("outer")));
        output("- attribute event listeners -");
        dumpObject(getEventListeners(document.getElementById("button")));
        output("- window -");
        dumpObject(getEventListeners(window));
        output("- empty -");
        dumpObject(getEventListeners(document.getElementById("empty")));
        output("- invalid -");
        dumpObject(getEventListeners(document.getElementById("invalid")));
        output("- object -");
        output(typeof getEventListeners({}));
        output("- null -");
        output(typeof getEventListeners(null));
        output("- undefined -");
        output(typeof getEventListeners(undefined));
    }
  `);

  ConsoleTestRunner.evaluateInConsole('runTestsInPage(getEventListeners)', async function() {
    const output = await TestRunner.evaluateInPageAsync('JSON.stringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    TestRunner.completeTest();
  });
})();
