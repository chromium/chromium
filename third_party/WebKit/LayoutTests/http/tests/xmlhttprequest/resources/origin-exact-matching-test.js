if (window.description)
    description("Check that exact matching is used when comparing a request's originating url and the value provided by Access-Control-Allow-Origin.");

var baseUrl = "http://127.0.0.1:8000/xmlhttprequest/resources/access-control-allow-lists.php";

function generateURL(origin)
{
    if (Array.isArray(origin))
        return baseUrl + "?origins=" + origin.map(encodeURIComponent).join(",");
    else
        return baseUrl + "?origin=" + encodeURIComponent(origin);
}

function shouldPass(origin) {
    debug("Should allow origin: '" + origin + "'");
    xhr = new XMLHttpRequest();
    xhr.open('GET', generateURL(origin), false);
    shouldBeUndefined("xhr.send(null)");
}

function shouldFail(origin) {
    debug("Should disallow origin: '" + origin + "'");
    xhr = new XMLHttpRequest();
    xhr.open('GET', generateURL(origin), false);
    shouldThrow("xhr.send(null)");
}

var thisOrigin = location.protocol + "//" + location.host;

function injectIframeTest() {
    if (window.testRunner)
        testRunner.dumpChildFrames();
    var which = window.location.href.match(/(\d\d).html/)[1];
    var frame = document.createElement('iframe');
    frame.src = "http://localhost:8000/xmlhttprequest/resources/origin-exact-matching-iframe.html?" + which;
    document.body.appendChild(frame);
}
