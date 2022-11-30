description("Test to make sure we preserve styles when removing links")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function testSingleToggle(toggleCommand, initialContents, selector, expectedContents)
{
    testContainer.innerHTML = initialContents;
    var selected = selector(testContainer);
    document.execCommand(toggleCommand, false, 'http://webkit.org/');
    action = toggleCommand + ' on ' + selected + ' of "' + initialContents + '" yields "' + testContainer.innerHTML + '"';
    if (testContainer.innerHTML === expectedContents)
        testPassed(action);
    else
        testFailed(action + ', expected "' + expectedContents + '"');
}

function selectAll(container) {
    window.getSelection().selectAllChildren(container);
    return 'all';
}

function selectFirstTwoWords(container) {
    window.getSelection().collapse(container, 0);
    window.getSelection().modify('extend', 'forward', 'word');
    window.getSelection().modify('extend', 'forward', 'word');
    return 'first two words';
}

function selectLastTwoWords(container) {
    window.getSelection().collapse(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last two words';
}

function selectLastWord(container) {
    window.getSelection().collapse(container, container.childNodes.length);
    window.getSelection().modify('extend', 'backward', 'word');
    return 'last word';
}

if (window.internals)
   internals.settings.setEditingBehavior('win');

testSingleToggle("unlink", 'hello <b>world</b>', selectAll, 'hello <b>world</b>');
testSingleToggle("unlink", '<a href="http://webkit.org/"><u>hello world</u></a>', selectAll, '<u>hello world</u>');
testSingleToggle("unlink", 'hello <i><a href="http://webkit.org/">world</a></i>', selectAll, 'hello <i>world</i>');
testSingleToggle("unlink", 'hello <a href="http://webkit.org/" style="font-weight: bold;">world</a>', selectAll, 'hello <b>world</b>');
testSingleToggle("unlink", 'hello <a href="http://webkit.org/" style="color: blue;">world</a> WebKit', selectAll, 'hello <font color="rgb(0, 0, 255)">world</font> WebKit');
testSingleToggle("unlink", 'hello <a href="http://webkit.org/" style="color: blue; display: block;">world</a> WebKit',
    selectAll, 'hello <font color="rgb(0, 0, 255)"><span style="display: block;">world</span></font> WebKit');
testSingleToggle("unlink", '<a href="http://webkit.org/" style="font-size: large;">hello world</a> WebKit',
    selectLastTwoWords, '<a href="http://webkit.org/" style="font-size: large;">hello </a><font size="4">world</font> WebKit');
testSingleToggle("unlink", 'hello <a href="http://webkit.org/" style="font-size: large;">world <span style="font-size: small;">WebKit</span> rocks</a>',
    selectLastTwoWords, 'hello <a href="http://webkit.org/"><font size="4">world </font></a><span style="font-size: small;">WebKit</span><font size="4"> rocks</font>');
testSingleToggle("unlink", 'hello <a href="http://webkit.org/" style="font-style: italic;"><b>world</b> WebKit</a>',
    selectFirstTwoWords, 'hello <b style="font-style: italic;">world</b> <a href="http://webkit.org/" style="font-style: italic;">WebKit</a>');
testSingleToggle("unlink", '<a href="http://webkit.org/" style="background-color: yellow;"><div>hello</div><div>world</div></a>',
    selectAll, '<div style="background-color: yellow;">hello</div><div style="background-color: yellow;">world</div>');
testSingleToggle("unlink", 'hello<a href="http://webkit.org/" style="background-color: yellow;"><div>world</div></a>WebKit',
    selectAll, 'hello<div style="background-color: yellow;">world</div><span style="background-color: yellow;">WebKit</span>');
testSingleToggle("unlink", '<a href="http://webkit.org/" style="font-weight: bold;"><div>hello</div><div>world WebKit</div></a>',
    selectLastTwoWords, '<a href="http://webkit.org/"><div style="font-weight: bold;">hello</div></a><div style="font-weight: bold;">world WebKit</div>');
testSingleToggle("unlink", '<a href="http://webkit.org/" style="font-weight: bold;"><div style="font-weight: normal;">hello</div><div>world</div></a>',
    selectLastWord, '<a href="http://webkit.org/"><div style="font-weight: normal;">hello</div></a><div style="font-weight: bold;">world</div>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
