// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Build;
import android.os.SystemClock;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;

import java.util.ArrayDeque;
import java.util.function.Predicate;

/**
 * Manages capturing and replaying physical keyboard key down events that are consumed by IMEs (such
 * as Gboard).
 *
 * <p>When a physical keyboard is attached, some IMEs intercept hardware key events (e.g. printable
 * characters or backspace) and dispatch IME operations (like commitText or deleteSurroundingText)
 * instead of letting key events reach the web page. This class records key down events in {@link
 * #maybeCaptureKeyEventOnKeyPreIme(int, KeyEvent)} and finds matching events to replay to native
 * Blink so that standard DOM key events are fired.
 */
@NullMarked
public class ImeKeyEventReplayer {
    private static final int MAX_QUEUE_SIZE = 1000;
    private static final long EVENT_EXPIRATION_THRESHOLD_MS = 1000;

    /** Delegate to dispatch replayed events back to the native IME pipeline. */
    public interface Delegate {
        void sendReplayedKeyEvent(KeyEvent event);
    }

    private final Delegate mDelegate;
    private final ArrayDeque<KeyEvent> mKeyDownEvents = new ArrayDeque<>();

    public ImeKeyEventReplayer(Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Records a key down event if key capturing is enabled.
     *
     * @param keyCode The key code of the event.
     * @param event The KeyEvent received in onKeyPreIme.
     */
    public void maybeCaptureKeyEventOnKeyPreIme(int keyCode, KeyEvent event) {
        if (event.getAction() != KeyEvent.ACTION_DOWN) return;

        // HACK: Remember key down events to use later in sendCompositionToNative(),
        // deleteSurroundingText() and deleteSurroundingTextInCodePoints().
        // TODO(b/432367402): Use a new Android API to replace this hack with a proper solution.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS)
                && Build.VERSION.SDK_INT <= 38) {
            int unicodeChar = event.getUnicodeChar();
            if ((keyCode == KeyEvent.KEYCODE_DEL
                            && ContentFeatureMap.isEnabled(
                                    ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT))
                    || (unicodeChar != 0
                            && (unicodeChar & KeyCharacterMap.COMBINING_ACCENT) == 0)) {
                removeOldKeyDownEvents();
                mKeyDownEvents.add(new KeyEvent(event));
                if (mKeyDownEvents.size() > MAX_QUEUE_SIZE) {
                    mKeyDownEvents.remove();
                }
            }
        }
    }

    /**
     * Checks if a captured key down event matches the committed text and replays it.
     *
     * @param text The committed text.
     * @return True if a matching key down event was found and replayed.
     */
    public boolean willReplayKeyDownEventWithMatchingCommitText(CharSequence text) {
        if (text.length() != 1) return false;
        return replayMatchingKeyDownEvent(
                event -> Character.toString(event.getUnicodeChar()).contentEquals(text));
    }

    /**
     * Checks if a KEYCODE_DEL key down event should be replayed for a deleteSurroundingText(1, 0)
     * call and replays it.
     *
     * @param beforeLength Number of characters/code points to delete before cursor.
     * @param afterLength Number of characters/code points to delete after cursor.
     * @return True if a matching KEYCODE_DEL event was found and replayed.
     */
    public boolean willReplayBackspaceKeyDownEventWithMatchingDeleteSurroundingText(
            int beforeLength, int afterLength) {
        if (!ContentFeatureMap.isEnabled(ContentFeatures.ANDROID_REPLAY_DEL_KEY_EVENT)) {
            return false;
        }

        // Many websites (such as Sheets and Slides) report the empty field as having a selection of
        // a space character.
        // This confuses IMEs such as Gboard and leads to them sending a deleteSurroundingText(1, 0)
        // instead of a KEY_DOWN backspace event, which is again not received by these websites.
        if (beforeLength != 1 || afterLength != 0) {
            return false;
        }

        return replayMatchingKeyDownEvent(event -> event.getKeyCode() == KeyEvent.KEYCODE_DEL);
    }

    private boolean replayMatchingKeyDownEvent(Predicate<KeyEvent> matcher) {
        removeOldKeyDownEvents();
        KeyEvent matchedKeyEvent = null;
        for (KeyEvent event : mKeyDownEvents) {
            if (matcher.test(event)) {
                matchedKeyEvent = event;
                break;
            }
        }
        if (matchedKeyEvent != null) {
            // If there is a matching event, remove all events before and including it.
            while (!mKeyDownEvents.isEmpty()) {
                if (mKeyDownEvents.remove() == matchedKeyEvent) {
                    break;
                }
            }
            mDelegate.sendReplayedKeyEvent(matchedKeyEvent);
            return true;
        }
        return false;
    }

    private void removeOldKeyDownEvents() {
        // Remove events that happened more than a second ago.
        long timestampMs = SystemClock.uptimeMillis();
        while (!mKeyDownEvents.isEmpty()
                && timestampMs - mKeyDownEvents.element().getEventTime()
                        >= EVENT_EXPIRATION_THRESHOLD_MS) {
            mKeyDownEvents.remove();
        }
    }
}
