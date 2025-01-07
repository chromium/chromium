// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.graphics.Rect;
import android.os.Build;
import android.view.AttachedSurfaceControl;
import android.view.SurfaceControl;
import android.view.View;
import android.widget.Magnifier;

import androidx.annotation.RequiresApi;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content.browser.webcontents.WebContentsImpl;

/**
 * Magnifier implementation using GPU/viz compositor and surface control.
 * This has the benefit that surface control does not need to be turned off, though
 * there are some small visual differences.
 */
@RequiresApi(Build.VERSION_CODES.TIRAMISU)
@JNINamespace("content")
public class MagnifierSurfaceControl implements MagnifierWrapper {
    // Shadows are implemented as linear gradients with the same rounded corner as the main
    // content. Values are in device independent pixels, and converted to pixels at run time.
    // Shadow height is amount that shadow extends above and below the magnifier; they increase
    // the size of surface control.
    private static final int TOP_SHADOW_HEIGHT_DP = 3;
    private static final int BOTTOM_SHADOW_HEIGHT_DP = 6;
    // The bottom shadow is shrunk horizontally by this amount each side.
    private static final int BOTTOM_SHADOW_WIDTH_REDUCTION_DP = 3;

    private long mNativeMagnifierSurfaceControl;

    private final WebContentsImpl mWebContents;
    private final SelectionPopupControllerImpl.ReadbackViewCallback mViewCallback;
    private View mView;
    private int mWidthPx;
    private int mHeightPx;
    private int mVerticalOffsetPx;
    private SurfaceControl mSurfaceControl;
    private SurfaceControl.Transaction mTransaction;

    public MagnifierSurfaceControl(
            WebContentsImpl webContents,
            SelectionPopupControllerImpl.ReadbackViewCallback callback) {
        mWebContents = webContents;
        mViewCallback = callback;
    }

    @Override
    public void show(float x, float y) {
        Rect localVisibleRect = new Rect();
        if (!getView().getLocalVisibleRect(localVisibleRect)) {
            dismiss();
            return;
        }

        createNativeIfNeeded();
        if (mSurfaceControl != null) {
            x = x - mWidthPx / 2f;
            y = y - mHeightPx / 2f;
            float readback_y = y;
            y = y + mVerticalOffsetPx;

            y = y - scaleByDeviceFactor(TOP_SHADOW_HEIGHT_DP);

            // Clamp to localVisibleRect.
            x = Math.max(x, localVisibleRect.left);
            y = Math.max(y, localVisibleRect.top);
            x = Math.min(x, localVisibleRect.right - mWidthPx);
            y = Math.min(y, localVisibleRect.bottom - mHeightPx);

            readback_y = readback_y - mWebContents.getRenderCoordinates().getContentOffsetYPix();
            MagnifierSurfaceControlJni.get()
                    .setReadbackOrigin(mNativeMagnifierSurfaceControl, x, readback_y);

            int[] viewOriginInSurface = new int[2];
            getView().getLocationInSurface(viewOriginInSurface);
            mTransaction.setPosition(
                    mSurfaceControl, x + viewOriginInSurface[0], y + viewOriginInSurface[1]);
            mTransaction.apply();
        }
    }

    @Override
    public void dismiss() {
        destroyNativeIfNeeded();
    }

    @Override
    public boolean isAvailable() {
        return mViewCallback.getReadbackView() != null;
    }

    @Override
    public void childLocalSurfaceIdChanged() {
        if (mNativeMagnifierSurfaceControl == 0) return;
        MagnifierSurfaceControlJni.get().childLocalSurfaceIdChanged(mNativeMagnifierSurfaceControl);
    }

    private void createNativeIfNeeded() {
        if (mNativeMagnifierSurfaceControl != 0) return;
        if (getView() == null) return;
        AttachedSurfaceControl attachedSurfaceControl = getView().getRootSurfaceControl();
        if (attachedSurfaceControl == null) return;

        SurfaceControl surfaceControl =
                new SurfaceControl.Builder().setName("cr_magnifier").build();
        SurfaceControl.Transaction attachTransaction =
                attachedSurfaceControl.buildReparentTransaction(surfaceControl);
        if (attachTransaction == null) {
            surfaceControl.release();
            return;
        }
        attachTransaction.setVisibility(surfaceControl, true);

        float cornerRadius;
        float zoom;
        {
            Magnifier androidMagnifier = new Magnifier(getView());
            mWidthPx = androidMagnifier.getWidth();
            mHeightPx = androidMagnifier.getHeight();
            mVerticalOffsetPx = androidMagnifier.getDefaultVerticalSourceToMagnifierOffset();
            cornerRadius = androidMagnifier.getCornerRadius();
            zoom = androidMagnifier.getZoom();
            androidMagnifier.dismiss();
        }

        float density = getView().getResources().getDisplayMetrics().density;
        mNativeMagnifierSurfaceControl =
                MagnifierSurfaceControlJni.get()
                        .create(
                                mWebContents,
                                surfaceControl,
                                density,
                                mWidthPx,
                                mHeightPx,
                                cornerRadius,
                                zoom,
                                scaleByDeviceFactor(TOP_SHADOW_HEIGHT_DP),
                                scaleByDeviceFactor(BOTTOM_SHADOW_HEIGHT_DP),
                                scaleByDeviceFactor(BOTTOM_SHADOW_WIDTH_REDUCTION_DP));
        mSurfaceControl = surfaceControl;
        mTransaction = attachTransaction;
    }

    private void destroyNativeIfNeeded() {
        if (mNativeMagnifierSurfaceControl != 0) {
            MagnifierSurfaceControlJni.get().destroy(mNativeMagnifierSurfaceControl);
        }
        mNativeMagnifierSurfaceControl = 0;
        if (mSurfaceControl != null) {
            mTransaction.reparent(mSurfaceControl, null);
            mTransaction.apply();
            mTransaction.close();
            mSurfaceControl.release();
        }
        mSurfaceControl = null;
        mTransaction = null;
        mView = null;
    }

    private View getView() {
        if (mView == null) {
            mView = mViewCallback.getReadbackView();
        }
        return mView;
    }

    private int scaleByDeviceFactor(int value) {
        return (int) (value * mWebContents.getRenderCoordinates().getDeviceScaleFactor());
    }

    @NativeMethods
    interface Natives {
        long create(
                WebContentsImpl webContents,
                SurfaceControl surfaceControl,
                float deviceScale,
                int width,
                int height,
                float cornerRadius,
                float zoom,
                int topShadowHeight,
                int bottomShadowHeight,
                int bottomShadowWidthReduction);

        void destroy(long magnifierSurfaceControl);

        void setReadbackOrigin(long nativeMagnifierSurfaceControl, float x, float y);

        void childLocalSurfaceIdChanged(long nativeMagnifierSurfaceControl);
    }
}
