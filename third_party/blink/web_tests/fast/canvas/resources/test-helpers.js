function coroutine(generator) {
  var g = generator();
  var promise = g.next().value;
  function result(r) {
    promise = g.next(r).value;
    promise && promise.then(result);
  }
  promise.then(result)
}

function clickOrTouch(x, y) {
  return new Promise(function(resolve, reject) {
    window.ontouchstart = function(e) {
      if (!window.eventSender)
        console.log('x: ' + e.pageX, ', y: ' + e.pageY, ', region: ' + e.region);
      resolve(e.targetTouches[0].region);
    };

    window.onclick = function(e) {
      if (!window.eventSender)
        console.log('x: ' + e.pageX, ', y: ' + e.pageY, ', region: ' + e.region);
      resolve(e.region);
    };

    if (window.eventSender) {
      eventSender.clearTouchPoints();
      eventSender.addTouchPoint(x, y);
      eventSender.touchStart();
      eventSender.touchEnd();

      eventSender.mouseMoveTo(x, y);
      eventSender.mouseDown();
      eventSender.mouseUp();
    }
  });
}

function createFace(context) {
  context.fillStyle = 'pink';
  context.arc(200, 175, 150, 0, Math.PI * 2, true);
  context.fill();
  context.addHitRegion({ id : 'face', control : document.getElementById('face') });

  context.beginPath();
  context.fillStyle = 'black';
  context.globalAlpha = .5;
  context.moveTo(200, 165);
  context.lineTo(240, 205);
  context.lineTo(160, 205);
  context.closePath();
  context.fill();
  context.addHitRegion({ id : 'nose' });

  context.beginPath();
  context.fillStyle = 'red';
  context.rect(125, 240, 150, 20);
  context.fill();
  context.addHitRegion({ id : 'mouth' });

  context.beginPath();
  context.globalAlpha = 1;
  context.fillStyle = 'blue';
  context.arc(150, 125, 25, 0, Math.PI * 2, true);
  context.arc(250, 125, 25, 0, Math.PI * 2, true);
  context.fill();
  context.addHitRegion({ id: 'eye', control : document.getElementById('eyes') });
}
