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

package androidx.window.extensions;

import static androidx.annotation.RestrictTo.Scope.LIBRARY_GROUP;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.os.IBinder;

import androidx.annotation.Nullable;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.area.WindowAreaComponent;
import androidx.window.extensions.core.util.function.Consumer;
import androidx.window.extensions.embedding.ActivityEmbeddingComponent;
import androidx.window.extensions.embedding.ActivityStack;
import androidx.window.extensions.embedding.SplitAttributes;
import androidx.window.extensions.embedding.SplitInfo;
import androidx.window.extensions.layout.WindowLayoutComponent;

import java.util.Set;

/**
 * A class to provide instances of different WindowManager Jetpack extension components. An OEM must
 * implement all the availability methods to state which WindowManager Jetpack extension
 * can be used. If a component is not available then the check must return {@code false}. Trying to
 * get a component that is not available will throw an {@link UnsupportedOperationException}.
 * All components must support the API level returned in
 * {@link WindowExtensions#getVendorApiLevel()}.
 */
public interface WindowExtensions {
    // TODO(b/241323716) Removed after we have annotation to check API level
    /**
     * An invalid {@link #getVendorApiLevel vendor API level}
     */
    @RestrictTo(LIBRARY_GROUP)
    int INVALID_VENDOR_API_LEVEL = -1;

    // TODO(b/241323716) Removed after we have annotation to check API level
    /**
     * A vendor API level constant. It helps to unify the format of documenting {@code @since}
     * block.
     * <p>
     * The added APIs for Vendor API level 1 are:
     * <ul>
     *     <li>{@link androidx.window.extensions.embedding.ActivityRule} APIs</li>
     *     <li>{@link androidx.window.extensions.embedding.SplitPairRule} APIs</li>
     *     <li>{@link androidx.window.extensions.embedding.SplitPlaceholderRule} APIs</li>
     *     <li>{@link androidx.window.extensions.embedding.SplitInfo} APIs</li>
     *     <li>{@link androidx.window.extensions.layout.DisplayFeature} APIs</li>
     *     <li>{@link androidx.window.extensions.layout.FoldingFeature} APIs</li>
     *     <li>{@link androidx.window.extensions.layout.WindowLayoutInfo} APIs</li>
     *     <li>{@link androidx.window.extensions.layout.WindowLayoutComponent} APIs</li>
     * </ul>
     * </p>
     */
    @RestrictTo(LIBRARY_GROUP)
    int VENDOR_API_LEVEL_1 = 1;

    // TODO(b/241323716) Removed after we have annotation to check API level
    /**
     * A vendor API level constant. It helps to unify the format of documenting {@code @since}
     * block.
     * The added APIs for Vendor API level 2 are:
     * <ul>
     *     <li>{@link WindowAreaComponent#addRearDisplayStatusListener(Consumer)}</li>
     *     <li>{@link WindowAreaComponent#startRearDisplaySession(Activity, Consumer)}</li>
     *     <li>{@link androidx.window.extensions.embedding.SplitPlaceholderRule.Builder#setFinishPrimaryWithPlaceholder(int)}</li>
     *     <li>{@link androidx.window.extensions.embedding.SplitAttributes}</li>
     *     <li>{@link ActivityEmbeddingComponent#setSplitAttributesCalculator(
     *      androidx.window.extensions.core.util.function.Function)}</li>
     *     <li>{@link WindowLayoutComponent#addWindowLayoutInfoListener(Context, Consumer)}</li>
     * </ul>
     */
    @RestrictTo(LIBRARY_GROUP)
    int VENDOR_API_LEVEL_2 = 2;

    // TODO(b/241323716) Removed after we have annotation to check API level
    /**
     * A vendor API level constant. It helps to unify the format of documenting {@code @since}
     * block.
     * <p>
     * The added APIs for Vendor API level 3 are:
     * <ul>
     *     <li>{@link ActivityStack#getToken()}</li>
     *     <li>{@link SplitInfo#getToken()}</li>
     *     <li>{@link ActivityEmbeddingComponent#setLaunchingActivityStack(ActivityOptions,
     *     IBinder)}</li>
     *     <li>{@link ActivityEmbeddingComponent#invalidateTopVisibleSplitAttributes()}</li>
     *     <li>{@link ActivityEmbeddingComponent#updateSplitAttributes(IBinder, SplitAttributes)}
     *     </li>
     *     <li>{@link ActivityEmbeddingComponent#finishActivityStacks(Set)}</li>
     *     <li>{@link WindowAreaComponent#addRearDisplayPresentationStatusListener(Consumer)}</li>
     *     <li>{@link WindowAreaComponent#startRearDisplayPresentationSession(Activity, Consumer)}
     *     </li>
     *     <li>{@link WindowAreaComponent#getRearDisplayMetrics()}</li>
     *     <li>{@link WindowAreaComponent#getRearDisplayPresentation()}</li>
     * </ul>
     * </p>
     */
    @RestrictTo(LIBRARY_GROUP)
    int VENDOR_API_LEVEL_3 = 3;

    /**
     * Returns the API level of the vendor library on the device. If the returned version is not
     * supported by the WindowManager library, then some functions may not be available or replaced
     * with stub implementations.
     *
     * The expected use case is for the WindowManager library to determine which APIs are
     * available and wrap the API so that app developers do not need to deal with the complexity.
     * @return the API level supported by the library.
     */
    default int getVendorApiLevel() {
        throw new RuntimeException("Not implemented. Must override in a subclass.");
    }

    /**
     * Returns the OEM implementation of {@link WindowLayoutComponent} if it is supported on the
     * device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     * @return the OEM implementation of {@link WindowLayoutComponent}
     */
    @Nullable
    WindowLayoutComponent getWindowLayoutComponent();

    /**
     * Returns the OEM implementation of {@link ActivityEmbeddingComponent} if it is supported on
     * the device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     * @return the OEM implementation of {@link ActivityEmbeddingComponent}
     */
    @Nullable
    default ActivityEmbeddingComponent getActivityEmbeddingComponent() {
        return null;
    }

    /**
     * Returns the OEM implementation of {@link WindowAreaComponent} if it is supported on
     * the device, {@code null} otherwise. The implementation must match the API level reported in
     * {@link WindowExtensions}.
     * @return the OEM implementation of {@link WindowAreaComponent}
     */
    @Nullable
    default WindowAreaComponent getWindowAreaComponent() {
        return null;
    }
}
