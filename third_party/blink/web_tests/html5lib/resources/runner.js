// Copyright (c) 2008 Geoffrey Sneddon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

document.writeln("<title>html5lib test runner</title>");
document.writeln("<style>");
document.writeln(".overview:hover {");
document.writeln("background: #ccc;");
document.writeln("}");
document.writeln("iframe {");
document.writeln("display: none;");
document.writeln("}");
document.writeln("</style>");
document.writeln("<p>Script did not run</p>");
document.writeln("<iframe></iframe>");

if (window.testRunner)
    testRunner.waitUntilDone();

Markup.noAutoDump();
Markup.useHTML5libOutputFormat();

var tests = [],
    iframe = document.getElementsByTagName("iframe")[0],
    stat = document.getElementsByTagName("p")[0].firstChild,
    file = "",
    test_number = 1,
    fail_list = [],
    log = "";

iframe.contentWindow.document.open()
iframe.contentWindow.document.write("Test");
iframe.contentWindow.document.close();
var write = iframe.contentWindow.document.lastChild.lastChild.lastChild !== null;
var ignoreTitle = iframe.contentWindow.document.getElementsByTagName("title")[0] !== undefined;

if (window.forceDataURLs)
    write = false;

window.onload = function()
{
    stat.data = "Running";
    run();
}

function run()
{
    var xhr = window.XMLHttpRequest ? new XMLHttpRequest() : new ActiveXObject("Microsoft.XMLHTTP");
    if (file = test_files.shift())
    {
        stat.data = "Retriving " + file;
        test_number = 1;
        fail_list = [];
        log = "";
        xhr.open("GET", file);
        xhr.onreadystatechange = function()
        {
            if (xhr.readyState === 4)
            {
                tests = xhr.responseText.split(/(?:^|\n\n)#data\n/);
                tests.shift();
                test();
            }
        }
        xhr.send(null);
    } else {
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function test()
{
    var input, errorsStart, fragmentStart, contextElement, domStart, dom;
    if (data = tests.shift())
    {
        stat.data = "Running test " + test_number + " of " + (test_number + tests.length) + " in " + file;
        errorsStart = data.indexOf("\n#errors\n");
        if (errorsStart !== -1)
        {
            input = data.substring(0, errorsStart);
            fragmentStart = data.indexOf("\n#document-fragment\n")
            domStart = data.indexOf("\n#document\n")
            if (fragmentStart !== -1)
            {
                contextElement = data.substring(fragmentStart + 20, domStart);
            }
            if (domStart !== -1)
            {
                dom = data.substring(domStart + 11);
                if (dom.substring(dom.length - 1) === "\n")
                {
                    dom = dom.substring(0, dom.length - 1);
                }
                run_test(input, contextElement, dom);
                return;
            }
        }
        alert("Invalid test: " + data);
        test();
        return;
    }
    else
    {
        stat.data = "Finished running " + file;
        var overview = document.createElement("p");
        if (fail_list.length)
        {
            overview.innerHTML = file + ":<br>" + fail_list.join("<br>");
            overview.className = "overview";
            overview.title = "Click for more details";
            overview.onclick = function()
            {
                this.nextSibling.style.display = this.nextSibling.style.display == "none" ? "block" : "none";
            }
            var detail = document.createElement("pre");
            detail.appendChild(document.createTextNode(log.substring(2)));
            detail.style.display = "block";
            document.body.appendChild(overview);
            document.body.appendChild(detail);
        }
        else
        {
            overview.innerHTML = file + ": PASS";
            document.body.appendChild(overview);
        }
        stat.data = "";
        run();
    }
}

function run_test(input, contextElement, expected)
{
    if (contextElement)
    {
        var element = document.createElement(contextElement);
        try
        {
            element.innerHTML = input;
        }
        catch(e) {}
        process_result(input, element, expected);
    }
    else if (write)
    {
        iframe.contentWindow.document.open();
        try
        {
            iframe.contentWindow.document.write(input);
        }
        catch(e) {}
        iframe.contentWindow.document.close();
        if (ignoreTitle)
        {
            var title = iframe.contentWindow.document.getElementsByTagName("title")[0];
            if (!title.innerHTML)
            {
                title.parentElement.removeChild(title);
            }
        }
        process_result(input, iframe.contentWindow.document, expected);
    }
    else
    {
        iframe.onload = function()
        {
            if (ignoreTitle)
            {
                var title = iframe.contentWindow.document.getElementsByTagName("title")[0];
                if (!title.innerHTML)
                {
                    title.parentElement.removeChild(title);
                }
            }
            process_result(input, iframe.contentWindow.document, expected);
        }
        iframe.srcdoc = input;
    }
}

function process_result(input, result, expected)
{
    result = Markup.get(result);
    if (result !== expected)
    {
        fail_list.push(test_number);
        log += "\n\nTest " + (test_number) + " of " + (test_number + tests.length) + " in " + file + " failed. Input:\n" + input + "\nGot:\n" + result + "\nExpected:\n" + expected;
    }
    test_number++;
    test();
}
