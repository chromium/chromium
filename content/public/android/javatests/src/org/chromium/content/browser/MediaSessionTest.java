// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioManager;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.media.MediaSwitches;

import java.util.ArrayList;

/** Tests for MediaSession. */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add(MediaSwitches.AUTOPLAY_NO_GESTURE_REQUIRED_POLICY)
public class MediaSessionTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final String MEDIA_SESSION_TEST_URL =
            "content/test/data/media/session/media-session.html";
    private static final String VERY_SHORT_AUDIO = "very-short-audio";
    private static final String SHORT_AUDIO = "short-audio";
    private static final String LONG_AUDIO = "long-audio";
    private static final String VERY_SHORT_VIDEO = "very-short-video";
    private static final String SHORT_VIDEO = "short-video";
    private static final String LONG_VIDEO = "long-video";
    private static final String LONG_VIDEO_SILENT = "long-video-silent";
    private static final int AUDIO_FOCUS_CHANGE_TIMEOUT = 500; // ms

    private AudioManager getAudioManager() {
        return (AudioManager)
                mActivityTestRule
                        .getActivity()
                        .getApplicationContext()
                        .getSystemService(Context.AUDIO_SERVICE);
    }

    private class MockAudioFocusChangeListener implements AudioManager.OnAudioFocusChangeListener {
        private int mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;

        @Override
        public void onAudioFocusChange(int focusChange) {
            mAudioFocusState = focusChange;
        }

        public int getAudioFocusState() {
            return mAudioFocusState;
        }

        public void requestAudioFocus(int focusType) {
            int result =
                    getAudioManager().requestAudioFocus(this, AudioManager.STREAM_MUSIC, focusType);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                Assert.fail("Did not get audio focus");
            } else {
                mAudioFocusState = focusType;
            }
        }

        public void abandonAudioFocus() {
            getAudioManager().abandonAudioFocus(this);
            mAudioFocusState = AudioManager.AUDIOFOCUS_LOSS;
        }

        public void waitForFocusStateChange(int focusType) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> Criteria.checkThat(getAudioFocusState(), Matchers.is(focusType)));
        }
    }

    private MockAudioFocusChangeListener mAudioFocusChangeListener;

    private MediaSessionObserver mObserver;

    private ArrayList<StateRecord> mStateRecords = new ArrayList<StateRecord>();

    private static class StateRecord {
        public boolean isControllable;
        public boolean isSuspended;

        public StateRecord(boolean isControllable, boolean isSuspended) {
            this.isControllable = isControllable;
            this.isSuspended = isSuspended;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) return true;
            if (!(obj instanceof StateRecord)) return false;

            StateRecord other = (StateRecord) obj;
            return isControllable == other.isControllable && isSuspended == other.isSuspended;
        }

        @Override
        public int hashCode() {
            return (isControllable ? 2 : 0) + (isSuspended ? 1 : 0);
        }

        @Override
        public String toString() {
            return String.format("isControllable=%b isSuspended=%b", isControllable, isSuspended);
        }
    }

    @Before
    public void setUp() {
        try {
            mActivityTestRule.launchContentShellWithUrlSync(MEDIA_SESSION_TEST_URL);
        } catch (Throwable t) {
            throw new AssertionError("Couldn't load test page", t);
        }

        mAudioFocusChangeListener = new MockAudioFocusChangeListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mObserver =
                            new MediaSessionObserver(
                                    MediaSession.fromWebContents(
                                            mActivityTestRule.getWebContents())) {
                                @Override
                                public void mediaSessionStateChanged(
                                        boolean isControllable, boolean isSuspended) {
                                    mStateRecords.add(new StateRecord(isControllable, isSuspended));
                                }
                            };
                });
    }

    @After
    public void tearDown() {
        mAudioFocusChangeListener.abandonAudioFocus();
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    public void testDontStopEachOther() throws Exception {
        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_AUDIO));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);

        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_VIDEO));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);

        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), SHORT_VIDEO));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_VIDEO);

        Assert.assertTrue(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), SHORT_AUDIO));
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_AUDIO);

        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), SHORT_AUDIO));
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_AUDIO));
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), SHORT_VIDEO));
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_VIDEO));
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/916535")
    public void testShortAudioIsTransient() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VERY_SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VERY_SHORT_AUDIO);

        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_GAIN);
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    public void testShortVideoIsTransient() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), VERY_SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), VERY_SHORT_VIDEO);

        // AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK is not reliably set by automotive devices, as
        // ducking is often handled by the hardware instead on Android Automotive OS.
        if (!BuildInfo.getInstance().isAutomotive) {
            mAudioFocusChangeListener.waitForFocusStateChange(
                    AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        }

        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_GAIN);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    public void testAudioGainFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);

        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    public void testVideoGainFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);

        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    public void testSilentVideoDontGainFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO_SILENT);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO_SILENT);

        // TODO(zqzhang): we need to wait for the OS to notify the audio focus loss.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    public void testLongAudioAfterShortGainsFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_AUDIO);

        // AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK is not reliably set by automotive devices, as
        // ducking is often handled by the hardware instead on Android Automotive OS.
        if (!BuildInfo.getInstance().isAutomotive) {
            mAudioFocusChangeListener.waitForFocusStateChange(
                    AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        }

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    public void testLongVideoAfterShortGainsFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_VIDEO);

        // AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK is not reliably set by automotive devices, as
        // ducking is often handled by the hardware instead on Android Automotive OS.
        if (!BuildInfo.getInstance().isAutomotive) {
            mAudioFocusChangeListener.waitForFocusStateChange(
                    AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);
        }

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);
    }

    // TODO(zqzhang): Investigate why this test fails after switching to .ogg from .mp3
    @Test
    @SmallTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "https://crbug.com/1315305")
    public void testShortAudioStopsIfLostFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), SHORT_AUDIO);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "https://crbug.com/1157320")
    public void testShortVideoStopsIfLostFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), SHORT_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), SHORT_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(
                AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), SHORT_VIDEO);
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/1157494")
    public void testAudioStopsIfLostFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_AUDIO);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/1157494")
    public void testVideoStopsIfLostFocus() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_VIDEO);
    }

    @Test
    @SmallTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/625584")
    public void testMediaDuck() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK,
                mAudioFocusChangeListener.getAudioFocusState());

        // TODO(zqzhang): Currently, the volume change cannot be observed. If it could, the volume
        // should be lower now.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_AUDIO));
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_VIDEO));

        mAudioFocusChangeListener.abandonAudioFocus();
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());

        // TODO(zqzhang): Currently, the volume change cannot be observed. If it could, the volume
        // should be higher now.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_AUDIO));
        Assert.assertFalse(DOMUtils.isMediaPaused(mActivityTestRule.getWebContents(), LONG_VIDEO));
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE) // crbug.com/589176
    @DisabledTest(message = "https://crbug.com/1157320")
    public void testMediaResumeAfterTransientFocusLoss() throws Exception {
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_VIDEO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN_TRANSIENT,
                mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_VIDEO);

        mAudioFocusChangeListener.abandonAudioFocus();

        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_VIDEO);
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    @DisabledTest(message = "crbug.com/1157494")
    public void testSessionSuspendedAfterFocusLossWhenPlaying() throws Exception {
        ArrayList<StateRecord> expectedStates = new ArrayList<StateRecord>();
        expectedStates.add(new StateRecord(true, false));
        expectedStates.add(new StateRecord(true, true));

        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_AUDIO);

        Assert.assertEquals(expectedStates, mStateRecords);
    }

    @Test
    @MediumTest
    @Feature({"MediaSession"})
    public void testSessionSuspendedAfterFocusLossWhenPaused() throws Exception {
        ArrayList<StateRecord> expectedStates = new ArrayList<StateRecord>();
        expectedStates.add(new StateRecord(false, true));
        expectedStates.add(new StateRecord(true, false));
        expectedStates.add(new StateRecord(true, true));

        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_LOSS, mAudioFocusChangeListener.getAudioFocusState());
        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        DOMUtils.playMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPlay(mActivityTestRule.getWebContents(), LONG_AUDIO);

        // Wait for the media to be really playing.
        mAudioFocusChangeListener.waitForFocusStateChange(AudioManager.AUDIOFOCUS_LOSS);

        DOMUtils.pauseMedia(mActivityTestRule.getWebContents(), LONG_AUDIO);
        DOMUtils.waitForMediaPauseBeforeEnd(mActivityTestRule.getWebContents(), LONG_AUDIO);

        Assert.assertEquals(expectedStates, mStateRecords);

        mAudioFocusChangeListener.requestAudioFocus(AudioManager.AUDIOFOCUS_GAIN);
        Assert.assertEquals(
                AudioManager.AUDIOFOCUS_GAIN, mAudioFocusChangeListener.getAudioFocusState());

        // Wait for 1 second before observing MediaSession state change.
        Thread.sleep(AUDIO_FOCUS_CHANGE_TIMEOUT);

        Assert.assertEquals(expectedStates, mStateRecords);
    }
}
