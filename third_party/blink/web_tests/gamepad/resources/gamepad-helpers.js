'use strict';

// TODO(crbug.com/146285): Allow more than 4 connected gamepads.
var MAX_GAMEPADS = 4;

function disconnectGamepads() {
    // Simulate disconnecting all gamepads.
    for (let i = 0; i < MAX_GAMEPADS; ++i) {
        gamepadController.disconnect(i);
    }
}

function connectGamepads(gamepadCount) {
    // Simulate connecting |gamepadCount| gamepads.
    assert_less_than_equal(gamepadCount, MAX_GAMEPADS, 'too many gamepads');
    for (let i = 0; i < gamepadCount; ++i) {
        // Simulate a connected gamepad at index i with:
        // * id 'MockStick 3000'
        // * two buttons (values 1.0/pressed, 0.0/unpressed)
        // * two axes (values 0.5, -1.0)
        gamepadController.connect(i);
        gamepadController.setId(i, "MockStick 3000");
        gamepadController.setButtonCount(i, 2);
        gamepadController.setAxisCount(i, 2);
        gamepadController.setButtonData(i, 0, 1);
        gamepadController.setButtonData(i, 1, 0);
        gamepadController.setAxisData(i, 0, .5);
        gamepadController.setAxisData(i, 1, -1.0);
        gamepadController.dispatchConnected(i);
    }
}

function testGamepadStateAllDisconnected() {
    // To pass this test, the getGamepads array should have only null elements.
    let pads = navigator.getGamepads();
    // According to the spec, the length of the array returned by getGamepads
    // must be one greater than the maximum index of Gamepad objects in the
    // array. It does not specify the size when there are no objects in the
    // array. The current behavior in Chrome is to return an array with length
    // equal to the maximum number of connected gamepads, and to fill all unused
    // slots with null.
    assert_equals(pads.length, MAX_GAMEPADS, 'pads.length');
    for (let i = 0; i < pads.length; ++i) {
        assert_equals(pads[i], null);
    }
}

async function onGamepadEvent(expectedEvent) {
  await new Promise(resolve => {
    window.addEventListener(expectedEvent, resolve, { once: true });
  });

  // The gamepadController test API can be used to update gamepad state inside
  // a gamepad listener, which is normally not possible and may cause updates
  // to be delayed until the listener has exited. To avoid this in tests,
  // schedule the promise to resolve after a zero-length timeout.
  return new Promise(resolve => setTimeout(resolve, 0));
}

async function onGamepadEventWithIndex(expectedEvent, gamepadIndex) {
    // Wait until the expected event is received from the correct gamepad. The
    // listener is removed before resolving.
    await new Promise(resolve => {
        let listener = (e) => {
            if (e.gamepad.index == gamepadIndex) {
                window.removeEventListener(expectedEvent, listener);
                resolve();
            }
        };
        window.addEventListener(expectedEvent, listener);
    });

    // The gamepadController test API can be used to update gamepad state inside
    // a gamepad listener, which is normally not possible and may cause updates
    // to be delayed until the listener has exited. To avoid this in tests,
    // schedule the promise to resolve after a zero-length timeout.
    return new Promise(resolve => setTimeout(resolve, 0));
}

async function ongamepadconnected() {
  return onGamepadEvent('gamepadconnected');
}

async function ongamepaddisconnected() {
  return onGamepadEvent('gamepaddisconnected');
}
