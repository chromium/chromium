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

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.Display;

import androidx.annotation.UiContext;
import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Consumer;

import org.jspecify.annotations.NonNull;

/**
 * The interface definition that will be used by the WindowManager library to get custom
 * OEM-provided information about the window that isn't covered by platform APIs. Exposes methods to
 * listen to changes in the {@link WindowLayoutInfo}. A {@link WindowLayoutInfo} contains a list of
 * {@link DisplayFeature}s.
 *
 * <p>Currently {@link FoldingFeature} is the only {@link DisplayFeature}. A {@link FoldingFeature}
 * exposes the state of a hinge and the relative bounds within the window. Developers can optimize
 * their UI to support a {@link FoldingFeature} by avoiding it and placing content in relevant
 * logical areas.
 *
 * <p>This interface should be implemented by OEM and deployed to the target devices.
 *
 * @see WindowExtensions#getWindowLayoutComponent()
 */
public interface WindowLayoutComponent {
    /**
     * @deprecated Use {@link #addWindowLayoutInfoListener(Context, Consumer)} starting with vendor
     *     API level 2. Only used if {@link #addWindowLayoutInfoListener(Context, Consumer)} can't
     *     be called on vendor API level 1.
     */
    @RequiresVendorApiLevel(level = 1, deprecatedSince = 2)
    @Deprecated
    void addWindowLayoutInfoListener(
            @NonNull Activity activity,
            java.util.function.@NonNull Consumer<WindowLayoutInfo> consumer);

    /**
     * @deprecated Use {@link #removeWindowLayoutInfoListener(Consumer)} starting with vendor API
     *     level 2. Only used if {@link #removeWindowLayoutInfoListener(Consumer)} can't be called
     *     on vendor API level 1.
     */
    @RequiresVendorApiLevel(level = 1, deprecatedSince = 2)
    @Deprecated
    void removeWindowLayoutInfoListener(
            java.util.function.@NonNull Consumer<WindowLayoutInfo> consumer);

    /**
     * Adds a listener interested in receiving updates to {@link WindowLayoutInfo}. Use {@link
     * WindowLayoutComponent#removeWindowLayoutInfoListener} to remove listener.
     *
     * <p>A {@link Context} or a Consumer instance can only be registered once. Registering the same
     * {@link Context} or Consumer more than once will result in a noop.
     *
     * @param context a {@link UiContext} that corresponds to a window or an area on the screen - an
     *     {@link Activity}, a {@link Context} created with {@link
     *     Context#createWindowContext(Display, int, Bundle)}, or {@link
     *     android.inputmethodservice.InputMethodService}.
     * @param consumer interested in receiving updates to {@link WindowLayoutInfo}
     */
    @RequiresVendorApiLevel(level = 2)
    @SuppressWarnings("PairedRegistration")
    // The paired method for unregistering is also removeWindowLayoutInfoListener.
    default void addWindowLayoutInfoListener(
            @UiContext @NonNull Context context, @NonNull Consumer<WindowLayoutInfo> consumer) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Removes a listener no longer interested in receiving updates.
     *
     * @param consumer no longer interested in receiving updates to {@link WindowLayoutInfo}
     */
    @RequiresVendorApiLevel(level = 2)
    default void removeWindowLayoutInfoListener(@NonNull Consumer<WindowLayoutInfo> consumer) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Returns the {@link SupportedWindowFeatures} for the device. This value will not change over
     * time.
     *
     * @see WindowLayoutComponent#addWindowLayoutInfoListener(Context, Consumer) to register a
     *     listener for features that impact the window.
     */
    @RequiresVendorApiLevel(level = 6)
    default @NonNull SupportedWindowFeatures getSupportedWindowFeatures() {
        throw new UnsupportedOperationException(
                "This method will not be called unless there is a"
                        + " corresponding override implementation on the device");
    }

    /**
     * Returns the current {@link WindowLayoutInfo} for the given {@link Context}.
     *
     * <p>This API provides a convenient way to access the current {@link WindowLayoutInfo} without
     * registering a listener via {@link #addWindowLayoutInfoListener(Context, Consumer)}. It
     * simplifies the retrieval of {@link WindowLayoutInfo} in scenarios like {@link
     * Activity#onCreate(Bundle)}.
     *
     * @param context a {@link Context} that corresponds to a window or an area on the screen. This
     *     can be an {@link Activity}, a {@link Context} created with {@link
     *     Context#createWindowContext(Display, int, Bundle)}, or an {@link
     *     android.inputmethodservice.InputMethodService}.
     * @return the current {@link WindowLayoutInfo} for the given {@link Context}.
     */
    @RequiresVendorApiLevel(level = 9)
    default @NonNull WindowLayoutInfo getCurrentWindowLayoutInfo(
            @UiContext @NonNull Context context) {
        throw new UnsupportedOperationException(
                "This method will not be called unless there is a"
                        + " corresponding override implementation on the device");
    }
}
