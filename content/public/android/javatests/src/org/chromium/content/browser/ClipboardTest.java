// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.support.test.filters.LargeTest;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.content_shell_apk.ContentShellActivityTestRule.RerunWithUpdatedContainerView;

import java.util.concurrent.Callable;

/**
 * Tests rich text clipboard functionality.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ClipboardTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String TEST_PAGE_DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><body>Hello, <a href=\"http://www.example.com/\">world</a>, how <b> "
                    + "Chromium</b> doing today?</body></html>");

    private static final String EXPECTED_TEXT_RESULT = "Hello, world, how Chromium doing today?";

    // String to search for in the HTML representation on the clipboard.
    private static final String EXPECTED_HTML_NEEDLE = "http://www.example.com/";

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrl(TEST_PAGE_DATA_URL);
        mActivityTestRule.waitForActiveShellToBeDoneLoading();
    }

    /**
     * Tests that copying document fragments will put at least a plain-text representation
     * of the contents on the clipboard. For Android JellyBean and higher, we also expect
     * the HTML representation of the fragment to be available.
     */
    @Test
    @LargeTest
    @Feature({"Clipboard", "TextInput"})
    @RerunWithUpdatedContainerView
    @DisabledTest(message = "https://crbug.com/791021")
    public void testCopyDocumentFragment() {
        ClipboardManager clipboardManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<ClipboardManager>() {
                    @Override
                    public ClipboardManager call() {
                        return (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                    }
                });

        Assert.assertNotNull(clipboardManager);

        // Clear the clipboard to make sure we start with a clean state.
        clipboardManager.setPrimaryClip(ClipData.newPlainText(null, ""));
        Assert.assertFalse(hasPrimaryClip(clipboardManager));

        final WebContentsImpl webContents = (WebContentsImpl) mActivityTestRule.getWebContents();
        selectAll(webContents);
        copy(webContents);

        // Waits until data has been made available on the Android clipboard.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return hasPrimaryClip(clipboardManager);
            }
        });

        // Verify that the data on the clipboard is what we expect it to be. For Android JB MR2
        // and higher we expect HTML content, for other versions the plain-text representation.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final ClipData clip = clipboardManager.getPrimaryClip();
            Assert.assertEquals(EXPECTED_TEXT_RESULT,
                    clip.getItemAt(0).coerceToText(mActivityTestRule.getActivity()));

            String htmlText = clip.getItemAt(0).getHtmlText();
            Assert.assertNotNull(htmlText);
            Assert.assertTrue(htmlText.contains(EXPECTED_HTML_NEEDLE));
        });
    }

    private void copy(final WebContentsImpl webContents) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { webContents.copy(); });
    }

    private void selectAll(final WebContentsImpl webContents) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { webContents.selectAll(); });
    }

    // Returns whether there is a primary clip with content on the current clipboard.
    private Boolean hasPrimaryClip(ClipboardManager clipboardManager) {
        final ClipData clip = clipboardManager.getPrimaryClip();
        if (clip != null && clip.getItemCount() > 0) {
            return !TextUtils.isEmpty(clip.getItemAt(0).getText());
        }

        return false;
    }
}
