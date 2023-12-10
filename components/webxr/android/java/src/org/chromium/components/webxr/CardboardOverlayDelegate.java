// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.Gravity;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.PopupMenu;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.content_public.browser.LoadUrlParams;

/** Provides a fullscreen overlay for immersive Cardboard (VR) mode. */
public class CardboardOverlayDelegate
        implements XrImmersiveOverlay.Delegate, PopupMenu.OnMenuItemClickListener {
    private static final String TAG = "CardboardOverlay";
    private static final String PRODUCT_SAFETY_URL = "google.com/get/cardboard/product-safety";
    private static final boolean DEBUG_LOGS = false;
    static final int VR_SYSTEM_UI_FLAGS =
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    private Activity mActivity;
    private VrCompositorDelegate mCompositorDelegate;

    private View mCardboardView;

    public CardboardOverlayDelegate(
            VrCompositorDelegate compositorDelegate, @NonNull Activity activity) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor");
        }

        mActivity = activity;
        mCompositorDelegate = compositorDelegate;
    }

    private void setupWidgetsLayout() {
        mCardboardView = mActivity.getLayoutInflater().inflate(R.layout.cardboard_ui, null);

        // Close button.
        ImageButton closeButton = mCardboardView.findViewById(R.id.cardboard_ui_back_button);
        closeButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        XrSessionCoordinator.endActiveSession();
                    }
                });

        // Settings button.
        ImageButton settingsButton = mCardboardView.findViewById(R.id.cardboard_ui_settings_button);
        settingsButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        showSettings(v);
                    }
                });
    }

    /**
     * Returns the view which contains the Content and Controls of the underlying Chrome instance
     * which are hidden when in VR.
     */
    private View getContentView() {
        return mActivity.getWindow().findViewById(android.R.id.content);
    }

    /**
     * Returns the view that should be used as the parent root for the cardboard UI. Must not be
     * below the content view.
     */
    private ViewGroup getParentView() {
        return (ViewGroup) getContentView().getParent();
    }

    public void showSettings(View view) {
        PopupMenu popup =
                new PopupMenu(mActivity, view, Gravity.END, 0, R.style.CardboardSettingsPopupMenu);
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
        } else if (item.getItemId() == R.id.cardboard_menu_option_product_safety) {
            LoadUrlParams url = new LoadUrlParams(PRODUCT_SAFETY_URL);
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
        getParentView().addView(mCardboardView);
    }

    @Override
    public void configureSurfaceView(SurfaceView surfaceView) {}

    @Override
    public void parentAndShowSurfaceView(SurfaceView surfaceView) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "Parenting Surface for AR");
        }

        // We need to hide the Content View from Accessibility while we are in VR, otherwise the
        // tools will try to go through all of the tab control and web content rather than just the
        // controls that are actually visible. Note that we also need to hide descendants.
        getContentView()
                .setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);

        int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
        mActivity.getWindow().getDecorView().setSystemUiVisibility(flags | VR_SYSTEM_UI_FLAGS);

        FrameLayout surface_view_holder =
                (FrameLayout) mCardboardView.findViewById(R.id.surface_view_holder);
        surface_view_holder.addView(surfaceView);
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

        // Restore the accessibility state of the underlying Chrome content when we stop covering it
        // with the Cardboard view.
        getContentView().setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);

        int flags = mActivity.getWindow().getDecorView().getSystemUiVisibility();
        mActivity.getWindow().getDecorView().setSystemUiVisibility(flags & ~VR_SYSTEM_UI_FLAGS);

        ViewGroup parent = getParentView();
        if (parent != null) {
            parent.removeView(mCardboardView);
        }
    }

    @Override
    public void maybeForwardTouchEvent(MotionEvent ev) {}

    @Override
    public int getDesiredOrientation() {
        return Configuration.ORIENTATION_LANDSCAPE;
    }

    @Override
    public boolean useDisplaySizes() {
        // When in VR, it is expected to occupy only the safe area taking into account the notch
        // of the device.
        return false;
    }
}
