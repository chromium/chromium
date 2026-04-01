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

import android.content.res.Configuration;
import android.view.WindowMetrics;

import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.layout.WindowLayoutInfo;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

/**
 * The parent container information of an {@link ActivityStack}. The data class is designed to
 * provide information to calculate the presentation of an {@link ActivityStack}.
 */
@RequiresVendorApiLevel(level = 6)
public class ParentContainerInfo {
    private final @NonNull WindowMetrics mWindowMetrics;

    private final @NonNull Configuration mConfiguration;

    private final @NonNull WindowLayoutInfo mWindowLayoutInfo;

    /**
     * {@link ParentContainerInfo} constructor, which is used in Window Manager Extensions to
     * provide information of a parent window container.
     *
     * @param windowMetrics The parent container's {@link WindowMetrics}
     * @param configuration The parent container's {@link Configuration}
     * @param windowLayoutInfo The parent container's {@link WindowLayoutInfo}
     */
    ParentContainerInfo(
            @NonNull WindowMetrics windowMetrics,
            @NonNull Configuration configuration,
            @NonNull WindowLayoutInfo windowLayoutInfo) {
        mWindowMetrics = windowMetrics;
        mConfiguration = configuration;
        mWindowLayoutInfo = windowLayoutInfo;
    }

    /** Returns the parent container's {@link WindowMetrics}. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull WindowMetrics getWindowMetrics() {
        return mWindowMetrics;
    }

    /** Returns the parent container's {@link Configuration}. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull Configuration getConfiguration() {
        return mConfiguration;
    }

    /** Returns the parent container's {@link WindowLayoutInfo}. */
    @RequiresVendorApiLevel(level = 6)
    public @NonNull WindowLayoutInfo getWindowLayoutInfo() {
        return mWindowLayoutInfo;
    }

    @Override
    public int hashCode() {
        int result = mWindowMetrics.hashCode();
        result = 31 * result + mConfiguration.hashCode();
        result = 31 * result + mWindowLayoutInfo.hashCode();
        return result;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof ParentContainerInfo)) return false;
        final ParentContainerInfo parentContainerInfo = (ParentContainerInfo) obj;
        return mWindowMetrics.equals(parentContainerInfo.mWindowMetrics)
                && mConfiguration.equals(parentContainerInfo.mConfiguration)
                && mWindowLayoutInfo.equals(parentContainerInfo.mWindowLayoutInfo);
    }

    @Override
    public @NonNull String toString() {
        return ParentContainerInfo.class.getSimpleName()
                + ": {"
                + "windowMetrics="
                + WindowMetricsCompat.toString(mWindowMetrics)
                + ", configuration="
                + mConfiguration
                + ", windowLayoutInfo="
                + mWindowLayoutInfo
                + "}";
    }
}
