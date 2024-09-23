description("This tests CompositeEditCommand::breakOutOfEmptyListItem by inserting new paragraph")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

function pressKey(key)
{
    eventSender.keyDown(key);
}

function enterAtTarget(initialContent)
{
    testContainer.innerHTML = initialContent;
    var r = document.createRange();
    var s = window.getSelection();

    var t = document.getElementById('target');
    if (!t)
        return 'target element not found';
    r.setStart(t, 0);
    r.setEnd(t, 0);
    s.removeAllRanges();
    s.addRange(r);

    pressKey('Enter');
    
    return testContainer.innerHTML;
}

function testBreakOutOfEmptyListItem(initialContents, expectedContents)
{
    shouldBe("enterAtTarget('"+initialContents+"')", "'"+expectedContents+"'");
}

testBreakOutOfEmptyListItem('<ul><li>a <ul><li>b</li><li id="target"></li></ul> </li></ul>', '<ul><li>a </li><ul><li>b</li></ul><li><br></li> </ul>');
testBreakOutOfEmptyListItem('<ul><li>a <ul><li id="target"></li><li>b</li></ul> </li></ul>', '<ul><li>a </li><li><br></li><ul><li>b</li></ul> </ul>');
testBreakOutOfEmptyListItem('<ul><li>a <ul><li>b</li><li id="target"></li><li>c</li></ul> </li></ul>', '<ul><li>a </li><ul><li>b</li></ul><li><br></li><ul><li>c</li></ul> </ul>');
testBreakOutOfEmptyListItem('<ul><li>hello<ul><li id="target"><br></li></ul>world</li></ul>', '<ul><li>hello<div><br></div>world</li></ul>');
testBreakOutOfEmptyListItem('<ul><li>hello<ul><li id="target"><br></li></ul></li></ul>', '<ul><li>hello</li><li><br></li></ul>');
testBreakOutOfEmptyListItem('<ul><li><ul><li id="target"><br></li></ul>world</li></ul>', '<ul><li><div><br></div>world</li></ul>');
testBreakOutOfEmptyListItem('<ul><li><ul><li id="target"><br></li></ul></li></ul>', '<ul><li><br></li></ul>');
testBreakOutOfEmptyListItem('<ul><li>hello</li><br id="target"></ul>', '<ul><li>hello</li></ul><div><br></div>');
testBreakOutOfEmptyListItem('<ul><br id="target"></ul>', '<div><br></div>');
testBreakOutOfEmptyListItem('<ul><li>hello</li>abc<li id="target"></li></ul>', '<ul><li>hello</li>abc</ul><div><br></div>');
testBreakOutOfEmptyListItem('<ul><li>1</li><ul><li>2.1</li></ul><li id="target"></li></ul>', '<ul><li>1</li><ul><li>2.1</li></ul></ul><div><br></div>');
testBreakOutOfEmptyListItem('<ul><li>1</li><ul><li>2.1</li><li>2.2</li><li id="target"></li></ul><li>3</li></ul>', '<ul><li>1</li><ul><li>2.1</li><li>2.2</li></ul><li><br></li><li>3</li></ul>');
testBreakOutOfEmptyListItem('<ul><li>1</li><ul><li>2.1</li><li>2.2</li>abc<li id="target"></li></ul><li>3</li></ul>', '<ul><li>1</li><ul><li>2.1</li><li>2.2</li>abc</ul><li><br></li><li>3</li></ul>');
testBreakOutOfEmptyListItem('<ul><li>1</li><li id="target"></li><li>3</li></ul>', '<ul><li>1</li></ul><div><br></div><ul><li>3</li></ul>');

document.body.removeChild(testContainer);

var successfullyParsed = true;
