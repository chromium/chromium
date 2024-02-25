// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImplJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.ui.MotionEventUtils;
import org.chromium.ui.base.EventForwarder;

/** Unit tests for {@link ContentUiEventHandler} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentUiEventHandlerTest {
    private static final int NATIVE_WEB_CONTENTS_ANDROID = 1;
    private static final int NATIVE_CONTENT_UI_EVENT_HANDLER = 2;

    @Mock private NavigationController mNavigationController;
    @Mock private WebContentsImpl.Natives mWebContentsJniMock;
    @Mock private ContentUiEventHandler.Natives mContentUiEventHandlerJniMock;
    @Rule public JniMocker mJniMocker = new JniMocker();

    private ContentUiEventHandler mContentUiEventHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebContentsImplJni.TEST_HOOKS, mWebContentsJniMock);
        mJniMocker.mock(ContentUiEventHandlerJni.TEST_HOOKS, mContentUiEventHandlerJniMock);

        WebContentsImpl webContentsImpl =
                spy(WebContentsImpl.create(NATIVE_WEB_CONTENTS_ANDROID, mNavigationController));
        webContentsImpl.initializeForTesting();

        Gamepad gamepad = mock(Gamepad.class);
        when(gamepad.onGenericMotionEvent(any())).thenReturn(false);
        webContentsImpl.setUserDataForTesting(Gamepad.class, gamepad);

        JoystickHandler joystickHandler = mock(JoystickHandler.class);
        when(joystickHandler.onGenericMotionEvent(any())).thenReturn(false);
        webContentsImpl.setUserDataForTesting(JoystickHandler.class, joystickHandler);

        EventForwarder eventForwarder = mock(EventForwarder.class);
        when(eventForwarder.isTrackpadToMouseEventConversionEnabled()).thenReturn(true);
        when(eventForwarder.createOffsetMotionEventIfNeeded(any()))
                .thenAnswer(
                        (Answer<MotionEvent>)
                                invocation -> {
                                    Object[] args = invocation.getArguments();
                                    return (MotionEvent) args[0];
                                });
        doReturn(eventForwarder).when(webContentsImpl).getEventForwarder();

        mContentUiEventHandler =
                ContentUiEventHandler.createForTesting(
                        webContentsImpl, NATIVE_CONTENT_UI_EVENT_HANDLER);
    }

    @Test
    public void testOnGenericMotionEventSendsTrackpadClicksToNative() {
        MotionEvent trackpadLeftClickEvent = getTrackpadLeftClickEvent();
        mContentUiEventHandler.onGenericMotionEvent(getTrackpadLeftClickEvent());
        verifySendMouseEvent(trackpadLeftClickEvent);

        MotionEvent trackpadRightClickEvent = getTrackRightClickEvent();
        mContentUiEventHandler.onGenericMotionEvent(getTrackRightClickEvent());
        verifySendMouseEvent(trackpadRightClickEvent);
    }

    private void verifySendMouseEvent(MotionEvent event) {
        verify(mContentUiEventHandlerJniMock)
                .sendMouseEvent(
                        NATIVE_CONTENT_UI_EVENT_HANDLER,
                        mContentUiEventHandler,
                        MotionEventUtils.getEventTimeNanos(event),
                        event.getActionMasked(),
                        event.getX(),
                        event.getY(),
                        event.getPointerId(0),
                        event.getPressure(0),
                        event.getOrientation(0),
                        event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(event),
                        event.getButtonState(),
                        event.getMetaState(),
                        MotionEvent.TOOL_TYPE_MOUSE);
    }

    private static MotionEvent getTrackpadLeftClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_PRIMARY);
    }

    private static MotionEvent getTrackRightClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_SECONDARY);
    }

    private static MotionEvent getTrackpadEvent(int action, int buttonState) {
        return MotionEvent.obtain(
                0,
                1,
                action,
                1,
                getToolTypeFingerProperties(),
                getPointerCoords(),
                0,
                buttonState,
                0,
                0,
                0,
                0,
                getTrackpadSource(),
                0);
    }

    private static MotionEvent.PointerProperties[] getToolTypeFingerProperties() {
        MotionEvent.PointerProperties[] pointerPropertiesArray =
                new MotionEvent.PointerProperties[1];
        MotionEvent.PointerProperties trackpadProperties = new MotionEvent.PointerProperties();
        trackpadProperties.id = 7;
        trackpadProperties.toolType = MotionEvent.TOOL_TYPE_FINGER;
        pointerPropertiesArray[0] = trackpadProperties;
        return pointerPropertiesArray;
    }

    private static MotionEvent.PointerCoords[] getPointerCoords() {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = 14;
        coords.y = 21;
        pointerCoordsArray[0] = coords;
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }
}
