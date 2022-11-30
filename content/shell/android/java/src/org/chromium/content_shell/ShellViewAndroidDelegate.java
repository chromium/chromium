// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.graphics.Bitmap;
import android.view.ViewGroup;

import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.mojom.CursorType;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for content shell.
 * Extended for testing.
 */
public class ShellViewAndroidDelegate extends ViewAndroidDelegate {
    /**
     * An interface delegates a {@link CallbackHelper} for cursor update. see more in {@link
     * ContentViewPointerTypeTest.OnCursorUpdateHelperImpl}.
     */
    public interface OnCursorUpdateHelper {
        /**
         * Record the last notifyCalled pointer type, see more {@link CallbackHelper#notifyCalled}.
         * @param type The pointer type of the notifyCalled.
         */
        void notifyCalled(int type);
    }

    private OnCursorUpdateHelper mOnCursorUpdateHelper;

    public ShellViewAndroidDelegate(ViewGroup containerView) {
        super(containerView);
    }

    public void setOnCursorUpdateHelper(OnCursorUpdateHelper helper) {
        mOnCursorUpdateHelper = helper;
    }

    public OnCursorUpdateHelper getOnCursorUpdateHelper() {
        return mOnCursorUpdateHelper;
    }

    @Override
    public void onCursorChangedToCustom(Bitmap customCursorBitmap, int hotspotX, int hotspotY) {
        super.onCursorChangedToCustom(customCursorBitmap, hotspotX, hotspotY);
        if (mOnCursorUpdateHelper != null) {
            mOnCursorUpdateHelper.notifyCalled(CursorType.CUSTOM);
        }
    }

    @Override
    public void onCursorChanged(int cursorType) {
        super.onCursorChanged(cursorType);
        if (mOnCursorUpdateHelper != null) {
            mOnCursorUpdateHelper.notifyCalled(cursorType);
        }
    }
}
