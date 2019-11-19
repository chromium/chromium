// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.pm.ActivityInfo;
import android.provider.Settings;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.media.MediaSwitches;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the feature that automatically enters and exits fullscreen when the device
 * is rotated whilst watching a video.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY})
public class VideoRotateToFullscreenTest {
    @Rule
    public ContentShellActivityTestRule mRule = new ContentShellActivityTestRule();

    private static final String TEST_URL = "content/test/data/media/video-player.html";
    private static final String VIDEO_ID = "video";

    private void waitForContentsFullscreenState(boolean fullscreenValue) {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(fullscreenValue, new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws TimeoutException {
                        return DOMUtils.isFullscreen(mRule.getWebContents());
                    }
                }));
    }

    private void waitForScreenOrientation(String orientationValue) {
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(orientationValue, new Callable<String>() {
                    @Override
                    public String call() throws TimeoutException {
                        return screenOrientation();
                    }
                }));
    }

    private String screenOrientation() throws TimeoutException {
        // Returns "\"portrait\"" or "\"landscape\"" (strips the "-primary" or "-secondary" suffix).
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mRule.getWebContents(), "screen.orientation.type.split('-')[0]");
    }

    @Before
    public void setUp() throws Exception {
        mRule.launchContentShellWithUrlSync(TEST_URL);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(mRule.getWebContents(),
                "document.getElementById('video').src = 'bear-320x240.webm';");
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRule.getActivity().setRequestedOrientation(
                    ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        });
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @DisabledTest(message = "crbug.com/726977")
    public void testPortraitToLandscapeAndBack() throws Exception {
        // Start off in portrait screen orientation.
        mRule.getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");

        // Start playback.
        Assert.assertTrue(DOMUtils.isMediaPaused(mRule.getWebContents(), VIDEO_ID));
        DOMUtils.playMedia(mRule.getWebContents(), VIDEO_ID);
        DOMUtils.waitForMediaPlay(mRule.getWebContents(), VIDEO_ID);

        // Rotate screen from portrait to landscape.
        mRule.getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForScreenOrientation("\"landscape\"");

        // Should enter fullscreen if there is no portrait system lock.
        boolean autoRotateEnabled =
                Settings.System.getInt(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.System.ACCELEROMETER_ROTATION, 0)
                == 1;
        waitForContentsFullscreenState(autoRotateEnabled);

        // Rotate screen from landscape to portrait(?).
        mRule.getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForScreenOrientation("\"portrait\"");

        // Should no longer be fullscreen (either exitno longer be fullscreen
        // (either exit or never went).
        waitForContentsFullscreenState(false);
    }
}
