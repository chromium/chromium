// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;

import org.chromium.ui.accessibility.AccessibilityState;

/**
 * Helper class for Autofill state and password preferences for accessibility related code.
 */
public class AccessibilityAutofillHelper {
    public static boolean shouldRespectDisplayedPasswordText() {
        // Previous to O, shouldExposePasswordText() should be used to check a system setting
        // that determines whether we should return the unobscured password or all dots,
        // independent of what was displayed visually. Always return false here before Android O.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;

        // On Android O and higher, we should respect whatever is displayed in a password box and
        // report that via accessibility APIs, whether that's the unobscured password, or all dots.
        // However, we deviate from this rule if the only consumer of accessibility information is
        // Autofill in order to allow third-party Autofill services to save the real, unmasked
        // password.
        return isAutofillOnlyPossibleAccessibilityConsumer();
    }

    public static boolean shouldExposePasswordText() {
        // On Android P and higher, when no other accessibility services are running other than
        // Autofill, we should always expose the actual password text so that third-party Autofill
        // services can save it rather than obtain only the masking characters.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                && isAutofillOnlyPossibleAccessibilityConsumer()) {
            return true;
        }

        // When additional services are running besides Autofill, we fall back to checking the
        // user's system preference. The preference to check varies by OS version.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return AccessibilityState.isTextShowPasswordEnabled();
        }

        return AccessibilityState.isAccessibilitySpeakPasswordEnabled();
    }

    public static boolean isAutofillOnlyPossibleAccessibilityConsumer() {
        // The Android Autofill CompatibilityBridge, which is responsible for translating
        // Accessibility information to Autofill events, directly hooks into the
        // AccessibilityManager via an AccessibilityPolicy rather than by running an
        // AccessibilityService. We can thus check whether it is the only consumer of Accessibility
        // information by reading the names of active accessibility services from settings.
        //
        // Note that the CompatibilityBridge makes getEnabledAccessibilityServicesList return a mock
        // service to indicate its presence. It is thus easier to read the setting directly than
        // to filter out this service from the returned list. Furthermore, since Accessibility is
        // only initialized if there is at least one actual service or if Autofill is enabled,
        // there is no need to check that Autofill is enabled here.
        //
        // https://cs.android.com/android/platform/superproject/+/HEAD:frameworks/base/core/java/android/view/autofill/AutofillManager.java;l=2817;drc=dd7d52f9632a0dbb8b14b69520c5ea31e0b3b4a2

        // Use the AccessibilityState to verify if >= 1 service(s) is/are running.
        return !AccessibilityState.isAnyAccessibilityServiceEnabled();
    }
}