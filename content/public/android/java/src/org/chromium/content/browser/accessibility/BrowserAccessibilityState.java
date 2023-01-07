// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl.TAG;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.provider.Settings;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Provides utility methods relating to measuring accessibility state on the current platform (i.e.
 * Android in this case). See content::BrowserAccessibilityStateImpl.
 */
@JNINamespace("content")
public class BrowserAccessibilityState {
    /**
     * An interface for classes that want to be notified whenever the accessibility
     * state has changed, which can happen when accessibility services start or stop.
     */
    public interface Listener {
        public void onBrowserAccessibilityStateChanged(boolean newScreenReaderEnabledState);
    }

    // Analysis of the most popular accessibility services on Android suggests
    // that any service that requests any of these three events is a screen reader
    // or other complete assistive technology. If none of these events are requested,
    // we can enable some optimizations.
    private static final int SCREEN_READER_EVENT_TYPE_MASK = AccessibilityEvent.TYPE_VIEW_SELECTED
            | AccessibilityEvent.TYPE_VIEW_SCROLLED | AccessibilityEvent.TYPE_ANNOUNCEMENT;

    private static boolean sInitialized;

    // A bitmask containing the union of all event types, feedback types, flags,
    // and capabilities of running accessibility services.
    private static int sEventTypeMask;
    private static int sFeedbackTypeMask;
    private static int sFlagsMask;
    private static int sCapabilitiesMask;

    // Simple boolean that will be true when any accessibility service is running on the device.
    private static boolean sHasAnyAccessibilityServiceEnabled;

    // True when we determine that genuine assistive technology such as a screen reader
    // is running, based on the information from running accessibility services. False otherwise.
    private static boolean sScreenReader;

    // True when android version is less than 31 or at least one enabled accessibility service
    // returns true for isAccessibilityTool(). False otherwise.
    private static boolean sAccessibilityToolPresent;

    // True when the user has enabled the Android-OS privacy setting for showing passwords, found
    // in: Settings > Privacy > Show passwords. (Settings.System.TEXT_SHOW_PASSWORD). False
    // otherwise.
    private static boolean sTextShowPasswordEnabled;

    /**
     * Whether the user has enabled the Android-OS speak password when in accessibility mode,
     * available on pre-Android O. (Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD).
     *
     * From Android docs:
     * @deprecated The speaking of passwords is controlled by individual accessibility services.
     * Apps should ignore this setting and provide complete information to accessibility
     * at all times, which was the behavior when this value was {@code true}.
     */
    @Deprecated
    private static boolean sAccessibilitySpeakPasswordEnabled;

    // The IDs of all running accessibility services.
    private static String[] sServiceIds;

    private static Handler sHandler;

    // The set of listeners of BrowserAccessibilityState, implemented using
    // a WeakHashSet behind the scenes so that listeners can be garbage-collected
    // and will be automatically removed from this set.
    private static Set<Listener> sListeners =
            Collections.newSetFromMap(new WeakHashMap<Listener, Boolean>());

    // The number of milliseconds to wait before checking the set of running
    // accessibility services again, when we think it changed. Uses an exponential
    // back-off until it's greater than MAX_DELAY_MILLIS.
    private static final int MIN_DELAY_MILLIS = 500;
    private static final int MAX_DELAY_MILLIS = 60000;
    private static int sNextDelayMillis = MIN_DELAY_MILLIS;

    public static void addListener(Listener listener) {
        sListeners.add(listener);
    }

    public static boolean hasAnyAccessibilityServiceEnabled() {
        if (!sInitialized) updateAccessibilityServices();

        return sHasAnyAccessibilityServiceEnabled;
    }

    public static boolean hasAccessibilityToolPresent() {
        if (!sInitialized) updateAccessibilityServices();

        return sAccessibilityToolPresent;
    }

    public static boolean screenReaderMode() {
        if (!sInitialized) updateAccessibilityServices();

        return sScreenReader;
    }

    public static boolean hasEnabledTextShowPassword() {
        if (!sInitialized) updateAccessibilityServices();

        return sTextShowPasswordEnabled;
    }

    public static boolean hasEnabledAccessibilitySpeakPassword() {
        if (!sInitialized) updateAccessibilityServices();

        return sAccessibilitySpeakPasswordEnabled;
    }

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
            // Note that when Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES changes,
            // the set of running accessibility services doesn't always reflect that
            // immediately, but updateAccessibilityServices checks for this and keeps
            // polling until they agree.
            getHandler().post(() -> { updateAccessibilityServices(); });
        }
    }

    @VisibleForTesting
    public static void setFeedbackTypeMaskForTesting(int value) {
        if (!sInitialized) updateAccessibilityServices();

        sFeedbackTypeMask = value;

        // Inform all listeners of this change.
        for (Listener listener : sListeners) {
            listener.onBrowserAccessibilityStateChanged(sScreenReader);
        }
    }

    @VisibleForTesting
    public static void setEventTypeMaskForTesting() {
        if (!sInitialized) updateAccessibilityServices();

        // Explicitly set mask so all events are relevant to currently enabled service.
        sEventTypeMask = ~0;

        // Inform all listeners of this change.
        for (Listener listener : sListeners) {
            listener.onBrowserAccessibilityStateChanged(true);
        }
    }

    @VisibleForTesting
    public static void setEventTypeMaskEmptyForTesting() {
        if (!sInitialized) updateAccessibilityServices();

        // Explicitly set mask so no events are relevant to currently enabled service.
        sEventTypeMask = 0;

        // Inform all listeners of this change.
        for (Listener listener : sListeners) {
            listener.onBrowserAccessibilityStateChanged(true);
        }
    }

    @VisibleForTesting
    public static void setScreenReaderModeForTesting(boolean enabled) {
        if (!sInitialized) updateAccessibilityServices();

        // Explicitly set screen reader mode since a real screen reader isn't run during tests.
        sScreenReader = enabled;

        // Inform all listeners of this change.
        for (Listener listener : sListeners) {
            listener.onBrowserAccessibilityStateChanged(sScreenReader);
        }
    }

    static void updateAccessibilityServices() {
        sInitialized = true;
        sEventTypeMask = 0;
        sFeedbackTypeMask = 0;
        sFlagsMask = 0;
        sCapabilitiesMask = 0;
        sHasAnyAccessibilityServiceEnabled = false;
        sAccessibilityToolPresent = false;

        // Get the list of currently running accessibility services.
        Context context = ContextUtils.getApplicationContext();
        AccessibilityManager accessibilityManager =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        List<AccessibilityServiceInfo> services =
                accessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
        sServiceIds = new String[services.size()];
        ArrayList<String> runningServiceNames = new ArrayList<String>();
        int i = 0;
        for (AccessibilityServiceInfo service : services) {
            if (service == null) continue;
            sEventTypeMask |= service.eventTypes;
            sFeedbackTypeMask |= service.feedbackType;
            sFlagsMask |= service.flags;
            sCapabilitiesMask |= service.getCapabilities();
            sHasAnyAccessibilityServiceEnabled = true;
            sAccessibilityToolPresent |= (Build.VERSION.SDK_INT < Build.VERSION_CODES.S
                    || service.isAccessibilityTool());

            String serviceId = service.getId();
            sServiceIds[i++] = serviceId;

            // Try to canonicalize the component name.
            ComponentName componentName = ComponentName.unflattenFromString(serviceId);
            if (componentName != null) {
                runningServiceNames.add(componentName.flattenToShortString());
            } else {
                runningServiceNames.add(serviceId);
            }
        }

        // Update the user password show/speak preferences.
        int textShowPasswordSetting = Settings.System.getInt(
                context.getContentResolver(), Settings.System.TEXT_SHOW_PASSWORD, 1);
        sTextShowPasswordEnabled = textShowPasswordSetting == 1;

        int accessibilitySpeakPasswordSetting = Settings.Secure.getInt(
                context.getContentResolver(), Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD, 0);
        sAccessibilitySpeakPasswordEnabled = accessibilitySpeakPasswordSetting == 1;

        // Get the list of enabled accessibility services, from settings, in
        // case it's different.
        ArrayList<String> enabledServiceNames = new ArrayList<String>();
        String serviceNamesString = Settings.Secure.getString(
                context.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (serviceNamesString != null && !serviceNamesString.isEmpty()) {
            String[] serviceNames = serviceNamesString.split(":");
            for (String name : serviceNames) {
                // null or empty names can be skipped
                if (name == null || name.isEmpty()) continue;
                // Try to canonicalize the component name if possible.
                ComponentName componentName = ComponentName.unflattenFromString(name);
                if (componentName != null) {
                    enabledServiceNames.add(componentName.flattenToShortString());
                } else {
                    enabledServiceNames.add(name);
                }
            }
        }

        // Compare the list of enabled package names to the list of running package names.
        // When the system setting containing the list of running accessibility services
        // changes, it isn't always reflected in getEnabledAccessibilityServiceList
        // immediately. To ensure we always have an up-to-date value, check that the
        // set of services match, and if they don't, schedule an update with an exponential
        // back-off.
        Collections.sort(runningServiceNames);
        Collections.sort(enabledServiceNames);
        if (runningServiceNames.equals(enabledServiceNames)) {
            Log.v(TAG, "Enabled accessibility services list updated.");
            sNextDelayMillis = MIN_DELAY_MILLIS;
        } else {
            Log.v(TAG, "Enabled accessibility services: " + enabledServiceNames.toString());
            Log.v(TAG, "Running accessibility services: " + runningServiceNames.toString());
            Log.v(TAG, "Will check again after " + sNextDelayMillis + " milliseconds.");
            getHandler().postDelayed(() -> { updateAccessibilityServices(); }, sNextDelayMillis);
            if (sNextDelayMillis < MAX_DELAY_MILLIS) sNextDelayMillis *= 2;
        }

        // Update all listeners that there was a state change and pass whether or not the
        // new state includes a screen reader.
        sScreenReader = (0 != (sEventTypeMask & SCREEN_READER_EVENT_TYPE_MASK));
        for (Listener listener : sListeners) {
            Log.v(TAG, "Informing listeners of changes.");
            listener.onBrowserAccessibilityStateChanged(sScreenReader);
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
    public static int getAccessibilityServiceEventTypeMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sEventTypeMask;
    }

    /**
     * Return a bitmask containing the union of all feedback types that running accessibility
     * services provide.
     * @return
     */
    @CalledByNative
    public static int getAccessibilityServiceFeedbackTypeMask() {
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
    static void registerObservers() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();

        // We want to be notified whenever the user has updated the animator duration scale.
        contentResolver.registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false,
                new AnimatorDurationScaleObserver(getHandler()));

        // We want to be notified whenever the currently enabled services changes.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES), false,
                new AccessibilityServicesObserver(getHandler()));

        // We want to be notified if the user changes their preferred password show/speak settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD), false,
                new AccessibilityServicesObserver(getHandler()));
        contentResolver.registerContentObserver(
                Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD), false,
                new AccessibilityServicesObserver(getHandler()));

        if (!sInitialized) updateAccessibilityServices();
    }

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();
    }
}
