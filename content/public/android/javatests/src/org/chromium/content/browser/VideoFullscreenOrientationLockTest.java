// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the feature that auto locks the orientation when a video goes fullscreen.
 * See also chrome layer org.chromium.chrome.browser.VideoFullscreenOrientationLockChromeTest
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
public class VideoFullscreenOrientationLockTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String TEST_URL = "content/test/data/media/video-player.html";
    private static final String VIDEO_ID = "video";

    private void waitForContentsFullscreenState(boolean fullscreenValue) {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(fullscreenValue, new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws TimeoutException {
                        return DOMUtils.isFullscreen(mActivityTestRule.getWebContents());
                    }
                }));
    }

    private boolean isScreenOrientationLocked() {
        return mActivityTestRule.getActivity().getRequestedOrientation()
                != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    }

    private boolean isScreenOrientationLandscape() throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  return  screen.orientation.type.startsWith('landscape');");
        sb.append("})();");

        return JavaScriptUtils
                .executeJavaScriptAndWaitForResult(
                        mActivityTestRule.getWebContents(), sb.toString())
                .equals("true");
    }

    private void waitUntilLockedToLandscape() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return isScreenOrientationLocked() && isScreenOrientationLandscape();
                } catch (TimeoutException e) {
                    return false;
                }
            }
        });
    }

    private void waitUntilUnlocked() {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !isScreenOrientationLocked();
            }
        });
    }

    // TODO(mlamouri): move these constants and bounds  methods to a dedicated helper file for
    // media tests.
    private static final int TIMELINE_HEIGHT = 24;
    private static final int BUTTON_PANEL_HEIGHT = 48;
    private static final int BUTTON_WIDTH = 48;

    private Rect buttonPanelBounds(Rect videoRect) {
        int left = videoRect.left;
        int right = videoRect.right;
        int bottom = videoRect.bottom - TIMELINE_HEIGHT;
        int top = bottom - BUTTON_PANEL_HEIGHT;
        return new Rect(left, top, right, bottom);
    }

    private Rect fullscreenButtonBounds(Rect videoRect) {
        Rect panel = buttonPanelBounds(videoRect);

        // In these tests, we have no overflow items, so the fullscreen button is the rightmost
        // button in the panel.
        int right = panel.right;
        int left = right - BUTTON_WIDTH;
        return new Rect(left, panel.top, right, panel.bottom);
    }

    private boolean clickFullscreenButton() throws TimeoutException {
        return DOMUtils.clickRect(mActivityTestRule.getWebContents(),
                fullscreenButtonBounds(
                        DOMUtils.getNodeBounds(mActivityTestRule.getWebContents(), VIDEO_ID)));
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchContentShellWithUrlSync(TEST_URL);
    }

    @Test
    @MediumTest
    @DisableIf.Build(message = "crbug.com/837423", sdk_is_greater_than = Build.VERSION_CODES.KITKAT,
            sdk_is_less_than = Build.VERSION_CODES.M)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testEnterExitFullscreenWithControlsButton() throws Exception {
        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VIDEO_ID);

        // Simulate click on fullscreen button.
        Assert.assertTrue(clickFullscreenButton());
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Because of the fullscreen animation, the click on the exit fullscreen button will fail
        // roughly 10% of the time. Settling down the UI reduces the flake to 0%.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // Leave fullscreen by clicking back on the button.
        Assert.assertTrue(clickFullscreenButton());
        waitForContentsFullscreenState(false);
        waitUntilUnlocked();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testEnterExitFullscreenWithAPI() throws Exception {
        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(mActivityTestRule.getWebContents(), "fullscreen"));
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Leave fullscreen from API.
        DOMUtils.exitFullscreen(mActivityTestRule.getWebContents());
        waitForContentsFullscreenState(false);
        waitUntilUnlocked();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testExitFullscreenByRemovingVideo() throws Exception {
        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(mActivityTestRule.getWebContents(), "fullscreen"));
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Leave fullscreen by removing video element from page.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "document.body.innerHTML = '';");
        waitForContentsFullscreenState(false);
        waitUntilUnlocked();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testExitFullscreenWithNavigation() throws Exception {
        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VIDEO_ID);

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(DOMUtils.clickNode(mActivityTestRule.getWebContents(), "fullscreen"));
        waitForContentsFullscreenState(true);

        // Should be locked to landscape now, `waitUntilLockedToLandscape` will throw otherwise.
        waitUntilLockedToLandscape();

        // Leave fullscreen by navigating page.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getWebContents(), "location.reload();");
        waitForContentsFullscreenState(false);
        waitUntilUnlocked();
    }
}
