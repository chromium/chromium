var hitRegionEventList = {
  click : {
    send : function(x, y) {
             eventSender.mouseMoveTo(x, y);
             eventSender.mouseDown();
             eventSender.mouseUp();
           },
    getId : function(event) { return event.region; }
  },
  touchstart : {
    send : function(x, y) {
             eventSender.clearTouchPoints();
             eventSender.addTouchPoint(x, y);
             eventSender.touchStart();
             eventSender.touchEnd();
           },
    getId : function(event) { return event.targetTouches[0].region; }
  }
};

function clickCanvas(x, y, scaleFactor)
{
  if (!window.eventSender)
    return "This test requires eventSender";

  var actualId = null;
  var result = {};

  function listener(event) {
    result[event.type] = hitRegionEventList[event.type].getId(event);
    canvas.removeEventListener(type, listener, false);
  }

  if (scaleFactor === undefined)
    scaleFactor = 1;
  var rect = canvas.getBoundingClientRect();
  var translatedCanvasLeft = rect.left * scaleFactor * window.devicePixelRatio;
  var translatedCanvasTop = rect.top * scaleFactor * window.devicePixelRatio;
  for (var type in hitRegionEventList) {
    canvas.addEventListener(type, listener, false);
    hitRegionEventList[type].send(translatedCanvasLeft + x, translatedCanvasTop + y);
  }

  // single actualId guarantees that results of eventlist are same
  for (var type in hitRegionEventList) {
    if (result[type] === undefined)
      result[type] = "no event sent to canvas";
    if (actualId === null) {
      actualId = result[type];
    } else {
      if (actualId != result[type]) {
        // some results of eventlist are different, makes log
        !function() {
          actualId = "{";
          for (var type in hitRegionEventList) {
             actualId += type + ": '" + result[type] + "'; ";
          }
          actualId += "}";
        }();
        break;
      }
    }
  }

  for (var type in hitRegionEventList) {
    canvas.removeEventListener(type, listener, false);
  }

  return actualId;
}
