// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.device.gamepad.GamepadList;

/**
 * Encapsulates component class {@link GamepadList} for use in content, with regards to its state
 * according to content being attached to/detached from window.
 */
@NullMarked
class Gamepad implements WindowEventObserver, UserData {

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<Gamepad> INSTANCE = Gamepad::new;
    }

    public static Gamepad from(WebContents webContents) {
        Gamepad ret =
                webContents.getOrSetUserData(Gamepad.class, UserDataFactoryLazyHolder.INSTANCE);
        assert ret != null;
        return ret;
    }

    private Gamepad(WebContents webContents) {
        WindowEventObserverManager.from(webContents).addObserver(this);
    }

    // WindowEventObserver

    @Override
    public void onAttachedToWindow() {
        GamepadList.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        GamepadList.onDetachedFromWindow();
    }

    public boolean onGenericMotionEvent(MotionEvent event) {
        return GamepadList.onGenericMotionEvent(event);
    }

    public boolean dispatchKeyEvent(KeyEvent event) {
        return GamepadList.dispatchKeyEvent(event);
    }
}
