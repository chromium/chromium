// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.PopupMenu;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Provides a fullscreen overlay for immersive Cardboard (VR) mode.
 */
public class CardboardOverlayDelegate
        implements XrImmersiveOverlay.Delegate, PopupMenu.OnMenuItemClickListener {
    private static final String TAG = "CardboardOverlay";
    private static final String ABOUT_VR_URL = "google.com/cardboard";
    private static final boolean DEBUG_LOGS = false;
    static final int VR_SYSTEM_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    private Activity mActivity;
    private VrCompositorDelegate mCompositorDelegate;

    private View mWidgetsLayout;

    public CardboardOverlayDelegate(
            VrCompositorDelegate compositorDelegate, @NonNull Activity activity) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor");
        }

        mActivity = activity;
        mCompositorDelegate = compositorDelegate;
    }

    private void setupWidgetsLayout() {
        mWidgetsLayout = mActivity.getLayoutInflater().inflate(R.layout.cardboard_ui, null);

        // Close button.
        ImageButton closeButton = mWidgetsLayout.findViewById(R.id.cardboard_ui_back_button);
        closeButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                XrSessionCoordinator.endActiveSession();
            }
        });

        // Settings button.
        ImageButton settingsButton = mWidgetsLayout.findViewById(R.id.cardboard_ui_settings_button);
        settingsButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showSettings(v);
            }
        });
    }

    private ViewGroup getParentView() {
        return (ViewGroup) mActivity.getWindow().findViewById(android.R.id.content);
    }

    public void showSettings(View view) {
        PopupMenu popup = new PopupMenu(mActivity, view);
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(R.menu.settings_menu, popup.getMenu());
        popup.setOnMenuItemClickListener(this);
        popup.show();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        if (item.getItemId() == R.id.cardboard_menu_option_use_another_device) {
            XrSessionCoordinator.onActiveXrSessionButtonTouched();
            XrSessionCoordinator.endActiveSession();
            return true;
        } else if (item.getItemId() == R.id.cardboard_menu_option_about_vr) {
            LoadUrlParams url = new LoadUrlParams(ABOUT_VR_URL);
            // Storing this value in a new variable as the ending the active
            // session  could clear it otherwise.
            VrCompositorDelegate delegate = mCompositorDelegate;
            XrSessionCoordinator.endActiveSession();
            delegate.openNewTab(url);
            return true;
        }
        return false;
    }

    @Override
    public void prepareToCreateSurfaceView() {
        mCompositorDelegate.setOverlayImmersiveVrMode(true);

        setupWidgetsLayout();
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

        getParentView().addView(surfaceView);
        getParentView().addView(mWidgetsLayout);
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

        if (getParentView() != null) {
            getParentView().removeView(mWidgetsLayout);
        }
    }

    @Override
    public void maybeForwardTouchEvent(MotionEvent ev) {}

    @Override
    public int getDesiredOrientation() {
        return Configuration.ORIENTATION_LANDSCAPE;
    }
}
