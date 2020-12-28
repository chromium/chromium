function trimURL(url)
{
    if (!url)
        return;
    if (/^data:/.test(url))
        return url.replace(/,.*$/, "...");
    return url.replace(/.*\//, ".../");
}

function dumpObject(object, nondeterministicProps, prefix, firstLinePrefix)
{
    prefix = prefix || "";
    firstLinePrefix = firstLinePrefix || prefix;
    output(firstLinePrefix + "{");
    var propertyNames = [];
    for (var property in object)
        propertyNames.push(property);
    propertyNames.sort();

    for (var i = 0; i < propertyNames.length; ++i) {
        var prop = propertyNames[i];
        var prefixWithName = prefix + "    " + prop + " : ";
        var propValue = object[prop];
        if (nondeterministicProps && prop in nondeterministicProps) {
            var value = nondeterministicProps[prop] === "url" ? trimURL(propValue) : "<" + typeof propValue + ">";
            output(prefixWithName + value);
        } else if (typeof propValue === "object" && propValue != null)
            dumpObject(propValue, nondeterministicProps, prefix + "    ", prefixWithName);
        else if (typeof propValue === "string")
            output(prefixWithName + "\"" + propValue + "\"");
        else if (typeof propValue === "function")
            output(prefixWithName + "<function>");
        else
            output(prefixWithName + propValue);
    }
    output(prefix + "}");
}

function dumpArray(result)
{
    if (result instanceof Array) {
        for (var i = 0; i < result.length; ++i)
            output(result[i]);
    } else
        output(result);
}

function evaluateOnFrontend(expression, callback)
{
    window._extensionServerForTests.sendRequest({ command: "evaluateForTestInFrontEnd", expression: expression }, callback);
}

function invokePageFunctionAsync(functionName, callback)
{
    evaluateOnFrontend("TestRunner.callFunctionInPageAsync('" + functionName + "').then(() => reply())", callback);
}

function output(message)
{
    evaluateOnFrontend("TestRunner.addResult(unescape('" + escape(message) + "'));");
}

function onError(event)
{
    window.removeEventListener("error", onError);
    output("Uncaught exception in extension context: " + event.message + " [" + event.filename + ":" + event.lineno + "]\n");
    evaluateOnFrontend("TestRunner.completeTest();");
}

window.addEventListener("error", onError);

function fetchTests() {
  evaluateOnFrontend('reply(ExtensionsTestRunner._pendingTests)', result => {
    window.eval(result);
    runTests();
  });
}

function runTests()
{
    output("Running tests...");
    var tests = [];
    for (var symbol in this) {
        if (/^extension_test/.test(symbol) && typeof this[symbol] === "function")
            tests.push(symbol);
    }
    tests = tests.sort();
    var testChain = extension_onTestsDone;
    for (var test = tests.pop(); test; test = tests.pop())
        testChain = bind(runTest, this, bind(this[test], this, testChain), test);
    testChain();
}

function extension_onTestsDone()
{
    output("All tests done.");
    evaluateOnFrontend("TestRunner.completeTest();");
}

function extension_showPanel(panelId, callback)
{
    evaluateOnFrontend("ExtensionsTestRunner.showPanel(unescape('" + escape(panelId) + "')).then(function() { reply(); });", callback);
}

function extension_runAudits(callback) {
    evaluateOnFrontend('ExtensionsTestRunner.startExtensionAudits(reply);', callback);
}

function extension_getRequestByUrl(urls, callback)
{
    function onHAR(response)
    {
        var entries = response.entries;
        for (var i = 0; i < entries.length; ++i) {
            for (var url = 0; url < urls.length; ++url) {
                if (urls[url].test(entries[i].request.url)) {
                    callback(entries[i]);
                    return;
                }
            }
        }
        output("no item found");
        callback(null);
    }
    webInspector.network.getHAR(onHAR);
}

function runTest(test, name)
{
    output("RUNNING TEST: " + name);
    try {
        test();
    } catch (e) {
        output(name + ": " + e);
    }
}

function callbackAndNextTest(callback, nextTest)
{
    function callbackWrapper()
    {
        callback.apply(this, arguments);
        nextTest.call(this);
    }
    return callbackWrapper;
}

function bind(func, thisObject)
{
    var args = Array.prototype.slice.call(arguments, 2);
    return function() { return func.apply(thisObject, args.concat(Array.prototype.slice.call(arguments, 0))); };
}
