var div = document.createElement("div");
div.style.width = "200%";
var slider1 = document.createElement("input");
slider1.id = "slider1";
slider1.setAttribute("type", "range");
slider1.style.width = "200px";
slider1.style.height = "30px";
var slider2 = document.createElement("input");
slider2.id = "slider2";
slider2.setAttribute("type", "range");
slider2.style.width = "30px";
slider2.style.height = "200px";
slider2.style.setProperty("writing-mode", "vertical-lr");
slider2.style.direction = "rtl";
var slider3 = document.createElement("input");
slider3.id = "slider3";
slider3.setAttribute("type", "range");
slider3.style.width = "200px";
slider3.style.height = "30px";
slider3.style.setProperty("-webkit-transform", "rotate(-90deg)");
slider3.style.setProperty("margin", "120px 0");

document.body.insertBefore(div, document.getElementById('console'));
div.appendChild(slider1);
div.appendChild(slider2);
div.appendChild(slider3);

var onTouchStart = (function() {

    var slider = [slider1, slider2, slider3];
    var nCheck = 0;

    return function() {
        shouldBeEqualToString("event.touches[0].target.id", slider[nCheck++].id);
        checkPosition();
    }

})();

function onTouchMove() {
    checkPosition();
}

function onTouchEnd() {
    checkPosition();
}

function onKeyDown() {
    isSuccessfullyParsed();
    testRunner.notifyDone();
}

var sliderValue = 0;

var checkPosition = (function() {

    var nCheck = 0;
    var slider = [slider1, slider2, slider3];
    var expectedPositions = [50, 50, 0, 100, 50];
    var sliderIndex = 0;

    return function() {
        sliderValue = slider[sliderIndex].value;
        shouldBeEqualToString("sliderValue", String(expectedPositions[nCheck++]));
        if (nCheck % expectedPositions.length == 0) {
            sliderIndex++;
            nCheck = 0;
        }
    };

})();

function runTest(slider, rotated) {

    var w = slider.clientWidth;
    var h = slider.clientHeight;
    var x = slider.offsetLeft + w/2;
    var y = slider.offsetTop + h/2;

    if (rotated) {
      w = slider.clientHeight;
      h = slider.clientWidth;
    }

    eventSender.clearTouchPoints();
    eventSender.addTouchPoint(x, y);
    eventSender.touchStart();

    eventSender.updateTouchPoint(0, x - w/2, y + h/2);
    eventSender.touchMove();

    eventSender.updateTouchPoint(0, x + w/2, y - h/2);
    eventSender.touchMove();

    eventSender.updateTouchPoint(0, x, y);
    eventSender.touchMove();

    eventSender.releaseTouchPoint(0);
    eventSender.touchEnd();
}

document.addEventListener("touchstart", onTouchStart, false);
document.addEventListener("touchmove", onTouchMove, false);
document.addEventListener("touchend", onTouchEnd, false);
document.addEventListener("keydown", onKeyDown, false);

description("Tests that the touch events originating on an input element with type=range update the slider position. This test is only expected to pass if ENABLE_TOUCH_SLIDER is defined.");

if (window.testRunner) {
    testRunner.waitUntilDone();
}

if (window.eventSender) {
    runTest(slider1, false);
    runTest(slider2, false);
    runTest(slider3, true);

    eventSender.keyDown(' ');
} else {
    debug('This test requires DRT.');
}
