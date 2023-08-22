// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Log;

/**
 * Provides a fullscreen overlay for immersive Cardboard (VR) mode.
 */
public class CardboardOverlayDelegate implements XrImmersiveOverlay.Delegate {
    private static final String TAG = "CardboardOverlay";
    private static final boolean DEBUG_LOGS = false;
    static final int VR_SYSTEM_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    private Activity mActivity;
    private VrCompositorDelegate mCompositorDelegate;

    public CardboardOverlayDelegate(
            VrCompositorDelegate compositorDelegate, @NonNull Activity activity) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor");
        }

        mActivity = activity;
        mCompositorDelegate = compositorDelegate;
    }

    @Override
    public void prepareToCreateSurfaceView() {
        mCompositorDelegate.setOverlayImmersiveVrMode(true);
    }

    @Override
    public void configureSurfaceView(SurfaceView surfaceView) {}

    @Override
    public void parentAndShowSurfaceView(SurfaceView surfaceView) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "Parenting Surface for AR");
        }

        int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
        mActivity.getWindow().getDecorView().setSystemUiVisibility(flags | VR_SYSTEM_UI_FLAGS);

        ViewGroup parent = (ViewGroup) mActivity.getWindow().findViewById(android.R.id.content);
        parent.addView(surfaceView);
    }

    @Override
    public void onStopUsingSurfaceView() {
        mCompositorDelegate.setOverlayImmersiveVrMode(false);
    }

    @Override
    public void removeAndHideSurfaceView(SurfaceView surfaceView) {
        if (surfaceView == null) {
            return;
        }

        int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
        mActivity.getWindow().getDecorView().setSystemUiVisibility(flags & ~VR_SYSTEM_UI_FLAGS);

        ViewGroup parent = (ViewGroup) surfaceView.getParent();
        if (parent != null) {
            // Remove the surfaceView before changing the parent's visibility, so that we
            // don't trigger any duplicate destruction events.
            parent.removeView(surfaceView);
        }
    }

    @Override
    public void maybeForwardTouchEvent(MotionEvent ev) {}

    @Override
    public int getDesiredOrientation() {
        return Configuration.ORIENTATION_LANDSCAPE;
    }
}
