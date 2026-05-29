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
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.common.ContentInternalFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
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
    private static final String TAG = "WebContentsAXTest";

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

    private void waitForPageLoadAndInitialContentChange() throws Throwable {
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

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
        Assume.assumeTrue(
                "Requires Android 16 QPR2 (36.1) or higher",
                Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1);

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

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

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
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

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
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

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
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                        .setClassName("android.webkit.WebView")
                                        .build());
        Assert.assertTrue(
                "Service did not receive TYPE_VIEW_TEXT_SELECTION_CHANGED event",
                selectionEventReceived);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        String expectedDump =
"""
WebView focusable focused actions:[CLEAR_FOCUS, AX_FOCUS] bundle:[chromeRole="rootWebArea"] isInputFocusedViaFindFocus
  TextView text:"Some selected text" viewIdResName:"p1" actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="paragraph"] extendedSelectionStart:5 extendedSelectionEnd:13
""";
        Assert.assertEquals("Tree dump does not match expected value", expectedDump, treeDump);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.BAKLAVA)
    @DisabledTest(message = "https://crbug.com/517964367")
    public void testFindFocus() throws Throwable {
        // Load a page with 56 arbitrary buttons and two focusable elements and a tall div.
        // The idea behind 56 buttons comes from the flakyness of the test: we do a scroll to clear
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
                  for (let i = 0; i < 56; i++) {
                    document.body.appendChild(document.createElement('button'));
                  }
                </script>
                <button id='b1'>Input Focus</button>
                <button id='b2'>Accessibility Focus</button>
                <div style='height: 5000px;'></div>
                """;
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));

        // Initialize mWcax so we can make assertions on it.
        mActivityTestRule.mockWebContentsAccessibilityImpl();
        mActivityTestRule.mWcax = mActivityTestRule.getWebContentsAccessibility();
        mActivityTestRule.mWcax.setThrottleDelayForTesting(new java.util.HashMap<>());

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

        // Find the second button and perform an accessibility focus action.
        boolean actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                "android.widget.Button",
                                "Accessibility Focus",
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        boolean axEventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                        .setClassName("android.widget.Button")
                                        .setText("Accessibility Focus")
                                        .build());
        Assert.assertTrue("Service did not receive accessibility focus event", axEventReceived);

        // Focus the first button using JavaScript (input focus). We must do the input focus after
        // the accessibility focus, since the input focus is not fired as long as there was no
        // accessibility focus set.
        mActivityTestRule.executeJSAndGetResult("document.querySelector('#b1').focus()");

        // Wait for the input focus event.
        boolean inputFocusEventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED)
                                        .setClassName("android.widget.Button")
                                        .setText("Input Focus")
                                        .build());
        Assert.assertTrue("Service did not receive input focus event", inputFocusEventReceived);

        // Accessibility focus the second button since ({@link
        // org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl#handleFocusChanged}).
        // syncs the accessibility focus with the input focus.
        actionRes =
                getAccessibilityHelperService()
                        .performActionOnNode(
                                "android.widget.Button",
                                "Accessibility Focus",
                                AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
        Assert.assertTrue("Failed to perform accessibility focus action", actionRes);

        axEventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED)
                                        .setClassName("android.widget.Button")
                                        .setText("Accessibility Focus")
                                        .build());
        Assert.assertTrue("Service did not receive accessibility focus event", axEventReceived);

        // Scroll the page down to move both buttons off screen. This should trigger a scroll event
        // and clear the accessibility focus.
        mActivityTestRule.executeJSAndGetResult("window.scrollTo(0, 5000)");

        // Wait for scroll event
        boolean scrollEventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(AccessibilityEvent.TYPE_VIEW_SCROLLED)
                                        .setClassName("android.webkit.WebView")
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

        assertNodeLineExpectation(
                treeDump,
                "Button text:\"Input Focus\"",
                """
                Button text:"Input Focus" viewIdResName:"b1" clickable focusable focused actions:[CLEAR_FOCUS, CLICK, AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="button", clickableScore="300"] isInputFocusedViaFindFocus
                """);
        assertNodeLineExpectation(
                treeDump,
                "Button text:\"Accessibility Focus\"",
                """
                Button text:"Accessibility Focus" viewIdResName:"b2" clickable focusable actions:[FOCUS, CLICK, CLEAR_AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="button", clickableScore="300"] isAccessibilityFocusedViaFindFocus
                """);
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
        String url = UrlUtils.encodeHtmlDataUri(html);
        mActivityTestRule.launchContentShellWithUrl(url);

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

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

        boolean selectionReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED)
                                        .setClassName("android.webkit.WebView")
                                        .build());
        Assert.assertTrue("Service did not receive selection change event", selectionReceived);

        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        String expectedDump =
"""
WebView focusable actions:[FOCUS, AX_FOCUS] bundle:[chromeRole="rootWebArea"]
  EditText text:"Line one\\nLink text node" clickable editable focusable focused multiLine textSelectionStart:9 textSelectionEnd:10 actions:[CLEAR_FOCUS, CLICK, AX_FOCUS, NEXT, PREVIOUS, COPY, PASTE, CUT, SET_SELECTION, SET_TEXT, IME_ENTER] bundle:[chromeRole="genericContainer", clickableScore="200"] isInputFocusedViaFindFocus extendedSelectionStart:9 extendedSelectionEnd:10
    TextView text:"Line one" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"]
    View text:"\\n" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="lineBreak", clickableScore="100"]
    View text:"null" contentDescription:"Link text" viewIdResName:"link" clickable editable actions:[CLICK, AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="link", clickableScore="300", roleDescription="link", targetUrl="data:text/html;utf-8,%3Chtml%3E%3Cbody%3E%3Cdiv%20contenteditable%3E%0ALine%20one%3Cbr%3E%0A%3Ca%20id%3D%27link%27%20href%3D%27%23%27%3ELink%20text%3C%2Fa%3E%20node%0A%3C%2Fdiv%3E%3C%2Fbody%3E%3C%2Fhtml%3E%0A#"] extendedSelectionEnd:1
      TextView text:"Link text" editable actions:[AX_FOCUS, NEXT, PREVIOUS] bundle:[chromeRole="staticText", clickableScore="100"] extendedSelectionStart:0
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
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

        // Set aria-invalid="true" on the input element.
        mActivityTestRule.executeJSAndGetResult(
                "document.getElementById('input').setAttribute('aria-invalid', 'true');");

        // Wait for TWCC event with ContentChangeType CONTENT_INVALID to be fired as a result of the
        // invalid status changing.
        boolean eventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                        .setContentChangeTypes(
                                                AccessibilityEvent
                                                        .CONTENT_CHANGE_TYPE_CONTENT_INVALID)
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
        mActivityTestRule.launchContentShellWithUrl(UrlUtils.encodeHtmlDataUri(html));

        // Wait for the page to load and for the service to receive a content change.
        waitForPageLoadAndInitialContentChange();

        // Set aria-invalid="false" on the input element.
        mActivityTestRule.executeJSAndGetResult(
                "document.getElementById('input').setAttribute('aria-invalid', 'false');");

        // Wait for TWCC event with ContentChangeType CONTENT_INVALID to be fired as a result of the
        // invalid status changing.
        boolean eventReceived =
                getAccessibilityHelperService()
                        .waitForEvent(
                                new WaitForEventParamsBuilder()
                                        .setEventType(
                                                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
                                        .setContentChangeTypes(
                                                AccessibilityEvent
                                                        .CONTENT_CHANGE_TYPE_CONTENT_INVALID)
                                        .build());
        Assert.assertTrue("Service did not receive CONTENT_INVALID event", eventReceived);

        // Dump the accessibility tree.
        String treeDump = getAccessibilityHelperService().dumpWebContentsAccessibilityTree();

        // Verify that the input element's AccessibilityNodeInfo does not contain contentInvalid.
        Assert.assertFalse(
                "Tree dump should not contain 'contentInvalid'",
                treeDump.contains("contentInvalid"));
    }

    private void assertNodeLineExpectation(
            String treeDump, String nodeSelector, String nodeLineExpectation) {
        for (String line : treeDump.split("\\n")) {
            if (line.contains(nodeSelector)) {
                Assert.assertEquals(
                        "Node line matching '" + nodeSelector + "' is incorrect.",
                        nodeLineExpectation.trim(),
                        line.trim());
                return;
            }
        }
        Assert.fail("Node matching '" + nodeSelector + "' not found in tree dump.");
    }

    private static class WaitForEventParamsBuilder {
        private static final long DEFAULT_TIMEOUT_MS = 5000;

        private int mEventType;
        private String mClassName = "";
        private int mContentChangeTypes;
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

        public WaitForEventParamsBuilder setContentChangeTypes(int contentChangeTypes) {
            mContentChangeTypes = contentChangeTypes;
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
            params.contentChangeTypes = mContentChangeTypes;
            params.text = mText;
            params.timeoutMs = mTimeoutMs;
            return params;
        }
    }
}
