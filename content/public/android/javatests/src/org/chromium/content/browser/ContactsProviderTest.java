// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.content_public.browser.ContactsPicker;
import org.chromium.content_public.browser.ContactsPickerListener;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.content_public.browser.test.util.FencedFrameUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests Contacts Web API functionality. */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ContactsProviderTest {
    private static final String TEST_URL = "/content/test/data/android/title1.html";
    private static final String FENCED_FRAME_URL =
            "/content/test/data/android/fenced_frames/title1.html";
    private static final String CONTACTS_SCRIPT =
            "var result = null;"
                    + "((async() => {"
                    + "  try {"
                    + "    const contacts = await navigator.contacts.select("
                    + "        ['name', 'email'], { multiple: true });"
                    + "    if (contacts) result = 'success'"
                    + "  } catch(e) { result = e.message; }"
                    + "})())";

    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    @Before
    public void setUp() {
        try {
            mActivityTestRule.launchContentShellWithUrlSync(TEST_URL);
        } catch (Throwable t) {
            throw new AssertionError("Couldn't load test page.", t);
        }
    }

    @After
    public void tearDown() {}

    private static String executeJavaScript(
            final RenderFrameHost frame, String js, boolean userGesture) {
        RenderFrameHostTestExt rfh =
                ThreadUtils.runOnUiThreadBlocking(() -> new RenderFrameHostTestExt(frame));
        final CountDownLatch latch = new CountDownLatch(1);
        final AtomicReference<String> result = new AtomicReference<String>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (userGesture) {
                        rfh.executeJavaScriptWithUserGesture(js);
                        latch.countDown();
                    } else {
                        rfh.executeJavaScript(
                                js,
                                (String r) -> {
                                    result.set(r);
                                    latch.countDown();
                                });
                    }
                });

        try {
            Assert.assertTrue(latch.await(CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return result.get();
    }

    private String hasValue(final RenderFrameHost frame) throws TimeoutException {
        return executeJavaScript(frame, "!!result", false);
    }

    private String getValue(final RenderFrameHost frame) {
        return executeJavaScript(frame, "result", false);
    }

    private void waitUntilHasValue(final RenderFrameHost frame) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(hasValue(frame), Matchers.equalTo("true"));
                    } catch (TimeoutException e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Tests that Contacts API gets contacts correctly with the user gesture in the primary page.
     */
    @Test
    @SmallTest
    public void testGetContactsInPrimaryPageWithUserGesture() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContactsPicker.setContactsPickerDelegate(
                            (WebContents webContents,
                                    ContactsPickerListener listener,
                                    boolean multiple,
                                    boolean names,
                                    boolean emails,
                                    boolean tels,
                                    boolean addresses,
                                    boolean icons,
                                    String formattedOrigin) -> {
                                List<ContactsPickerListener.Contact> contacts = new ArrayList();
                                List<String> contactsNames = new ArrayList();
                                contactsNames.add("test");
                                contacts.add(
                                        new ContactsPickerListener.Contact(
                                                contactsNames,
                                                /* contactEmails= */ null,
                                                /* contactTel= */ null,
                                                /* contactAddresses= */ null,
                                                /* contactIcons= */ null));

                                listener.onContactsPickerUserAction(
                                        ContactsPickerListener.ContactsPickerAction
                                                .CONTACTS_SELECTED,
                                        contacts,
                                        /* percentageShared= */ 0,
                                        /* propertiesSiteRequested= */ 0,
                                        /* propertiesUserRejected= */ 0);
                                return true;
                            });
                });

        RenderFrameHost frame =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getWebContents().getMainFrame());
        executeJavaScript(frame, CONTACTS_SCRIPT, true);
        waitUntilHasValue(frame);
        Assert.assertEquals("\"success\"", getValue(frame));
    }

    /**
     * Tests that Contacts API fails to get contacts without the user gesture in the primary page.
     */
    @Test
    @SmallTest
    public void testGetContactsInPrimaryPageWithoutUserGesture() throws Exception {
        RenderFrameHost frame =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getWebContents().getMainFrame());
        executeJavaScript(frame, CONTACTS_SCRIPT, false);
        waitUntilHasValue(frame);
        Assert.assertEquals(
                "\"Failed to execute 'select' on 'ContactsManager':"
                        + " A user gesture is required to call this method\"",
                getValue(frame));
    }

    /** Tests that Contacts API fails to get contacts with the user gesture in the fenced frame. */
    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=FencedFrames<Study,PrivacySandboxAdsAPIsOverride,FencedFramesAPIChanges,FencedFramesDefaultMode",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:implementation_type/mparch"
    })
    public void testDontGetContactsInFencedFrame() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        String url = testServer.getURL(FENCED_FRAME_URL);
        RenderFrameHost frame =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mActivityTestRule.getWebContents().getMainFrame());
        RenderFrameHost fencedFrame =
                FencedFrameUtils.createFencedFrame(mActivityTestRule.getWebContents(), frame, url);
        executeJavaScript(fencedFrame, CONTACTS_SCRIPT, true);
        waitUntilHasValue(fencedFrame);
        Assert.assertEquals(
                "\"Failed to execute 'select' on 'ContactsManager': The contacts API can only be"
                        + " used in the top frame\"",
                getValue(fencedFrame));
    }
}
