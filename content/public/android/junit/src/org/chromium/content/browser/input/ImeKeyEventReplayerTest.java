// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.KeyEvent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentFeatures;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ImeKeyEventReplayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
    ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
})
public class ImeKeyEventReplayerTest {
    private final List<KeyEvent> mReplayedEvents = new ArrayList<>();
    private ImeKeyEventReplayer mReplayer;

    @Before
    public void setUp() {
        mReplayedEvents.clear();
        mReplayer = new ImeKeyEventReplayer(mReplayedEvents::add);
    }

    @Test
    @EnableFeatures({
        ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
        ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
    })
    public void testWillReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText_success() {
        long time = SystemClock.uptimeMillis();
        KeyEvent delEvent =
                new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(delEvent.getKeyCode(), delEvent);

        assertTrue(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
        assertEquals(1, mReplayedEvents.size());
        assertEquals(KeyEvent.KEYCODE_DEL, mReplayedEvents.get(0).getKeyCode());
        assertEquals(time, mReplayedEvents.get(0).getEventTime());

        // Queue is now empty.
        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
    }

    @Test
    @EnableFeatures({
        ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
        ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
    })
    public void testWillReplayKeyDownEventWithMatchingCommitText_success() {
        long time = SystemClock.uptimeMillis();
        KeyEvent keyA = new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(keyA.getKeyCode(), keyA);

        assertTrue(mReplayer.willReplayKeyDownEventWithMatchingCommitText("a"));
        assertEquals(1, mReplayedEvents.size());
        assertEquals(KeyEvent.KEYCODE_A, mReplayedEvents.get(0).getKeyCode());

        // Queue is now empty.
        assertFalse(mReplayer.willReplayKeyDownEventWithMatchingCommitText("a"));
    }

    @Test
    @EnableFeatures({
        ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
        ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
    })
    public void
            testWillReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText_ignoreOldEvents() {
        long time = SystemClock.uptimeMillis() - 60 * 1000;
        KeyEvent delEvent =
                new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(delEvent.getKeyCode(), delEvent);

        // Event is expired (> 1000ms old).
        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
        assertTrue(mReplayedEvents.isEmpty());
    }

    @Test
    @EnableFeatures({ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS})
    @DisableFeatures({ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT})
    public void testDelEventNotCapturedWhenReplayFlagDisabled() {
        long time = SystemClock.uptimeMillis();
        KeyEvent delEvent =
                new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(delEvent.getKeyCode(), delEvent);

        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
        assertTrue(mReplayedEvents.isEmpty());
    }

    @Test
    @DisableFeatures({
        ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
        ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
    })
    public void testDelEventNotCapturedWhenCaptureFlagDisabled() {
        long time = SystemClock.uptimeMillis();
        KeyEvent delEvent =
                new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(delEvent.getKeyCode(), delEvent);

        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
        assertTrue(mReplayedEvents.isEmpty());
    }

    @Test
    @EnableFeatures({
        ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS,
        ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT
    })
    public void
            testWillReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText_invalidLengths() {
        long time = SystemClock.uptimeMillis();
        KeyEvent delEvent =
                new KeyEvent(time, time, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0, 0);
        mReplayer.maybeCaptureKeyEventOnKeyPreIme(delEvent.getKeyCode(), delEvent);

        // Invalid lengths should not replay.
        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(2, 0));
        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 1));
        assertFalse(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(0, 1));
        assertTrue(mReplayedEvents.isEmpty());

        // But (1, 0) should still succeed.
        assertTrue(
                mReplayer.willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(1, 0));
        assertEquals(1, mReplayedEvents.size());
    }
}
