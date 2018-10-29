//
// Here is the shared test code among the various navigation tests.  The idea is to apply
// a number of operation sequences to a variety of navigation techniques.  "Operations"
// are things like loading a page, or going back or forward. "Navigation techniques" are
// things like a HTTP 302 redirect, or the idiom of a POST followed by a redirect.


// utility function to fill the test form with some state, simulating the user's input
function fillTestForm() {
    // Add data to form here instead of inside longForm.html, so these settings aren't
    // also done when we return to that page, so we are sure to test the form state restore.
    // Currently the state of checkboxes and radio buttons doesn't affect the DumpRenderTree
    // output, so those settings are moot.
    testDoc = (window.frames.length == 0) ? document : window.frames['main'].document;
    testForm = testDoc.getElementById('testform');
    testForm.textfield1.value='New form text from user';
    testDoc.getElementById('radiooption2').checked=true;
    testForm.checkbox2.checked=true;
    testForm.selectgroup1.selectedIndex=2;
    testForm.textarea1.value='More new form text from user, which should be restored when we return to this page.';
}

// utility function to scroll the document down a bit, to test save/restore
function scrollDocDown() {
    testDoc = (window.frames.length == 0) ? document : window.frames['main'].document;
    testDoc.getElementById('testbody').scrollTop=50;
}

// utility function to make a form post
function submitFormWithPost() {
    testDoc = (window.frames.length == 0) ? document : window.frames['main'].document;
    testDoc.getElementById('testform').submitwithpost.click();
}

// utility function to make a form post, using the postredirect idiom
function submitFormWithPostRedirect() {
    testDoc = (window.frames.length == 0) ? document : window.frames['main'].document;
    testDoc.getElementById('testform').submitwithpostredirect.click();
}

// utility function to make a form post, using the postredirect idiom
function submitFormWithPostRedirectReload() {
    testDoc = (window.frames.length == 0) ? document : window.frames['main'].document;
    testDoc.getElementById('testform').submitwithpostredirectreload.click();
}

// utility function to do a jump within the page to an anchor
function jumpToAnchor() {
    testWin = (window.frames.length == 0) ? window : window.frames['main'];
    testWin.location.hash = "anchor1";
}



// This is the most basic sequence.  Just load the page and verify it worked.
// This is used for the non-frames and frames case.
// Optionally we will also poke the page to perform a post or jump to an anchor.
//
// Note most back/forward bugs are due to the b/f list not being created
// right during the loading, so this catches a lot of those.
// When testCase is a URL with an anchor, or when we queue a jumpToAnchor(),
// it's important to check that that we end up scrolled to the right place,
// proving the anchor was visited.
function runBasicTest(testCase, extraStep) {
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.dumpBackForwardList();
        testRunner.queueLoad(testCase);
        if (extraStep == "post") {
            testRunner.queueNonLoadingScript("fillTestForm()");
            testRunner.queueLoadingScript("submitFormWithPost()");
        } else if (extraStep == "postredirect") {
            testRunner.queueNonLoadingScript("fillTestForm()");
            testRunner.queueLoadingScript("submitFormWithPostRedirect()");
        } else if (extraStep == "relativeanchor") {
            testRunner.queueLoadingScript("jumpToAnchor()");
        }
    }
}

// A sequence testing back/forward.  Tests that we made it back to
// right page, and that the form and scroll state was saved and restored.
// This is used for the non-frames and frames case.
// Optionally we will also poke the page to perform a post or jump to an anchor.
//
// When testCase is a URL with an anchor, or when we queue a jumpToAnchor(),
// when we go back the scroll set by the user in this sequence should override
// the scroll implied by the anchor.
// When we POST it is interesting to test going back to the post result,
// and going back 2 pages to the original form.
function runBackTest(testCase, howFarBack, extraStep) {
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.dumpBackForwardList();
        testRunner.queueLoad(testCase);
        testRunner.queueNonLoadingScript("fillTestForm()");
        testRunner.queueNonLoadingScript("scrollDocDown()");
        if (extraStep == "post") {
            testRunner.queueLoadingScript("submitFormWithPost()");
        } else if (extraStep == "postredirect") {
            testRunner.queueLoadingScript("submitFormWithPostRedirect()");
        } else if (extraStep == "relativeanchor") {
            testRunner.queueLoadingScript("jumpToAnchor()");
        }
        testRunner.queueLoad("resources/otherpage.html");
        testRunner.queueBackNavigation(howFarBack);
    }
}

// A sequence testing frames, where the given nav technique is used to
// load a single child frame, after the load of the whole frameset.
function runLoadChildFrameTest(testCase) {
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.dumpBackForwardList();
        testRunner.queueLoad("resources/frameset.pl?frameURL=otherpage.html");
        testRunner.queueLoad(testCase, "main");
    }
}

// A sequence testing frames, where the given nav technique is used to
// load a single child frame, and then we go back to that point to check
// state save/restore.  Some browsers do not let you restablish the
// set of subframes you were viewing when you go back in a case like this.
function runLoadChildFrameBackTest(testCase) {
    if (window.testRunner) {
        testRunner.dumpBackForwardList();
        testRunner.queueLoad("resources/frameset.pl?frameURL=otherpage.html");
        testRunner.queueLoad(testCase, "main");
        testRunner.queueNonLoadingScript("fillTestForm()");
        testRunner.queueNonLoadingScript("scrollDocDown()");
        testRunner.queueLoad("resources/otherpage.html");
        testRunner.queueBackNavigation(1);
    }
}


// A sequence testing reload.  The goals are that form state is cleared,
// scroll state is restored, and nothing is added to b/f list.
function runReloadTest(testCase) {
    if (window.testRunner) {
        testRunner.dumpBackForwardList();
        testRunner.queueLoad(testCase);
        testRunner.queueNonLoadingScript("fillTestForm()");
        testRunner.queueNonLoadingScript("scrollDocDown()");
        testRunner.queueReload();
    }
}

// A sequence testing the repeated load of the same URL, not via the reload
// button (e.g., the user hits return in the location field).  It was decided
// that this case should not preserve scroll state or form state, and not add
// anything to the b/f list.
function runLoadSameTest(testCase) {
    testRunner.dumpBackForwardList();
    testRunner.queueLoad(testCase);
    testRunner.queueNonLoadingScript("fillTestForm()");
    testRunner.queueNonLoadingScript("scrollDocDown()");
    testRunner.queueLoad(testCase);
}

// A sequence testing a reload after a redirect. The goal is to check
// that in a reload we use the method set by the redirect, GET,
// instead of the original one, POST.
function runRedirectReloadTest(testCase) {
    testRunner.dumpBackForwardList();
    testRunner.queueLoad(testCase);
    testRunner.queueLoadingScript("submitFormWithPostRedirectReload()");
    testRunner.queueReload();
}
