if (window.testRunner)
    testRunner.dumpAsText();

function description(msg)
{
    // For MSIE 6 compatibility
    var span = document.createElement("span");
    span.innerHTML = '<p>' + msg + '</p><p>On success, you will see a series of "<span class="pass">PASS</span>" messages, followed by "<span class="pass">TEST COMPLETE</span>".</p>';
    var description = document.getElementById("description");
    if (description.firstChild)
        description.replaceChild(span, description.firstChild);
    else
        description.appendChild(span);
}

function debug(msg)
{
    var span = document.createElement("span");
    document.getElementById("console").appendChild(span); // insert it first so XHTML knows the namespace
    span.innerHTML = msg + '<br />';
}

function escapeHTML(text)
{
    return text.replace(/&/g, "&amp;").replace(/</g, "&lt;");
}

function testPassed(msg)
{
    debug('<span><span class="pass">PASS</span> ' + escapeHTML(msg) + '</span>');
}

function testFailed(msg)
{
    debug('<span><span class="fail">FAIL</span> ' + escapeHTML(msg) + '</span>');
}

function areArraysEqual(_a, _b)
{
    if (_a.length !== _b.length)
        return false;
    for (var i = 0; i < _a.length; i++)
        if (_a[i] !== _b[i])
            return false;
    return true;
}

function isMinusZero(n)
{
    // the only way to tell 0 from -0 in JS is the fact that 1/-0 is
    // -Infinity instead of Infinity
    return n === 0 && 1/n < 0;
}

function isResultCorrect(_actual, _expected)
{
    if (_expected === 0)
        return _actual === _expected && (1/_actual) === (1/_expected);
    if (_actual === _expected)
        return true;
    if (typeof(_expected) == "number" && isNaN(_expected))
        return typeof(_actual) == "number" && isNaN(_actual);
    if (Object.prototype.toString.call(_expected) == Object.prototype.toString.call([]))
        return areArraysEqual(_actual, _expected);
    return false;
}

function stringify(v)
{
    if (v === 0 && 1/v < 0)
        return "-0";
    else return "" + v;
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
        testFailed(_a + " should be " + _bv + ". Threw exception " + exception);
    else if (isResultCorrect(_av, _bv))
        testPassed(_a + " is " + _b);
    else if (typeof(_av) == typeof(_bv))
        testFailed(_a + " should be " + _bv + ". Was " + stringify(_av) + ".");
    else
        testFailed(_a + " should be " + _bv + " (of type " + typeof _bv + "). Was " + _av + " (of type " + typeof _av + ").");
}

function shouldBeTrue(_a) { shouldBe(_a, "true"); }
function shouldBeFalse(_a) { shouldBe(_a, "false"); }
function shouldBeNaN(_a) { shouldBe(_a, "NaN"); }
function shouldBeNull(_a) { shouldBe(_a, "null"); }

function shouldBeEqualToString(a, b)
{
    var unevaledString = '"' + b.replace(/\\/g, "\\\\").replace(/"/g, "\"").replace(/\n/g, "\\n").replace(/\r/g, "\\r") + '"';
    shouldBe(a, unevaledString);
}

function shouldBeUndefined(_a)
{
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    if (exception)
        testFailed(_a + " should be undefined. Threw exception " + exception);
    else if (typeof _av == "undefined")
        testPassed(_a + " is undefined.");
    else
        testFailed(_a + " should be undefined. Was " + _av);
}

function shouldThrow(_a, _e)
{
    var exception;
    var _av;
    try {
        _av = eval(_a);
    } catch (e) {
        exception = e;
    }

    var _ev;
    if (_e)
        _ev =  eval(_e);

    if (exception) {
        if (typeof _e == "undefined" || exception == _ev)
            testPassed(_a + " threw exception " + exception + ".");
        else
            testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Threw exception " + exception + ".");
    } else if (typeof _av == "undefined")
        testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was undefined.");
    else
        testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was " + _av + ".");
}

var cookies = new Array();

// This method sets the cookies using XMLHttpRequest.
// We do not set the cookie right away as it is forbidden by the XHR spec.
// FIXME: Add the possibility to set multiple cookies in a row.
function setCookies(cookie)
{
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "resources/setCookies.cgi", false);
        xhr.setRequestHeader("X-SET-COOKIE", cookie);
        xhr.send(null);
        if (xhr.status == 200) {
            // This is to clear them later.
            cookies.push(cookie);
            return true;
        } else
            return false;
    } catch (e) {
        return false;
    }
}

// Normalize a cookie string
function normalizeCookie(cookie)
{
    // Split the cookie string, sort it and then put it back together.
    return cookie.split('; ').sort().join('; ');
}

// We get the cookies throught an XMLHttpRequest.
function testCookies(result)
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "resources/getCookies.cgi", false);
    xhr.send(null);
    var cookie = xhr.getResponseHeader("HTTP_COOKIE") == null ? '"null"' : xhr.getResponseHeader("HTTP_COOKIE");

    // Normalize the cookie strings.
    result = normalizeCookie(result);
    cookie = normalizeCookie(cookie);
    
    if (cookie === result)
        testPassed("cookie is '" + cookie + "'.");
    else
        testFailed("cookie was '" + cookie + "'. Expected '" + result + "'.");
}

function clearAllCookies()
{
    var cookieString;
    while (cookieString = document.cookie) {
        var cookieName = cookieString.substr(0, cookieString.indexOf("=") || cookieString.length());
        cookies.push(cookieName);
        clearCookies();

        // In case clearCookies.cgi failed, for example,
        // the domain/path do not match exactly:
        document.cookie = cookieName + "=;Max-Age=0";
    }
}

function clearCookies()
{
    if (!cookies.length)
        return;

    try {
        var xhr = new XMLHttpRequest();
        var cookie;
        // We need to clean one cookie at a time because to be cleared the
        // cookie must be exactly the same except for the "Max-Age"
        // and "Expires" fields.
        while (cookie = cookies.pop()) {
            xhr.open("GET", "resources/clearCookies.cgi", false);
            xhr.setRequestHeader("CLEAR-COOKIE", cookie);
            xhr.send(null);
        }
    } catch (e) {
        debug("Could not clear the cookies expect the following results to fail");
    }
}

// This method check one cookie at a time.
function cookiesShouldBe(cookiesToSet, result)
{
    if (!setCookies(cookiesToSet)) {
        testFailed("could not set cookie(s) " + cookiesToSet);
        return;
    }
    testCookies(result);
}
