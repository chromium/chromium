description('Tests that dragging a file into an editable area does not insert a filename.');

var editableArea = document.createElement('div');
editableArea.contentEditable = true;
editableArea.style.background = 'green';
editableArea.style.height = '100px';
editableArea.style.width = '100px';
// Important that we put this at the top of the doc so that logging does not
// cause it to go out of view (where it can't be dragged to).
document.body.insertBefore(editableArea, document.body.firstChild);

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function dragFilesOntoEditableArea(files)
{
    eventSender.beginDragWithFiles(files);
    moveMouseToCenterOfElement(editableArea);
    eventSender.mouseUp();
    window.stop();
}

function runTest()
{
    window.attemptedSameTabFileNavigation = false;
    window.onbeforeunload = function() {
        window.attemptedSameTabFileNavigation = true;

        // Don't remove the editable node, since we want to make sure no stray
        // file URLs were inserted during the drop.
    };
    dragFilesOntoEditableArea(['DRTFakeFile']);

    // The file load should occur in a new tab (crbug.com/451659), so we
    // should not attempt to navigate this tab.
    shouldBeFalse("window.attemptedSameTabFileNavigation");
    finishJSTest();
}

var successfullyParsed = true;

if (window.eventSender && window.testRunner) {
    window.jsTestIsAsync = true;

    window.onload = runTest;
} else {
    testFailed('This test is not interactive, please run using DumpRenderTree');
}
