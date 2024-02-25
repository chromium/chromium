// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.text.BreakIterator;

/** Unit tests for the {@link SmartSelectionEventProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SmartSelectionEventProcessorTest {
    private WebContentsImpl mWebContents;
    private WindowAndroid mWindowAndroid;

    @Mock private TextClassifier mTextClassifier;

    // Char index (in 10s)
    // Word index (thou)
    private static String sText =
            ""
                    // 0         1         2         3         4
                    // -7-6  -5-4   -3-2        -1   0    1    2
                    + "O Romeo, Romeo! Wherefore art thou Romeo?\n"
                    //         5         6         7
                    // 3    4   5      6   7      8   9   0
                    + "Deny thy father and refuse thy name.\n"
                    // 8         9         0         1         2
                    // 1 2 3  4    5    6  7 8  9   0     1  2   3
                    + "Or, if thou wilt not, be but sworn my love,\n"
                    //       3         4         5
                    // 4   567  8  9      0  1 2      34
                    + "And I’ll no longer be a Capulet.\n";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;

        mWebContents = Mockito.mock(WebContentsImpl.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext())
                .thenReturn(
                        new WeakReference<Context>(ApplicationProvider.getApplicationContext()));

        TextClassificationManager tcm =
                ApplicationProvider.getApplicationContext()
                        .getSystemService(TextClassificationManager.class);
        tcm.setTextClassifier(mTextClassifier);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testOverlap() {
        assertTrue(SelectionIndicesConverter.overlap(1, 3, 2, 4));
        assertTrue(SelectionIndicesConverter.overlap(2, 4, 1, 3));

        assertTrue(SelectionIndicesConverter.overlap(1, 4, 2, 3));
        assertTrue(SelectionIndicesConverter.overlap(2, 3, 1, 4));

        assertTrue(SelectionIndicesConverter.overlap(1, 4, 1, 3));
        assertTrue(SelectionIndicesConverter.overlap(1, 3, 1, 4));

        assertTrue(SelectionIndicesConverter.overlap(1, 4, 2, 4));
        assertTrue(SelectionIndicesConverter.overlap(2, 4, 1, 4));

        assertTrue(SelectionIndicesConverter.overlap(1, 4, 1, 4));

        assertFalse(SelectionIndicesConverter.overlap(1, 2, 3, 4));
        assertFalse(SelectionIndicesConverter.overlap(3, 4, 1, 2));

        assertFalse(SelectionIndicesConverter.overlap(1, 2, 2, 4));
        assertFalse(SelectionIndicesConverter.overlap(2, 4, 1, 2));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testContains() {
        assertFalse(SelectionIndicesConverter.contains(1, 3, 2, 4));
        assertFalse(SelectionIndicesConverter.contains(2, 4, 1, 3));

        assertTrue(SelectionIndicesConverter.contains(1, 4, 2, 3));
        assertFalse(SelectionIndicesConverter.contains(2, 3, 1, 4));

        assertTrue(SelectionIndicesConverter.contains(1, 4, 1, 3));
        assertFalse(SelectionIndicesConverter.contains(1, 3, 1, 4));

        assertTrue(SelectionIndicesConverter.contains(1, 4, 2, 4));
        assertFalse(SelectionIndicesConverter.contains(2, 4, 1, 4));

        assertTrue(SelectionIndicesConverter.contains(1, 4, 1, 4));

        assertFalse(SelectionIndicesConverter.contains(1, 2, 3, 4));
        assertFalse(SelectionIndicesConverter.contains(3, 4, 1, 2));

        assertFalse(SelectionIndicesConverter.contains(1, 2, 2, 4));
        assertFalse(SelectionIndicesConverter.contains(2, 4, 1, 2));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testUpdateSelectionState() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        String address = "1600 Amphitheatre Parkway, Mountain View, CA 94043.";
        assertTrue(converter.updateSelectionState(address.substring(18, 25), 18));
        assertEquals("Parkway", converter.getGlobalSelectionText());
        assertEquals(18, converter.getGlobalStartOffset());

        // Expansion.
        assertTrue(converter.updateSelectionState(address.substring(5, 35), 5));
        assertEquals("Amphitheatre Parkway, Mountain", converter.getGlobalSelectionText());
        assertEquals(5, converter.getGlobalStartOffset());

        // Drag left handle. Select "Mountain".
        assertTrue(converter.updateSelectionState(address.substring(27, 35), 27));
        assertEquals("Amphitheatre Parkway, Mountain", converter.getGlobalSelectionText());
        assertEquals(5, converter.getGlobalStartOffset());

        // Drag left handle. Select " View".
        assertTrue(converter.updateSelectionState(address.substring(35, 40), 35));
        assertEquals("Amphitheatre Parkway, Mountain View", converter.getGlobalSelectionText());
        assertEquals(5, converter.getGlobalStartOffset());

        // Drag left handle. Select "1600 Amphitheatre Parkway, Mountain View".
        assertTrue(converter.updateSelectionState(address.substring(0, 40), 0));
        assertEquals(
                "1600 Amphitheatre Parkway, Mountain View", converter.getGlobalSelectionText());
        assertEquals(0, converter.getGlobalStartOffset());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testUpdateSelectionStateOffsets() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        String address = "1600 Amphitheatre Parkway, Mountain View, CA 94043.";
        assertTrue(converter.updateSelectionState(address.substring(18, 25), 18));
        assertEquals("Parkway", converter.getGlobalSelectionText());
        assertEquals(18, converter.getGlobalStartOffset());

        // Drag to select "Amphitheatre Parkway". Overlap.
        assertFalse(converter.updateSelectionState(address.substring(5, 25), 18));
        assertNull(converter.getGlobalSelectionText());

        // Reset.
        converter = new SelectionIndicesConverter();
        assertTrue(converter.updateSelectionState(address.substring(18, 25), 18));
        assertEquals("Parkway", converter.getGlobalSelectionText());
        assertEquals(18, converter.getGlobalStartOffset());

        // Drag to select "Amphitheatre Parkway". Overlap.
        assertFalse(converter.updateSelectionState(address.substring(5, 25), 0));
        assertNull(converter.getGlobalSelectionText());

        // Reset.
        converter = new SelectionIndicesConverter();
        assertTrue(converter.updateSelectionState(address.substring(36, 40), 36));
        assertEquals("View", converter.getGlobalSelectionText());
        assertEquals(36, converter.getGlobalStartOffset());

        // Drag to select "Mountain View". Not overlap.
        assertFalse(converter.updateSelectionState(address.substring(27, 40), 0));
        assertNull(converter.getGlobalSelectionText());

        // Reset.
        converter = new SelectionIndicesConverter();
        assertTrue(converter.updateSelectionState(address.substring(36, 40), 36));
        assertEquals("View", converter.getGlobalSelectionText());
        assertEquals(36, converter.getGlobalStartOffset());

        // Drag to select "Mountain View". Adjacent. Wrong case.
        assertTrue(converter.updateSelectionState(address.substring(27, 40), 40));
        assertEquals("ViewMountain View", converter.getGlobalSelectionText());
        assertEquals(36, converter.getGlobalStartOffset());

        String text = "a a a a a";
        // Reset.
        converter = new SelectionIndicesConverter();
        assertTrue(converter.updateSelectionState(text.substring(2, 3), 2));
        assertEquals("a", converter.getGlobalSelectionText());
        assertEquals(2, converter.getGlobalStartOffset());

        // Drag to select "a a". Contains. Wrong case.
        assertTrue(converter.updateSelectionState(text.substring(4, 7), 2));
        assertEquals("a a", converter.getGlobalSelectionText());
        assertEquals(2, converter.getGlobalStartOffset());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testIsWhitespace() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        String test = "\u202F\u00A0 \t\n\u000b\f\r";
        converter.updateSelectionState(test, 0);
        assertTrue(converter.isWhitespace(0, test.length()));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testCountWordsBackward() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        converter.updateSelectionState(sText, 0);
        BreakIterator iterator = BreakIterator.getWordInstance();
        iterator.setText(sText);
        // End with "Deny" from right.
        assertEquals(0, converter.countWordsBackward(42, 42, iterator));
        assertEquals(1, converter.countWordsBackward(43, 42, iterator));
        assertEquals(8, converter.countWordsBackward(79, 42, iterator));
        assertEquals(31, converter.countWordsBackward(sText.length(), 42, iterator));

        // End with "e" in "Deny" from right.
        assertEquals(1, converter.countWordsBackward(44, 43, iterator));
        assertEquals(8, converter.countWordsBackward(79, 43, iterator));
        assertEquals(31, converter.countWordsBackward(sText.length(), 43, iterator));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testCountWordsForward() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        converter.updateSelectionState(sText, 0);
        BreakIterator iterator = BreakIterator.getWordInstance();
        iterator.setText(sText);
        // End with "Deny" from left.
        assertEquals(0, converter.countWordsForward(42, 42, iterator));
        assertEquals(0, converter.countWordsForward(41, 42, iterator));
        assertEquals(5, converter.countWordsForward(16, 42, iterator));
        assertEquals(10, converter.countWordsForward(0, 42, iterator));

        // End with "e" in "Deny" from left.
        assertEquals(1, converter.countWordsForward(42, 43, iterator));
        assertEquals(6, converter.countWordsForward(16, 43, iterator));
        assertEquals(11, converter.countWordsForward(0, 43, iterator));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testGetWordDelta() {
        SelectionIndicesConverter converter = new SelectionIndicesConverter();
        // char offset 0         5     0    5     0    5
        //            -1       0    1   2         3     45
        String test = "It\u00A0has\nnon breaking\tspaces.";
        converter.updateSelectionState(test, 0);
        converter.setInitialStartOffset(3);
        int[] indices = new int[2];
        // Whole range. "It\u00A0has\nnon breaking\tspaces."
        converter.getWordDelta(0, test.length(), indices);
        assertEquals(-1, indices[0]);
        assertEquals(5, indices[1]);

        // Itself. "has"
        converter.getWordDelta(3, 6, indices);
        assertEquals(0, indices[0]);
        assertEquals(1, indices[1]);

        // No overlap left. "It"
        converter.getWordDelta(0, 2, indices);
        assertEquals(-1, indices[0]);
        assertEquals(0, indices[1]);

        // No overlap right. "space"
        converter.getWordDelta(20, 25, indices);
        assertEquals(3, indices[0]);
        assertEquals(4, indices[1]);

        // "breaking\tspace"
        converter.getWordDelta(11, 25, indices);
        assertEquals(2, indices[0]);
        assertEquals(4, indices[1]);

        // Extra space case. " breaking\tspace"
        converter.getWordDelta(10, 25, indices);
        assertEquals(2, indices[0]);
        assertEquals(4, indices[1]);

        // Partial word. "re" in "breaking".
        converter.getWordDelta(12, 14, indices);
        assertEquals(2, indices[0]);
        assertEquals(3, indices[1]);

        // Partial word. "t" in "It".
        converter.getWordDelta(1, 2, indices);
        assertEquals(-1, indices[0]);
        assertEquals(0, indices[1]);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testNormalLoggingFlow() {
        SmartSelectionEventProcessor logger = SmartSelectionEventProcessor.create(mWebContents);
        ArgumentCaptor<SelectionEvent> captor = ArgumentCaptor.forClass(SelectionEvent.class);
        InOrder inOrder = inOrder(mTextClassifier);

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        SelectionEvent selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Smart Selection, expand to "Wherefore art thou Romeo?".
        logger.onSelectionModified("Wherefore art thou Romeo?", 16, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 3);

        // Smart Selection reset, to the last Romeo in row#1.
        logger.onSelectionAction("Romeo", 35, SelectionEvent.ACTION_RESET, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEquals(SelectionEvent.ACTION_RESET, selectionEvent.getEventType());
        assertEvent(
                selectionEvent,
                SelectionEvent.ACTION_RESET,
                /* expectedStart= */ 1,
                /* expectedEnd= */ 2);

        // User clear selection.
        logger.onSelectionAction("Romeo", 35, SelectionEvent.ACTION_ABANDON, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.ACTION_ABANDON,
                /* expectedStart= */ 1,
                /* expectedEnd= */ 2);

        // User start a new selection without abandon.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Smart Selection, expand to "Wherefore art thou Romeo?".
        logger.onSelectionModified("Wherefore art thou Romeo?", 16, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 3);

        // COPY, PASTE, CUT, SHARE, SMART_SHARE are basically the same.
        logger.onSelectionAction("Wherefore art thou Romeo?", 16, SelectionEvent.ACTION_COPY, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.ACTION_COPY,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 3);

        // SELECT_ALL
        logger.onSelectionStarted("thou", 30, /* editable= */ true);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        logger.onSelectionAction(sText, 0, SelectionEvent.ACTION_SELECT_ALL, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.ACTION_SELECT_ALL,
                /* expectedStart= */ -7,
                /* expectedEnd= */ 34);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testMultipleDrag() {
        SmartSelectionEventProcessor logger = SmartSelectionEventProcessor.create(mWebContents);
        ArgumentCaptor<SelectionEvent> captor = ArgumentCaptor.forClass(SelectionEvent.class);
        InOrder inOrder = inOrder(mTextClassifier);

        // Start new selection. First "Deny" in row#2.
        logger.onSelectionStarted("Deny", 42, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        SelectionEvent selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Drag right handle to "father".
        logger.onSelectionModified("Deny thy father", 42, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ 0,
                /* expectedEnd= */ 3);

        // Drag left handle to " and refuse"
        logger.onSelectionModified(" and refuse", 57, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ 3,
                /* expectedEnd= */ 5);

        // Drag right handle to " Romeo?\nDeny thy father".
        logger.onSelectionModified(" Romeo?\nDeny thy father", 34, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 3);

        // Dismiss the selection.
        logger.onSelectionAction(
                " Romeo?\nDeny thy father", 34, SelectionEvent.ACTION_ABANDON, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.ACTION_ABANDON,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 3);

        // Start a new selection.
        logger.onSelectionStarted("Deny", 42, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testTextShift() {
        SmartSelectionEventProcessor logger = SmartSelectionEventProcessor.create(mWebContents);
        ArgumentCaptor<SelectionEvent> captor = ArgumentCaptor.forClass(SelectionEvent.class);
        InOrder inOrder = inOrder(mTextClassifier);

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        SelectionEvent selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Smart Selection, expand to "Wherefore art thou Romeo?".
        logger.onSelectionModified("Wherefore art thou Romeo?", 30, null);
        inOrder.verify(mTextClassifier, never())
                .onSelectionEvent(Mockito.any(SelectionEvent.class));

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Drag. Non-intersect case.
        logger.onSelectionModified("Wherefore art thou", 10, null);
        inOrder.verify(mTextClassifier, never())
                .onSelectionEvent(Mockito.any(SelectionEvent.class));

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Drag. Adjacent case, form "Wherefore art thouthou". Wrong case.
        logger.onSelectionModified("Wherefore art thou", 12, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ -3,
                /* expectedEnd= */ 0);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSelectionChanged() {
        SmartSelectionEventProcessor logger = SmartSelectionEventProcessor.create(mWebContents);
        ArgumentCaptor<SelectionEvent> captor = ArgumentCaptor.forClass(SelectionEvent.class);
        InOrder inOrder = inOrder(mTextClassifier);

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        SelectionEvent selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Change "thou" to "math".
        logger.onSelectionModified("Wherefore art math", 16, null);
        inOrder.verify(mTextClassifier, never())
                .onSelectionEvent(Mockito.any(SelectionEvent.class));

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Drag while deleting "art ". Wrong case.
        logger.onSelectionModified("Wherefore thou", 16, null);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertEvent(
                selectionEvent,
                SelectionEvent.EVENT_SELECTION_MODIFIED,
                /* expectedStart= */ -2,
                /* expectedEnd= */ 0);

        // Start to select, selected "thou" in row#1.
        logger.onSelectionStarted("thou", 30, /* editable= */ false);
        inOrder.verify(mTextClassifier).onSelectionEvent(captor.capture());
        selectionEvent = captor.getValue();
        assertSelectionStartedEvent(selectionEvent);

        // Drag while deleting "Wherefore art ".
        logger.onSelectionModified("thou", 16, null);
        inOrder.verify(mTextClassifier, never())
                .onSelectionEvent(Mockito.any(SelectionEvent.class));
    }

    private static void assertSelectionStartedEvent(SelectionEvent event) {
        assertEquals(SelectionEvent.EVENT_SELECTION_STARTED, event.getEventType());
        assertEquals(0, event.getStart());
        assertEquals(1, event.getEnd());
    }

    private static void assertEvent(
            SelectionEvent event, int eventType, int expectedStart, int expectedEnd) {
        assertEquals(eventType, event.getEventType());
        assertEquals(expectedStart, event.getStart());
        assertEquals(expectedEnd, event.getEnd());
    }
}
