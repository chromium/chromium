// Returns a Promise that gets resolved with `event.data` when `window` receives from `source` a
// "message" event whose `event.data.type` matches the string `message_data_type`.
function getMessageData(message_data_type, source) {
    return new Promise(resolve => {
        function waitAndRemove(e) {
            if (e.source != source || !e.data || e.data.type != message_data_type)
                return;
            window.removeEventListener("message", waitAndRemove);
            resolve(e.data);
        }
        window.addEventListener("message", waitAndRemove);
    });
}

// A helper that simulates user activation on the current frame if `activate` is true, then posts
// `message` to `frame` with the target `origin` and specified `capability` to delegate. This helper
// awaits and returns the result message sent in reply from `frame`.
async function postCapabilityDelegationMessage(frame, message, origin, capability, activate) {
    let result_promise = getMessageData("result", frame);
    if (activate)
        await test_driver.bless();
    frame.postMessage(message, {targetOrigin: origin, delegate: capability});
    return await result_promise;
}
