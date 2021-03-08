// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Build;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.widget.Toast;

import java.util.HashMap;
import java.util.Map;

/**
 * Provides a fullscreen overlay for immersive AR mode.
 */
public class ArImmersiveOverlay
        implements SurfaceHolder.Callback2, View.OnTouchListener, ScreenOrientationDelegate {
    private static final String TAG = "ArImmersiveOverlay";
    private static final boolean DEBUG_LOGS = false;

    private ArCoreJavaUtils mArCoreJavaUtils;
    private ArCompositorDelegate mArCompositorDelegate;
    private Activity mActivity;
    private boolean mSurfaceReportedReady;
    private Integer mRestoreOrientation;
    private boolean mCleanupInProgress;
    private SurfaceUiWrapper mSurfaceUi;
    private WebContents mWebContents;

    // Set containing all currently touching pointers.
    private HashMap<Integer, PointerData> mPointerIdToData;
    // ID of primary pointer (if present).
    private Integer mPrimaryPointerId;

    public void show(@NonNull ArCompositorDelegate compositorDelegate,
            @NonNull WebContents webContents, @NonNull ArCoreJavaUtils caller, boolean useOverlay) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor");
        mArCoreJavaUtils = caller;

        mWebContents = webContents;
        mArCompositorDelegate = compositorDelegate;

        mActivity = ArCoreJavaUtils.getActivity(webContents);

        mPointerIdToData = new HashMap<Integer, PointerData>();
        mPrimaryPointerId = null;

        // Choose a concrete implementation to create a drawable Surface and make it fullscreen.
        // It forwards SurfaceHolder callbacks and touch events to this ArImmersiveOverlay object.
        if (useOverlay) {
            mSurfaceUi = new SurfaceUiCompositor();
        } else {
            mSurfaceUi = new SurfaceUiDialog();
        }
    }

    private interface SurfaceUiWrapper {
        public void onSurfaceVisible();
        public void forwardMotionEvent(MotionEvent ev);
        public void destroy();
    }

    private class SurfaceUiDialog implements SurfaceUiWrapper, DialogInterface.OnCancelListener {
        private Toast mNotificationToast;
        private Dialog mDialog;
        // Android supports multiple variants of fullscreen applications. Use fully-immersive
        // "sticky" mode without navigation or status bars, and show a toast with a "pull from top
        // and press back button to exit" prompt.
        private static final int VISIBILITY_FLAGS_IMMERSIVE = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        public SurfaceUiDialog() {
            // Create a fullscreen dialog and use its backing Surface for drawing.
            mDialog = new Dialog(mActivity, android.R.style.Theme_NoTitleBar_Fullscreen);
            mDialog.getWindow().setBackgroundDrawable(null);
            mDialog.getWindow().takeSurface(ArImmersiveOverlay.this);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                // Use maximum fullscreen, ignoring a notch if present. This code path is used
                // for non-DOM-Overlay mode where the browser compositor view isn't visible.
                // In DOM Overlay mode (SurfaceUiCompositor), Blink configures this separately
                // via ViewportData::SetExpandIntoDisplayCutout.
                mDialog.getWindow().setFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                        WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
                mDialog.getWindow().getAttributes().layoutInDisplayCutoutMode =
                        WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            }
            View view = mDialog.getWindow().getDecorView();
            view.setSystemUiVisibility(VISIBILITY_FLAGS_IMMERSIVE);
            view.setOnTouchListener(ArImmersiveOverlay.this);
            view.setKeepScreenOn(true);
            mDialog.setOnCancelListener(this);
            mDialog.getWindow().setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mDialog.show();
        }

        @Override // SurfaceUiWrapper
        public void onSurfaceVisible() {
            if (mNotificationToast != null) {
                mNotificationToast.cancel();
            }
            int resId = R.string.immersive_fullscreen_api_notification;
            mNotificationToast = Toast.makeText(mActivity, resId, Toast.LENGTH_LONG);
            mNotificationToast.setGravity(Gravity.TOP | Gravity.CENTER, 0, 0);
            mNotificationToast.show();
        }

        @Override // SurfaceUiWrapper
        public void forwardMotionEvent(MotionEvent ev) {}

        @Override // SurfaceUiWrapper
        public void destroy() {
            if (mNotificationToast != null) {
                mNotificationToast.cancel();
                mNotificationToast = null;
            }
            mDialog.dismiss();
        }

        @Override // DialogInterface.OnCancelListener
        public void onCancel(DialogInterface dialog) {
            if (DEBUG_LOGS) Log.i(TAG, "onCancel");
            cleanupAndExit();
        }
    }

    private class PointerData {
        public float x;
        public float y;
        public boolean touching;

        public PointerData(float x, float y, boolean touching) {
            this.x = x;
            this.y = y;
            this.touching = touching;
        }
    }

    private class SurfaceUiCompositor implements SurfaceUiWrapper {
        private SurfaceView mSurfaceView;
        private WebContentsObserver mWebContentsObserver;

        @SuppressLint("ClickableViewAccessibility")
        public SurfaceUiCompositor() {
            mSurfaceView = new SurfaceView(mActivity);
            // Keep the camera layer at "default" Z order. Chrome's compositor SurfaceView is in
            // OverlayVideoMode, putting it in front of that, but behind other non-SurfaceView UI.
            mSurfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);
            mSurfaceView.getHolder().addCallback(ArImmersiveOverlay.this);
            mSurfaceView.setKeepScreenOn(true);

            // Process touch input events for XR input. This consumes them, they'll be resent to
            // the compositor view via forwardMotionEvent.
            mSurfaceView.setOnTouchListener(ArImmersiveOverlay.this);

            View content = mActivity.getWindow().findViewById(android.R.id.content);
            ViewGroup group = (ViewGroup) content.getParent();
            group.addView(mSurfaceView);

            // Enable alpha channel for the compositor and make the background
            // transparent. (A variant of CompositorView::SetOverlayVideoMode.)
            if (DEBUG_LOGS) {
                Log.i(TAG, "calling mArCompositorDelegate.setOverlayImmersiveArMode(true)");
            }
            mArCompositorDelegate.setOverlayImmersiveArMode(true);

            mWebContentsObserver = new WebContentsObserver() {
                @Override
                public void hasEffectivelyFullscreenVideoChange(boolean isFullscreen) {
                    if (DEBUG_LOGS) {
                        Log.i(TAG,
                                "hasEffectivelyFullscreenVideoChange(), isFullscreen="
                                        + isFullscreen);
                    }

                    if (!isFullscreen) {
                        cleanupAndExit();
                    }
                }
            };

            // Watch for fullscreen exit triggered from JS, this needs to end the session.
            mWebContents.addObserver(mWebContentsObserver);
        }

        @Override // SurfaceUiWrapper
        public void onSurfaceVisible() {}

        @Override // SurfaceUiWrapper
        public void forwardMotionEvent(MotionEvent ev) {
            mArCompositorDelegate.dispatchTouchEvent(ev);
        }

        @Override // SurfaceUiWrapper
        public void destroy() {
            mWebContents.removeObserver(mWebContentsObserver);
            View content = mActivity.getWindow().findViewById(android.R.id.content);
            ViewGroup group = (ViewGroup) content.getParent();
            group.removeView(mSurfaceView);
            mSurfaceView = null;
            mArCompositorDelegate.setOverlayImmersiveArMode(false);
        }
    }

    @Override // View.OnTouchListener
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouch(View v, MotionEvent ev) {
        // Only forward primary actions, ignore more complex events such as secondary pointer
        // touches. Ignore batching since we're only sending one ray pose per frame.

        if (DEBUG_LOGS) {
            Log.i(TAG,
                    "Received motion event, action: " + MotionEvent.actionToString(ev.getAction())
                            + ", pointer count: " + ev.getPointerCount()
                            + ", action index: " + ev.getActionIndex());
            for (int i = 0; i < ev.getPointerCount(); i++) {
                Log.i(TAG,
                        "Pointer index: " + i + ", id: " + ev.getPointerId(i) + ", coordinates: ("
                                + ev.getX(i) + ", " + ev.getY(i) + ")");
            }
        }

        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_UP
                || action == MotionEvent.ACTION_POINTER_DOWN
                || action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL
                || action == MotionEvent.ACTION_MOVE) {
            // ACTION_DOWN - gesture starts. Pointer with index 0 will be considered as a primary
            // pointer until it's raised. Then, there will be no primary pointer until the
            // gesture ends (ACTION_UP / ACTION_CANCEL).
            if (action == MotionEvent.ACTION_DOWN) {
                int pointerId = ev.getPointerId(0);

                // Remember primary pointer's ID. The start of the gesture is the only time when the
                // primary pointer is set.
                mPrimaryPointerId = pointerId;
                PointerData previousData = mPointerIdToData.put(
                        mPrimaryPointerId, new PointerData(ev.getX(0), ev.getY(0), true));

                if (previousData != null) {
                    // Not much we can do here, just log and continue.
                    Log.w(TAG,
                            "New pointer with ID " + pointerId
                                    + " introduced by ACTION_DOWN when old pointer with the same ID already exists.");
                }

                // Send the events to the device.
                // This needs to happen after we have updated the state.
                sendMotionEvents(false);
            }

            // ACTION_UP - gesture ends.
            // ACTION_CANCEL - gesture was canceled - there will be no more points in it.
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                // Send the events to the device - all pointers are no longer `touching`:
                sendMotionEvents(true);

                // Clear the state - the gesture has ended.
                mPrimaryPointerId = null;
                mPointerIdToData.clear();
            }

            // ACTION_POINTER_DOWN - new pointer joined the gesture. Its index is passed
            // through MotionEvent.getActionIndex().
            if (action == MotionEvent.ACTION_POINTER_DOWN) {
                int pointerIndex = ev.getActionIndex();
                int pointerId = ev.getPointerId(pointerIndex);

                if (DEBUG_LOGS) Log.i(TAG, "New pointer, ID=" + pointerId);

                PointerData previousData = mPointerIdToData.put(pointerId,
                        new PointerData(ev.getX(pointerIndex), ev.getY(pointerIndex), true));

                if (previousData != null) {
                    // Not much we can do here, just log and continue.
                    Log.w(TAG,
                            "New pointer with ID " + pointerId
                                    + " introduced by ACTION_POINTER_DOWN when old pointer with the same ID already exists.");
                }

                if (DEBUG_LOGS) {
                    Log.i(TAG, "Known pointer IDs after ACTION_POINTER_DOWN:");
                    for (Map.Entry<Integer, PointerData> entry : mPointerIdToData.entrySet()) {
                        Log.i(TAG, "ID=" + entry.getKey());
                    }
                }

                // Send the events to the device.
                sendMotionEvents(false);
            }

            // ACTION_POINTER_UP - pointer left the gesture. Its index is passed though
            // MotionEvent.getActionIndex().
            if (action == MotionEvent.ACTION_POINTER_UP) {
                int pointerIndex = ev.getActionIndex();
                int pointerId = ev.getPointerId(pointerIndex);

                if (!mPointerIdToData.containsKey(pointerId)) {
                    // The pointer with ID that was not previously known has been somehow introduced
                    // outside of ACTION_DOWN / ACTION_POINTER_DOWN - this should never happen!
                    // Nevertheless, it happens in the wild, so ignore the pointer to prevent crash.
                    Log.w(TAG,
                            "Pointer with ID " + pointerId
                                    + " not found in mPointerIdToData, ignoring ACTION_POINTER_UP for it.");
                } else {
                    // Send the events to the device.
                    // The pointer that was raised needs to no longer be `touching`.
                    mPointerIdToData.get(pointerId).touching = false;
                    sendMotionEvents(false);

                    // If it so happened that it was a primary pointer, we need to remember that
                    // there is no primary pointer anymore.
                    if (mPrimaryPointerId != null && mPrimaryPointerId == pointerId) {
                        mPrimaryPointerId = null;
                    }
                    mPointerIdToData.remove(pointerId);
                }
            }

            if (action == MotionEvent.ACTION_MOVE) {
                for (int i = 0; i < ev.getPointerCount(); i++) {
                    int pointerId = ev.getPointerId(i);
                    PointerData pd = mPointerIdToData.get(pointerId);

                    // If pointer data is null for the given pointer id, then something is wrong
                    // with the code's assumption - new pointers can only appear due to ACTION_DOWN
                    // and ACTION_POINTER_DOWN, but it did not seem to happen in this case. In case
                    // logs are enabled, log this information.
                    if (DEBUG_LOGS && pd == null) {
                        Log.i(TAG,
                                "Pointer with ID " + pointerId + " (index " + i
                                        + ") not found in mPointerIdToData. Known pointer IDs:");
                        for (Map.Entry<Integer, PointerData> entry : mPointerIdToData.entrySet()) {
                            Log.i(TAG, "ID=" + entry.getKey());
                        }
                    }

                    if (pd == null) {
                        // The pointer with ID that was not previously known has been somehow
                        // introduced outside of ACTION_DOWN / ACTION_POINTER_DOWN - this should
                        // never happen! Nevertheless, it happens in the wild, so ignore the pointer
                        // to prevent crash.
                        Log.w(TAG,
                                "Pointer with ID " + pointerId + "(index " + i
                                        + ") not found in mPointerIdToData, ignoring ACTION_MOVE for it.");
                        continue;
                    }

                    pd.x = ev.getX(i);
                    pd.y = ev.getY(i);
                }

                sendMotionEvents(false);
            }
        }

        // We need to consume the touch (returning true) to ensure that we get
        // followup events such as MOVE and UP. DOM Overlay mode needs to forward
        // the touch to the content view so that its UI elements keep working.
        mSurfaceUi.forwardMotionEvent(ev);
        return true;
    }

    // If the gestureEnded is set to true, the touching state present on the
    // PointerData entries will be ignored - none of them will be touching and
    // the entire collection will be cleared anyway.
    private void sendMotionEvents(boolean gestureEnded) {
        for (Map.Entry<Integer, PointerData> entry : mPointerIdToData.entrySet()) {
            mArCoreJavaUtils.onDrawingSurfaceTouch(
                    mPrimaryPointerId != null && mPrimaryPointerId.equals(entry.getKey()),
                    gestureEnded ? false : entry.getValue().touching, entry.getKey().intValue(),
                    entry.getValue().x, entry.getValue().y);
        }
    }

    @Override // ScreenOrientationDelegate
    public boolean canUnlockOrientation(Activity activity, int defaultOrientation) {
        if (mActivity == activity && mRestoreOrientation != null) {
            mRestoreOrientation = defaultOrientation;
            return false;
        }
        return true;
    }

    @Override // ScreenOrientationDelegate
    public boolean canLockOrientation() {
        return false;
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceCreated(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceCreated");
        // Do nothing here, we'll handle setup on the following surfaceChanged.
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceRedrawNeeded(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceRedrawNeeded");
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // The surface may not immediately start out at the expected fullscreen size due to
        // animations or not-yet-hidden navigation bars. WebXR immersive sessions use a fixed-size
        // frame transport that can't be resized, so we need to pick a single size and stick with it
        // for the duration of the session. Use the expected fullscreen size for WebXR frame
        // transport even if the currently-visible part in the surface view is smaller than this. We
        // shouldn't get resize events since we're using FLAG_LAYOUT_STABLE and are locking screen
        // orientation.
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(mActivity);
        if (mSurfaceReportedReady) {
            int rotation = display.getRotation();
            if (DEBUG_LOGS) {
                Log.i(TAG,
                        "surfaceChanged ignoring change to width=" + width + " height=" + height
                                + " rotation=" + rotation);
            }
            return;
        }

        // Need to ensure orientation is locked at this point to avoid race conditions. Save current
        // orientation mode, and then lock current orientation. It's unclear if there's still a risk
        // of races, for example if an orientation change was already in progress at this point but
        // wasn't fully processed yet. In that case the user may need to exit and re-enter the
        // session to get the intended layout.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(this);
        if (mRestoreOrientation == null) {
            mRestoreOrientation = mActivity.getRequestedOrientation();
        }
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);

        // Display.getRealSize "gets the real size of the display without subtracting any window
        // decor or applying any compatibility scale factors", and "the size is adjusted based on
        // the current rotation of the display". This is what we want since the surface and WebXR
        // frame sizes also use the same current rotation which is now locked, so there's no need to
        // separately adjust for portrait vs landscape modes.
        //
        // While it would be preferable to wait until the surface is at the desired fullscreen
        // resolution, i.e. via mActivity.getFullscreenManager().getPersistentFullscreenMode(), that
        // causes a chicken-and-egg problem for SurfaceUiCompositor mode as used for DOM overlay.
        // Chrome's fullscreen mode is triggered by the Blink side setting an element fullscreen
        // after the session starts, but the session doesn't start until we report the drawing
        // surface being ready (including a configured size), so we use this reported size assuming
        // that's what the fullscreen mode will use.
        Point size = new Point();
        display.getRealSize(size);

        if (width < size.x || height < size.y) {
            if (DEBUG_LOGS) {
                Log.i(TAG,
                        "surfaceChanged adjusting size from " + width + "x" + height + " to "
                                + size.x + "x" + size.y);
            }
            width = size.x;
            height = size.y;
        }

        int rotation = display.getRotation();
        if (DEBUG_LOGS) {
            Log.i(TAG, "surfaceChanged size=" + width + "x" + height + " rotation=" + rotation);
        }
        mArCoreJavaUtils.onDrawingSurfaceReady(holder.getSurface(), rotation, width, height);
        mSurfaceReportedReady = true;

        // Show the toast with instructions how to exit fullscreen mode now if necessary.
        // Not needed in DOM overlay mode which uses FullscreenHtmlApiHandler to do so.
        mSurfaceUi.onSurfaceVisible();
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceDestroyed");
        cleanupAndExit();
    }

    public void cleanupAndExit() {
        if (DEBUG_LOGS) Log.i(TAG, "cleanupAndExit");

        // Avoid duplicate cleanup if we're exiting via ArCoreJavaUtils's endSession.
        // That triggers cleanupAndExit -> remove SurfaceView -> surfaceDestroyed -> cleanupAndExit.
        if (mCleanupInProgress) return;
        mCleanupInProgress = true;

        mSurfaceUi.destroy();

        // The JS app may have put an element into fullscreen mode during the immersive session,
        // even if this wasn't visible to the user. Ensure that we fully exit out of any active
        // fullscreen state on session end to avoid being left in a confusing state.
        if (!mWebContents.isDestroyed()) {
            mWebContents.exitFullscreen();
        }

        // Restore orientation.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(null);
        if (mRestoreOrientation != null) mActivity.setRequestedOrientation(mRestoreOrientation);
        mRestoreOrientation = null;

        // The surface is destroyed when exiting via "back" button, but also in other lifecycle
        // situations such as switching apps or toggling the phone's power button. Treat each of
        // these as exiting the immersive session. We need to run the destroy callbacks to ensure
        // consistent state after non-exiting lifecycle events.
        mArCoreJavaUtils.onDrawingSurfaceDestroyed();
    }
}
