#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Access-Control-Allow-Credentials: true\n";
print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";

print <<EOF
<html><body>
<script>
var scriptListToLoad = [
    'http://127.0.0.1:8000/htmlimports/resources/external-script.js',
    'http://127.0.0.1:8000/htmlimports/resources/cors-js.cgi',
    'http://127.0.0.1:8000/htmlimports/resources/cors-js-for-localhost.cgi',
    'http://127.0.0.1:8000/htmlimports/resources/run-check.js'
];

thisDocument = document.currentScript.ownerDocument;
function loadTestIfReady() {
    if (scriptListToLoad.length)
        loadPendingScript();
}

function loadScriptFrom(url) {
    var element = thisDocument.createElement("script");
    element.setAttribute("crossorigin", "");
    element.setAttribute("src", url);
    element.onload = element.onerror = loadTestIfReady;
    thisDocument.head.appendChild(element);
    return element;
}

function loadPendingScript()
{
    var url = scriptListToLoad.shift();
    loadScriptFrom(url);
}

loadPendingScript();
</script>
</body></html>
EOF
