// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.IBinder;
import android.view.accessibility.AccessibilityEvent;

import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.ui.accessibility.testservice.IAccessibilityTestHelperService;
import org.chromium.ui.accessibility.testservice.WaitForEventParams;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for Accessibility end-to-end. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class WebContentsAccessibilityE2ETest {
    private static final String ACCESSIBILITY_TEST_SERVICE_PACKAGE =
            "org.chromium.ui.accessibility.testservice";
    private static final String ACCESSIBILITY_TEST_SERVICE_CLASS =
            "org.chromium.ui.accessibility.testservice.AccessibilityTestService";
    private static final String ACCESSIBILITY_TEST_HELPER_SERVICE_CLASS =
            "org.chromium.ui.accessibility.testservice.AccessibilityTestHelperService";
    private static final ComponentName ACCESSIBILITY_TEST_SERVICE_COMPONENT_NAME =
            new ComponentName(ACCESSIBILITY_TEST_SERVICE_PACKAGE, ACCESSIBILITY_TEST_SERVICE_CLASS);
    private static final ComponentName ACCESSIBILITY_TEST_HELPER_SERVICE_COMPONENT_NAME =
            new ComponentName(
                    ACCESSIBILITY_TEST_SERVICE_PACKAGE, ACCESSIBILITY_TEST_HELPER_SERVICE_CLASS);
    private static final String ACCESSIBILITY_TEST_SERVICE_NAME =
            ACCESSIBILITY_TEST_SERVICE_COMPONENT_NAME.flattenToString();
    private static final long BIND_TIMEOUT_MS = 5000;
    private static final long EVENT_TIMEOUT_MS = 5000;
    private static final String TAG = "WebContentsAccessibilityE2ETest";

    private final AtomicReference<CompletableFuture<IAccessibilityTestHelperService>>
            mServiceFuture = new AtomicReference<>(new CompletableFuture<>());

    @Rule
    public AccessibilityContentShellActivityTestRule mActivityTestRule =
            new AccessibilityContentShellActivityTestRule();

    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    // Ensure calls made in this block are thread safe.
                    mServiceFuture
                            .get()
                            .complete(IAccessibilityTestHelperService.Stub.asInterface(service));
                }

                @Override
                public void onServiceDisconnected(ComponentName arg0) {
                    // Ensure calls made in this block are thread safe.
                    mServiceFuture.set(new CompletableFuture<>());
                }
            };

    @Before
    public void setUp() throws IOException {
        enableAccessibilityService();
        ensureBoundToHelperService();
    }

    @After
    public void tearDown() throws IOException {
        disableAccessibilityService();
    }

    private void ensureBoundToHelperService() {
        if (mServiceFuture.get().isDone()) {
            return;
        }

        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
        intent.setComponent(ACCESSIBILITY_TEST_HELPER_SERVICE_COMPONENT_NAME);
        intent.setPackage(ACCESSIBILITY_TEST_SERVICE_PACKAGE);
        boolean bound =
                InstrumentationRegistry.getInstrumentation()
                        .getContext()
                        .bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
        Assert.assertTrue("Failed to bind to helper service", bound);
    }

    private IAccessibilityTestHelperService getAccessibilityHelperService()
            throws TimeoutException, InterruptedException, ExecutionException {
        return mServiceFuture.get().get(BIND_TIMEOUT_MS, TimeUnit.MILLISECONDS);
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
    public void testAccessibilityServiceReceivesInitialEvent() throws Throwable {
        // Load a page.
        String url = UrlUtils.encodeHtmlDataUri("<p>hello</p>");
        mActivityTestRule.launchContentShellWithUrl(url);

        // Wait for the window to appear.
        boolean wscReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)
                                        .build());
        Assert.assertTrue("Service did not receive WINDOW_STATE_CHANGED", wscReceived);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testAccessibilityServiceReceivesInitialEvent_SdkBalklavaAndAbove()
            throws Throwable {
        // Load a page.
        String url = UrlUtils.encodeHtmlDataUri("<p>hello</p>");
        mActivityTestRule.launchContentShellWithUrl(url);

        // Wait for the window to appear.
        boolean wscReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)
                                        .build());
        Assert.assertTrue("Service did not receive WINDOW_STATE_CHANGED", wscReceived);

        // Ask the service to wait for a text selection changed on the omnibox.
        boolean tscReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                        .setClassName("android.widget.EditText")
                                        .setText(url)
                                        .build());
        Assert.assertTrue("Service did not receive TEXT_SELECTION_CHANGED", tscReceived);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testAccessibilityServiceReceivesAccessibilityFocusEvent() throws Throwable {
        // Load a page with a focusable element.
        mActivityTestRule.launchContentShellWithUrl(
                UrlUtils.encodeHtmlDataUri("<button>Click Me</button>"));

        // Wait for the page to load by waiting for the initial TWCC.
        boolean initialEventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                        .setClassName("android.webkit.WebView")
                                        .build());
        Assert.assertTrue(
                "Service did not receive initial TYPE_WINDOW_CONTENT_CHANGED event",
                initialEventReceived);

        // Find the button and perform a focus action.
        boolean actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                "android.widget.Button",
                                "Click Me",
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        // Ask the service to wait for the event.
        boolean eventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                        .setClassName("android.widget.Button")
                                        .setText("Click Me")
                                        .build());
        Assert.assertTrue("Service did not receive accessibility focus event", eventReceived);
    }

    private static class WaitForEventParamsBuilder {
        private static final long DEFAULT_TIMEOUT_MS = 5000;

        private int mEventType;
        private String mClassName = "";
        private String mText = "";
        private final long mTimeoutMs = DEFAULT_TIMEOUT_MS;

        public WaitForEventParamsBuilder setEventType(int eventType) {
            mEventType = eventType;
            return this;
        }

        public WaitForEventParamsBuilder setClassName(String className) {
            mClassName = className;
            return this;
        }

        public WaitForEventParamsBuilder setText(String text) {
            mText = text;
            return this;
        }

        public WaitForEventParams build() {
            WaitForEventParams params = new WaitForEventParams();
            params.eventType = mEventType;
            params.className = mClassName;
            params.text = mText;
            params.timeoutMs = mTimeoutMs;
            return params;
        }
    }
}
