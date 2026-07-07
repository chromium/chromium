// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sClassNameMatcher;
import static org.chromium.content.browser.accessibility.AccessibilityContentShellTestUtils.sViewIdResourceNameMatcher;

import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.Selection;
import android.view.accessibility.AccessibilityNodeInfo.SelectionPosition;

import androidx.annotation.Nullable;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Log;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.common.ContentInternalFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.ui.accessibility.testservice.EventMatcher;
import org.chromium.ui.accessibility.testservice.IAccessibilityTestHelperService;
import org.chromium.ui.accessibility.testservice.NodeMatcher;
import org.chromium.ui.accessibility.testservice.WaitForParams;
import org.chromium.ui.test.util.DeviceRestriction;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for Accessibility end-to-end. */
@Batch(Batch.PER_CLASS)
@RunWith(ContentJUnit4ClassRunner.class)
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
    private static final String TAG = "WebContentsAXTest";

    private static final String EXTRA_SELECTION_START_OFFSET_TYPE =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SELECTION_START_OFFSET_TYPE";
    private static final String EXTRA_SELECTION_END_OFFSET_TYPE =
            "androidx.view.accessibility.AccessibilityNodeInfoCompat.SELECTION_END_OFFSET_TYPE";

    // Extended selection offset types, defined in:
    // androidx.view.accessibility.AccessibilityNodeInfoCompat
    private static final int OFFSET_TYPE_TEXT = 0;
    private static final int OFFSET_TYPE_CHILD = 1;

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

    private boolean waitForEvent(EventMatcher matcher) {
        try {
            return getAccessibilityHelperService()
                    .waitFor(new WaitForParamsBuilder().setEventMatcher(matcher).build());
        } catch (Exception e) {
            Log.e(TAG, "Error waiting for event", e);
            return false;
        }
    }

    private boolean waitForNode(NodeMatcher matcher) {
        try {
            return getAccessibilityHelperService()
                    .waitFor(new WaitForParamsBuilder().setNodeMatcher(matcher).build());
        } catch (Exception e) {
            Log.e(TAG, "Error waiting for node", e);
            return false;
        }
    }

    private boolean waitForNodeOnEvent(EventMatcher eventMatcher, NodeMatcher nodeMatcher) {
        try {
            return getAccessibilityHelperService()
                    .waitFor(
                            new WaitForParamsBuilder()
                                    .setEventMatcher(eventMatcher)
                                    .setNodeMatcher(nodeMatcher)
                                    .build());
        } catch (Exception e) {
            Log.e(TAG, "Error waiting for node on event", e);
            return false;
        }
    }

    private void setupTest(String html, NodeMatcher matcher) throws Throwable {
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));
        mActivityTestRule.mockWebContentsAccessibilityImpl();
        mActivityTestRule.mWcax = mActivityTestRule.getWebContentsAccessibility();
        mActivityTestRule.mWcax.setThrottleDelayForTesting(new java.util.HashMap<>());

        if (matcher != null) {
            boolean nodeFound = waitForNode(matcher);
            Assert.assertTrue("Failed to find expected node after HTML setup.", nodeFound);
        }
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

    private void initializeMockWebContentsAccessibility() {
        // Initialize mWcax as a Mockito mock so that we can verify performAction interactions on
        // it.
        mActivityTestRule.mockWebContentsAccessibilityImpl();
        org.chromium.base.ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.mWcax = mActivityTestRule.getWebContentsAccessibility();
                });
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.mWcax.getAccessibilityNodeProviderCompat() != null,
                "AccessibilityNodeProvider is null");
        mActivityTestRule.mNodeProvider =
                mActivityTestRule.mWcax.getAccessibilityNodeProviderCompat();
    }

    private Bundle createSelectionArgs(
            int startVvid,
            int startOffset,
            int startOffsetType,
            int endVvid,
            int endOffset,
            int endOffsetType) {
        SelectionPosition startPosition =
                new SelectionPosition(mActivityTestRule.getContainerView(), startVvid, startOffset);
        SelectionPosition endPosition =
                new SelectionPosition(mActivityTestRule.getContainerView(), endVvid, endOffset);
        Selection selection = new Selection(startPosition, endPosition);

        Bundle args = new Bundle();
        args.putParcelable(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_PARCELABLE, selection);
        args.putInt(EXTRA_SELECTION_START_OFFSET_TYPE, startOffsetType);
        args.putInt(EXTRA_SELECTION_END_OFFSET_TYPE, endOffsetType);
        return args;
    }

    @Test
    @SmallTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO) // crbug.com/529881530
    public void testAccessibilityServiceReceivesInitialEvent() throws Throwable {
        // Load a page.
        setupTest("<p>hello</p>", new NodeMatcherBuilder().setText("hello").build());

        // Wait for the window to appear.
        boolean wscReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)
                                .build());
        Assert.assertTrue("Service did not receive WINDOW_STATE_CHANGED", wscReceived);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testAccessibilityServiceReceivesInitialEvent_SdkBalklavaAndAbove()
            throws Throwable {
        Assume.assumeTrue(
                "Requires Android 16 QPR2 (36.1) or higher",
                Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1);

        // Load a page.
        String html = "<p>hello</p>";
        setupTest(html, new NodeMatcherBuilder().setText("hello").build());
        String url = UrlUtils.encodeHtmlDataUri(html);

        // Wait for the window to appear.
        boolean wscReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED)
                                .build());
        Assert.assertTrue("Service did not receive WINDOW_STATE_CHANGED", wscReceived);

        // Ask the service to wait for a text selection changed on the omnibox.
        boolean tscReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.EditText")
                                                .setText(url)
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive TEXT_SELECTION_CHANGED", tscReceived);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testAccessibilityServiceReceivesAccessibilityFocusEvent() throws Throwable {
        // Load a page with a focusable element.
        setupTest(
                "<button>Click Me</button>", new NodeMatcherBuilder().setText("Click Me").build());

        // Find the button and perform a focus action.
        boolean actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                new NodeMatcherBuilder()
                                        .setClassName("android.widget.Button")
                                        .setText("Click Me")
                                        .build(),
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS,
                                /* arguments= */ null);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        // Ask the service to wait for the event.
        boolean eventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.Button")
                                                .setText("Click Me")
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive accessibility focus event", eventReceived);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    public void testDumpWebContentsAccessibilityTree() throws Throwable {
        // Load a page with more complex HTML content.
        String html =
                """
                <h1>Heading</h1>
                <p>Some text</p>
                <button>Click Me</button>
                <div><a href="#">Link</a></div>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Heading").build());

        // Wait for the scroll event to be fired. There is a scroll event fired once the
        // page loads, after which we guarantee the isInputFocusedViaFindFocus annotation
        // will be present.
        boolean nodeFound =
                waitForNodeOnEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_SCROLLED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.FrameLayout")
                                                .build())
                                .build(),
                        new NodeMatcherBuilder()
                                .setClassName("android.webkit.WebView")
                                .setInputFocused(true)
                                .build());

        Assert.assertTrue(
                "Expected node with text 'Line one' and input focus was not found.", nodeFound);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();
        String expectedDump =
"""
WebView focusable focused actions:[CLEAR_FOCUS, AX_FOCUS] bundle:[chromeRole="rootWebArea"] isInputFocusedViaFindFocus
  TextView text:"Heading" heading actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="heading", roleDescription="heading 1"]
  TextView text:"Some text" actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="paragraph"]
  Button text:"Click Me" clickable focusable actions:[FOCUS, CLICK, AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="button", clickableScore="300"]
  View actions:[AX_FOCUS] bundle:[chromeRole="genericContainer"]
    View text:"null" contentDescription:"Link" clickable focusable actions:[FOCUS, CLICK, AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="link", clickableScore="300", roleDescription="link", targetUrl="data:text/html;utf-8,%3Ch1%3EHeading%3C%2Fh1%3E%0A%3Cp%3ESome%20text%3C%2Fp%3E%0A%3Cbutton%3EClick%20Me%3C%2Fbutton%3E%0A%3Cdiv%3E%3Ca%20href%3D%22%23%22%3ELink%3C%2Fa%3E%3C%2Fdiv%3E%0A#"]
      TextView text:"Link" actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"]
""";
        Assert.assertEquals("Tree dump does not match expected value", expectedDump, treeDump);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_EXTENDED_SELECTION})
    public void testDumpTreeWithInitialSelection() throws Throwable {
        Assume.assumeTrue(
                "Requires Android 16 QPR2 (36.1) or higher",
                Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1);

        // Load a page with an initial selection.
        String html =
                """
                <p id="p1">Some selected text</p>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Some selected text").build());

        // Inject script to set the selection.
        String script =
                """
                  var range = document.createRange();
                  var p1 = document.getElementById("p1").firstChild;
                  range.setStart(p1, 5);
                  range.setEnd(p1, 13);
                  window.getSelection().removeAllRanges();
                  window.getSelection().addRange(range);
                """;
        mActivityTestRule.executeJSAndGetResult(script);

        // Wait for the selection event to be fired.
        boolean selectionEventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.webkit.WebView")
                                                .build())
                                .build());
        Assert.assertTrue(
                "Service did not receive TYPE_VIEW_TEXT_SELECTION_CHANGED event",
                selectionEventReceived);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        String expectedDump =
"""
WebView focusable focused actions:[CLEAR_FOCUS, AX_FOCUS] bundle:[chromeRole="rootWebArea"] isInputFocusedViaFindFocus
  TextView text:"Some selected text" viewIdResName:"p1" actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="paragraph"] extendedSelectionStart:5 (text) extendedSelectionEnd:13 (text)
""";
        Assert.assertEquals("Tree dump does not match expected value", expectedDump, treeDump);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    @DisabledTest(message = "https://crbug.com/517964367")
    public void testFindFocus() throws Throwable {
        // Load a page with 100 arbitrary buttons and two focusable elements and a tall div.
        // The idea behind 100 buttons comes from the flakiness of the test: we do a scroll to clear
        // cache focus but somehow there is a race condition where the cache gets refilled just
        // after the scroll event is fired. The most probable responsible is the logic in
        // ({frameworks/base/core/java/android/view/AccessibilityInteractionController.java.AccessibilityNodePrefetcher})
        // which prefetches nodes for optimization purposes. Most probably we are retrieving the
        // root node from the client and it gets prefetched along the very few nodes that are part
        // of this test. The max prefetching count is 50 as displayed in
        // ({frameworks/base/core/java/android/view/accessibility/AccessibilityNodeInfo.java.java.AccessibilityNodeInfo#MAX_PREFETCH_COUNT}).
        // so that is the reason the tests starts with so many arbitrary buttons.
        // The tall div on the bottom allows scrolling.
        String html =
                """
                <script>
                  for (let i = 0; i < 100; i++) {
                    document.body.appendChild(document.createElement('button'));
                  }
                </script>
                <button id='b1'>Input Focus</button>
                <button id='b2'>Accessibility Focus</button>
                <div style='height: 5000px;'></div>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Input Focus").build());

        // Find the second button and perform an accessibility focus action.
        boolean actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                new NodeMatcherBuilder()
                                        .setClassName("android.widget.Button")
                                        .setText("Accessibility Focus")
                                        .build(),
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS,
                                /* arguments= */ null);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        boolean axEventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.Button")
                                                .setText("Accessibility Focus")
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive accessibility focus event", axEventReceived);

        // Focus the first button using JavaScript (input focus). We must do the input focus after
        // the accessibility focus, since the input focus is not fired as long as there was no
        // accessibility focus set.
        mActivityTestRule.executeJSAndGetResult("document.querySelector('#b1').focus()");

        // Wait for the input focus event.
        boolean inputFocusEventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.Button")
                                                .setText("Input Focus")
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive input focus event", inputFocusEventReceived);

        // Accessibility focus the second button since ({@link
        // org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl#handleFocusChanged}).
        // syncs the accessibility focus with the input focus.
        actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                new NodeMatcherBuilder()
                                        .setClassName("android.widget.Button")
                                        .setText("Accessibility Focus")
                                        .build(),
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS,
                                /* arguments= */ null);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        axEventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.Button")
                                                .setText("Accessibility Focus")
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive accessibility focus event", axEventReceived);

        // Scroll the page down to move both buttons off screen. This should trigger a scroll event
        // and clear the accessibility focus.
        mActivityTestRule.executeJSAndGetResult("window.scrollTo(0, 5000)");

        // Wait for scroll event
        boolean scrollEventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_SCROLLED)
                                .build());
        Assert.assertTrue("Service did not receive scroll event", scrollEventReceived);

        // Dump the tree and verify both types of focus.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        // The Android framework ({@link android.view.ViewRootImpl}) explicitly tracks accessibility
        // focus by ID. {@link android.view.AccessibilityInteractionController} handles
        // accessibility focus through this tracking and will skip calling findFocus(int) on the
        // provider. Instead, it will directly call createAccessibilityNodeInfo(int) for that ID.
        // Therefore, we shouldn't expect findFocus(FOCUS_ACCESSIBILITY) to be called.
        Mockito.verify(
                        mActivityTestRule.mWcax,
                        Mockito.never()
                                .description(
                                        "Accessibility focus findFocus should not be called due to"
                                                + " framework optimization"))
                .findFocus(AccessibilityNodeInfoCompat.FOCUS_ACCESSIBILITY);
        // Input focus is not tracked by virtual ID in the framework, so it must always query it via
        // {@link android.view.accessibility.AccessibilityNodeProvider#findFocus(int)}.
        Mockito.verify(
                        mActivityTestRule.mWcax,
                        Mockito.atLeastOnce()
                                .description(
                                        "Input focus findFocus was not called on"
                                                + " WebContentsAccessibilityImpl"))
                .findFocus(AccessibilityNodeInfoCompat.FOCUS_INPUT);

        boolean inputFocus =
                waitForNode(
                        new NodeMatcherBuilder()
                                .setText("Input Focus")
                                .setInputFocused(true)
                                .build());
        Assert.assertTrue("Input focus was not set on the expected button", inputFocus);
        boolean accessibilityFocus =
                waitForNode(
                        new NodeMatcherBuilder()
                                .setText("Accessibility Focus")
                                .setAccessibilityFocused(true)
                                .build());
        Assert.assertTrue(
                "Accessibility focus was not set on the expected button", accessibilityFocus);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    @EnableFeatures({
        ContentInternalFeatures.ACCESSIBILITY_EXPOSE_NON_ATOMIC_TEXT_FIELD_CHILDREN,
        ContentFeatureList.ACCESSIBILITY_EXTENDED_SELECTION
    })
    public void testSelectionInContentEditable() throws Throwable {
        Assume.assumeTrue(
                "Requires Android 16 QPR2 (36.1) or higher",
                Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1);

        // Load a page with a contenteditable containing a line break and a link.
        String html =
                """
                <html><body><div contenteditable>
                Line one<br>
                <a id='link' href='#'>Link text</a> node
                </div></body></html>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Line one").build());

        // Set selection in the contenteditable via JS.
        mActivityTestRule.executeJSAndGetResult(
                """
                const link = document.getElementById('link');
                const range = document.createRange();
                range.selectNodeContents(link);
                const selection = window.getSelection();
                selection.removeAllRanges();
                selection.addRange(range);
                """);

        boolean nodeFound =
                waitForNodeOnEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.webkit.WebView")
                                                .build())
                                .build(),
                        new NodeMatcherBuilder()
                                .setText("Line one\nLink text node")
                                .setInputFocused(true)
                                .build());
        Assert.assertTrue(
                "Expected node with text 'Line one' and input focus was not found.", nodeFound);

        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();
        String expectedDump =
"""
WebView focusable actions:[FOCUS, AX_FOCUS] bundle:[chromeRole="rootWebArea"]
  EditText text:"Line one\\nLink text node" clickable editable focusable focused multiLine textSelectionStart:9 textSelectionEnd:10 actions:[CLEAR_FOCUS, CLICK, AX_FOCUS, NEXT, PREVIOUS, COPY, PASTE, CUT, SET_SELECTION, SET_TEXT, IME_ENTER] bundle:[chromeRole="genericContainer", clickableScore="200"] isInputFocusedViaFindFocus extendedSelectionStart:9 extendedSelectionEnd:10
    TextView text:"Line one" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"]
    View text:"\\n" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="lineBreak", clickableScore="100"]
    View text:"null" contentDescription:"Link text" viewIdResName:"link" clickable editable actions:[CLICK, AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="link", clickableScore="300", roleDescription="link", targetUrl="data:text/html;utf-8,%3Chtml%3E%3Cbody%3E%3Cdiv%20contenteditable%3E%0ALine%20one%3Cbr%3E%0A%3Ca%20id%3D%27link%27%20href%3D%27%23%27%3ELink%20text%3C%2Fa%3E%20node%0A%3C%2Fdiv%3E%3C%2Fbody%3E%3C%2Fhtml%3E%0A#"] extendedSelectionEnd:1 (child)
      TextView text:"Link text" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"] extendedSelectionStart:0 (text)
    TextView text:" node" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"]
""";
        Assert.assertEquals("Tree dump does not match expected value", expectedDump, treeDump);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE) // API Level 34
    public void fireGeneratedEvent_ariaInvalidTrue_firesContentInvalid() throws Throwable {
        // Create an HTML document where there is an input element and an element containing the
        // text for the input's aria-errormessage.
        String html =
                """
                <html><body>
                <input type="text" id="input" aria-errormessage="err" aria-label="Name">
                <div id="err">Invalid Name</div>
                </body></html>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Invalid Name").build());

        // Set aria-invalid="true" on the input element.
        mActivityTestRule.executeJSAndGetResult(
                "document.getElementById('input').setAttribute('aria-invalid', 'true');");

        // Wait for TWCC event with ContentChangeType CONTENT_INVALID to be fired as a result of the
        // invalid status changing.
        boolean eventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                .setContentChangeTypes(
                                        AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_INVALID)
                                .build());
        Assert.assertTrue("Service did not receive CONTENT_INVALID event", eventReceived);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        // Verify that the input element's AccessibilityNodeInfo has contentInvalid set to true.
        Assert.assertTrue(
                "Tree dump should contain 'contentInvalid'", treeDump.contains("contentInvalid"));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE) // API Level 34
    public void fireGeneratedEvent_ariaInvalidChangesToFalse_firesContentInvalid()
            throws Throwable {
        // Create an HTML document where there is an input element and an element containing
        // the text for the input's aria-errormessage.
        String html =
                """
                <html><body>
                <input type="text" id="input" aria-errormessage="err" aria-invalid="true" aria-label="Name">
                <div id="err">Invalid Name</div>
                </body></html>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Invalid Name").build());

        // Set aria-invalid="false" on the input element.
        mActivityTestRule.executeJSAndGetResult(
                "document.getElementById('input').setAttribute('aria-invalid', 'false');");

        // Wait for TWCC event with ContentChangeType CONTENT_INVALID to be fired as a result of the
        // invalid status changing.
        boolean eventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                .setContentChangeTypes(
                                        AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_INVALID)
                                .build());
        Assert.assertTrue("Service did not receive CONTENT_INVALID event", eventReceived);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        // Verify that the input element's AccessibilityNodeInfo does not contain contentInvalid.
        Assert.assertFalse(
                "Tree dump should not contain 'contentInvalid'",
                treeDump.contains("contentInvalid"));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    @EnableFeatures({ContentFeatureList.ACCESSIBILITY_EXTENDED_SELECTION})
    public void testExtendedSelection() throws Throwable {
        Assume.assumeTrue(
                "Requires Android 16 QPR2 (36.1) or higher",
                Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1);

        String html =
                """
                <p id="p1">Paragraph1</p>
                <button>Button</button>
                <p id="p2">Paragraph2</p>
                """;
        setupTest(html, new NodeMatcherBuilder().setClassName("android.webkit.WebView").build());

        // Initialize the Mockito mock for WebContentsAccessibilityImpl.
        initializeMockWebContentsAccessibility();

        // Find nodes.
        int rootVvid =
                mActivityTestRule.waitForNodeMatching(sClassNameMatcher, "android.webkit.WebView");
        int paragraph1Vvid =
                mActivityTestRule.waitForNodeMatching(sViewIdResourceNameMatcher, "p1");
        int paragraph2Vvid =
                mActivityTestRule.waitForNodeMatching(sViewIdResourceNameMatcher, "p2");

        Bundle args =
                createSelectionArgs(
                        paragraph1Vvid, 0, OFFSET_TYPE_TEXT, paragraph2Vvid, 5, OFFSET_TYPE_TEXT);

        boolean actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                new NodeMatcherBuilder()
                                        .setClassName("android.webkit.WebView")
                                        .build(),
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SET_EXTENDED_SELECTION
                                        .getId(),
                                args);
        Assert.assertTrue("Failed to perform set extended selection action", actionRes);

        // Verify Mockito interaction on mWcax.
        Mockito.verify(mActivityTestRule.mWcax)
                .performAction(
                        Mockito.eq(rootVvid),
                        Mockito.eq(
                                AccessibilityNodeInfoCompat.AccessibilityActionCompat
                                        .ACTION_SET_EXTENDED_SELECTION
                                        .getId()),
                        Mockito.any());

        // Verify the selection is applied correctly on the native side by retrieving it.
        org.chromium.base.test.util.CriteriaHelper.pollUiThread(
                () -> {
                    org.chromium.base.test.util.Criteria.checkThat(
                            mActivityTestRule.mWcax.getExtendedSelection(rootVvid),
                            org.hamcrest.Matchers.notNullValue());
                });

        Object[] selectionResult =
                org.chromium.base.ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.mWcax.getExtendedSelection(rootVvid));
        Assert.assertNotNull("Extended selection should not be null", selectionResult);

        AccessibilityNodeInfoCompat startNode =
                (AccessibilityNodeInfoCompat)
                        selectionResult[WebContentsAccessibilityImpl.EXT_SEL_START_NODE];
        int startOffset = (int) selectionResult[WebContentsAccessibilityImpl.EXT_SEL_START_OFFSET];
        int startOffsetType =
                (int) selectionResult[WebContentsAccessibilityImpl.EXT_SEL_START_OFFSET_TYPE];
        AccessibilityNodeInfoCompat endNode =
                (AccessibilityNodeInfoCompat)
                        selectionResult[WebContentsAccessibilityImpl.EXT_SEL_END_NODE];
        int endOffset = (int) selectionResult[WebContentsAccessibilityImpl.EXT_SEL_END_OFFSET];
        int endOffsetType =
                (int) selectionResult[WebContentsAccessibilityImpl.EXT_SEL_END_OFFSET_TYPE];

        Assert.assertEquals("Start offset should be 0", 0, startOffset);
        Assert.assertEquals("End offset should be 5", 5, endOffset);
        Assert.assertEquals("Start offset type should be text", OFFSET_TYPE_TEXT, startOffsetType);
        Assert.assertEquals("End offset type should be text", OFFSET_TYPE_TEXT, endOffsetType);
        Assert.assertEquals("Paragraph1", startNode.getText().toString());
        Assert.assertEquals("Paragraph2", endNode.getText().toString());
    }

    @Test
    @SmallTest
    public void fireGeneratedEvent_defaultActionVerbChanged_firesContentChanged() throws Throwable {
        // Create an HTML document with a disabled button (default action verb of NONE).
        String html =
                """
                <html><body>
                <button id="button" disabled>Click Me</button>
                </body></html>
                """;
        setupTest(html, new NodeMatcherBuilder().setText("Click Me").build());

        // Enable the button by removing the disabled attribute. This will change the default action
        // verb of the button, as it is now clickable.
        mActivityTestRule.executeJSAndGetResult(
                "document.getElementById('button').removeAttribute('disabled');");

        // Wait for TWCC event with ContentChangeType UNDEFINED to be fired as a result of the
        // default action verb changing.
        boolean eventReceived =
                waitForEvent(
                        new EventMatcherBuilder()
                                .setEventType(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                .setContentChangeTypes(
                                        AccessibilityEvent.CONTENT_CHANGE_TYPE_UNDEFINED)
                                .setSourceMatcher(
                                        new NodeMatcherBuilder()
                                                .setClassName("android.widget.Button")
                                                .setText("Click Me")
                                                .build())
                                .build());
        Assert.assertTrue("Service did not receive WINDOW_CONTENT_CHANGED event", eventReceived);

        // Dump the accessibility tree, and verify that the button's AccessibilityNodeInfo now has
        // actions including CLICK.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();
        Assert.assertTrue("Tree dump should contain CLICK action", treeDump.contains("CLICK"));
    }

    private static class WaitForParamsBuilder {
        private static final long DEFAULT_TIMEOUT_MS = 5000;

        @Nullable private EventMatcher mEventMatcher;
        @Nullable private NodeMatcher mNodeMatcher;
        private final long mTimeoutMs = DEFAULT_TIMEOUT_MS;

        public WaitForParamsBuilder setEventMatcher(EventMatcher eventMatcher) {
            mEventMatcher = eventMatcher;
            return this;
        }

        public WaitForParamsBuilder setNodeMatcher(NodeMatcher nodeMatcher) {
            mNodeMatcher = nodeMatcher;
            return this;
        }

        public WaitForParams build() {
            WaitForParams matcher = new WaitForParams();
            matcher.eventMatcher = mEventMatcher;
            matcher.nodeMatcher = mNodeMatcher;
            matcher.timeoutMs = mTimeoutMs;
            return matcher;
        }
    }

    @SuppressWarnings("unused")
    private static class EventMatcherBuilder {
        private static final long DEFAULT_TIMEOUT_MS = 5000;

        private int mEventType;
        private int mContentChangeTypes;
        @Nullable private NodeMatcher mSourceMatcher;

        public EventMatcherBuilder setEventType(int eventType) {
            mEventType = eventType;
            return this;
        }

        public EventMatcherBuilder setContentChangeTypes(int contentChangeTypes) {
            mContentChangeTypes = contentChangeTypes;
            return this;
        }

        public EventMatcherBuilder setSourceMatcher(NodeMatcher sourceMatcher) {
            mSourceMatcher = sourceMatcher;
            return this;
        }

        public EventMatcher build() {
            EventMatcher matcher = new EventMatcher();
            matcher.eventType = mEventType;
            matcher.contentChangeTypes = mContentChangeTypes;
            matcher.sourceMatcher = mSourceMatcher;
            return matcher;
        }
    }

    @SuppressWarnings("unused")
    private static class NodeMatcherBuilder {
        private static final long DEFAULT_TIMEOUT_MS = 5000;

        private String mClassName = "";
        private String mText = "";
        private Boolean mInputFocused;
        private Boolean mAccessibilityFocused;

        public NodeMatcherBuilder setClassName(String className) {
            mClassName = className;
            return this;
        }

        public NodeMatcherBuilder setText(String text) {
            mText = text;
            return this;
        }

        public NodeMatcherBuilder setInputFocused(boolean inputFocused) {
            mInputFocused = inputFocused;
            return this;
        }

        public NodeMatcherBuilder setAccessibilityFocused(boolean accessibilityFocused) {
            mAccessibilityFocused = accessibilityFocused;
            return this;
        }

        public NodeMatcher build() {
            NodeMatcher matcher = new NodeMatcher();
            matcher.className = mClassName;
            matcher.text = mText;
            matcher.hasInputFocused = mInputFocused != null;
            matcher.inputFocused = mInputFocused != null ? mInputFocused : false;
            matcher.hasAccessibilityFocused = mAccessibilityFocused != null;
            matcher.accessibilityFocused =
                    mAccessibilityFocused != null ? mAccessibilityFocused : false;
            return matcher;
        }
    }
}
