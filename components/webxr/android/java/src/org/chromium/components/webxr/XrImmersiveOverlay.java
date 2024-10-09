// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.display.DisplayAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * Provides a fullscreen overlay for immersive sessions, allows tailoring setup/etc. due to the
 * particular needs of AR/VR sessions via the XrImmersiveOverlay.Delegate interface.
 */
public class XrImmersiveOverlay
        implements SurfaceHolder.Callback2, View.OnTouchListener, ScreenOrientationDelegate {
    /**
     * Abstraction layer for runtime-specific configuration that needs to happen when setting up a
     * SurfaceView.
     */
    public interface Delegate {
        /**
         * Called to allow for any configuration that may need to happen (e.g. in the Chrome
         * compositor), before a SurfaceView is created. This is required so that AR can ensure that
         * it ends up on top of the DOM surface if we don't need to show it.
         */
        void prepareToCreateSurfaceView();

        /**
         * Configure any Z-order, transparency, or other configuration details that may need to be
         * attached to the surfaceView. The SurfaceView should not yet be parented or made visible.
         * The SurfaceView will already be configured to keep the screen on.
         */
        void configureSurfaceView(SurfaceView surfaceView);

        /**
         * Add the SurfaceView to the Scene Hierarchy in the appropriate place and ensure that it
         * (and any relevant parents) are visible.
         */
        void parentAndShowSurfaceView(SurfaceView surfaceView);

        /**
         * Called when the surfaceView is no longer being used. It may or may not have been
         * destroyed or unparented just yet (See DEFER_SURFACE_VIEW_DESTRUCTION comments),; but
         * there may be some other system configuration that can/should be undone.
         */
        void onStopUsingSurfaceView();

        /**
         * Remove the SurfaceView from the Scene Hierarchy and ensure that it is hidden. Also
         * provides an opportunity to restore any state that may have been setup by
         * @{link parentAndShowSurfaceView}.
         */
        void removeAndHideSurfaceView(SurfaceView surfaceView);

        /**
         * This allows the delegate the opportunity to forward any touch events that would have
         * otherwise been consumed by the XrImmersiveOverlay; e.g. to the compositor.
         */
        void maybeForwardTouchEvent(MotionEvent ev);

        /**
         * Returns the desired @{link android.content.res.Configuration} int representing the
         * orientation that the OverlayDelegate desires the device to be in. Should be one of:
         * Configuration.ORIENTATION_LANDSCAPE
         * Configuration.ORIENTATION_PORTRAIT
         * Configuration.ORIENTATION_UNDEFINED
         */
        int getDesiredOrientation();

        /**
         * Returns whether the size of the rendering surface should be the size of the entire
         * display or should have the size of cutout areas into account.
         */
        boolean useDisplaySizes();
    }

    private static final String TAG = "XrImmersiveOverlay";
    private static final boolean DEBUG_LOGS = false;

    // See class comment for XrSurfaceViewImpl below.
    private static final boolean DEFER_SURFACE_VIEW_DESTRUCTION =
            (Build.VERSION.SDK_INT < Build.VERSION_CODES.O);

    private XrSessionCoordinator mXrSessionCoordinator;
    private Delegate mOverlayDelegate;
    private Activity mActivity;
    private boolean mSurfaceReportedReady;
    private Integer mRestoreOrientation;
    private boolean mCleanupInProgress;
    private XrSurfaceView mXrSurfaceView;
    private WebContents mWebContents;
    private boolean mUseOverlay;

    // Set containing all currently touching pointers.
    private HashMap<Integer, PointerData> mPointerIdToData;
    // ID of primary pointer (if present).
    private Integer mPrimaryPointerId;

    public void show(
            @NonNull Delegate overlayDelegate,
            @NonNull WebContents webContents,
            @NonNull XrSessionCoordinator caller) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor");
        mXrSessionCoordinator = caller;

        mWebContents = webContents;
        mOverlayDelegate = overlayDelegate;

        mActivity = XrSessionCoordinator.getActivity(webContents);

        mPointerIdToData = new HashMap<Integer, PointerData>();
        mPrimaryPointerId = null;

        // Choose a concrete implementation to create a drawable Surface and make it fullscreen.
        // It forwards SurfaceHolder callbacks and touch events to this XrImmersiveOverlay object.
        mXrSurfaceView = new XrSurfaceView();
    }

    private static class PointerData {
        public float x;
        public float y;
        public boolean touching;

        public PointerData(float x, float y, boolean touching) {
            this.x = x;
            this.y = y;
            this.touching = touching;
        }
    }

    private class XrSurfaceView {
        private SurfaceView mSurfaceView;
        private WebContentsObserver mWebContentsObserver;
        private boolean mDomSurfaceNeedsConfiguring;
        private boolean mSurfaceViewNeedsDestruction;
        private boolean mDestructionFromVisibilityChanged;

        // On some versions of Android (primarily N), the onWindowVisibilityChanged event is
        // responsible for firing the surfaceDestroyed event which ultimately removes the
        // SurfaceView from the window hierarchy. However in these cases, the synchronous
        // call to surfaceDestroyed appears to not expect or account for the View in question
        // potentially being detached from the window and causes a null reference exception.
        // This class works around that by setting and checking flags to ensure that on the
        // OS's where this is necessary, the SurfaceView is only *actually* detached from the
        // window after the parent onWindowVisibilityChanged event has finished processing
        // (assuming that the onWindowVisibilityChanged event would have caused the destruction).
        private class XrSurfaceViewImpl extends SurfaceView {
            public XrSurfaceViewImpl(Activity activity) {
                super(activity);
            }

            @Override
            protected void onWindowVisibilityChanged(int visibility) {
                if (mCleanupInProgress) return;
                mDestructionFromVisibilityChanged = true;
                super.onWindowVisibilityChanged(visibility);
                mDestructionFromVisibilityChanged = false;

                if (DEFER_SURFACE_VIEW_DESTRUCTION && mSurfaceViewNeedsDestruction) {
                    removeAndDestroySurfaceView();
                    mSurfaceViewNeedsDestruction = false;
                }
            }
        }

        @SuppressLint("ClickableViewAccessibility")
        public XrSurfaceView() {
            mOverlayDelegate.prepareToCreateSurfaceView();

            mSurfaceView = new XrSurfaceViewImpl(mActivity);
            mSurfaceView.setKeepScreenOn(true);
            mSurfaceView.getHolder().addCallback(XrImmersiveOverlay.this);
            mOverlayDelegate.configureSurfaceView(mSurfaceView);

            // Process touch input events for XR input. This consumes them, they'll be resent to
            // the compositor view via forwardMotionEvent.
            mSurfaceView.setOnTouchListener(XrImmersiveOverlay.this);

            mOverlayDelegate.parentAndShowSurfaceView(mSurfaceView);

            mWebContentsObserver =
                    new WebContentsObserver() {
                        @Override
                        public void didToggleFullscreenModeForTab(
                                boolean enteredFullscreen, boolean willCauseResize) {
                            if (DEBUG_LOGS) {
                                Log.i(
                                        TAG,
                                        "didToggleFullscreenModeForTab(), enteredFullscreen="
                                                + enteredFullscreen);
                            }

                            if (!enteredFullscreen) {
                                cleanupAndExit();
                            }
                        }
                    };

            // Watch for fullscreen exit triggered from JS, this needs to end the session.
            mWebContents.addObserver(mWebContentsObserver);
        }

        public void destroy() {
            mWebContents.removeObserver(mWebContentsObserver);

            if (!(DEFER_SURFACE_VIEW_DESTRUCTION && mDestructionFromVisibilityChanged)) {
                removeAndDestroySurfaceView();
            } else {
                mSurfaceViewNeedsDestruction = true;
            }

            mOverlayDelegate.onStopUsingSurfaceView();
        }

        private void removeAndDestroySurfaceView() {
            if (mSurfaceView == null) return;
            mOverlayDelegate.removeAndHideSurfaceView(mSurfaceView);
            mSurfaceView = null;
        }
    }

    @Override // View.OnTouchListener
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouch(View v, MotionEvent ev) {
        // Only forward primary actions, ignore more complex events such as secondary pointer
        // touches. Ignore batching since we're only sending one ray pose per frame.

        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "Received motion event, action: "
                            + MotionEvent.actionToString(ev.getAction())
                            + ", pointer count: "
                            + ev.getPointerCount()
                            + ", action index: "
                            + ev.getActionIndex());
            for (int i = 0; i < ev.getPointerCount(); i++) {
                Log.i(
                        TAG,
                        "Pointer index: "
                                + i
                                + ", id: "
                                + ev.getPointerId(i)
                                + ", coordinates: ("
                                + ev.getX(i)
                                + ", "
                                + ev.getY(i)
                                + ")");
            }
        }

        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN
                || action == MotionEvent.ACTION_UP
                || action == MotionEvent.ACTION_POINTER_DOWN
                || action == MotionEvent.ACTION_POINTER_UP
                || action == MotionEvent.ACTION_CANCEL
                || action == MotionEvent.ACTION_MOVE) {
            // ACTION_DOWN - gesture starts. Pointer with index 0 will be considered as a primary
            // pointer until it's raised. Then, there will be no primary pointer until the
            // gesture ends (ACTION_UP / ACTION_CANCEL).
            if (action == MotionEvent.ACTION_DOWN) {
                int pointerId = ev.getPointerId(0);

                // Remember primary pointer's ID. The start of the gesture is the only time when the
                // primary pointer is set.
                mPrimaryPointerId = pointerId;
                PointerData previousData =
                        mPointerIdToData.put(
                                mPrimaryPointerId, new PointerData(ev.getX(0), ev.getY(0), true));

                if (previousData != null) {
                    // Not much we can do here, just log and continue.
                    Log.w(
                            TAG,
                            "New pointer with ID "
                                    + pointerId
                                    + " introduced by ACTION_DOWN when old pointer with the same ID"
                                    + " already exists.");
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

                PointerData previousData =
                        mPointerIdToData.put(
                                pointerId,
                                new PointerData(
                                        ev.getX(pointerIndex), ev.getY(pointerIndex), true));

                if (previousData != null) {
                    // Not much we can do here, just log and continue.
                    Log.w(
                            TAG,
                            "New pointer with ID "
                                    + pointerId
                                    + " introduced by ACTION_POINTER_DOWN when old pointer with the"
                                    + " same ID already exists.");
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
                    Log.w(
                            TAG,
                            "Pointer with ID "
                                    + pointerId
                                    + " not found in mPointerIdToData, ignoring ACTION_POINTER_UP"
                                    + " for it.");
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
                        Log.i(
                                TAG,
                                "Pointer with ID "
                                        + pointerId
                                        + " (index "
                                        + i
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
                        Log.w(
                                TAG,
                                "Pointer with ID "
                                        + pointerId
                                        + "(index "
                                        + i
                                        + ") not found in mPointerIdToData, ignoring ACTION_MOVE"
                                        + " for it.");
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
        // the touch to the content view so that its UI elements keep working, so
        // we need to afford the particular session the chance to forward the event.
        mOverlayDelegate.maybeForwardTouchEvent(ev);
        return true;
    }

    // If the gestureEnded is set to true, the touching state present on the
    // PointerData entries will be ignored - none of them will be touching and
    // the entire collection will be cleared anyway.
    private void sendMotionEvents(boolean gestureEnded) {
        for (Map.Entry<Integer, PointerData> entry : mPointerIdToData.entrySet()) {
            mXrSessionCoordinator.onDrawingSurfaceTouch(
                    mPrimaryPointerId != null && mPrimaryPointerId.equals(entry.getKey()),
                    gestureEnded ? false : entry.getValue().touching,
                    entry.getKey().intValue(),
                    entry.getValue().x,
                    entry.getValue().y);
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
        DisplayAndroid display = mWebContents.getTopLevelNativeWindow().getDisplay();
        if (mSurfaceReportedReady) {
            int rotation = display.getRotation();
            if (DEBUG_LOGS) {
                Log.i(
                        TAG,
                        "surfaceChanged ignoring change to width="
                                + width
                                + " height="
                                + height
                                + " rotation="
                                + rotation);
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

        int desiredOrientation = mOverlayDelegate.getDesiredOrientation();
        int currentOrientation = mActivity.getResources().getConfiguration().orientation;

        int requestOrientation = configurationToActivityInfoOrientation(desiredOrientation);

        // If we have a desired orientation and it does not equal the current orientation, then we
        // will need to swap dimensions.
        boolean swapScreenDimensions =
                desiredOrientation != Configuration.ORIENTATION_UNDEFINED
                        && desiredOrientation != currentOrientation;

        mActivity.setRequestedOrientation(requestOrientation);

        // While it would be preferable to wait until the surface is at the desired fullscreen
        // resolution, i.e. via mActivity.getFullscreenManager().getPersistentFullscreenMode(), that
        // causes a chicken-and-egg problem for XrSurfaceView mode as used for DOM overlay.
        // Chrome's fullscreen mode is triggered by the Blink side setting an element fullscreen
        // after the session starts, but the session doesn't start until we report the drawing
        // surface being ready (including a configured size), so we use the reported size of the
        // display assuming that's what the fullscreen mode will use.
        if (mOverlayDelegate.useDisplaySizes()) {
            int screenWidth = display.getDisplayWidth();
            int screenHeight = display.getDisplayHeight();

            if (width < screenWidth || height < screenHeight) {
                if (DEBUG_LOGS) {
                    Log.i(
                            TAG,
                            "surfaceChanged adjusting size from "
                                    + width
                                    + "x"
                                    + height
                                    + " to"
                                    + screenWidth
                                    + "x"
                                    + screenHeight);
                }
                width = screenWidth;
                height = screenHeight;
            }
        }

        if (swapScreenDimensions) {
            // Swap width and height.
            int auxWidth = width;
            width = height;
            height = auxWidth;
        }

        int rotation = display.getRotation();
        if (DEBUG_LOGS) {
            Log.i(TAG, "surfaceChanged size=" + width + "x" + height + " rotation=" + rotation);
        }
        mXrSessionCoordinator.onDrawingSurfaceReady(
                holder.getSurface(),
                mWebContents.getTopLevelNativeWindow(),
                rotation,
                width,
                height);
        mSurfaceReportedReady = true;
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceDestroyed");
        cleanupAndExit();
    }

    public void cleanupAndExit() {
        if (DEBUG_LOGS) Log.i(TAG, "cleanupAndExit");

        // Avoid duplicate cleanup if we're exiting via XrSessionCoordinator's endSession.
        // That triggers cleanupAndExit -> remove SurfaceView -> surfaceDestroyed -> cleanupAndExit.
        if (mCleanupInProgress) return;
        mCleanupInProgress = true;

        // The surface is destroyed when exiting via "back" button, but also in other lifecycle
        // situations such as switching apps or toggling the phone's power button. Treat each of
        // these as exiting the immersive session. At this point, the surface isn't destroyed yet.
        // but will be soon. We need to give the native code a chance to cleanup any state before
        // we start any other logic to ensure that the surface is destroyed. We also need to run
        // the destroy callbacks to ensure consistent state after non-exiting lifecycle events.
        mXrSessionCoordinator.onDrawingSurfaceDestroyed();

        mXrSurfaceView.destroy();

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
    }

    /**
     * Translates the provided int, which is expected to be of the result of
     * @{link Delegate.getDesiredOrientation} and thus one of:
     * Configuration.ORIENTATION_UNDEFINED,
     * Configuration.ORIENTATION_LANDSCAPE,
     * Configuration.ORIENTATION_PORTRAIT
     * and translates it to a corresponding "ActivityInfo" value that can then be passed on to
     * setRequestedOrientation.
     */
    private int configurationToActivityInfoOrientation(int configurationOrientation) {
        switch (configurationOrientation) {
            case Configuration.ORIENTATION_UNDEFINED:
                return ActivityInfo.SCREEN_ORIENTATION_LOCKED;
            case Configuration.ORIENTATION_LANDSCAPE:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case Configuration.ORIENTATION_PORTRAIT:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            default:
                Log.e(
                        TAG,
                        "Unexpected configurationOrientation: "
                                + configurationOrientation
                                + " using default of 'Locked'.");
                return ActivityInfo.SCREEN_ORIENTATION_LOCKED;
        }
    }
}
