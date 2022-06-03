const boundaryOffset = 2;

function scrollPageIfNeeded(targetSelector, targetDocument) {
  var target = targetDocument.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  if (targetRect.top < 0 || targetRect.left < 0 || targetRect.bottom > window.innerHeight || targetRect.right > window.innerWidth)
    window.scrollTo(targetRect.left, targetRect.top);
}

// Drag and drop actions
function mouseDragAndDropInTargets(targetSelectorList) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      eventSender.mouseMoveTo(xPosition, yPosition);
      eventSender.mouseDown();
      eventSender.leapForward(100);
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        eventSender.mouseMoveTo(xPosition, yPosition);
      }
      eventSender.mouseUp();
      resolve();
    } else {
      reject();
    }
  });
}

function inject_input() {
    return mouseDragAndDropInTargets(['#draggable', '#outerdiv', '#innerdiv', '#outerdiv']);
}

{
  var pointerevent_automation = async_test("PointerEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(function() {
    pointerevent_automation.done();
  });
}
