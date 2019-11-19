// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Handler;
import android.view.KeyCharacterMap;
import android.view.View;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.Callable;

/**
 * Unit tests for {@ThreadedInputConnection}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ThreadedInputConnectionTest {
    @Mock
    ImeAdapterImpl mImeAdapter;

    ThreadedInputConnection mConnection;
    InOrder mInOrder;
    View mView;
    Context mContext;
    boolean mRunningOnUiThread;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mImeAdapter = Mockito.mock(ImeAdapterImpl.class);
        mInOrder = inOrder(mImeAdapter);

        // Mocks required to create a ThreadedInputConnection object
        mView = Mockito.mock(View.class);
        mContext = Mockito.mock(Context.class);
        when(mView.getContext()).thenReturn(mContext);
        when(mContext.getSystemService(Context.INPUT_METHOD_SERVICE)).thenReturn(Mockito.mock(
                InputMethodManager.class));
        // Let's create Handler for test thread and pretend that it is running on IME thread.
        mConnection = new ThreadedInputConnection(mView, mImeAdapter, new Handler()) {
            @Override
            protected boolean runningOnUiThread() {
                return mRunningOnUiThread;
            }
        };
    }

    @Test
    @Feature({"TextInput"})
    public void testComposeGetTextFinishGetText() {
        // IME app calls setComposingText().
        mConnection.setComposingText("hello", 1);
        mInOrder.verify(mImeAdapter).sendCompositionToNative("hello", 1, false, 0);

        // Renderer updates states asynchronously.
        mConnection.updateStateOnUiThread("hello", 5, 5, 0, 5, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(5, 5, 0, 5);
        assertEquals(0, mConnection.getQueueForTest().size());

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 5, 5, 0, 5, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls getTextBeforeCursor().
        assertEquals("hello", mConnection.getTextBeforeCursor(20, 0));

        // IME app calls finishComposingText().
        mConnection.finishComposingText();
        mInOrder.verify(mImeAdapter).finishComposingText();
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(5, 5, -1, -1);

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls getTextBeforeCursor().
        assertEquals("hello", mConnection.getTextBeforeCursor(20, 0));

        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    public void testPressingDeadKey() {
        // On default keyboard "Alt+i" produces a dead key '\u0302'.
        mConnection.setCombiningAccentOnUiThread(0x0302);
        mConnection.updateComposingText("\u0302", 1, true);
        mInOrder.verify(mImeAdapter)
                .sendCompositionToNative(
                        "\u0302", 1, false, 0x0302 | KeyCharacterMap.COMBINING_ACCENT);
    }

    @Test
    @Feature({"TextInput"})
    public void testRenderChangeUpdatesSelection() {
        // User moves the cursor.
        mConnection.updateStateOnUiThread("hello", 4, 4, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateSelection(4, 4, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    public void testBatchEdit() {
        // IME app calls beginBatchEdit().
        assertTrue(mConnection.beginBatchEdit());
        // Type hello real fast.
        mConnection.commitText("hello", 1);
        mInOrder.verify(mImeAdapter).sendCompositionToNative("hello", 1, true, 0);

        // Renderer updates states asynchronously.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        mInOrder.verify(mImeAdapter, never()).updateSelection(5, 5, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());

        {
            // Nest another batch edit.
            assertTrue(mConnection.beginBatchEdit());
            // Move the cursor to the left.
            mConnection.setSelection(4, 4);
            assertTrue(mConnection.endBatchEdit());
        }
        // We still have one outer batch edit, so should not update selection yet.
        mInOrder.verify(mImeAdapter, never()).updateSelection(4, 4, -1, -1);

        // Prepare to call requestTextInputStateUpdate.
        mConnection.updateStateOnUiThread("hello", 4, 4, -1, -1, true, true);
        assertEquals(1, mConnection.getQueueForTest().size());
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // IME app calls endBatchEdit().
        assertFalse(mConnection.endBatchEdit());
        // Batch edit is finished, now update selection.
        mInOrder.verify(mImeAdapter).updateSelection(4, 4, -1, -1);
        assertEquals(0, mConnection.getQueueForTest().size());
    }

    @Test
    @Feature({"TextInput"})
    @Ignore("crbug/632792")
    public void testFailToRequestToRenderer() {
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(false);
        // Should not hang here. Return null to indicate failure.
        assertNull(null, mConnection.getTextBeforeCursor(10, 0));
    }

    @Test
    @Feature({"TextInput"})
    @Ignore("crbug/632792")
    public void testRendererCannotUpdateState() {
        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);
        // We found that renderer cannot update state, e.g., due to a crash.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                try {
                    // TODO(changwan): find a way to avoid this.
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                    fail();
                }
                mConnection.unblockOnUiThread();
            }
        });
        // Should not hang here. Return null to indicate failure.
        assertEquals(null, mConnection.getTextBeforeCursor(10, 0));
    }

    // crbug.com/643477
    @Test
    @Feature({"TextInput"})
    public void testUiThreadAccess() {
        assertTrue(mConnection.commitText("hello", 1));
        mRunningOnUiThread = true;
        // Depending on the timing, the result may not be up-to-date.
        assertNotEquals("hello",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return mConnection.getTextBeforeCursor(10, 0);
                    }
                }));
        // Or it could be.
        mConnection.updateStateOnUiThread("hello", 5, 5, -1, -1, true, false);
        assertEquals("hello",
                ThreadUtils.runOnUiThreadBlockingNoException(new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return mConnection.getTextBeforeCursor(10, 0);
                    }
                }));

        mRunningOnUiThread = false;
    }

    @Test
    @Feature("TextInput")
    public void testUpdateSelectionBehaviorWhenUpdatesRequested() {
        // Arrange.
        final ExtractedTextRequest request = new ExtractedTextRequest();

        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // Populate the TextInputState BlockingQueue for the getExtractedText() call.
        mConnection.updateStateOnUiThread("bello", 1, 1, -1, -1, true, true);

        // Act.
        final ExtractedText extractedText =
                mConnection.getExtractedText(request, InputConnection.GET_EXTRACTED_TEXT_MONITOR);

        // Assert.
        assertEquals("bello", extractedText.text);
        assertEquals(-1, extractedText.partialStartOffset);
        assertEquals("bello".length(), extractedText.partialEndOffset);
        assertEquals(1, extractedText.selectionStart);
        assertEquals(1, extractedText.selectionEnd);

        // Ensure that the next updateState events will invoke
        // both updateExtractedText() and updateSelection().
        mConnection.updateStateOnUiThread("mello", 2, 2, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(2, 2, -1, -1);

        mConnection.updateStateOnUiThread("cello", 3, 3, -1, -1, true, false);
        mInOrder.verify(mImeAdapter).updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(3, 3, -1, -1);
    }

    @Test
    @Feature("TextInput")
    public void testUpdateSelectionBehaviorWhenUpdatesNotRequested() {
        // Arrange.
        final ExtractedTextRequest request = new ExtractedTextRequest();

        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // Populate the TextInputState BlockingQueue for the getExtractedText() call.
        mConnection.updateStateOnUiThread("hello", 1, 2, 3, 4, true, true);

        // Initially we want to monitor extracted text updates.
        final ExtractedText extractedText1 =
                mConnection.getExtractedText(request, InputConnection.GET_EXTRACTED_TEXT_MONITOR);

        mConnection.updateStateOnUiThread("bello", 1, 1, 3, 4, true, false);

        // Assert.
        assertEquals("hello", extractedText1.text);
        assertEquals(-1, extractedText1.partialStartOffset);
        assertEquals("hello".length(), extractedText1.partialEndOffset);
        assertEquals(1, extractedText1.selectionStart);
        assertEquals(2, extractedText1.selectionEnd);

        mInOrder.verify(mImeAdapter).updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(1, 1, 3, 4);

        // Populate the TextInputState BlockingQueue for the getExtractedText() call.
        mConnection.updateStateOnUiThread("cello", 2, 2, 3, 4, true, true);

        // Act: Now we want to stop monitoring extracted text changes.
        final ExtractedText extractedText2 = mConnection.getExtractedText(request, 0);

        // Assert
        assertEquals("cello", extractedText2.text);
        assertEquals(-1, extractedText2.partialStartOffset);
        assertEquals("cello".length(), extractedText2.partialEndOffset);
        assertEquals(2, extractedText2.selectionStart);
        assertEquals(2, extractedText2.selectionEnd);

        // Perform another updateState
        mConnection.updateStateOnUiThread("ello", 0, 0, -1, -1, true, false);

        // Assert: No more update extracted text updates sent to ImeAdapter.
        mInOrder.verify(mImeAdapter, never())
                .updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(0, 0, -1, -1);
    }

    @Test
    @Feature("TextInput")
    public void testExtractedTextNotSentAfterInputConnectionReset() {
        // Arrange.
        final ExtractedTextRequest request = new ExtractedTextRequest();

        when(mImeAdapter.requestTextInputStateUpdate()).thenReturn(true);

        // Populate the TextInputState BlockingQueue for the getExtractedText() call.
        mConnection.updateStateOnUiThread("hello", 1, 2, 3, 4, true, true);

        // Start monitoring for extracted text updates
        final ExtractedText extractedText =
                mConnection.getExtractedText(request, InputConnection.GET_EXTRACTED_TEXT_MONITOR);

        // Assert.
        assertEquals("hello", extractedText.text);
        assertEquals(-1, extractedText.partialStartOffset);
        assertEquals("hello".length(), extractedText.partialEndOffset);
        assertEquals(1, extractedText.selectionStart);
        assertEquals(2, extractedText.selectionEnd);

        mConnection.updateStateOnUiThread("bello", 1, 1, 3, 4, true, false);

        mInOrder.verify(mImeAdapter).updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(1, 1, 3, 4);

        // Act: Force a connection reset Instead of calling ImeAdapter#onCreateInputConnection()
        // To stop monitoring extracted text changes.
        mConnection.resetOnUiThread();

        // Perform another updateState
        mConnection.updateStateOnUiThread("ello", 0, 0, -1, -1, true, false);

        // Assert: No more update extracted text updates sent to ImeAdapter.
        mInOrder.verify(mImeAdapter, never())
                .updateExtractedText(anyInt(), any(ExtractedText.class));
        mInOrder.verify(mImeAdapter).updateSelection(0, 0, -1, -1);
    }
}
