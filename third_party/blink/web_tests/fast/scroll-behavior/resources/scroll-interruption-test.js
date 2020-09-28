// A SmoothScrollInterruptionTest verifies that in-progress smooth scrolls
// stop when interrupted by an instant scroll, another smooth scroll, a
// touch scroll, or a mouse wheel scroll.
//
// The only SmoothScrollInerruptionTest method that should be called by
// outside code is run().
//
// Creates a SmoothScrollInterruptionTest with arguments:
// scrollElement - Element being scrolled.
// innerPoint - Absolute position (expressed as a dictionary with x and y fields)
//              of a point inside |scrollElement|, that can be used as the location
//              of input events that trigger scrolls on |scrollElement|.
// targets - A dictionary whose members y_min, y_mid, and y_max should be
//           y co-ordinates that are far enough apart from each other that a
//           smooth scroll between any pair of them will be non-trivial (that
//           is, take multiple frames to finish), and should be such that
//           y_min < y_mid < y_max.
// jsScroll - Callback that takes a y co-ordinate and executes a js-driven
//            smooth scroll to that y co-ordinate.
function SmoothScrollInterruptionTest(scrollElement, innerPoint, targets, jsScroll) {
    this.scrollElement = scrollElement;
    this.innerPoint = innerPoint;
    this.scrollStartPoint = targets.y_mid;
    this.scrollEndPoint = targets.y_max;
    this.scrollNewEndpoint = targets.y_min;
    this.jsScroll = jsScroll;

    this.testCases = [];
    this.testCases.push(new SmoothScrollInterruptionTestCase(interruptWithInstantScroll, verifyScrollInterruptedByInstantScroll, "instant scroll"));
    this.testCases.push(new SmoothScrollInterruptionTestCase(interruptWithSmoothScroll, verifyScrollInterruptedBySmoothScroll, "smooth scroll"));
    this.testCases.push(new SmoothScrollInterruptionTestCase(interruptWithTouchScroll, verifyScrollInterruptedByInputDrivenScroll, "touch scroll"));
    this.testCases.push(new SmoothScrollInterruptionTestCase(interruptWithWheelScroll, verifyScrollInterruptedByInputDrivenScroll, "wheel scroll"));

    this.currentTestCase = 0;
}

SmoothScrollInterruptionTest.prototype.startNextTestCase = function() {
    if (this.currentTestCase >= this.testCases.length) {
        this.allTestCasesComplete();
        return;
    }

    var testCase = this.testCases[this.currentTestCase];
    this.asyncTest = async_test(testCase.description);

    var scrollElement = this.scrollElement;
    var scrollStartPoint = this.scrollStartPoint;

    scrollElement.scrollTop = scrollStartPoint;
    window.requestAnimationFrame(this.waitForSyncScroll.bind(this));
}

SmoothScrollInterruptionTest.prototype.waitForSyncScroll = function() {
  // Wait until cc has received the commit from main with the scrollStartPoint.
  if (this.scrollElement.scrollTop != this.scrollStartPoint) {
    // TODO(flackr): There seems to be a bug in that we shouldn't have to
    // reapply the scroll position when cancelling a smooth scroll.
    // https://crbug.com/667477
    this.scrollElement.scrollTop = this.scrollStartPoint;
    window.requestAnimationFrame(this.waitForSyncScroll.bind(this));
    return;
  }

  this.performSmoothScroll();
}

SmoothScrollInterruptionTest.prototype.performSmoothScroll = function() {
    var testCase = this.testCases[this.currentTestCase];
    var scrollElement = this.scrollElement;
    var scrollStartPoint = this.scrollStartPoint;

    this.jsScroll(this.scrollEndPoint);
    this.asyncTest.step(function() {
        assert_equals(scrollElement.scrollTop, scrollStartPoint);
    });

    if (scrollElement.scrollTop == this.scrollEndPoint) {
        // We've instant-scrolled, and failed the assert above.
        this.testCaseComplete();
        return;
    }

    window.requestAnimationFrame(this.waitForSmoothScrollStart.bind(this));
}

SmoothScrollInterruptionTest.prototype.waitForSmoothScrollStart = function() {
    if (this.scrollElement.scrollTop == this.scrollStartPoint) {
        window.requestAnimationFrame(this.waitForSmoothScrollStart.bind(this));
        return;
    }

    var testCase = this.testCases[this.currentTestCase];
    testCase.interruptSmoothScroll(this);
    window.requestAnimationFrame(testCase.verifyScrollInterrupted.bind(testCase, this, this.testCaseComplete.bind(this)));
}

SmoothScrollInterruptionTest.prototype.testCaseComplete = function() {
    this.asyncTest.done();

    this.currentTestCase++;
    this.startNextTestCase();
}

SmoothScrollInterruptionTest.prototype.run = function() {
    setup({explicit_done: true, explicit_timeout: true});
    this.startNextTestCase();
}

SmoothScrollInterruptionTest.prototype.allTestCasesComplete = function() {
    done();
}

// A SmoothScrollInterruptionTestCase represents a single way of interrupting
// a smooth scroll and verifying that the smooth scroll gets canceled.
//
// Creates a SmoothScrollInterruptionTestCase with arguments:
// interruptSmoothScoll - Callback that takes a SmoothScrollInterruptionTest,
//                        and interrupts the on-going smooth scroll.
// verifyScrollInterrupted - Callback that takes a SmoothScrollInterruptionTest,
//                           a |verificationComplete| callback, and a timestamp,
//                           verifies (possibly asynchronously) that the smooth
//                           scroll has been superseded by the interruption, and
//                           then calls |verificationComplete|.
// description - String describing this test case.
function SmoothScrollInterruptionTestCase(interruptSmoothScroll, verifyScrollInterrupted, description) {
    this.interruptSmoothScroll = interruptSmoothScroll;
    this.verifyScrollInterrupted = verifyScrollInterrupted;
    this.description = description;
}


function interruptWithInstantScroll(smoothScrollTest) {
    smoothScrollTest.scrollElement.scrollTop = smoothScrollTest.scrollNewEndpoint;
    smoothScrollTest.asyncTest.step(function() {
        assert_equals(smoothScrollTest.scrollElement.scrollTop, smoothScrollTest.scrollNewEndpoint);
    });
}

function verifyScrollInterruptedByInstantScroll(smoothScrollTest, verificationComplete) {
    smoothScrollTest.asyncTest.step(function() {
        assert_equals(smoothScrollTest.scrollElement.scrollTop, smoothScrollTest.scrollNewEndpoint);
    });
    verificationComplete();
}

function interruptWithSmoothScroll(smoothScrollTest) {
    smoothScrollTest.jsScroll(smoothScrollTest.scrollNewEndpoint);
    smoothScrollTest.asyncTest.step(function() {
        assert_not_equals(smoothScrollTest.scrollElement.scrollTop, smoothScrollTest.scrollNewEndpoint);
    });

    this.scrollInterruptionPoint = smoothScrollTest.scrollElement.scrollTop;
}

function verifyScrollInterruptedBySmoothScroll(smoothScrollTest, verificationComplete) {
    var currentPosition = smoothScrollTest.scrollElement.scrollTop;

    if (currentPosition < this.scrollInterruptionPoint && currentPosition >= smoothScrollTest.scrollNewEndpoint) {
        verificationComplete();
    } else {
        window.requestAnimationFrame(this.verifyScrollInterrupted.bind(this, smoothScrollTest, verificationComplete));
    }
}

function interruptWithTouchScroll(smoothScrollTest) {
    if (window.eventSender) {
        eventSender.gestureScrollBegin(smoothScrollTest.innerPoint.x, smoothScrollTest.innerPoint.y);
        eventSender.gestureScrollUpdate(0, -10);
        eventSender.gestureScrollEnd(0, 0);
    } else {
        document.write("This test does not work in manual mode.");
    }
}

function verifyScrollInterruptedByInputDrivenScroll(smoothScrollTest, verificationComplete, timestamp) {
    var currentPosition = smoothScrollTest.scrollElement.scrollTop;

    if (this.previousPosition && this.previousPosition == currentPosition) {
        // Ensure that the animation has really stopped, not that we just have
        // two frames that are so close together that the animation only seems to
        // have stopped.
        if (timestamp - this.previousTimestamp > 16) {
            verificationComplete();
        } else {
            window.requestAnimationFrame(this.verifyScrollInterrupted.bind(this, smoothScrollTest, verificationComplete));
        }

        return;
    }

    this.previousPosition = currentPosition;
    this.previousTimestamp = timestamp;
    smoothScrollTest.asyncTest.step(function() {
        assert_not_equals(currentPosition, smoothScrollTest.scrollEndPoint);
    });
    window.requestAnimationFrame(this.verifyScrollInterrupted.bind(this, smoothScrollTest, verificationComplete));
}

function interruptWithWheelScroll(smoothScrollTest) {
    if (window.eventSender) {
        eventSender.mouseMoveTo(smoothScrollTest.innerPoint.x, smoothScrollTest.innerPoint.y);
        eventSender.mouseScrollBy(0, -10);
    } else {
        document.write("This test does not work in manual mode.");
    }
}
