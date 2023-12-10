// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.Matrix;
import android.os.Build;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorBoundsInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.InputMethodManagerWrapper;

import java.util.Arrays;

/**
 * A state machine interface which receives Chromium internal events to determines when to call
 * {@link InputMethodManager#updateCursorAnchorInfo(View, CursorAnchorInfo)}. This interface is
 * also used in unit tests to mock out {@link CursorAnchorInfo}.
 */
final class CursorAnchorInfoController {
    /** An interface to mock out {@link View#getLocationOnScreen(int[])} for testing. */
    public interface ViewDelegate {
        void getLocationOnScreen(View view, int[] location);
    }

    /** An interface to mock out composing text retrieval from ImeAdapter. */
    public interface ComposingTextDelegate {
        CharSequence getText();

        int getSelectionStart();

        int getSelectionEnd();

        int getComposingTextStart();

        int getComposingTextEnd();
    }

    // Current focus and monitoring states.
    private boolean mIsEditable;
    private boolean mHasPendingImmediateRequest;
    private boolean mMonitorModeEnabled;

    // Parameters for CursorAnchorInfo, updated by setBounds.
    @Nullable private float[] mCompositionCharacterBounds;
    @Nullable private float[] mVisibleLineBounds;
    // Parameters for CursorAnchorInfo, updated by onUpdateFrameInfo.
    private boolean mHasCoordinateInfo;
    private float mScale;
    private float mTranslationX;
    private float mTranslationY;
    private boolean mHasInsertionMarker;
    private boolean mIsInsertionMarkerVisible;
    private float mInsertionMarkerHorizontal;
    private float mInsertionMarkerTop;
    private float mInsertionMarkerBottom;

    // Data updated on stylus writing.
    @Nullable private EditorBoundsInfo mEditorBoundsInfo;

    @Nullable private CursorAnchorInfo mLastCursorAnchorInfo;

    @NonNull private final Matrix mMatrix = new Matrix();
    @NonNull private final int[] mViewOrigin = new int[2];

    @NonNull
    private final CursorAnchorInfo.Builder mCursorAnchorInfoBuilder =
            new CursorAnchorInfo.Builder();

    @Nullable private InputMethodManagerWrapper mInputMethodManagerWrapper;
    @Nullable private final ComposingTextDelegate mComposingTextDelegate;
    @NonNull private final ViewDelegate mViewDelegate;

    private CursorAnchorInfoController(
            InputMethodManagerWrapper inputMethodManagerWrapper,
            ComposingTextDelegate composingTextDelegate,
            ViewDelegate viewDelegate) {
        mInputMethodManagerWrapper = inputMethodManagerWrapper;
        mComposingTextDelegate = composingTextDelegate;
        mViewDelegate = viewDelegate;
    }

    public static CursorAnchorInfoController create(
            InputMethodManagerWrapper inputMethodManagerWrapper,
            ComposingTextDelegate composingTextDelegate) {
        return new CursorAnchorInfoController(
                inputMethodManagerWrapper,
                composingTextDelegate,
                new ViewDelegate() {
                    @Override
                    public void getLocationOnScreen(View view, int[] location) {
                        view.getLocationOnScreen(location);
                    }
                });
    }

    public void setInputMethodManagerWrapper(InputMethodManagerWrapper inputMethodManagerWrapper) {
        mInputMethodManagerWrapper = inputMethodManagerWrapper;
    }

    public static CursorAnchorInfoController createForTest(
            InputMethodManagerWrapper inputMethodManagerWrapper,
            ComposingTextDelegate composingTextDelegate,
            ViewDelegate viewDelegate) {
        return new CursorAnchorInfoController(
                inputMethodManagerWrapper, composingTextDelegate, viewDelegate);
    }

    /** Called by ImeAdapter when a IME related web content state is changed. */
    public void invalidateLastCursorAnchorInfo() {
        if (!mIsEditable) return;

        mLastCursorAnchorInfo = null;
    }

    /**
     * Sets positional information of composing text as an array of character bounds or line
     * bounding boxes as an array of line bounds (or both).
     * @param characterBounds Array of character bounds in local coordinates.
     * @param lineBounds Array of line bounds in local coordinates.
     * @param view The attached view.
     */
    public void setBounds(
            @Nullable float[] characterBounds, @Nullable float[] lineBounds, View view) {
        if (!mIsEditable) return;
        boolean shouldUpdate = false;

        if (characterBounds != null
                && !Arrays.equals(characterBounds, mCompositionCharacterBounds)) {
            shouldUpdate = true;
            mCompositionCharacterBounds = characterBounds;
        }
        if (lineBounds != null && !Arrays.equals(lineBounds, mVisibleLineBounds)) {
            shouldUpdate = true;
            mVisibleLineBounds = lineBounds;
        }
        if (shouldUpdate) {
            mLastCursorAnchorInfo = null;
            if (mHasCoordinateInfo) {
                updateCursorAnchorInfo(view);
            }
        }
    }

    /**
     * Sends one CursorAnchorInfo object with the EditorBoundsInfo field set. All subsequent
     * CursorAnchorInfo updates will not have this field set unless they are sent through this
     * method.
     * @param editorBoundsInfo The EditorBoundsInfo sent with the CursorAnchorInfo. This is not
     *         cached.
     * @param view The attached view.
     */
    public void updateWithEditorBoundsInfo(EditorBoundsInfo editorBoundsInfo, View view) {
        if (!mIsEditable) return;
        mLastCursorAnchorInfo = null;
        mEditorBoundsInfo = editorBoundsInfo;
        updateCursorAnchorInfo(view);
        mEditorBoundsInfo = null;
    }

    /**
     * Sets coordinates system parameters and selection marker information.
     * @param scale device scale factor.
     * @param contentOffsetYPix Y offset below the browser controls.
     * @param hasInsertionMarker {@code true} if the insertion marker exists.
     * @param isInsertionMarkerVisible {@code true} if the insertion insertion marker is visible.
     * @param insertionMarkerHorizontal X coordinate of the top of the first selection marker.
     * @param insertionMarkerTop Y coordinate of the top of the first selection marker.
     * @param insertionMarkerBottom Y coordinate of the bottom of the first selection marker.
     * @param view The attached view.
     */
    public void onUpdateFrameInfo(
            float scale,
            float contentOffsetYPix,
            boolean hasInsertionMarker,
            boolean isInsertionMarkerVisible,
            float insertionMarkerHorizontal,
            float insertionMarkerTop,
            float insertionMarkerBottom,
            @NonNull View view) {
        if (!mIsEditable) return;

        // Reuse {@param #mViewOrigin} to avoid object creation, as this method is supposed to be
        // called at relatively high rate.
        mViewDelegate.getLocationOnScreen(view, mViewOrigin);

        // Character bounds and insertion marker locations come in device independent pixels
        // relative from the top-left corner of the web view content area. (In other words, the
        // effects of various kinds of zooming and scrolling are already taken into account.)
        //
        // We need to prepare parameters that convert such values to physical pixels, in the
        // screen coordinate. Hence the following values are derived.
        float translationX = mViewOrigin[0];
        float translationY = mViewOrigin[1] + contentOffsetYPix;
        if (!mHasCoordinateInfo
                || scale != mScale
                || translationX != mTranslationX
                || translationY != mTranslationY
                || hasInsertionMarker != mHasInsertionMarker
                || isInsertionMarkerVisible != mIsInsertionMarkerVisible
                || insertionMarkerHorizontal != mInsertionMarkerHorizontal
                || insertionMarkerTop != mInsertionMarkerTop
                || insertionMarkerBottom != mInsertionMarkerBottom) {
            mLastCursorAnchorInfo = null;
            mHasCoordinateInfo = true;
            mScale = scale;
            mTranslationX = translationX;
            mTranslationY = translationY;
            mHasInsertionMarker = hasInsertionMarker;
            mIsInsertionMarkerVisible = isInsertionMarkerVisible;
            mInsertionMarkerHorizontal = insertionMarkerHorizontal;
            mInsertionMarkerTop = insertionMarkerTop;
            mInsertionMarkerBottom = insertionMarkerBottom;
        }

        // Notify to IME if there is a pending request, or if it is in monitor mode and we have
        // some change in the state.
        if (mHasPendingImmediateRequest || (mMonitorModeEnabled && mLastCursorAnchorInfo == null)) {
            updateCursorAnchorInfo(view);
        }
    }

    public void focusedNodeChanged(boolean isEditable) {
        mIsEditable = isEditable;
        mCompositionCharacterBounds = null;
        mVisibleLineBounds = null;
        mHasCoordinateInfo = false;
        mLastCursorAnchorInfo = null;
    }

    public boolean onRequestCursorUpdates(
            boolean immediateRequest, boolean monitorRequest, View view) {
        if (!mIsEditable) return false;

        if (mMonitorModeEnabled && !monitorRequest) {
            // Invalidate saved cursor anchor info if monitor request is cancelled since no longer
            // new values will be arrived from renderer and immediate request may return too old
            // position.
            invalidateLastCursorAnchorInfo();
        }
        mMonitorModeEnabled = monitorRequest;
        if (immediateRequest) {
            mHasPendingImmediateRequest = true;
            updateCursorAnchorInfo(view);
        }
        return true;
    }

    /** Computes the CursorAnchorInfo instance and notify to InputMethodManager if needed. */
    private void updateCursorAnchorInfo(View view) {
        if (!mHasCoordinateInfo) return;

        if (mLastCursorAnchorInfo == null) {
            // Reuse the builder object.
            mCursorAnchorInfoBuilder.reset();

            CharSequence text = mComposingTextDelegate.getText();
            int selectionStart = mComposingTextDelegate.getSelectionStart();
            int selectionEnd = mComposingTextDelegate.getSelectionEnd();
            int composingTextStart = mComposingTextDelegate.getComposingTextStart();
            int composingTextEnd = mComposingTextDelegate.getComposingTextEnd();
            if (text != null && 0 <= composingTextStart && composingTextEnd <= text.length()) {
                mCursorAnchorInfoBuilder.setComposingText(
                        composingTextStart, text.subSequence(composingTextStart, composingTextEnd));
                float[] compositionCharacterBounds = mCompositionCharacterBounds;
                if (compositionCharacterBounds != null) {
                    int numCharacter = compositionCharacterBounds.length / 4;
                    for (int i = 0; i < numCharacter; ++i) {
                        float left = compositionCharacterBounds[i * 4];
                        float top = compositionCharacterBounds[i * 4 + 1];
                        float right = compositionCharacterBounds[i * 4 + 2];
                        float bottom = compositionCharacterBounds[i * 4 + 3];
                        int charIndex = composingTextStart + i;
                        mCursorAnchorInfoBuilder.addCharacterBounds(
                                charIndex,
                                left,
                                top,
                                right,
                                bottom,
                                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION);
                    }
                }
            }
            addVisibleLineBoundsToCursorAnchorInfo();
            mCursorAnchorInfoBuilder.setSelectionRange(selectionStart, selectionEnd);
            mMatrix.setScale(mScale, mScale);
            mMatrix.postTranslate(mTranslationX, mTranslationY);
            mCursorAnchorInfoBuilder.setMatrix(mMatrix);
            if (mEditorBoundsInfo != null && Build.VERSION.SDK_INT > Build.VERSION_CODES.TIRAMISU) {
                mCursorAnchorInfoBuilder.setEditorBoundsInfo(mEditorBoundsInfo);
            }
            if (mHasInsertionMarker) {
                mCursorAnchorInfoBuilder.setInsertionMarkerLocation(
                        mInsertionMarkerHorizontal,
                        mInsertionMarkerTop,
                        mInsertionMarkerBottom,
                        mInsertionMarkerBottom,
                        mIsInsertionMarkerVisible
                                ? CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION
                                : CursorAnchorInfo.FLAG_HAS_INVISIBLE_REGION);
            }
            mLastCursorAnchorInfo = mCursorAnchorInfoBuilder.build();
        }

        if (mInputMethodManagerWrapper != null) {
            mInputMethodManagerWrapper.updateCursorAnchorInfo(view, mLastCursorAnchorInfo);
        }
        mHasPendingImmediateRequest = false;
    }

    private void addVisibleLineBoundsToCursorAnchorInfo() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                || mVisibleLineBounds == null) {
            return;
        }
        float[] visibleLineBounds = mVisibleLineBounds;
        int numBounds = visibleLineBounds.length / 4;
        for (int i = 0; i < numBounds; ++i) {
            float left = visibleLineBounds[i * 4];
            float top = visibleLineBounds[i * 4 + 1];
            float right = visibleLineBounds[i * 4 + 2];
            float bottom = visibleLineBounds[i * 4 + 3];
            mCursorAnchorInfoBuilder.addVisibleLineBounds(left, top, right, bottom);
        }
    }
}
