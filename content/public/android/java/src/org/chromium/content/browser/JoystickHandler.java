// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.chromium.base.UserData;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;

/** Bridges content and joystick device event conversion and forwarding. */
public class JoystickHandler implements ImeEventObserver, UserData {
    private final EventForwarder mEventForwarder;

    // Whether joystick scroll is enabled.  It's disabled when an editable field is focused.
    private boolean mScrollEnabled = true;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<JoystickHandler> INSTANCE = JoystickHandler::new;
    }

    public static JoystickHandler fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(JoystickHandler.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    /**
     * Creates JoystickHandler instance.
     * @param webContents WebContents instance with which this JoystickHandler is associated.
     */
    private JoystickHandler(WebContents webContents) {
        mEventForwarder = webContents.getEventForwarder();
        ImeAdapterImpl.fromWebContents(webContents).addEventObserver(this);
    }

    public void setScrollEnabled(boolean enabled) {
        mScrollEnabled = enabled;
    }

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        setScrollEnabled(!editable);
    }

    /**
     * Handles joystick input events.
     * @param event {@link MotionEvent} object.
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (!mScrollEnabled || (event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0) {
            return false;
        }
        float velocityX = getVelocityFromJoystickAxis(event, MotionEvent.AXIS_X);
        float velocityY = getVelocityFromJoystickAxis(event, MotionEvent.AXIS_Y);
        if (velocityX == 0.f && velocityY == 0.f) return false;
        mEventForwarder.startFling(event.getEventTime(), velocityX, velocityY, true, true);
        return true;
    }

    /** Removes noise from joystick motion events. */
    private static float getVelocityFromJoystickAxis(MotionEvent event, int axis) {
        final float kJoystickScrollDeadzone = 0.2f;
        float axisValWithNoise = event.getAxisValue(axis);
        if (Math.abs(axisValWithNoise) <= kJoystickScrollDeadzone) return 0f;
        return -axisValWithNoise;
    }
}
