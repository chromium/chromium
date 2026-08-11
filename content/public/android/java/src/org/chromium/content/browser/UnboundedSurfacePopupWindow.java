// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Build;
import android.view.AttachedSurfaceControl;
import android.view.SurfaceControl;
import android.view.View;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A popup window for hosting unbounded visual surfaces on Android U+. */
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
@JNINamespace("content")
@NullMarked
public class UnboundedSurfacePopupWindow {
    private final WindowAndroid mWindowAndroid;
    private final View mParentView;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private @Nullable SurfaceControl mSurfaceControl;

    @CalledByNative
    private static @Nullable UnboundedSurfacePopupWindow create(
            WindowAndroid parentWindowAndroid, @Nullable View parentView, int x, int y) {
        // Unbounded elements rely on AttachedSurfaceControl.buildReparentTransaction(),
        // which requires Android U (API level 34) or higher.
        // See HTMLElement::showUnboundedElement() in
        // third_party/blink/renderer/core/html/html_element.cc
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return null;
        }
        Context context = parentWindowAndroid.getContext().get();
        if (context == null || parentView == null) {
            return null;
        }
        AttachedSurfaceControl attachedSurfaceControl = parentView.getRootSurfaceControl();
        if (attachedSurfaceControl == null) {
            return null;
        }

        SurfaceControl surfaceControl =
                new SurfaceControl.Builder().setName("UnboundedSurfaceControl").build();
        SurfaceControl.Transaction transaction =
                attachedSurfaceControl.buildReparentTransaction(surfaceControl);
        if (transaction == null) {
            surfaceControl.release();
            return null;
        }

        transaction.setVisibility(surfaceControl, true);
        transaction.apply();
        transaction.close();

        WindowAndroid windowAndroid = new WindowAndroid(context, false);
        UnboundedSurfacePopupWindow popupWindow =
                new UnboundedSurfacePopupWindow(windowAndroid, parentView, surfaceControl);
        popupWindow.updatePosition(x, y);
        return popupWindow;
    }

    private UnboundedSurfacePopupWindow(
            WindowAndroid windowAndroid, View parentView, SurfaceControl surfaceControl) {
        mWindowAndroid = windowAndroid;
        mParentView = parentView;
        mSurfaceControl = surfaceControl;
    }

    @CalledByNative
    private @Nullable SurfaceControl getSurfaceControl() {
        return mSurfaceControl;
    }

    @CalledByNative
    private WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    private void updatePosition(int x, int y) {
        if (mSurfaceControl == null) {
            return;
        }
        int[] viewOriginInSurface = new int[2];
        mParentView.getLocationInSurface(viewOriginInSurface);
        SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
        transaction.setPosition(
                mSurfaceControl, x + viewOriginInSurface[0], y + viewOriginInSurface[1]);
        transaction.apply();
        transaction.close();
    }

    @CalledByNative
    private void resize(int x, int y) {
        updatePosition(x, y);
    }

    @CalledByNative
    private void dismissPopup() {
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        if (mSurfaceControl != null) {
            SurfaceControl.Transaction transaction = new SurfaceControl.Transaction();
            transaction.reparent(mSurfaceControl, null);
            transaction.apply();
            transaction.close();
            mSurfaceControl.release();
            mSurfaceControl = null;
        }
        mWindowAndroid.destroy();
    }
}
