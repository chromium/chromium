function test()
{
    applicationCache.oncached = function() { log("cached") }
    applicationCache.onnoupdate = function() { log("noupdate") }
    applicationCache.onerror = function() { log("error") }

    try {
        var expectedContent = "Hello, World!";
        var appCachedResource =
            "/resources/network-simulator.php?path=/appcache/resources/simple.txt";
        var redirectToAppCachedResource =
            "/resources/redirect.php?url=" + appCachedResource;

        var req = new XMLHttpRequest;
        req.open("GET", appCachedResource, false);
        req.send(null);
        if (req.responseText != expectedContent)
            throw "wrong response text for appCachedResource";

        req = new XMLHttpRequest;
        req.open("GET", redirectToAppCachedResource, false);
        req.send(null);
        if (req.responseText != expectedContent)
            throw "wrong response text for redirectToAppCachedResource";

        parent.postMessage("done", "*");
    } catch (ex) {
        alert("FAIL, unexpected error: " + ex);
        parent.postMessage(ex, "*");
    }
}
