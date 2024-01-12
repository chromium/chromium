/*
 * Copyright 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package androidx.window.extensions.layout;

import android.graphics.Rect;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A feature that describes a fold in a flexible display
 * or a hinge between two physical display panels.
 */
public class FoldingFeature implements DisplayFeature {

    /**
     * A fold in the flexible screen without a physical gap.
     */
    public static final int TYPE_FOLD = 1;

    /**
     * A physical separation with a hinge that allows two display panels to fold.
     */
    public static final int TYPE_HINGE = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            TYPE_FOLD,
            TYPE_HINGE,
    })
    @interface Type{}

    /**
     * The foldable device's hinge is completely open, the screen space that is presented to the
     * user is flat. See the
     * <a href="https://developer.android.com/guide/topics/ui/foldables#postures">Posture</a>
     * section in the official documentation for visual samples and references.
     */
    public static final int STATE_FLAT = 1;

    /**
     * The foldable device's hinge is in an intermediate position between opened and closed state,
     * there is a non-flat angle between parts of the flexible screen or between physical screen
     * panels. See the
     * <a href="https://developer.android.com/guide/topics/ui/foldables#postures">Posture</a>
     * section in the official documentation for visual samples and references.
     */
    public static final int STATE_HALF_OPENED = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            STATE_HALF_OPENED,
            STATE_FLAT
    })
    @interface State {}

    /**
     * The bounding rectangle of the feature within the application window in the window
     * coordinate space.
     */
    @NonNull
    private final Rect mBounds;

    /**
     * The physical type of the feature.
     */
    @Type
    private final int mType;

    /**
     * The state of the feature.
     */
    @State
    private final int mState;

    public FoldingFeature(@NonNull Rect bounds, @Type int type, @State int state) {
        validateFeatureBounds(bounds);
        mBounds = new Rect(bounds);
        mType = type;
        mState = state;
    }

    /** Gets the bounding rect of the display feature in window coordinate space. */
    @NonNull
    @Override
    public Rect getBounds() {
        return new Rect(mBounds);
    }

    /** Gets the type of the display feature. */
    @Type
    public int getType() {
        return mType;
    }

    /** Gets the state of the display feature. */
    @State
    public int getState() {
        return mState;
    }

    /**
     * Verifies the bounds of the folding feature.
     */
    private static void validateFeatureBounds(@NonNull Rect bounds) {
        if (bounds.width() == 0 && bounds.height() == 0) {
            throw new IllegalArgumentException("Bounds must be non zero.  Bounds: " + bounds);
        }
        if (bounds.left != 0 && bounds.top != 0) {
            throw new IllegalArgumentException("Bounding rectangle must start at the top or "
                    + "left window edge for folding features.  Bounds: " + bounds);
        }
    }

    @NonNull
    private static String typeToString(int type) {
        switch (type) {
            case TYPE_FOLD:
                return "FOLD";
            case TYPE_HINGE:
                return "HINGE";
            default:
                return "Unknown feature type (" + type + ")";
        }
    }

    @NonNull
    private static String stateToString(int state) {
        switch (state) {
            case STATE_FLAT:
                return "FLAT";
            case STATE_HALF_OPENED:
                return "HALF_OPENED";
            default:
                return "Unknown feature state (" + state + ")";
        }
    }

    @NonNull
    @Override
    public String toString() {
        return "ExtensionDisplayFoldFeature { " + mBounds
                + ", type=" + typeToString(getType()) + ", state=" + stateToString(mState) + " }";
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof FoldingFeature)) {
            return false;
        }
        final FoldingFeature other = (FoldingFeature) obj;
        if (mType != other.mType) {
            return false;
        }
        if (mState != other.mState) {
            return false;
        }
        return mBounds.equals(other.mBounds);
    }

    /**
     * Manually computes the hashCode for the {@link Rect} since it is not implemented
     * on API 15
     */
    private static int hashBounds(Rect bounds) {
        int result = bounds.left;
        result = 31 * result + bounds.top;
        result = 31 * result + bounds.right;
        result = 31 * result + bounds.bottom;
        return result;
    }

    @Override
    public int hashCode() {
        int result = hashBounds(mBounds);
        result = 31 * result + mType;
        result = 31 * result + mState;
        return result;
    }
}
