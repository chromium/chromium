if (window.testRunner) {
    testRunner.setBlockThirdPartyCookies(false);
}

var ORIGINAL_HOST  = "example.test";
var TEST_ROOT = "not-example.test";
var TEST_HOST = "cookies." + TEST_ROOT;
var TEST_SUB  = "subdomain." + TEST_HOST;

var STRICT_DOM = "strict_from_dom";
var LAX_DOM = "lax_from_dom";
var UNSPECIFIED_DOM = "unspecified_from_dom";
var NONE_DOM = "none_from_dom";

// Clear the three well-known cookies.
function clearKnownCookies() {
    var cookies = [ STRICT_DOM, LAX_DOM, UNSPECIFIED_DOM, NONE_DOM ];
    cookies.forEach(c => { document.cookie = c + "=0; expires=Thu, 01 Jan 1970 00:00:01 GMT; path=/"; });
}

