description("This tests that the input events are dispatched when contentEditable nodes are edited");

var actualInputEventCount = 0;
var expectedInputEventCount = 0;

function fired(evt, expectedId, expectedText)
{
    actualInputEventCount++;
    shouldBe("event.target.id", "'" + expectedId + "'");
    shouldBe("event.target.innerHTML", "'" + expectedText + "'");
}

var testDataRoot = document.createElement("div");
document.body.appendChild(testDataRoot);

function makeTestTarget(fragment)
{
    testDataRoot.innerHTML = fragment;
    return testDataRoot.firstChild;
}

function clearTestTarget()
{
    makeTestTarget("");
}

function setupForFiringTest(fragment, expectedText)
{
    var target = makeTestTarget(fragment);
    target.addEventListener("input", function(evt) { fired(evt, target.id, expectedText); });
    sel.selectAllChildren(target);
    expectedInputEventCount++;
    return target;
}

var sel = document.getSelection();

// A trivial case: inserting text should dispatch the event
var target0 = setupForFiringTest('<p id="target0" contentEditable>Text should be replace this</p>', "Text");
sel.selectAllChildren(target0);
document.execCommand("insertText", false, "Text");

// A direct DOM manipulation shouldn't dispatch the event
var target1 = makeTestTarget('<p id="target1" contentEditable>Text should be insert via script:</p>');
target1.addEventListener("input", function(evt) { testFailed("should not be reached"); });
target1.firstChild.data += "Text";

// An event should be dispatched even if resulting DOM tree doesn't change after the edit. (with replacing it)
var target2Text = "This text should not be changed.";
var target2 = setupForFiringTest('<p id="target2" contentEditable>This text should not be changed.</p>', target2Text);
document.execCommand("insertText", false, target2Text);

// An "delete" command should dispatch an input event.
var target3 = setupForFiringTest('<p id="target3" contentEditable>This text shouldn be deleted.</p>', '<br>');
document.execCommand("delete", false);

// A command other than text-editing should dispatch an input event.
// Also note that createLink is a composite command,
// so this test also ensures that even composite command dispatches the event only once.
var target4 = setupForFiringTest('<p id="target4" contentEditable>This text should be a link.</p>', "<a href=\"http://www.example.com/\">This text should be a link.</a>");
document.execCommand("createLink", false, "http://www.example.com/");

// An event shouldn't be dispatched to a 'display:none' node.
var target5 = makeTestTarget('<p id="target5" contentEditable>This will not be rendered.</p>');
target5.addEventListener("input", function(evt) { testFailed("should not be reached"); });
sel.selectAllChildren(target5);
target5.style.display = "none";
document.execCommand("insertText", false, "Text");

// The event should be dispatched from the editable root.
makeTestTarget('<p id="target6parent" contentEditable><span id="target6start">Start,</span><span id="target6middle">Middle,</span><span id="target6end">End.</span></p>');
var target6parent = document.getElementById("target6parent");
var target6start = document.getElementById("target6start");
var target6middle = document.getElementById("target6middle");
var target6end = document.getElementById("target6end");

var expectedText6 = "<a href=\"http://www.example.com/\"><span id=\"target6start\">Start,</span><span id=\"target6middle\">Middle,</span><span id=\"target6end\">End.</span></a>";
target6parent.addEventListener("input", function(evt) { fired(evt, "target6parent", expectedText6); });
expectedInputEventCount++;
target6start.addEventListener("input", function(evt) { testFailed("should not be reached"); });
target6end.addEventListener("input", function(evt) { testFailed("should not be reached"); });
target6middle.addEventListener("input", function(evt) { fired(evt, "target6end", ""); });
sel.selectAllChildren(target6parent);
document.execCommand("createLink", false, "http://www.example.com/");

// Ensure key events can cause the event 
var target7 = setupForFiringTest('<p id="target7" contentEditable>Replaced</p>', "X");
sel.selectAllChildren(target7);
eventSender.keyDown('X');

var target8 = setupForFiringTest('<p id="target8" contentEditable>Deleted</p>', '<br>');
sel.selectAllChildren(target8);
eventSender.keyDown('Delete');

var target9parent = makeTestTarget('<div id="target9parent" contenteditable><div id="target9child" contenteditable>foo</div></div>');
var target9child = document.getElementById('target9child');
target9child.addEventListener("input", function(evt) { testFailed("should not be reached"); });
target9parent.addEventListener("input", function(evt) { fired(evt, target9parent.id, '<div id="target9child" contenteditable="">Replacing</div>'); });
sel.selectAllChildren(target9child);
document.execCommand("insertText", false, "Replacing");
expectedInputEventCount++;

var target10parent = makeTestTarget('<div id="t10parent" contenteditable><div id="t10child" contenteditable=false><div id="t10gch" contenteditable>foo</div></div></div>');
var target10child = document.getElementById("t10child");
var target10gch = document.getElementById("t10gch");
target10gch.addEventListener("input", function(evt) { fired(evt, target10gch.id, 'Replacing'); });
sel.selectAllChildren(target10gch);
document.execCommand("insertText", false, "Replacing");
expectedInputEventCount++;

shouldBe("expectedInputEventCount", "actualInputEventCount");
clearTestTarget();
