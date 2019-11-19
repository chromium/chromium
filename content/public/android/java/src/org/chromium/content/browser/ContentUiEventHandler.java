// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventForwarder;

/**
 * Called from native to handle UI events that need access to various Java layer
 * content components.
 */
@JNINamespace("content")
public class ContentUiEventHandler implements UserData {
    private final WebContentsImpl mWebContents;
    private InternalAccessDelegate mEventDelegate;
    private long mNativeContentUiEventHandler;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ContentUiEventHandler> INSTANCE =
                ContentUiEventHandler::new;
    }

    public static ContentUiEventHandler fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(ContentUiEventHandler.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    public ContentUiEventHandler(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        mNativeContentUiEventHandler =
                ContentUiEventHandlerJni.get().init(ContentUiEventHandler.this, webContents);
    }

    public void setEventDelegate(InternalAccessDelegate delegate) {
        mEventDelegate = delegate;
    }

    private EventForwarder getEventForwarder() {
        return mWebContents.getEventForwarder();
    }

    // Returns the scaling being applied to the event's source. Typically only used for VR when
    // drawing Android UI to a texture.
    private float getEventSourceScaling() {
        return mWebContents.getTopLevelNativeWindow().getDisplay().getAndroidUIScaling();
    }

    @CalledByNative
    private boolean onGenericMotionEvent(MotionEvent event) {
        if (Gamepad.from(mWebContents).onGenericMotionEvent(event)) return true;
        if (JoystickHandler.fromWebContents(mWebContents).onGenericMotionEvent(event)) return true;
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_SCROLL:
                    onMouseWheelEvent(event);
                    return true;
                case MotionEvent.ACTION_BUTTON_PRESS:
                case MotionEvent.ACTION_BUTTON_RELEASE:
                    // TODO(mustaq): Should we include MotionEvent.TOOL_TYPE_STYLUS here?
                    // https://crbug.com/592082
                    if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
                        return onMouseEvent(event);
                    }
            }
        }
        return mEventDelegate.super_onGenericMotionEvent(event);
    }

    private void onMouseWheelEvent(MotionEvent event) {
        assert mNativeContentUiEventHandler != 0;
        float scale = getEventSourceScaling();
        ContentUiEventHandlerJni.get().sendMouseWheelEvent(mNativeContentUiEventHandler,
                ContentUiEventHandler.this, event.getEventTime(), event.getX() / scale,
                event.getY() / scale, event.getAxisValue(MotionEvent.AXIS_HSCROLL),
                event.getAxisValue(MotionEvent.AXIS_VSCROLL));
    }

    private boolean onMouseEvent(MotionEvent event) {
        assert mNativeContentUiEventHandler != 0;
        EventForwarder eventForwarder = mWebContents.getEventForwarder();
        boolean didOffsetEvent = false;
        MotionEvent newEvent = eventForwarder.createOffsetMotionEventIfNeeded(event);
        if (newEvent != event) {
            didOffsetEvent = true;
            event = newEvent;
        }
        float scale = getEventSourceScaling();
        ContentUiEventHandlerJni.get().sendMouseEvent(mNativeContentUiEventHandler,
                ContentUiEventHandler.this, event.getEventTime(), event.getActionMasked(),
                event.getX() / scale, event.getY() / scale, event.getPointerId(0),
                event.getPressure(0), event.getOrientation(0),
                event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                EventForwarder.getMouseEventActionButton(event), event.getButtonState(),
                event.getMetaState(), event.getToolType(0));
        if (didOffsetEvent) event.recycle();
        return true;
    }

    @CalledByNative
    private boolean onKeyUp(int keyCode, KeyEvent event) {
        return mEventDelegate.super_onKeyUp(keyCode, event);
    }

    @CalledByNative
    private boolean dispatchKeyEvent(KeyEvent event) {
        if (Gamepad.from(mWebContents).dispatchKeyEvent(event)) return true;
        if (!shouldPropagateKeyEvent(event)) {
            return mEventDelegate.super_dispatchKeyEvent(event);
        }

        if (ImeAdapterImpl.fromWebContents(mWebContents).dispatchKeyEvent(event)) return true;

        return mEventDelegate.super_dispatchKeyEvent(event);
    }

    /**
     * Check whether a key should be propagated to the embedder or not.
     * We need to send almost every key to Blink. However:
     * 1. We don't want to block the device on the renderer for
     * some keys like menu, home, call.
     * 2. There are no WebKit equivalents for some of these keys
     * (see app/keyboard_codes_win.h)
     * Note that these are not the same set as KeyEvent.isSystemKey:
     * for instance, AKEYCODE_MEDIA_* will be dispatched to webkit*.
     */
    private static boolean shouldPropagateKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_HOME
                || keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_CALL
                || keyCode == KeyEvent.KEYCODE_ENDCALL || keyCode == KeyEvent.KEYCODE_POWER
                || keyCode == KeyEvent.KEYCODE_HEADSETHOOK || keyCode == KeyEvent.KEYCODE_CAMERA
                || keyCode == KeyEvent.KEYCODE_FOCUS || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_MUTE
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            return false;
        }
        return true;
    }

    /**
     * @see View#scrollBy(int, int)
     * Currently the ContentView scrolling happens in the native side. In
     * the Java view system, it is always pinned at (0, 0). scrollBy() and scrollTo()
     * are overridden, so that View's mScrollX and mScrollY will be unchanged at
     * (0, 0). This is critical for drawing ContentView correctly.
     */
    @CalledByNative
    private void scrollBy(float dxPix, float dyPix) {
        if (dxPix == 0 && dyPix == 0) return;
        long time = SystemClock.uptimeMillis();
        // It's a very real (and valid) possibility that a fling may still
        // be active when programatically scrolling. Cancelling the fling in
        // such cases ensures a consistent gesture event stream.
        if (GestureListenerManagerImpl.fromWebContents(mWebContents).hasActiveFlingScroll()) {
            ContentUiEventHandlerJni.get().cancelFling(
                    mNativeContentUiEventHandler, ContentUiEventHandler.this, time);
        }
        ContentUiEventHandlerJni.get().sendScrollEvent(
                mNativeContentUiEventHandler, ContentUiEventHandler.this, time, dxPix, dyPix);
    }

    @CalledByNative
    private void scrollTo(float xPix, float yPix) {
        final float xCurrentPix = mWebContents.getRenderCoordinates().getScrollXPix();
        final float yCurrentPix = mWebContents.getRenderCoordinates().getScrollYPix();
        final float dxPix = xPix - xCurrentPix;
        final float dyPix = yPix - yCurrentPix;
        scrollBy(dxPix, dyPix);
    }

    @NativeMethods
    interface Natives {
        long init(ContentUiEventHandler caller, WebContents webContents);
        void sendMouseWheelEvent(long nativeContentUiEventHandler, ContentUiEventHandler caller,
                long timeMs, float x, float y, float ticksX, float ticksY);
        void sendMouseEvent(long nativeContentUiEventHandler, ContentUiEventHandler caller,
                long timeMs, int action, float x, float y, int pointerId, float pressure,
                float orientation, float tilt, int changedButton, int buttonState, int metaState,
                int toolType);
        void sendScrollEvent(long nativeContentUiEventHandler, ContentUiEventHandler caller,
                long timeMs, float deltaX, float deltaY);
        void cancelFling(
                long nativeContentUiEventHandler, ContentUiEventHandler caller, long timeMs);
    }
}
