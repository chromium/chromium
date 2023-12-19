/**
 * Calls the function (f) after N number of frames have passed.
 *
 * @param {Function} f
 * @param {number} num_frames
 */
function delayByFrames(f, num_frames) {
  function recurse(depth) {
    if (depth == 0) f();
    else requestAnimationFrame(() => recurse(depth - 1));
  }
  recurse(num_frames);
}

/**
 * @param {string} eventType
 * @param {object} options EventListenerOptions
 * @returns {Promise<Event>} Resolved when the event is fired.
 */
function getEvent(eventType, options = { once: true }) {
  return new Promise((resolve) => {
    document.body.addEventListener(eventType, resolve, options);
  });
}

/**
 *
 * @param {Window} context
 * @returns {Promise<Boolean>} resolved with a true if transient activation is consumed.
 */
async function consumeTransientActivation(context = window) {
  if (!context.navigator.userActivation.isActive) {
    throw new Error(
      "User activation is not active so can't be consumed. Something is probably wrong with the test."
    );
  }
  if (test_driver?.consume_user_activation) {
    return test_driver.consume_user_activation(context);
  }
  // fallback to Fullscreen API.
  if (!context.document.fullscreenElement) {
    await context.document.documentElement.requestFullscreen();
  }
  await context.document.exitFullscreen();
  return !context.navigator.userActivation.isActive;
}

/**
 * Waits for a message to be sent from an iframe.
 *
 * @param {string} type
 * @returns {Promise<Object>}
 */
function receiveMessage(type) {
  return new Promise((resolve) => {
    window.addEventListener("message", function listener(event) {
      if (typeof event.data !== "string") {
        return;
      }
      const data = JSON.parse(event.data);
      if (data.type === type) {
        window.removeEventListener("message", listener);
        resolve(data);
      }
    });
  });
}

/**
 * Creates an iframe and waits for it to load...
 *
 * @param {String} src
 * @returns {Promise<HTMLIFrameElement>}
 */
async function attachIframe(src, document = window.document) {
  const iframe = document.createElement("iframe");
  await new Promise((resolve) => {
    iframe.addEventListener("load", resolve, { once: true });
    document.body.appendChild(iframe);
    iframe.src = src;
  });
  return iframe;
}
