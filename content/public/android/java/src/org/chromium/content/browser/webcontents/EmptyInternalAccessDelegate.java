// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.webcontents;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.content_public.browser.ViewEventSink;

/**
 * Empty implementation of {@link ViewEventSink.InternalAccessDelegate}. Intentional no-op for
 * transient stage usage.
 */
public class EmptyInternalAccessDelegate implements ViewEventSink.InternalAccessDelegate {
    @Override
    public boolean super_onKeyUp(int keyCode, KeyEvent event) {
        return false;
    }

    @Override
    public boolean super_dispatchKeyEvent(KeyEvent event) {
        return false;
    }

    @Override
    public boolean super_onGenericMotionEvent(MotionEvent event) {
        return false;
    }

    @Override
    public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {}
}
