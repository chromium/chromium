// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;

import java.io.IOException;

/** Tests for Accessibility end-to-end. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsAccessibilityE2ETest {
    private static final String ACCESSIBILITY_TEST_SERVICE_NAME =
            "org.chromium.ui.accessibility.testservice/.AccessibilityTestService";
    private static final String ACTION_ACCESSIBILITY_EVENT =
            "org.chromium.ui.accessibility.testservice.ACCESSIBILITY_EVENT";

    private Context mContext;
    private boolean mReceivedIntent;

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    @Before
    public void setUp() throws IOException {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        enableAccessibilityService();
    }

    @After
    public void tearDown() throws IOException {
        disableAccessibilityService();
    }

    private void enableAccessibilityService() throws IOException {
        UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation()
                        .getUiAutomation(UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);

        // Adopt shell permissions so we can write to secure settings.
        uiAutomation.adoptShellPermissionIdentity(
                android.Manifest.permission.WRITE_SECURE_SETTINGS);

        try {
            // Enable the service via ADB shell command under the hood.
            uiAutomation
                    .executeShellCommand(
                            "settings put secure enabled_accessibility_services "
                                    + ACCESSIBILITY_TEST_SERVICE_NAME)
                    .close();
            uiAutomation.executeShellCommand("settings put secure accessibility_enabled 1").close();
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    private void disableAccessibilityService() throws IOException {
        UiAutomation uiAutomation =
                InstrumentationRegistry.getInstrumentation()
                        .getUiAutomation(UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);

        // Adopt shell permissions so we can write to secure settings.
        uiAutomation.adoptShellPermissionIdentity(
                android.Manifest.permission.WRITE_SECURE_SETTINGS);

        try {
            // Disable the service.
            uiAutomation
                    .executeShellCommand("settings delete secure enabled_accessibility_services")
                    .close();
            uiAutomation.executeShellCommand("settings put secure accessibility_enabled 0").close();
        } finally {
            uiAutomation.dropShellPermissionIdentity();
        }
    }

    @Test
    @SmallTest
    public void testAccessibilityServiceConnected() throws Throwable {
        // Register a receiver for the intent fired by the accessibility service.
        IntentFilter filter = new IntentFilter(ACTION_ACCESSIBILITY_EVENT);
        BroadcastReceiver receiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(Context context, Intent intent) {
                        mReceivedIntent = true;
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContext.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
                });

        try {
            // Showing this page will ensure our AccessibilityTestService receives events in
            // onAccessibilityEvent.
            mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri("<p>hello</p>"));

            // Wait for the intent to be received. The service should send an intent when it gets an
            // event.
            CriteriaHelper.pollUiThread(
                    () -> mReceivedIntent, "Did not receive intent from service.");
        } finally {
            // Unregister the receiver.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mContext.unregisterReceiver(receiver);
                    });
        }
    }
}
