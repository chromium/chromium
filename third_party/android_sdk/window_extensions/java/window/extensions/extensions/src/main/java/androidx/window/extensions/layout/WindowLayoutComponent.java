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

import androidx.annotation.NonNull;
import androidx.annotation.UiContext;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Consumer;

/**
 * The interface definition that will be used by the WindowManager library to get custom
 * OEM-provided information about the window that isn't covered by platform APIs. Exposes methods
 * to listen to changes in the {@link WindowLayoutInfo}. A {@link WindowLayoutInfo} contains a list
 * of {@link DisplayFeature}s.
 * <p>
 * Currently {@link FoldingFeature} is the only {@link DisplayFeature}. A {@link FoldingFeature}
 * exposes the state of a hinge and the relative bounds within the window. Developers can
 * optimize their UI to support a {@link FoldingFeature} by avoiding it and placing content in
 * relevant logical areas.
 *
 * <p>This interface should be implemented by OEM and deployed to the target devices.
 * @see WindowExtensions#getWindowLayoutComponent()
 */
public interface WindowLayoutComponent {
    /**
     * @deprecated Use {@link #addWindowLayoutInfoListener(Context, Consumer)}
     * starting with {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
     * {@link #addWindowLayoutInfoListener(Context, Consumer)} can't be
     * called on {@link WindowExtensions#VENDOR_API_LEVEL_1}.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_1}
     */
    @Deprecated
    void addWindowLayoutInfoListener(@NonNull Activity activity,
            @NonNull java.util.function.Consumer<WindowLayoutInfo> consumer);

    /**
     * @deprecated Use {@link #removeWindowLayoutInfoListener(Consumer)} starting with
     * {@link WindowExtensions#VENDOR_API_LEVEL_2}. Only used if
     * {@link #removeWindowLayoutInfoListener(Consumer)} can't be called on
     * {@link WindowExtensions#VENDOR_API_LEVEL_1}.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_1}
     */
    @Deprecated
    void removeWindowLayoutInfoListener(
            @NonNull java.util.function.Consumer<WindowLayoutInfo> consumer);

    /**
     * Adds a listener interested in receiving updates to {@link WindowLayoutInfo}.
     * Use {@link WindowLayoutComponent#removeWindowLayoutInfoListener} to remove listener.
     * <p>
     * A {@link Context} or a Consumer instance can only be registered once.
     * Registering the same {@link Context} or Consumer more than once will result in
     * a noop.
     *
     * @param context a {@link UiContext} that corresponds to a window or an area on the
     *                      screen - an {@link Activity}, a {@link Context} created with
     *                      {@link Context#createWindowContext(Display, int , Bundle)}, or
     *                      {@link android.inputmethodservice.InputMethodService}.
     * @param consumer interested in receiving updates to {@link WindowLayoutInfo}
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    // TODO(b/238905747): Add api guard for extensions.
    @SuppressWarnings("PairedRegistration")
    // The paired method for unregistering is also removeWindowLayoutInfoListener.
    default void addWindowLayoutInfoListener(@NonNull @UiContext Context context,
            @NonNull Consumer<WindowLayoutInfo> consumer) {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }

    /**
     * Removes a listener no longer interested in receiving updates.
     *
     * @param consumer no longer interested in receiving updates to {@link WindowLayoutInfo}
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_2}
     */
    default void removeWindowLayoutInfoListener(@NonNull Consumer<WindowLayoutInfo> consumer) {
        throw new UnsupportedOperationException("This method must not be called unless there is a"
                + " corresponding override implementation on the device.");
    }
}
