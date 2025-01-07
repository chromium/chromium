// Copyright 2014 The Chromium Authors
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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Map;
import java.util.WeakHashMap;

/** This is the implementation of the C++ counterpart ScreenOrientationProvider. */
@JNINamespace("content")
public class ScreenOrientationProviderImpl
        implements ActivityStateListener, ScreenOrientationProvider {
    private static class Holder {
        private static ScreenOrientationProviderImpl sInstance =
                new ScreenOrientationProviderImpl();
    }

    private static final String TAG = "ScreenOrientation";

    // More readable constants to be passed to |addPendingRequest|.
    private static final boolean LOCK = true;
    private static final boolean UNLOCK = false;

    private ScreenOrientationDelegate mDelegate;

    /**
     * The keys of the map are the activities for which screen orientation are
     * trying to lock.
     * The values of the map are the most recent default web screen orientation request for each
     * activity.
     */
    private Map<Activity, Byte> mDefaultOrientationOverrides = new WeakHashMap<>();

    /**
     * The keys of the map are the activities for which screen orientation requests are
     * delayed.
     * The values of the map are the most recent screen orientation request for each activity.
     * The map will contain an entry with a null value if screen orientation requests are delayed
     * for an activity but no screen orientation requests have been made for the activity.
     */
    private Map<Activity, Pair<Boolean, Integer>> mDelayedRequests = new WeakHashMap<>();

    private static final class PendingRequest implements WindowEventObserver {
        private final ScreenOrientationProviderImpl mProvider;
        private final WindowEventObserverManager mWindowEventManager;
        private final boolean mLockOrUnlock;
        private final byte mWebScreenOrientation;
        private boolean mObserverRemoved;

        public PendingRequest(
                ScreenOrientationProviderImpl provider,
                WindowEventObserverManager windowEventManager,
                boolean lockOrUnlock,
                byte webScreenOrientation) {
            mProvider = provider;
            mWindowEventManager = windowEventManager;
            mLockOrUnlock = lockOrUnlock;
            mWebScreenOrientation = webScreenOrientation;
            mWindowEventManager.addObserver(this);
        }

        public void cancel() {
            if (mObserverRemoved) return;
            mWindowEventManager.removeObserver(this);
            mObserverRemoved = true;
        }

        @Override
        public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
            if (newWindowAndroid == null) return;

            if (mLockOrUnlock) {
                mProvider.lockOrientation(newWindowAndroid, mWebScreenOrientation);
            } else {
                mProvider.unlockOrientation(newWindowAndroid);
            }
            mWindowEventManager.removeObserver(this);
            mObserverRemoved = true;
        }
    }

    private final Map<WebContents, PendingRequest> mPendingRequests = new WeakHashMap<>();

    @CalledByNative
    public static ScreenOrientationProviderImpl getInstance() {
        return Holder.sInstance;
    }

    private static int getOrientationFromWebScreenOrientations(
            byte orientation, @Nullable WindowAndroid window, Context context) {
        switch (orientation) {
            case ScreenOrientationLockType.DEFAULT:
                return ActivityInfo.SCREEN_ORIENTATION_USER;
            case ScreenOrientationLockType.PORTRAIT_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            case ScreenOrientationLockType.PORTRAIT_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
            case ScreenOrientationLockType.LANDSCAPE_PRIMARY:
                return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            case ScreenOrientationLockType.LANDSCAPE_SECONDARY:
                return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
            case ScreenOrientationLockType.PORTRAIT:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
            case ScreenOrientationLockType.LANDSCAPE:
                return ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
            case ScreenOrientationLockType.ANY:
                return ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
            case ScreenOrientationLockType.NATURAL:
                // If the tab is being reparented, we don't have a display strongly associated with
                // it, so we get the default display.
                DisplayAndroid displayAndroid =
                        (window != null)
                                ? window.getDisplay()
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

    private void addPendingRequest(
            WebContents webContents, boolean lockOrUnlock, byte webScreenOrientation) {
        WindowEventObserverManager windowEventManager =
                WindowEventObserverManager.from(webContents);
        PendingRequest existingRequest = mPendingRequests.get(webContents);
        if (existingRequest != null) existingRequest.cancel();
        mPendingRequests.put(
                webContents,
                new PendingRequest(this, windowEventManager, lockOrUnlock, webScreenOrientation));
    }

    @CalledByNative
    private void lockOrientationForWebContents(WebContents webContents, byte webScreenOrientation) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            addPendingRequest(webContents, LOCK, webScreenOrientation);
        } else {
            lockOrientation(window, webScreenOrientation);
        }
    }

    @Override
    public void lockOrientation(@Nullable WindowAndroid window, byte webScreenOrientation) {
        // WindowAndroid may be null if the tab is being reparented.
        if (window == null) return;
        Activity activity = window.getActivity().get();

        // Locking orientation is only supported for web contents that have an associated activity.
        // Note that we can't just use the focused activity, as that would lead to bugs where
        // unlockOrientation unlocks a different activity to the one that was locked.
        if (activity == null) return;

        int orientation =
                getOrientationFromWebScreenOrientations(webScreenOrientation, window, activity);
        if (orientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
            return;
        }

        setMaybeDelayedRequestedOrientation(activity, /* lock= */ true, orientation);
    }

    @CalledByNative
    private void unlockOrientationForWebContents(WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            addPendingRequest(webContents, UNLOCK, (byte) 0);
        } else {
            unlockOrientation(window);
        }
    }

    @Override
    public void unlockOrientation(@Nullable WindowAndroid window) {
        // WindowAndroid may be null if the tab is being reparented.
        if (window == null) return;
        Activity activity = window.getActivity().get();

        // Locking orientation is only supported for web contents that have an associated activity.
        // Note that we can't just use the focused activity, as that would lead to bugs where
        // unlockOrientation unlocks a different activity to the one that was locked.
        if (activity == null) return;
        byte mDefaultWebOrientation = (byte) ScreenOrientationLockType.DEFAULT;
        if (mDefaultOrientationOverrides.containsKey(activity)) {
            mDefaultWebOrientation = mDefaultOrientationOverrides.get(activity);
        }

        int defaultOrientation =
                getOrientationFromWebScreenOrientations(mDefaultWebOrientation, window, activity);

        try {
            if (defaultOrientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED) {
                ActivityInfo info =
                        activity.getPackageManager()
                                .getActivityInfo(
                                        activity.getComponentName(), PackageManager.GET_META_DATA);
                defaultOrientation = info.screenOrientation;
            }
        } catch (PackageManager.NameNotFoundException e) {
            // Do nothing, defaultOrientation should be SCREEN_ORIENTATION_UNSPECIFIED.
        } finally {
            setMaybeDelayedRequestedOrientation(activity, /* lock= */ false, defaultOrientation);
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

    @Override
    public void setOverrideDefaultOrientation(WindowAndroid window, byte defaultWebOrientation) {
        if (window == null) return;
        Activity activity = window.getActivity().get();

        if (activity == null) return;

        if (defaultWebOrientation != ScreenOrientationLockType.DEFAULT) {
            mDefaultOrientationOverrides.put(activity, defaultWebOrientation);
        } else {
            mDefaultOrientationOverrides.remove(activity);
        }
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
