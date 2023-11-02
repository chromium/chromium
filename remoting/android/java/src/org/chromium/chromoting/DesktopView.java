// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.chromoting.jni.Client;

/**
 * The class for viewing and interacting with a specific remote host.
 */
public final class DesktopView extends SurfaceView {

    private final Event.Raisable<TouchEventParameter> mOnTouch = new Event.Raisable<>();

    /** The parent Desktop activity. */
    private Desktop mDesktop;

    private TouchInputHandler mInputHandler;

    private InputEventSender mInputEventSender;

    public DesktopView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        // Give this view keyboard focus, allowing us to customize the soft keyboard's settings.
        setFocusableInTouchMode(true);
    }

    /**
     * Initializes the view.
     */
    public void init(Client client, Desktop desktop, RenderStub renderStub) {
        Preconditions.isNull(mDesktop);
        Preconditions.isNull(mInputHandler);
        Preconditions.isNull(mInputEventSender);
        Preconditions.notNull(desktop);
        Preconditions.notNull(renderStub);

        mDesktop = desktop;
        mInputEventSender = new InputEventSender(client);
        renderStub.setDesktopView(this);
        mInputHandler = new TouchInputHandler(this, mDesktop, renderStub, mInputEventSender);
    }

    /**
     * Destroys the view. Should be called in {@link android.app.Activity#onDestroy()}.
     */
    public void destroy() {
        mInputHandler.detachEventListeners();
    }

    /** An {@link Event} which is triggered when user touches the screen. */
    public final Event<TouchEventParameter> onTouch() {
        return mOnTouch;
    }

    /** Called when a software keyboard is requested, and specifies its options. */
    @Override
    public final InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Disables rich input support and instead requests simple key events.
        outAttrs.inputType = InputType.TYPE_NULL;

        // Prevents most third-party IMEs from ignoring our Activity's adjustResize preference.
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;

        // Ensures that keyboards will not decide to hide the remote desktop on small displays.
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI;

        // Stops software keyboards from closing as soon as the enter key is pressed.
        outAttrs.imeOptions |= EditorInfo.IME_MASK_ACTION | EditorInfo.IME_FLAG_NO_ENTER_ACTION;

        return null;
    }

    /** Called whenever the user attempts to touch the canvas. */
    @Override
    public final boolean onTouchEvent(MotionEvent event) {
        TouchEventParameter parameter = new TouchEventParameter(event);
        mOnTouch.raise(parameter);
        return parameter.handled;
    }
}
