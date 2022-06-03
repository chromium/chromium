description('Blurring and focusing the window should blur and focus the focused element');

var divElement = document.createElement('div');
divElement.tabIndex = 0;
divElement.textContent = 'Outer';
document.body.appendChild(divElement);

var iframeElement = document.createElement('iframe');
document.body.appendChild(iframeElement);
var iframe = window.frames[0];
var innerDiv = iframeElement.contentDocument.createElement('div');
innerDiv.textContent = 'Inner';
innerDiv.tabIndex = 0;
iframeElement.contentDocument.body.appendChild(innerDiv);

var thisObjects = [];
var events = [];
var targets = [];

// Focus before setting up event listeners.
divElement.focus();

divElement.onfocus = divElement.onblur = window.onfocus = window.onblur =
    innerDiv.onfocus = innerDiv.onblur = iframe.onfocus = iframe.onblur = function(e)
{    
    thisObjects.push(this);
    events.push(e.type);
    targets.push(e.target);
};

if (window.testRunner) {
    testRunner.setWindowFocus(false);
    testRunner.setWindowFocus(true);
    
    innerDiv.focus();
    divElement.focus();
}

var i = 0;
function testNextEvent(target, eventType)
{
    shouldBe('thisObjects[' + i + ']', target);
    shouldBeEqualToString('events[' + i + ']', eventType);
    shouldBe('targets[' + i + ']', target);
    i++;
}

testNextEvent('divElement', 'blur');
testNextEvent('window', 'blur');
testNextEvent('window', 'focus');
testNextEvent('divElement', 'focus');

testNextEvent('divElement', 'blur');
testNextEvent('window', 'blur');
testNextEvent('iframe', 'focus');
testNextEvent('innerDiv', 'focus');

testNextEvent('innerDiv', 'blur');
testNextEvent('iframe', 'blur');
testNextEvent('window', 'focus');
testNextEvent('divElement', 'focus');

document.body.removeChild(divElement);
document.body.removeChild(iframeElement);
