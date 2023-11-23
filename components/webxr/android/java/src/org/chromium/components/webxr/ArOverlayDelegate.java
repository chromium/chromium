// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;

/** Provides a fullscreen overlay for immersive AR mode. */
public class ArOverlayDelegate implements XrImmersiveOverlay.Delegate {
    private static final String TAG = "ArOverlayDelegate";
    private static final boolean DEBUG_LOGS = false;

    private ArCompositorDelegate mArCompositorDelegate;
    private boolean mUseOverlay;
    private boolean mCanRenderDomContent;
    private boolean mDomSurfaceNeedsConfiguring;
    private WebContents mWebContents;

    public ArOverlayDelegate(
            @NonNull ArCompositorDelegate compositorDelegate,
            final WebContents webContents,
            boolean useOverlay,
            boolean canRenderDomContent) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor");

        mArCompositorDelegate = compositorDelegate;
        mWebContents = webContents;

        mUseOverlay = useOverlay;
        mCanRenderDomContent = canRenderDomContent;
        mDomSurfaceNeedsConfiguring = mUseOverlay && !mCanRenderDomContent;
    }

    @Override
    public void prepareToCreateSurfaceView() {
        if (DEBUG_LOGS) {
            Log.i(TAG, "calling mArCompositorDelegate.setOverlayImmersiveArMode(true)");
        }

        // While it's fine to omit if the page does not use DOMOverlay, once the page does
        // use DOMOverlay, something appears to have changed such that it becomes required,
        // otherwise the DOM SurfaceView will be in front of the XR content. This is done before
        // the surface view is created so that it ends up on top if the Dom Surface did not need
        // configuring.
        mArCompositorDelegate.setOverlayImmersiveArMode(true, mDomSurfaceNeedsConfiguring);
    }

    @Override
    public void configureSurfaceView(SurfaceView surfaceView) {
        surfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);

        // Exactly one surface view needs to call setZOrderMediaOverlay(true) otherwise the
        // resulting z-order is undefined. The DOM content's surface will set it to true if
        // |mDomSurfaceNeedsConfiguring| is set so we need to do the opposite here.
        surfaceView.setZOrderMediaOverlay(!mDomSurfaceNeedsConfiguring);

        if (!mUseOverlay) {
            WebContentsAccessibility.fromWebContents(mWebContents).setObscuredByAnotherView(true);
        }
    }

    @Override
    public void parentAndShowSurfaceView(SurfaceView surfaceView) {
        if (DEBUG_LOGS) Log.i(TAG, "Parenting Surface for AR");
        ViewGroup parent = mArCompositorDelegate.getArSurfaceParent();

        // If we need to toggle the parent's visibility, do it before we add the surfaceView.
        if (mArCompositorDelegate.shouldToggleArSurfaceParentVisibility()) {
            parent.setVisibility(View.VISIBLE);
        }

        parent.addView(surfaceView);
    }

    @Override
    public void onStopUsingSurfaceView() {
        mArCompositorDelegate.setOverlayImmersiveArMode(false, mDomSurfaceNeedsConfiguring);
    }

    @Override
    public void removeAndHideSurfaceView(SurfaceView surfaceView) {
        if (surfaceView == null) return;
        ViewGroup parent = (ViewGroup) surfaceView.getParent();

        if (!mUseOverlay) {
            WebContentsAccessibility.fromWebContents(mWebContents).setObscuredByAnotherView(false);
        }

        if (parent != null) {
            // Remove the surfaceView before changing the parent's visibility, so that we
            // don't trigger any duplicate destruction events.
            parent.removeView(surfaceView);

            if (mArCompositorDelegate.shouldToggleArSurfaceParentVisibility()) {
                parent.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void maybeForwardTouchEvent(MotionEvent ev) {
        // DOM Overlay mode needs to forward the touch to the content view so that its UI elements
        // keep working.
        if (mUseOverlay) {
            mArCompositorDelegate.dispatchTouchEvent(ev);
        }
    }

    @Override
    public int getDesiredOrientation() {
        return Configuration.ORIENTATION_UNDEFINED;
    }

    @Override
    public boolean useDisplaySizes() {
        // When in AR, it is expected to occupy the entire screen even if there is a notch.
        return true;
    }
}
