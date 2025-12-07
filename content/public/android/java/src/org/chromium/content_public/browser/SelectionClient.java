// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.graphics.drawable.Drawable;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextSelection;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.selection.SmartSelectionClient;
import org.chromium.ui.touch_selection.SelectionEventType;

import java.util.List;

/** Interface to a content layer client that can process and modify selection text. */
@NullMarked
public interface SelectionClient {
    /** The result of the text analysis. */
    class Result {
        /** The surrounding text including the selection. */
        public @Nullable String text;

        /** The start index of the selected text within the surrounding text. */
        public int start;

        /** The end index of the selected text within the surrounding text. */
        public int end;

        /**
         * The number of characters that the left boundary of the original selection should be
         * moved. Negative number means moving left.
         */
        public int startAdjust;

        /**
         * The number of characters that the right boundary of the original selection should be
         * moved. Negative number means moving left.
         */
        public int endAdjust;

        /** TextClassification for logging. */
        public @Nullable TextClassification textClassification;

        /** TextSelection for logging. */
        public @Nullable TextSelection textSelection;

        /** Icons for additional menu items. */
        public @Nullable List<Drawable> additionalIcons;

        /**
         * Convenience method mainly for testing the behaviour of {@link
         * org.chromium.content.browser.selection.SelectionMenuCachedResult}.
         */
        public void setTextClassificationForTesting(TextClassification textClassification) {
            this.textClassification = textClassification;
        }
    }

    /** The interface that returns the result of the selected text analysis. */
    interface ResultCallback {
        /** The result is delivered with this method. */
        void onClassified(Result result);
    }

    interface SurroundingTextCallback {
        /**
         * When the surrounding text is received from the native side. This will be called
         * regardless if the selected text is valid or not.
         */
        void onSurroundingTextReceived(String text, int start, int end);
    }

    /** Adds an observer to the smart selection surrounding text received callback */
    default void addSurroundingTextReceivedListeners(SurroundingTextCallback observer) {}

    /** Removes an observer from the smart selection surrounding text received callback */
    default void removeSurroundingTextReceivedListeners(SurroundingTextCallback observer) {}

    /**
     * Notification that the web content selection has changed, regardless of the causal action.
     *
     * @param selection The newly established selection.
     */
    void onSelectionChanged(String selection);

    /**
     * Notification that a user-triggered selection or insertion-related event has occurred.
     * @param eventType The selection event type, see {@link SelectionEventType}.
     * @param posXPix The x coordinate of the selection start handle.
     * @param posYPix The y coordinate of the selection start handle.
     */
    void onSelectionEvent(@SelectionEventType int eventType, float posXPix, float posYPix);

    /**
     * Acknowledges that a selectAroundCaret action has completed with the given result.
     * @param result Information about the selection including selection state and offset
     *         adjustments to determine the original or extended selection. {@code null} if the
     *         selection couldn't be made.
     */
    void selectAroundCaretAck(@Nullable SelectAroundCaretResult result);

    /**
     * Notifies the SelectionClient that the selection menu has been requested.
     * @param shouldSuggest Whether SelectionClient should suggest and classify or just classify.
     * @return True if embedder should wait for a response before showing selection menu.
     */
    boolean requestSelectionPopupUpdates(boolean shouldSuggest);

    /**
     * Cancel any outstanding requests the embedder had previously requested using
     * SelectionClient.requestSelectionPopupUpdates().
     */
    void cancelAllRequests();

    /** Returns a SelectionEventProcessor associated with the SelectionClient or null. */
    default @Nullable SelectionEventProcessor getSelectionEventProcessor() {
        return null;
    }

    /**
     * Sets the TextClassifier for the Smart Text Selection feature. Pass {@code null} to use the
     * system classifier.
     * @param textClassifier The custom {@link TextClassifier} to start using or {@code null} to
     *        switch back to the system's classifier.
     */
    default void setTextClassifier(TextClassifier textClassifier) {}

    /**
     * Gets TextClassifier that is used for the Smart Text selection. If the custom classifier
     * has been set with setTextClassifier, returns that object, otherwise returns the system
     * classifier.
     */
    default @Nullable TextClassifier getTextClassifier() {
        return null;
    }

    /** Returns the TextClassifier which has been set with setTextClassifier(), or null. */
    default @Nullable TextClassifier getCustomTextClassifier() {
        return null;
    }

    /** Creates a {@link SelectionClient} instance. */
    static @Nullable SelectionClient createSmartSelectionClient(WebContents webContents) {
        SelectionPopupController selectionPopupController =
                SelectionPopupController.fromWebContents(webContents);
        SelectionClient.ResultCallback callback = selectionPopupController.getResultCallback();
        return SmartSelectionClient.fromWebContents(callback, webContents);
    }
}
