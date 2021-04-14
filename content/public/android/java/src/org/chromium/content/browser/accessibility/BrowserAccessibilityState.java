// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.annotation.TargetApi;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.provider.Settings;
import android.view.accessibility.AccessibilityManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.List;

/**
 * Provides utility methods relating to measuring accessibility state on the current platform (i.e.
 * Android in this case). See content::BrowserAccessibilityStateImpl.
 */
@JNINamespace("content")
public class BrowserAccessibilityState {
    private static boolean sInitialized;

    // A bitmask containing the union of all event types, feedback types, flags,
    // and capabilities of running accessibility services.
    private static int sEventTypeMask;
    private static int sFeedbackTypeMask;
    private static int sFlagsMask;
    private static int sCapabilitiesMask;

    // The IDs of all running accessibility services.
    private static String[] sServiceIds;

    private static Handler sHandler;

    private static class AnimatorDurationScaleObserver extends ContentObserver {
        public AnimatorDurationScaleObserver(Handler handler) {
            super(handler);
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            assert ThreadUtils.runningOnUiThread();
            BrowserAccessibilityStateJni.get().onAnimatorDurationScaleChanged();
        }
    }

    private static class AccessibilityServicesObserver extends ContentObserver {
        public AccessibilityServicesObserver(Handler handler) {
            super(handler);
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, Uri uri) {
            // When this is initially called, the set of accessibility
            // services hasn't changed. Add a delay so that it propagates.
            // TODO(dmazzoni): make something more robust here.
            getHandler().postDelayed(() -> { updateAccessibilityServices(); }, 1000);
        }
    }

    static void updateAccessibilityServices() {
        sInitialized = true;
        sEventTypeMask = 0;
        sFeedbackTypeMask = 0;
        sFlagsMask = 0;
        sCapabilitiesMask = 0;

        AccessibilityManager accessibilityManager =
                (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
        List<AccessibilityServiceInfo> services =
                accessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
        sServiceIds = new String[services.size()];
        int i = 0;
        for (AccessibilityServiceInfo service : services) {
            sEventTypeMask |= service.eventTypes;
            sFeedbackTypeMask |= service.feedbackType;
            sFlagsMask |= service.flags;
            sCapabilitiesMask |= service.getCapabilities();
            sServiceIds[i++] = service.getId();
        }
    }

    static Handler getHandler() {
        if (sHandler == null) sHandler = new Handler(ThreadUtils.getUiThreadLooper());

        return sHandler;
    }

    /**
     * Return a bitmask containing the union of all event types that running accessibility
     * services listen to.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceEventTypeMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sEventTypeMask;
    }

    /**
     * Return a bitmask containing the union of all feedback types that running accessibility
     * services provide.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceFeedbackTypeMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sFeedbackTypeMask;
    }

    /**
     * Return a bitmask containing the union of all flags from running accessibility services.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceFlagsMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sFlagsMask;
    }

    /**
     * Return a bitmask containing the union of all service capabilities from running
     * accessibility services.
     * @return
     */
    @CalledByNative
    protected static int getAccessibilityServiceCapabilitiesMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sCapabilitiesMask;
    }

    /**
     * Return a list of ids of all running accessibility services.
     * @return
     */
    @CalledByNative
    protected static String[] getAccessibilityServiceIds() {
        if (!sInitialized) updateAccessibilityServices();
        return sServiceIds;
    }

    @CalledByNative
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    static void registerObservers() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        contentResolver.registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false,
                new AnimatorDurationScaleObserver(getHandler()));
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES), false,
                new AccessibilityServicesObserver(getHandler()));
    }

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();
    }
}
