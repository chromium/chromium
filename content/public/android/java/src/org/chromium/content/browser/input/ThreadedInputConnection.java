// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * An implementation of {@link InputConnection} to communicate with external input method
 * apps. Note that it is running on IME thread (except for constructor and calls from ImeAdapter)
 * such that it does not block UI thread and returns text values immediately after any change
 * to them.
 * Note that extending {@link BaseInputConnection} is a workaround for some OEM's email client
 * which tries to downcast the {@link InputConnection} from {@link View#onCreateInputConnection}
 * into {@link BaseInputConnection}. We are implementing every function of {@link InputConnection},
 * so 'extends' here should have no functional effect at all. See crbug.com/616334 for more
 * details.
 */
class ThreadedInputConnection extends BaseInputConnection implements ChromiumBaseInputConnection {
    private static final String TAG = "Ime";
    private static final boolean DEBUG_LOGS = false;

    private static final TextInputState UNBLOCKER = new TextInputState(
            "", new Range(0, 0), new Range(-1, -1), false, false /* notFromIme */) {

        @Override
        public boolean shouldUnblock() {
            return true;
        }
    };

    private final Runnable mProcessPendingInputStatesRunnable = new Runnable() {
        @Override
        public void run() {
            processPendingInputStates();
        }
    };

    private final Runnable mRequestTextInputStateUpdate = new Runnable() {
        @Override
        public void run() {
            boolean result = mImeAdapter.requestTextInputStateUpdate();
            if (!result) unblockOnUiThread();
        }
    };

    private final Runnable mNotifyUserActionRunnable = new Runnable() {
        @Override
        public void run() {
            mImeAdapter.notifyUserAction();
        }
    };

    private final Runnable mFinishComposingTextRunnable = new Runnable() {
        @Override
        public void run() {
            finishComposingTextOnUiThread();
        }
    };

    private final ImeAdapterImpl mImeAdapter;
    private final Handler mHandler;
    private int mNumNestedBatchEdits;

    // TODO(changwan): check if we can keep a pool of TextInputState to avoid creating
    // a bunch of new objects for each key stroke.
    private final BlockingQueue<TextInputState> mQueue = new LinkedBlockingQueue<>();
    private int mPendingAccent;
    private TextInputState mCachedTextInputState;
    private int mCurrentExtractedTextRequestToken;
    private boolean mShouldUpdateExtractedText;

    ThreadedInputConnection(View view, ImeAdapterImpl imeAdapter, Handler handler) {
        super(view, true);
        if (DEBUG_LOGS) Log.i(TAG, "constructor");
        ImeUtils.checkOnUiThread();
        mImeAdapter = imeAdapter;
        mHandler = handler;
    }

    void resetOnUiThread() {
        ImeUtils.checkOnUiThread();

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                mNumNestedBatchEdits = 0;
                mPendingAccent = 0;
                mCurrentExtractedTextRequestToken = 0;
                mShouldUpdateExtractedText = false;
            }
        });
    }

    @Override
    public void updateStateOnUiThread(String text, final int selectionStart, final int selectionEnd,
            final int compositionStart, final int compositionEnd, boolean singleLine,
            final boolean replyToRequest) {
        ImeUtils.checkOnUiThread();

        mCachedTextInputState = new TextInputState(text, new Range(selectionStart, selectionEnd),
                new Range(compositionStart, compositionEnd), singleLine, replyToRequest);
        if (DEBUG_LOGS) Log.i(TAG, "updateState: %s", mCachedTextInputState);

        addToQueueOnUiThread(mCachedTextInputState);
        if (!replyToRequest) {
            mHandler.post(mProcessPendingInputStatesRunnable);
        }
    }

    /**
     * @see ChromiumBaseInputConnection#getHandler()
     */
    @Override
    public Handler getHandler() {
        return mHandler;
    }

    /**
     * @see ChromiumBaseInputConnection#onRestartInputOnUiThread()
     */
    @Override
    public void onRestartInputOnUiThread() {}

    /**
     * @see ChromiumBaseInputConnection#sendKeyEventOnUiThread(KeyEvent)
     */
    @Override
    public boolean sendKeyEventOnUiThread(final KeyEvent event) {
        ImeUtils.checkOnUiThread();
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                sendKeyEvent(event);
            }
        });
        return true;
    }

    @Override
    @VisibleForTesting
    public void unblockOnUiThread() {
        if (DEBUG_LOGS) Log.i(TAG, "unblockOnUiThread");
        ImeUtils.checkOnUiThread();
        addToQueueOnUiThread(UNBLOCKER);
        mHandler.post(mProcessPendingInputStatesRunnable);
    }

    private void processPendingInputStates() {
        if (DEBUG_LOGS) Log.i(TAG, "checkQueue");
        assertOnImeThread();
        // Handle all the remaining states in the queue.
        while (true) {
            TextInputState state = mQueue.poll();
            if (state == null) {
                if (DEBUG_LOGS) Log.i(TAG, "checkQueue - finished");
                return;
            }
            // Unblocker was not used. Ignore.
            if (state.shouldUnblock()) {
                if (DEBUG_LOGS) Log.i(TAG, "checkQueue - ignoring one unblocker");
                continue;
            }
            if (DEBUG_LOGS) Log.i(TAG, "checkQueue: " + state);
            updateSelection(state);
        }
    }

    private void updateSelection(TextInputState textInputState) {
        if (textInputState == null) return;
        assertOnImeThread();
        if (mNumNestedBatchEdits != 0) return;
        Range selection = textInputState.selection();
        Range composition = textInputState.composition();
        // As per Guidelines in
        // https://developer.android.com/reference/android/view/inputmethod/InputConnection.html
        // #getExtractedText(android.view.inputmethod.ExtractedTextRequest,%20int)
        // States that if the GET_EXTRACTED_TEXT_MONITOR flag is set,
        // you should be calling updateExtractedText(View, int, ExtractedText)
        // whenever you call updateSelection(View, int, int, int, int).
        if (mShouldUpdateExtractedText) {
            final ExtractedText extractedText = convertToExtractedText(textInputState);
            mImeAdapter.updateExtractedText(mCurrentExtractedTextRequestToken, extractedText);
        }
        // This leads to containerView#onCheckIsTextEditor(). Run it on UI thread.
        // See https://crbug.com/1060361.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            mImeAdapter.updateSelection(
                    selection.start(), selection.end(), composition.start(), composition.end());
        });
    }

    private TextInputState requestAndWaitForTextInputState() {
        if (DEBUG_LOGS) Log.i(TAG, "requestAndWaitForTextInputState");
        if (runningOnUiThread()) {
            Log.w(TAG, "InputConnection API is not called on IME thread. Returning cached result.");
            // Returning cached result is a workaround for existing webview apps. (crbug.com/643477)
            return mCachedTextInputState;
        }
        assertOnImeThread();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, mRequestTextInputStateUpdate);
        return blockAndGetStateUpdate();
    }

    private void addToQueueOnUiThread(TextInputState textInputState) {
        ImeUtils.checkOnUiThread();
        try {
            mQueue.put(textInputState);
        } catch (InterruptedException e) {
            Log.e(TAG, "addToQueueOnUiThread interrupted", e);
        }
        if (DEBUG_LOGS) Log.i(TAG, "addToQueueOnUiThread finished: %d", mQueue.size());
    }

    /**
     * @return BlockingQueue for white box unit testing.
     */
    BlockingQueue<TextInputState> getQueueForTest() {
        return mQueue;
    }

    @VisibleForTesting
    protected boolean runningOnUiThread() {
        return ThreadUtils.runningOnUiThread();
    }

    private void assertOnImeThread() {
        ImeUtils.checkCondition(mHandler.getLooper() == Looper.myLooper());
    }

    /**
     * Block until we get the expected state update.
     * @return TextInputState if we get it successfully. null otherwise.
     */
    private TextInputState blockAndGetStateUpdate() {
        if (DEBUG_LOGS) Log.i(TAG, "blockAndGetStateUpdate");
        assertOnImeThread();
        boolean shouldUpdateSelection = false;
        while (true) {
            TextInputState state;
            try {
                state = mQueue.take();
            } catch (InterruptedException e) {
                // This should never happen since IME thread is artificial and is not exposed
                // to other components.
                e.printStackTrace();
                ImeUtils.checkCondition(false);
                return null;
            }
            if (state.shouldUnblock()) {
                if (DEBUG_LOGS) Log.i(TAG, "blockAndGetStateUpdate: unblocked");
                return null;
            } else if (state.replyToRequest()) {
                if (shouldUpdateSelection) updateSelection(state);
                if (DEBUG_LOGS) Log.i(TAG, "blockAndGetStateUpdate done: %d", mQueue.size());
                return state;
            }
            // Ignore when state is not from IME, but make sure to update state when we handle
            // state from IME.
            shouldUpdateSelection = true;
        }
    }

    private void notifyUserAction() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, mNotifyUserActionRunnable);
    }

    /**
     * @see InputConnection#setComposingText(java.lang.CharSequence, int)
     */
    @Override
    public boolean setComposingText(final CharSequence text, final int newCursorPosition) {
        if (DEBUG_LOGS) Log.i(TAG, "setComposingText [%s] [%d]", text, newCursorPosition);
        if (text == null) return false;
        return updateComposingText(text, newCursorPosition, false);
    }

    /**
     * Sends composing update to the InputMethodManager.
     */
    @VisibleForTesting
    public boolean updateComposingText(
            final CharSequence text, final int newCursorPosition, final boolean isPendingAccent) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                updateComposingTextOnUiThread(text, newCursorPosition, isPendingAccent);
            }
        });
        notifyUserAction();
        return true;
    }

    private void updateComposingTextOnUiThread(
            CharSequence text, int newCursorPosition, boolean isPendingAccent) {
        int accentToSend =
                isPendingAccent ? (mPendingAccent | KeyCharacterMap.COMBINING_ACCENT) : 0;
        cancelCombiningAccentOnUiThread();
        mImeAdapter.sendCompositionToNative(text, newCursorPosition, false, accentToSend);
    }

    /**
     * @see InputConnection#commitText(java.lang.CharSequence, int)
     */
    @Override
    public boolean commitText(final CharSequence text, final int newCursorPosition) {
        if (DEBUG_LOGS) Log.i(TAG, "commitText [%s] [%d]", text, newCursorPosition);
        if (text == null) return false;

        // One WebView app detects Enter in JS by looking at KeyDown (http://crbug/577967).
        if (TextUtils.equals(text, "\n")) {
            beginBatchEdit();
            // Clear the current composition range (the keypress alone wouldn't do this).
            commitText("", 1);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    mImeAdapter.sendSyntheticKeyPress(KeyEvent.KEYCODE_ENTER,
                            KeyEvent.FLAG_SOFT_KEYBOARD | KeyEvent.FLAG_KEEP_TOUCH_MODE);
                }
            });
            endBatchEdit();
            return true;
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                commitTextOnUiThread(text, newCursorPosition);
            }
        });
        notifyUserAction();
        return true;
    }

    private void commitTextOnUiThread(final CharSequence text, final int newCursorPosition) {
        cancelCombiningAccentOnUiThread();
        mImeAdapter.sendCompositionToNative(text, newCursorPosition, true, 0);
    }

    /**
     * @see InputConnection#performEditorAction(int)
     */
    @Override
    public boolean performEditorAction(final int actionCode) {
        if (DEBUG_LOGS) Log.i(TAG, "performEditorAction [%d]", actionCode);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.performEditorAction(actionCode);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#performContextMenuAction(int)
     */
    @Override
    public boolean performContextMenuAction(final int id) {
        if (DEBUG_LOGS) Log.i(TAG, "performContextMenuAction [%d]", id);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.performContextMenuAction(id);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#getExtractedText(android.view.inputmethod.ExtractedTextRequest, int)
     */
    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
        if (DEBUG_LOGS) Log.i(TAG, "getExtractedText");
        assertOnImeThread();
        mShouldUpdateExtractedText = (flags & GET_EXTRACTED_TEXT_MONITOR) > 0;
        if (mShouldUpdateExtractedText) {
            mCurrentExtractedTextRequestToken = request != null ? request.token : 0;
        }
        TextInputState textInputState = requestAndWaitForTextInputState();
        return convertToExtractedText(textInputState);
    }

    private ExtractedText convertToExtractedText(TextInputState textInputState) {
        if (textInputState == null) return null;
        ExtractedText extractedText = new ExtractedText();
        extractedText.text = textInputState.text();
        extractedText.partialEndOffset = textInputState.text().length();
        // Set the partial start offset to -1 because the content is the full text.
        // See: Android documentation for ExtractedText#partialStartOffset
        extractedText.partialStartOffset = -1;
        extractedText.selectionStart = textInputState.selection().start();
        extractedText.selectionEnd = textInputState.selection().end();
        extractedText.flags = textInputState.singleLine() ? ExtractedText.FLAG_SINGLE_LINE : 0;
        return extractedText;
    }

    /**
     * @see InputConnection#beginBatchEdit()
     */
    @Override
    public boolean beginBatchEdit() {
        assertOnImeThread();
        if (DEBUG_LOGS) Log.i(TAG, "beginBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        assertOnImeThread();
        mNumNestedBatchEdits++;
        return true;
    }

    /**
     * @see InputConnection#endBatchEdit()
     */
    @Override
    public boolean endBatchEdit() {
        assertOnImeThread();
        if (mNumNestedBatchEdits == 0) return false;
        --mNumNestedBatchEdits;
        if (DEBUG_LOGS) Log.i(TAG, "endBatchEdit [%b]", (mNumNestedBatchEdits == 0));
        if (mNumNestedBatchEdits == 0) {
            updateSelection(requestAndWaitForTextInputState());
        }
        return mNumNestedBatchEdits != 0;
    }

    /**
     * @see InputConnection#deleteSurroundingText(int, int)
     */
    @Override
    public boolean deleteSurroundingText(final int beforeLength, final int afterLength) {
        if (DEBUG_LOGS) Log.i(TAG, "deleteSurroundingText [%d %d]", beforeLength, afterLength);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                if (mPendingAccent != 0) {
                    finishComposingTextOnUiThread();
                }
                mImeAdapter.deleteSurroundingText(beforeLength, afterLength);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#deleteSurroundingTextInCodePoints(int, int)
     */
    @Override
    public boolean deleteSurroundingTextInCodePoints(
            final int beforeLength, final int afterLength) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "deleteSurroundingTextInCodePoints [%d %d]", beforeLength, afterLength);
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                if (mPendingAccent != 0) {
                    finishComposingTextOnUiThread();
                }
                mImeAdapter.deleteSurroundingTextInCodePoints(beforeLength, afterLength);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#sendKeyEvent(android.view.KeyEvent)
     */
    @Override
    public boolean sendKeyEvent(final KeyEvent event) {
        if (DEBUG_LOGS) Log.i(TAG, "sendKeyEvent [%d %d]", event.getAction(), event.getKeyCode());
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                if (handleCombiningAccentOnUiThread(event)) return;
                mImeAdapter.sendKeyEvent(event);
            }
        });
        notifyUserAction();
        return true;
    }

    private void commitCodePointOnUiThread(int codePoint, int pendingAccentToSet) {
        StringBuilder builder = new StringBuilder();
        builder.appendCodePoint(codePoint);
        String text = builder.toString();
        mImeAdapter.sendCompositionToNative(text, 1, true, 0);
        setCombiningAccentOnUiThread(pendingAccentToSet);
    }

    private boolean handleCombiningAccentOnUiThread(final KeyEvent event) {
        // TODO(changwan): this will break the current composition. check if we can
        // implement it in the renderer instead.
        int action = event.getAction();
        int unicodeChar = event.getUnicodeChar();

        if (action != KeyEvent.ACTION_DOWN) return false;

        if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
            // We clear the pending accent on receiving a backspace key event (and also delete the
            // preceding character).
            setCombiningAccentOnUiThread(0);
            return false;
        }

        if ((unicodeChar & KeyCharacterMap.COMBINING_ACCENT) != 0) {
            int newPendingAccent = unicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK;
            if (mPendingAccent != 0) {
                // Already have an accent pending. Commit the previous accent. If the newly-typed
                // accent is not the same as the previous one, set it as pending.
                if (newPendingAccent == mPendingAccent) {
                    commitCodePointOnUiThread(mPendingAccent, 0);
                } else {
                    commitCodePointOnUiThread(mPendingAccent, newPendingAccent);
                }
                return true;
            }

            // No accent currently pending. Just set the new accent as the pending accent and
            // return.
            setCombiningAccentOnUiThread(newPendingAccent);
            return true;
        } else if (mPendingAccent != 0 && unicodeChar != 0) {
            int combined = KeyEvent.getDeadChar(mPendingAccent, unicodeChar);
            if (combined != 0) {
                commitCodePointOnUiThread(combined, 0);
                return true;
            }
            // Noncombinable character; commit the accent character and fall through to sending
            // the key event for the character afterwards.
            commitCodePointOnUiThread(mPendingAccent, 0);
            finishComposingTextOnUiThread();
        }
        return false;
    }

    @VisibleForTesting
    public void setCombiningAccentOnUiThread(int pendingAccent) {
        mPendingAccent = pendingAccent;
    }

    private void cancelCombiningAccentOnUiThread() {
        mPendingAccent = 0;
    }

    /**
     * @see InputConnection#finishComposingText()
     */
    @Override
    public boolean finishComposingText() {
        if (DEBUG_LOGS) Log.i(TAG, "finishComposingText");
        // This is the only function that may be called on UI thread because
        // of direct calls from InputMethodManager.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, mFinishComposingTextRunnable);
        return true;
    }

    private void finishComposingTextOnUiThread() {
        mImeAdapter.finishComposingText();
    }

    /**
     * @see InputConnection#setSelection(int, int)
     */
    @Override
    public boolean setSelection(final int start, final int end) {
        if (DEBUG_LOGS) Log.i(TAG, "setSelection [%d %d]", start, end);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.setEditableSelectionOffsets(start, end);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#setComposingRegion(int, int)
     */
    @Override
    public boolean setComposingRegion(final int start, final int end) {
        if (DEBUG_LOGS) Log.i(TAG, "setComposingRegion [%d %d]", start, end);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.setComposingRegion(start, end);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#getTextBeforeCursor(int, int)
     */
    @Override
    public CharSequence getTextBeforeCursor(int maxChars, int flags) {
        if (DEBUG_LOGS) Log.i(TAG, "getTextBeforeCursor [%d %x]", maxChars, flags);
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getTextBeforeSelection(maxChars);
    }

    /**
     * @see InputConnection#getTextAfterCursor(int, int)
     */
    @Override
    public CharSequence getTextAfterCursor(int maxChars, int flags) {
        if (DEBUG_LOGS) Log.i(TAG, "getTextAfterCursor [%d %x]", maxChars, flags);
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getTextAfterSelection(maxChars);
    }

    /**
     * @see InputConnection#getSelectedText(int)
     */
    @Override
    public CharSequence getSelectedText(int flags) {
        if (DEBUG_LOGS) Log.i(TAG, "getSelectedText [%x]", flags);
        TextInputState textInputState = requestAndWaitForTextInputState();
        if (textInputState == null) return null;
        return textInputState.getSelectedText();
    }

    /**
     * @see InputConnection#getCursorCapsMode(int)
     */
    @Override
    public int getCursorCapsMode(int reqModes) {
        TextInputState textInputState = requestAndWaitForTextInputState();
        int result = 0;
        if (textInputState != null) {
            result = TextUtils.getCapsMode(
                    textInputState.text(), textInputState.selection().start(), reqModes);
        }
        if (DEBUG_LOGS) Log.i(TAG, "getCursorCapsMode [%x]: %x", reqModes, result);
        return result;
    }

    /**
     * @see InputConnection#commitCompletion(android.view.inputmethod.CompletionInfo)
     */
    @Override
    public boolean commitCompletion(CompletionInfo text) {
        if (DEBUG_LOGS) Log.i(TAG, "commitCompletion [%s]", text);
        return false;
    }

    /**
     * @see InputConnection#commitCorrection(android.view.inputmethod.CorrectionInfo)
     */
    @Override
    public boolean commitCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "commitCorrection [%s]",
                    ImeUtils.getCorrectionInfoDebugString(correctionInfo));
        }
        return false;
    }

    /**
     * @see InputConnection#clearMetaKeyStates(int)
     */
    @Override
    public boolean clearMetaKeyStates(int states) {
        if (DEBUG_LOGS) Log.i(TAG, "clearMetaKeyStates [%x]", states);
        return false;
    }

    /**
     * @see InputConnection#reportFullscreenMode(boolean)
     */
    @Override
    public boolean reportFullscreenMode(boolean enabled) {
        if (DEBUG_LOGS) Log.i(TAG, "reportFullscreenMode [%b]", enabled);
        // We ignore fullscreen mode for now. That's why we set
        // EditorInfo.IME_FLAG_NO_FULLSCREEN in constructor.
        // Note that this may be called on UI thread.
        return false;
    }

    /**
     * @see InputConnection#performPrivateCommand(java.lang.String, android.os.Bundle)
     */
    @Override
    public boolean performPrivateCommand(String action, Bundle data) {
        if (DEBUG_LOGS) Log.i(TAG, "performPrivateCommand [%s]", action);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.performPrivateCommand(action, data);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#requestCursorUpdates(int)
     */
    @Override
    public boolean requestCursorUpdates(final int cursorUpdateMode) {
        if (DEBUG_LOGS) Log.i(TAG, "requestCursorUpdates [%x]", cursorUpdateMode);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mImeAdapter.onRequestCursorUpdates(cursorUpdateMode);
            }
        });
        return true;
    }

    /**
     * @see InputConnection#closeConnection()
     */
    // TODO(crbug.com/635567): Fix this properly.
    @Override
    @SuppressLint("MissingSuperCall")
    public void closeConnection() {
        if (DEBUG_LOGS) Log.i(TAG, "closeConnection");
        // TODO(changwan): Implement this. http://crbug.com/595525
    }
}
