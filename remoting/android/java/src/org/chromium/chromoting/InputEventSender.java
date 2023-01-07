// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.chromoting.jni.TouchEventData;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * A set of functions to send users' activities, which are represented by Android classes, to
 * remote host machine. This class uses a {@link InputStub} to do the real injections.
 */
public final class InputEventSender {
    private static final int[] CTRL_ALT_DEL = {
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_FORWARD_DEL,
    };

    private final InputStub mInjector;

    /** Set of pressed keys for which we've sent TextEvent. */
    private final Set<Integer> mPressedTextKeys;

    public InputEventSender(InputStub injector) {
        Preconditions.notNull(injector);
        mInjector = injector;
        mPressedTextKeys = new TreeSet<>();
    }

    public void sendMouseEvent(PointF pos, int button, boolean down) {
        Preconditions.isTrue(button == InputStub.BUTTON_UNDEFINED
                || button == InputStub.BUTTON_LEFT
                || button == InputStub.BUTTON_MIDDLE
                || button == InputStub.BUTTON_RIGHT);
        mInjector.sendMouseEvent((int) pos.x, (int) pos.y, button, down);
    }

    public void sendMouseDown(PointF pos, int button) {
        sendMouseEvent(pos, button, true);
    }

    public void sendMouseUp(PointF pos, int button) {
        sendMouseEvent(pos, button, false);
    }

    public void sendMouseClick(PointF pos, int button) {
        sendMouseDown(pos, button);
        sendMouseUp(pos, button);
    }

    public void sendCursorMove(PointF pos) {
        sendMouseUp(pos, InputStub.BUTTON_UNDEFINED);
    }

    // TODO(zijiehe): This function will be eventually removed after {@link InputStrategyInterface}
    // has been deprecated.
    public void sendCursorMove(float x, float y) {
        sendCursorMove(new PointF(x, y));
    }

    public void sendMouseWheelEvent(float distanceX, float distanceY) {
        mInjector.sendMouseWheelEvent((int) distanceX, (int) distanceY);
    }

    public void sendReverseMouseWheelEvent(float distanceX, float distanceY) {
        sendMouseWheelEvent(-distanceX, -distanceY);
    }

    /**
     * Extracts the touch point data from a MotionEvent, converts each point into a marshallable
     * object and passes the set of points to the JNI layer to be transmitted to the remote host.
     *
     * @param event The event to send to the remote host for injection.  NOTE: This object must be
     *              updated to represent the remote machine's coordinate system before calling this
     *              function.
     */
    public void sendTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        @TouchEventData.EventType
        int touchEventType = TouchEventData.eventTypeFromMaskedAction(action);
        List<TouchEventData> touchEventList = new ArrayList<TouchEventData>();

        if (action == MotionEvent.ACTION_MOVE) {
            // In order to process all of the events associated with an ACTION_MOVE event, we need
            // to walk the list of historical events in order and add each event to our list, then
            // retrieve the current move event data.
            int pointerCount = event.getPointerCount();
            int historySize = event.getHistorySize();
            for (int h = 0; h < historySize; ++h) {
                for (int p = 0; p < pointerCount; ++p) {
                    touchEventList.add(new TouchEventData(event.getPointerId(p),
                            event.getHistoricalX(p, h), event.getHistoricalY(p, h),
                            event.getHistoricalSize(p, h), event.getHistoricalSize(p, h),
                            event.getHistoricalOrientation(p, h),
                            event.getHistoricalPressure(p, h)));
                }
            }

            for (int p = 0; p < pointerCount; p++) {
                touchEventList.add(new TouchEventData(event.getPointerId(p), event.getX(p),
                        event.getY(p), event.getSize(p), event.getSize(p), event.getOrientation(p),
                        event.getPressure(p)));
            }
        } else {
            // For all other events, we only want to grab the current/active pointer.  The event
            // contains a list of every active pointer but passing all of of these to the host can
            // cause confusion on the remote OS side and result in broken touch gestures.
            int activePointerIndex = event.getActionIndex();
            touchEventList.add(new TouchEventData(event.getPointerId(activePointerIndex),
                    event.getX(activePointerIndex), event.getY(activePointerIndex),
                    event.getSize(activePointerIndex), event.getSize(activePointerIndex),
                    event.getOrientation(activePointerIndex),
                    event.getPressure(activePointerIndex)));
        }

        if (!touchEventList.isEmpty()) {
            mInjector.sendTouchEvent(touchEventType, touchEventList.toArray(new TouchEventData[0]));
        }
    }

    /**
     * Converts the {@link KeyEvent} into low-level events and sends them to the host as either
     * key-events or text-events. This contains some logic for handling some special keys, and
     * avoids sending a key-up event for a key that was previously injected as a text-event.
     */
    public boolean sendKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        boolean pressed = event.getAction() == KeyEvent.ACTION_DOWN;

        // Events received from software keyboards generate TextEvent in two
        // cases:
        //   1. This is an ACTION_MULTIPLE event.
        //   2. Ctrl, Alt and Meta are not pressed.
        // This ensures that on-screen keyboard always injects input that
        // correspond to what user sees on the screen, while physical keyboard
        // acts as if it is connected to the remote host.
        if (event.getAction() == KeyEvent.ACTION_MULTIPLE) {
            mInjector.sendTextEvent(event.getCharacters());
            return true;
        }

        // For Enter getUnicodeChar() returns 10 (line feed), but we still
        // want to send it as KeyEvent.
        int unicode = keyCode != KeyEvent.KEYCODE_ENTER ? event.getUnicodeChar() : 0;

        boolean no_modifiers =
                !event.isAltPressed() && !event.isCtrlPressed() && !event.isMetaPressed();

        if (pressed && unicode != 0 && no_modifiers) {
            mPressedTextKeys.add(keyCode);
            int[] codePoints = {unicode};
            mInjector.sendTextEvent(new String(codePoints, 0, 1));
            return true;
        }

        if (!pressed && mPressedTextKeys.contains(keyCode)) {
            mPressedTextKeys.remove(keyCode);
            return true;
        }

        switch (keyCode) {
            // KEYCODE_AT, KEYCODE_POUND, KEYCODE_STAR and KEYCODE_PLUS are
            // deprecated, but they still need to be here for older devices and
            // third-party keyboards that may still generate these events. See
            // https://source.android.com/devices/input/keyboard-devices.html#legacy-unsupported-keys
            case KeyEvent.KEYCODE_AT:
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_2, pressed);
                return true;

            case KeyEvent.KEYCODE_POUND:
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_3, pressed);
                return true;

            case KeyEvent.KEYCODE_STAR:
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_8, pressed);
                return true;

            case KeyEvent.KEYCODE_PLUS:
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_SHIFT_LEFT, pressed);
                mInjector.sendKeyEvent(0, KeyEvent.KEYCODE_EQUALS, pressed);
                return true;

            default:
                // We try to send all other key codes to the host directly.
                return mInjector.sendKeyEvent(0, keyCode, pressed);
        }
    }

    public void sendKeyDown(int keyCode) {
        mInjector.sendKeyEvent(0, keyCode, true);
    }

    public void sendKeyUp(int keyCode) {
        mInjector.sendKeyEvent(0, keyCode, false);
    }

    /**
     * Sends key combinations such as Ctrl-Alt-Del. This injects a key-down event for every key in
     * the list, and then injects the corresponding key-up events. If null or an empty |keyCodes|
     * is passed, nothing will be injected.
     */
    public void sendKeysPress(int[] keyCodes) {
        if (keyCodes != null && keyCodes.length > 0) {
            for (int keyCode : keyCodes) {
                sendKeyDown(keyCode);
            }
            for (int keyCode : keyCodes) {
                sendKeyUp(keyCode);
            }
        }
    }

    public void sendCtrlAltDel() {
        sendKeysPress(CTRL_ALT_DEL);
    }
}
