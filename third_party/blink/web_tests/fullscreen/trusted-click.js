// Invokes callback from a trusted click event, to satisfy
// https://html.spec.whatwg.org/#triggered-by-user-activation
function trusted_click(test, callback, container,
                       clickRectInRootFrameCoordinate)
{
    var document = container.ownerDocument;

    if (window.testRunner) {
        // Running under LayoutTests. Use timeout to be async.
        setTimeout(test.step_func(function()
        {
            document.addEventListener("click", callback);
            if (clickRectInRootFrameCoordinate) {
                var x = (clickRectInRootFrameCoordinate.left
                         + clickRectInRootFrameCoordinate.right) / 2;
                var y = (clickRectInRootFrameCoordinate.top
                         + clickRectInRootFrameCoordinate.bottom) / 2;
                eventSender.mouseMoveTo(x, y);
            }
            eventSender.mouseDown();
            eventSender.mouseUp();
            document.removeEventListener("click", callback);
        }), 0);
    } else {
        // Running as manual test. Show a button to click.
        var button = document.createElement("button");
        button.textContent = "click to run test";
        button.style.display = "block";
        button.style.fontSize = "20px";
        button.style.padding = "10px";
        button.onclick = () => test.step_func(() => {
          callback();
          button.onclick = null;
          container.removeChild(button);
        });
        container.appendChild(button);
    }
}

function silence_rejection(promise) {
  // Keep the promise resolution silent. Otherwise unhandledrejection
  // may fire for the failure test cases.
  if (promise) promise.catch(() => {});
}

// Invokes element.requestFullscreen() from a trusted click.
function trusted_request(test, element, container, clickRectInRootFrameCoordinate)
{
    trusted_click(test, () => {
        silence_rejection(element.requestFullscreen());
    }, container || element.parentNode,
                  clickRectInRootFrameCoordinate);
}
