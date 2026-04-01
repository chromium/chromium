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

import androidx.annotation.IntDef;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Contains information about the layout of display features within the window. */
public class WindowLayoutInfo {

    /**
     * A flag indicating the engagement mode includes a visual presentation. When this flag is set,
     * it means the user can visually see the app UI on visible window.
     */
    public static final int ENGAGEMENT_MODE_FLAG_VISUALS_ON = 1 << 0;

    /**
     * A flag indicating the engagement mode includes an audio presentation. This can be set with or
     * without {@link #ENGAGEMENT_MODE_FLAG_VISUALS_ON}. When set without, it signifies an
     * audio-only experience.
     */
    public static final int ENGAGEMENT_MODE_FLAG_AUDIO_ON = 1 << 1;

    /** Annotation for the engagement mode flags. */
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
            flag = true,
            value = {ENGAGEMENT_MODE_FLAG_VISUALS_ON, ENGAGEMENT_MODE_FLAG_AUDIO_ON})
    public @interface EngagementModeFlags {}

    private static final int DEFAULT_ENGAGEMENT_MODE =
            ENGAGEMENT_MODE_FLAG_VISUALS_ON | ENGAGEMENT_MODE_FLAG_AUDIO_ON;

    /**
     * List of display features within the window.
     *
     * <p>NOTE: All display features returned with this container must be cropped to the application
     * window and reported within the coordinate space of the window that was provided by the app.
     */
    private final @NonNull List<DisplayFeature> mDisplayFeatures;

    /** The user engagement mode flags for this window. */
    private final @EngagementModeFlags int mEngagementModeFlags;

    /**
     * @deprecated Use the {@link Builder} instead.
     */
    @RequiresVendorApiLevel(level = 1, deprecatedSince = 10)
    @Deprecated
    public WindowLayoutInfo(@NonNull List<DisplayFeature> displayFeatures) {
        this(displayFeatures, DEFAULT_ENGAGEMENT_MODE);
    }

    private WindowLayoutInfo(
            @NonNull List<DisplayFeature> displayFeatures,
            @EngagementModeFlags int engagementModeFlags) {
        mDisplayFeatures = Collections.unmodifiableList(displayFeatures);
        mEngagementModeFlags = engagementModeFlags;
    }

    /** Gets the list of display features present within the window. */
    public @NonNull List<DisplayFeature> getDisplayFeatures() {
        return mDisplayFeatures;
    }

    /**
     * Returns the current user engagement mode flags for this window.
     *
     * @return The current {@link EngagementModeFlags}.
     */
    @RequiresVendorApiLevel(level = 10)
    @EngagementModeFlags
    public int getEngagementModeFlags() {
        return mEngagementModeFlags;
    }

    /**
     * Checks if a specific flag is present in the engagement mode.
     *
     * @param flag The specific {@link EngagementModeFlags} flag to check for.
     * @return {@code true} if the flag is set, {@code false} otherwise.
     */
    @RequiresVendorApiLevel(level = 10)
    public boolean hasEngagementModeFlag(@EngagementModeFlags int flag) {
        return (mEngagementModeFlags & flag) == flag;
    }

    @Override
    public @NonNull String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("ExtensionWindowLayoutInfo { ExtensionDisplayFeatures[ ");
        for (int i = 0; i < mDisplayFeatures.size(); i = i + 1) {
            sb.append(mDisplayFeatures.get(i));
            if (i < mDisplayFeatures.size() - 1) {
                sb.append(", ");
            }
        }
        sb.append(" ], ExtensionEngagementModeFlags=").append(mEngagementModeFlags).append(" }");
        return sb.toString();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }
        if (!(obj instanceof WindowLayoutInfo)) {
            return false;
        }
        final WindowLayoutInfo other = (WindowLayoutInfo) obj;
        return mDisplayFeatures.equals(other.mDisplayFeatures)
                && mEngagementModeFlags == other.mEngagementModeFlags;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mDisplayFeatures, mEngagementModeFlags);
    }

    /** Builder for {@link WindowLayoutInfo}. */
    public static final class Builder {
        private @NonNull List<DisplayFeature> mDisplayFeatures = Collections.emptyList();
        private @EngagementModeFlags int mEngagementModeFlags = DEFAULT_ENGAGEMENT_MODE;

        /** Creates a new instance of the {@link Builder}. */
        public Builder() {}

        /**
         * Sets the list of {@link DisplayFeature} present within the window.
         *
         * @param displayFeatures the list of {@link DisplayFeature} to set.
         * @return this {@link Builder} instance.
         */
        public @NonNull Builder setDisplayFeatures(@NonNull List<DisplayFeature> displayFeatures) {
            mDisplayFeatures = displayFeatures;
            return this;
        }

        /**
         * Sets the current user engagement mode flags for this window.
         *
         * @param flags the {@link EngagementModeFlags} to set.
         * @return this {@link Builder} instance.
         */
        @RequiresVendorApiLevel(level = 10)
        public @NonNull Builder setEngagementModeFlags(@EngagementModeFlags int flags) {
            mEngagementModeFlags = flags;
            return this;
        }

        /**
         * Builds a new {@link WindowLayoutInfo} instance.
         *
         * @return a new {@link WindowLayoutInfo} instance.
         */
        @RequiresVendorApiLevel(level = 10)
        public @NonNull WindowLayoutInfo build() {
            return new WindowLayoutInfo(mDisplayFeatures, mEngagementModeFlags);
        }
    }
}
