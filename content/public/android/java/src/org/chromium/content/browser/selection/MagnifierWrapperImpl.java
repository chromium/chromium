// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.annotation.SuppressLint;
import android.view.View;
import android.widget.Magnifier;

import org.chromium.base.Log;

/** Implements MagnifierWrapper interface. */
@SuppressLint("NewApi") // Magnifier requires API level 28.
public class MagnifierWrapperImpl implements MagnifierWrapper {
    private static final boolean DEBUG = false;
    private static final String TAG = "Magnifier";

    private Magnifier mMagnifier;
    private SelectionPopupControllerImpl.ReadbackViewCallback mCallback;

    /** Constructor. */
    public MagnifierWrapperImpl(SelectionPopupControllerImpl.ReadbackViewCallback callback) {
        mCallback = callback;
    }

    @Override
    public void show(float x, float y) {
        View view = mCallback.getReadbackView();
        if (view == null) return;
        if (mMagnifier == null) mMagnifier = new Magnifier(view);
        if (DEBUG) Log.i(TAG, "show (" + x + ", " + y + ")");
        mMagnifier.show(x, y);
    }

    @Override
    public void dismiss() {
        if (mMagnifier != null) {
            if (DEBUG) Log.i(TAG, "dismiss");
            mMagnifier.dismiss();
            mMagnifier = null;
        }
    }

    @Override
    public boolean isAvailable() {
        return mCallback.getReadbackView() != null;
    }

    @Override
    public void childLocalSurfaceIdChanged() {
        // Intentional not implemented.
    }
}
