// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.util.Pair;
import android.view.Surface;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.common.ScreenOrientationConstants;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Map;
import java.util.WeakHashMap;

/**
 * This is the implementation of the C++ counterpart ScreenOrientationProvider.
 */
@JNINamespace("content")
public class ScreenOrientationProviderImpl
        implements ActivityStateListener, ScreenOrientationProvider {
    private static class Holder {
        private static ScreenOrientationProviderImpl sInstance =
                new ScreenOrientationProviderImpl();
    }

    private static final String TAG = "ScreenOrientation";

    private ScreenOrientationDelegate mDelegate;

    /**
     * The keys of the map are the activities for which screen orientation requests are
     * delayed.
     * The values of the map are the most recent screen orientation request for each activity.
     * The map will contain an entry with a null value if screen orientation requests are delayed
     * for an activity but no screen orientation requests have been made for the activity.
     */
    private Map<Activity, Pair<Boolean, Integer>> mDelayedRequests = new WeakHashMap<>();

    @CalledByNative
    public static ScreenOrientationProviderImpl getInstance() {
        return Holder.sInstance;
    }

    private static int getOrientationFromWebScreenOrientations(byte orientation,
            @Nullable WindowAndroid window, Context context) {
        switch (orientation) {
            case ScreenOrientationValues.DEFAULT:
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            case ScreenOrientationValues.PORTRAIT_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            case ScreenOrientationValues.PORTRAIT_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
            case ScreenOrientationValues.LANDSCAPE_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case ScreenOrientationValues.LANDSCAPE_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            case ScreenOrientationValues.PORTRAIT:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
            case ScreenOrientationValues.LANDSCAPE:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
            case ScreenOrientationValues.ANY:
                return ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
            case ScreenOrientationValues.NATURAL:
                // If the tab is being reparented, we don't have a display strongly associated with
                // it, so we get the default display.
                DisplayAndroid displayAndroid = (window != null) ? window.getDisplay()
                        : DisplayAndroid.getNonMultiDisplay(context);
                int rotation = displayAndroid.getRotation();
                if (rotation == Surface.ROTATION_0 || rotation == Surface.ROTATION_180) {
                    if (displayAndroid.getDisplayHeight() >= displayAndroid.getDisplayWidth()) {
                        return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
                    }
                    return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
                } else {
                    if (displayAndroid.getDisplayHeight() < displayAndroid.getDisplayWidth()) {
                        return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
                    }
                    return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
                }
            default:
                Log.w(TAG, "Trying to lock to unsupported orientation!");
                return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.DESTROYED) {
            mDelayedRequests.remove(activity);
        }
    }

    @CalledByNative
    @Override
    public void lockOrientation(@Nullable WindowAndroid window, byte webScreenOrientation) {
        // WindowAndroid may be null if the tab is being reparented.
        if (window == null) return;
        Activity activity = window.getActivity().get();

        // Locking orientation is only supported for web contents that have an associated activity.
        // Note that we can't just use the focused activity, as that would lead to bugs where
        // unlockOrientation unlocks a different activity to the one that was locked.
        if (activity == null) return;

        int orientation = getOrientationFromWebScreenOrientations(webScreenOrientation, window,
                activity);
        if (orientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            return;
        }

        setMaybeDelayedRequestedOrientation(activity, true /* lock */, orientation);
    }

    @CalledByNative
    @Override
    public void unlockOrientation(@Nullable WindowAndroid window) {
        // WindowAndroid may be null if the tab is being reparented.
        if (window == null) return;
        Activity activity = window.getActivity().get();

        // Locking orientation is only supported for web contents that have an associated activity.
        // Note that we can't just use the focused activity, as that would lead to bugs where
        // unlockOrientation unlocks a different activity to the one that was locked.
        if (activity == null) return;

        int defaultOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

        // Activities opened from a shortcut may have EXTRA_ORIENTATION set. In
        // which case, we want to use that as the default orientation.
        int orientation = activity.getIntent().getIntExtra(
                ScreenOrientationConstants.EXTRA_ORIENTATION,
                ScreenOrientationValues.DEFAULT);
        defaultOrientation = getOrientationFromWebScreenOrientations(
                (byte) orientation, window, activity);

        try {
            if (defaultOrientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
                ActivityInfo info = activity.getPackageManager().getActivityInfo(
                        activity.getComponentName(), PackageManager.GET_META_DATA);
                defaultOrientation = info.screenOrientation;
            }
        } catch (PackageManager.NameNotFoundException e) {
            // Do nothing, defaultOrientation should be SCREEN_ORIENTATION_UNSPECIFIED.
        } finally {
            setMaybeDelayedRequestedOrientation(activity, false /* lock */, defaultOrientation);
        }
    }

    @Override
    public void delayOrientationRequests(WindowAndroid window) {
        Activity activity = window.getActivity().get();
        if ((activity == null || areRequestsDelayedForActivity(activity))) {
            return;
        }

        mDelayedRequests.put(activity, null);
        ApplicationStatus.registerStateListenerForActivity(this, activity);
    }

    @Override
    public void runDelayedOrientationRequests(WindowAndroid window) {
        Activity activity = window.getActivity().get();
        if ((activity == null || !areRequestsDelayedForActivity(activity))) {
            return;
        }

        Pair<Boolean, Integer> delayedRequest = mDelayedRequests.remove(activity);
        if (delayedRequest != null) {
            setRequestedOrientationNow(activity, delayedRequest.first, delayedRequest.second);
        }
        if (mDelayedRequests.isEmpty()) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    @CalledByNative
    public boolean isOrientationLockEnabled() {
        return mDelegate == null || mDelegate.canLockOrientation();
    }

    @Override
    public void setOrientationDelegate(ScreenOrientationDelegate delegate) {
        mDelegate = delegate;
    }

    /** Returns whether screen orientation requests are delayed for the passed-in activity. */
    private boolean areRequestsDelayedForActivity(Activity activity) {
        return mDelayedRequests.containsKey(activity);
    }

    /** Sets the requested orientation for the activity delaying the request if needed. */
    private void setMaybeDelayedRequestedOrientation(
            Activity activity, boolean lock, int orientation) {
        if (areRequestsDelayedForActivity(activity)) {
            mDelayedRequests.put(activity, Pair.create(lock, orientation));
        } else {
            setRequestedOrientationNow(activity, lock, orientation);
        }
    }

    /** Sets the requested orientation for the activity. */
    private void setRequestedOrientationNow(Activity activity, boolean lock, int orientation) {
        if (mDelegate != null) {
            if ((lock && !mDelegate.canLockOrientation())
                    || (!lock && !mDelegate.canUnlockOrientation(activity, orientation))) {
                return;
            }
        }

        activity.setRequestedOrientation(orientation);
    }
}
