/*
 * Copyright 2023 The Android Open Source Project
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

package androidx.window.extensions.embedding;

import android.app.Activity;
import android.graphics.Rect;

import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;

import java.util.Objects;

/**
 * Describes the embedded window related info of an activity.
 *
 * @see ActivityEmbeddingComponent#setEmbeddedActivityWindowInfoCallback
 * @see ActivityEmbeddingComponent#getEmbeddedActivityWindowInfo
 */
public class EmbeddedActivityWindowInfo {

    private final @NonNull Activity mActivity;
    private final boolean mIsEmbedded;
    private final @NonNull Rect mTaskBounds;
    private final @NonNull Rect mActivityStackBounds;

    EmbeddedActivityWindowInfo(
            @NonNull Activity activity,
            boolean isEmbedded,
            @NonNull Rect taskBounds,
            @NonNull Rect activityStackBounds) {
        mActivity = Objects.requireNonNull(activity);
        mIsEmbedded = isEmbedded;
        mTaskBounds = Objects.requireNonNull(taskBounds);
        mActivityStackBounds = Objects.requireNonNull(activityStackBounds);
    }

    /** Returns the {@link Activity} this {@link EmbeddedActivityWindowInfo} is about. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull Activity getActivity() {
        return mActivity;
    }

    /**
     * Whether this activity is embedded, which means it is in an ActivityStack window that doesn't
     * fill the Task.
     */
    @RequiresVendorApiLevel(level = 6)
    public boolean isEmbedded() {
        return mIsEmbedded;
    }

    /** Returns the bounds of the Task window in display space. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull Rect getTaskBounds() {
        return mTaskBounds;
    }

    /**
     * Returns the bounds of the ActivityStack window in display space. This can be referring to the
     * bounds of the same window as {@link #getTaskBounds()} when the activity is not embedded.
     */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull Rect getActivityStackBounds() {
        return mActivityStackBounds;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof EmbeddedActivityWindowInfo)) return false;
        final EmbeddedActivityWindowInfo that = (EmbeddedActivityWindowInfo) o;
        return mActivity.equals(that.mActivity)
                && mIsEmbedded == that.mIsEmbedded
                && mTaskBounds.equals(that.mTaskBounds)
                && mActivityStackBounds.equals(that.mActivityStackBounds);
    }

    @Override
    public int hashCode() {
        int result = mActivity.hashCode();
        result = result * 31 + (mIsEmbedded ? 1 : 0);
        result = result * 31 + mTaskBounds.hashCode();
        result = result * 31 + mActivityStackBounds.hashCode();
        return result;
    }

    @Override
    public @NonNull String toString() {
        return "EmbeddedActivityWindowInfo{"
                + "activity="
                + mActivity
                + ", isEmbedded="
                + mIsEmbedded
                + ", taskBounds="
                + mTaskBounds
                + ", activityStackBounds="
                + mActivityStackBounds
                + "}";
    }
}
