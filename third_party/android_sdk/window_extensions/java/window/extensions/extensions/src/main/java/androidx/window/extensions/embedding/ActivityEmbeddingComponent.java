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

package androidx.window.extensions.embedding;

import android.app.Activity;
import android.app.ActivityOptions;
import android.os.IBinder;
import android.view.WindowMetrics;

import androidx.annotation.RestrictTo;
import androidx.window.extensions.RequiresVendorApiLevel;
import androidx.window.extensions.WindowExtensions;
import androidx.window.extensions.core.util.function.Consumer;
import androidx.window.extensions.core.util.function.Function;
import androidx.window.extensions.util.SetCompat;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * Extension component definition that is used by the WindowManager library to trigger custom
 * OEM-provided methods for organizing activities that isn't covered by platform APIs.
 *
 * <p>This interface should be implemented by OEM and deployed to the target devices.
 *
 * @see androidx.window.extensions.WindowExtensions
 */
public interface ActivityEmbeddingComponent {

    /** The vendor API level of the overlay feature APIs. */
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    int OVERLAY_FEATURE_API_LEVEL = 8;

    /** Updates the rules of embedding activities that are started in the client process. */
    @RequiresVendorApiLevel(level = 1)
    void setEmbeddingRules(@NonNull Set<EmbeddingRule> splitRules);

    /**
     * @deprecated Use {@link #setSplitInfoCallback(Consumer)} starting with vendor API level 2.
     *     Only used if {@link #setSplitInfoCallback(Consumer)} can't be called on vendor level 1.
     */
    @RequiresVendorApiLevel(level = 1, deprecatedSince = 2)
    @Deprecated
    @SuppressWarnings("ExecutorRegistration") // Jetpack will post it on the app-provided executor.
    void setSplitInfoCallback(java.util.function.@NonNull Consumer<List<SplitInfo>> consumer);

    /**
     * Sets the callback that notifies WM Jetpack about changes in split states from the Extensions
     * Sidecar implementation. The listener should be registered for the lifetime of the process.
     * There are no threading guarantees where the events are dispatched from.
     *
     * @param consumer the callback to notify {@link SplitInfo} list changes
     */
    @RequiresVendorApiLevel(level = 2)
    @SuppressWarnings("ExecutorRegistration") // Jetpack will post it on the app-provided executor.
    default void setSplitInfoCallback(@NonNull Consumer<List<SplitInfo>> consumer) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Clears the callback that was set in {@link
     * ActivityEmbeddingComponent#setSplitInfoCallback(Consumer)}. Added in {@link
     * WindowExtensions#getVendorApiLevel()} 2, calling an earlier version will throw {@link
     * java.lang.NoSuchMethodError}.
     */
    @RequiresVendorApiLevel(level = 2)
    void clearSplitInfoCallback();

    /**
     * Checks if an activity's' presentation is customized by its or any other process and only
     * occupies a portion of Task bounds.
     */
    @RequiresVendorApiLevel(level = 1)
    boolean isActivityEmbedded(@NonNull Activity activity);

    /**
     * Pins the top-most {@link ActivityStack} to keep the stack of the Activities to be positioned
     * on top. The rest of the activities in the Task will be split with the pinned {@link
     * ActivityStack}. The pinned {@link ActivityStack} would also have isolated activity navigation
     * in which only the activities that are started from the pinned {@link ActivityStack} can be
     * added on top of the {@link ActivityStack}.
     *
     * <p>The pinned {@link ActivityStack} is unpinned whenever the parent Task bounds don't satisfy
     * the dimensions and aspect ratio requirements {@link SplitRule#checkParentMetrics} to show two
     * {@link ActivityStack}s. See {@link SplitPinRule.Builder#setSticky} if the same {@link
     * ActivityStack} should be pinned again whenever the parent Task bounds satisfies the
     * dimensions and aspect ratios requirements defined in the rule.
     *
     * @param taskId The id of the Task that top {@link ActivityStack} should be pinned.
     * @param splitPinRule The SplitRule that specifies how the top {@link ActivityStack} should be
     *     split with others.
     * @return Returns {@code true} if the top {@link ActivityStack} is successfully pinned.
     *     Otherwise, {@code false}. Few examples are: 1. There's no {@link ActivityStack}. 2. There
     *     is already an existing pinned {@link ActivityStack}. 3. There's no other {@link
     *     ActivityStack} to split with the top {@link ActivityStack}.
     */
    @RequiresVendorApiLevel(level = 5)
    default boolean pinTopActivityStack(int taskId, @NonNull SplitPinRule splitPinRule) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Unpins the pinned {@link ActivityStack}. The {@link ActivityStack} will still be the top-most
     * {@link ActivityStack} right after unpinned, and the {@link ActivityStack} could be expanded
     * or continue to be split with the next top {@link ActivityStack} if the current state matches
     * any of the existing {@link SplitPairRule}. It is a no-op call if the task does not have a
     * pinned {@link ActivityStack}.
     *
     * @param taskId The id of the Task that top {@link ActivityStack} should be unpinned.
     */
    @RequiresVendorApiLevel(level = 5)
    default void unpinTopActivityStack(int taskId) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Sets a function to compute the {@link SplitAttributes} for the {@link SplitRule} and current
     * window state provided in {@link SplitAttributesCalculatorParams}.
     *
     * <p>This method can be used to dynamically configure the split layout properties when new
     * activities are launched or window properties change.
     *
     * <p>If the {@link SplitAttributes} calculator function is not set or is cleared by {@link
     * #clearSplitAttributesCalculator()}, apps will update its split layout with registered {@link
     * SplitRule} configurations:
     *
     * <ul>
     *   <li>Split with {@link SplitRule#getDefaultSplitAttributes()} if parent task container size
     *       constraints defined by {@link SplitRule#checkParentMetrics(WindowMetrics)} are
     *       satisfied
     *   <li>Occupy the whole parent task bounds if the constraints are not satisfied.
     * </ul>
     *
     * <p>If the function is set, {@link SplitRule#getDefaultSplitAttributes()} and {@link
     * SplitRule#checkParentMetrics(WindowMetrics)} will be passed to {@link
     * SplitAttributesCalculatorParams} as {@link
     * SplitAttributesCalculatorParams#getDefaultSplitAttributes()} and {@link
     * SplitAttributesCalculatorParams#areDefaultConstraintsSatisfied()} instead, and the function
     * will be invoked for every device and window state change regardless of the size constraints.
     * Users can determine to follow the {@link SplitRule} behavior or customize the {@link
     * SplitAttributes} with the {@link SplitAttributes} calculator function.
     *
     * @param calculator the callback to set. It will replace the previously set callback if it
     *     exists.
     */
    @RequiresVendorApiLevel(level = 2)
    void setSplitAttributesCalculator(
            @NonNull Function<SplitAttributesCalculatorParams, SplitAttributes> calculator);

    /**
     * Clears the previously callback set in {@link #setSplitAttributesCalculator(Function)}.
     *
     * @see #setSplitAttributesCalculator(Function)
     */
    @RequiresVendorApiLevel(level = 2)
    void clearSplitAttributesCalculator();

    /**
     * @deprecated Use {@link ActivityEmbeddingOptionsProperties#KEY_ACTIVITY_STACK_TOKEN} instead.
     */
    @Deprecated
    @RequiresVendorApiLevel(level = 3, deprecatedSince = 5)
    default @NonNull ActivityOptions setLaunchingActivityStack(
            @NonNull ActivityOptions options, @NonNull IBinder token) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * @deprecated Use {@link #finishActivityStacksWithTokens(Set)} with instead.
     */
    @Deprecated
    @RequiresVendorApiLevel(level = 3, deprecatedSince = 5)
    default void finishActivityStacks(@NonNull Set<IBinder> activityStackTokens) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Finishes a set of {@link ActivityStack}s. When an {@link ActivityStack} that was in an active
     * split is finished, the other {@link ActivityStack} in the same {@link SplitInfo} can be
     * expanded to fill the parent task container.
     *
     * @param activityStackTokens The set of tokens of {@link ActivityStack}-s that is going to be
     *     finished.
     */
    @SuppressWarnings("deprecation") // Use finishActivityStacks(Set) as its core implementation.
    @RequiresVendorApiLevel(level = 5)
    default void finishActivityStacksWithTokens(
            @NonNull Set<ActivityStack.Token> activityStackTokens) {
        final Set<IBinder> binderSet = SetCompat.create();

        for (ActivityStack.Token token : activityStackTokens) {
            binderSet.add(token.getRawToken());
        }
        finishActivityStacks(binderSet);
    }

    /**
     * Triggers an update of the split attributes for the top split if there is one visible by
     * making extensions invoke the split attributes calculator callback. This method can be used
     * when a change to the split presentation originates from the application state change rather
     * than driven by parent window changes or new activity starts. The call will be ignored if
     * there is no visible split.
     *
     * @see #setSplitAttributesCalculator(Function)
     */
    @RequiresVendorApiLevel(level = 3)
    default void invalidateTopVisibleSplitAttributes() {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * @deprecated Use {@link #updateSplitAttributes(SplitInfo.Token, SplitAttributes)} instead.
     */
    @Deprecated
    @RequiresVendorApiLevel(level = 3, deprecatedSince = 5)
    default void updateSplitAttributes(
            @NonNull IBinder splitInfoToken, @NonNull SplitAttributes splitAttributes) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Updates the {@link SplitAttributes} of a split pair. This is an alternative to using a split
     * attributes calculator callback, applicable when apps only need to update the splits in a few
     * cases but rely on the default split attributes otherwise.
     *
     * @param splitInfoToken The identifier of the split pair to update.
     * @param splitAttributes The {@link SplitAttributes} to apply to the split pair.
     */
    @SuppressWarnings("deprecation") // Use finishActivityStacks(Set).
    @RequiresVendorApiLevel(level = 5)
    default void updateSplitAttributes(
            SplitInfo.@NonNull Token splitInfoToken, @NonNull SplitAttributes splitAttributes) {
        updateSplitAttributes(splitInfoToken.getRawToken(), splitAttributes);
    }

    /**
     * Returns the {@link ParentContainerInfo} by the {@link ActivityStack} token, or {@code null}
     * if there's not such {@link ActivityStack} associated with the {@code token}.
     *
     * @param activityStackToken the token of an {@link ActivityStack}.
     */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    default @Nullable ParentContainerInfo getParentContainerInfo(
            ActivityStack.@NonNull Token activityStackToken) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Sets a function to compute the {@link ActivityStackAttributes} for the ActivityStack given
     * for the current window and device state provided in {@link
     * ActivityStackAttributesCalculatorParams} on the main thread.
     *
     * <p>This calculator function is only triggered if the {@link ActivityStack#getTag()} is
     * specified. Similar to {@link #setSplitAttributesCalculator(Function)}, the calculator
     * function could be triggered multiple times. It will be triggered whenever there's a launching
     * standalone {@link ActivityStack} with {@link ActivityStack#getTag()} specified, or a parent
     * window or device state update, such as device rotation, folding state change, or the host
     * task goes to multi-window mode.
     *
     * @param calculator The calculator function to calculate {@link ActivityStackAttributes} based
     *     on {@link ActivityStackAttributesCalculatorParams}.
     */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    default void setActivityStackAttributesCalculator(
            @NonNull Function<ActivityStackAttributesCalculatorParams, ActivityStackAttributes>
                    calculator) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Clears the calculator function previously set by {@link
     * #setActivityStackAttributesCalculator(Function)}
     */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    default void clearActivityStackAttributesCalculator() {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Updates {@link ActivityStackAttributes} to an {@link ActivityStack} specified with {@code
     * token} and applies the change directly. If there's no such an {@link ActivityStack}, this
     * method is no-op.
     *
     * @param token The {@link ActivityStack} to update.
     * @param activityStackAttributes The attributes to be applied
     */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    default void updateActivityStackAttributes(
            ActivityStack.@NonNull Token token,
            @NonNull ActivityStackAttributes activityStackAttributes) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Gets the {@link ActivityStack}'s token by {@code tag}, or {@code null} if there's no {@link
     * ActivityStack} associated with the {@code tag}. For example, the {@link ActivityStack} is
     * dismissed before the is method is called.
     *
     * <p>The {@link ActivityStack} token can be obtained immediately after the {@link
     * ActivityStack} is created. This method is usually used when Activity Embedding library wants
     * to {@link #updateActivityStackAttributes} before receiving the {@link ActivityStack} record
     * from the callback set by {@link #registerActivityStackCallback}.
     *
     * <p>For example, an app launches an overlay container and calls {@link
     * #updateActivityStackAttributes} immediately right before the overlay {@link ActivityStack} is
     * received from {@link #registerActivityStackCallback}.
     *
     * @param tag A unique identifier of an {@link ActivityStack} if set
     * @return The {@link ActivityStack}'s token that the tag is associated with, or {@code null} if
     *     there's no such an {@link ActivityStack}.
     */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    default ActivityStack.@Nullable Token getActivityStackToken(@NonNull String tag) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Registers a callback that notifies WindowManager Jetpack about changes in {@link
     * ActivityStack}.
     *
     * <p>In most cases, {@link ActivityStack} are a part of {@link SplitInfo} as {@link
     * SplitInfo#getPrimaryActivityStack() the primary ActivityStack} or {@link
     * SplitInfo#getSecondaryActivityStack() the secondary ActivityStack} of a {@link SplitInfo}.
     *
     * <p>However, there are some cases that {@link ActivityStack} is standalone and usually
     * expanded. Cases are:
     *
     * <ul>
     *   <li>A started {@link Activity} matches {@link ActivityRule} with {@link
     *       ActivityRule#shouldAlwaysExpand()} {@code true}.
     *   <li>The {@code ActivityStack} is an overlay {@code ActivityStack}.
     *   <li>The associated {@link ActivityStack activityStacks} of a {@code ActivityStack} are
     *       dismissed by {@link #finishActivityStacks(Set)}.
     *   <li>One {@link ActivityStack} of {@link SplitInfo}(Either {@link
     *       SplitInfo#getPrimaryActivityStack() the primary ActivityStack} or {@link
     *       SplitInfo#getSecondaryActivityStack() the secondary ActivityStack}) is empty and
     *       finished, while the other {@link ActivityStack} is not finished with the finishing
     *       {@link ActivityStack}.
     *       <p>An example is a pair of activities matches a {@link SplitPairRule}, and its {@link
     *       SplitPairRule#getFinishPrimaryWithSecondary()} is {@link SplitRule#FINISH_NEVER}. Then
     *       if the last activity of {@link SplitInfo#getSecondaryActivityStack() the secondary
     *       ActivityStack}) is finished, {@link SplitInfo#getPrimaryActivityStack() the primary
     *       ActivityStack} will still remain.
     * </ul>
     *
     * @param executor the executor to dispatch {@link ActivityStack} list changes.
     * @param callback the callback to notify {@link ActivityStack} list changes.
     * @see ActivityEmbeddingComponent#finishActivityStacks(Set)
     */
    @RequiresVendorApiLevel(level = 5)
    default void registerActivityStackCallback(
            @NonNull Executor executor, @NonNull Consumer<List<ActivityStack>> callback) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Removes the callback previously registered in {@link #registerActivityStackCallback}, or
     * no-op if the callback hasn't been registered yet.
     *
     * @param callback The callback to remove, which should have been registered.
     */
    @RequiresVendorApiLevel(level = 5)
    default void unregisterActivityStackCallback(@NonNull Consumer<List<ActivityStack>> callback) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Sets a callback that notifies WindowManager Jetpack about changes for a given {@link
     * Activity} to its {@link EmbeddedActivityWindowInfo}.
     *
     * <p>The callback will be invoked when the {@link EmbeddedActivityWindowInfo} is changed after
     * the {@link Activity} is launched. Similar to {@link Activity#onConfigurationChanged}, the
     * callback will only be invoked for visible {@link Activity}.
     *
     * @param executor the executor to dispatch {@link EmbeddedActivityWindowInfo} change.
     * @param callback the callback to notify {@link EmbeddedActivityWindowInfo} change.
     */
    @RequiresVendorApiLevel(level = 6)
    default void setEmbeddedActivityWindowInfoCallback(
            @NonNull Executor executor, @NonNull Consumer<EmbeddedActivityWindowInfo> callback) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Clears the callback previously set by {@link #setEmbeddedActivityWindowInfoCallback(Executor,
     * Consumer)}
     */
    @RequiresVendorApiLevel(level = 6)
    default void clearEmbeddedActivityWindowInfoCallback() {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Returns the {@link EmbeddedActivityWindowInfo} of the given {@link Activity}, or {@code null}
     * if the {@link Activity} is not attached.
     *
     * <p>This API can be used when {@link #setEmbeddedActivityWindowInfoCallback} is not set before
     * the Activity is attached.
     *
     * @param activity the {@link Activity} to get {@link EmbeddedActivityWindowInfo} for.
     */
    @RequiresVendorApiLevel(level = 6)
    default @Nullable EmbeddedActivityWindowInfo getEmbeddedActivityWindowInfo(
            @NonNull Activity activity) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }

    /**
     * Sets whether to auto save the embedding state to the system, which can be used to restore the
     * app embedding state once the app process is restarted (if applicable).
     *
     * <p>The embedding state is not saved by default, in which case the embedding state and the
     * embedded activities are removed once the app process is killed.
     *
     * <p>**Note** that the applications should set the {@link EmbeddingRule}s using {@link
     * #setEmbeddingRules} when the application is initializing, such as configured in
     * [android.app.Application.onCreate], in order to allow the library to restore the state
     * properly. Otherwise, the state may not be restored and the activities may not be started and
     * layout as expected.
     *
     * @param saveEmbeddingState whether to save the embedding state.
     */
    @RequiresVendorApiLevel(level = 8)
    default void setAutoSaveEmbeddingState(boolean saveEmbeddingState) {
        throw new UnsupportedOperationException(
                "This method must not be called unless there is a"
                        + " corresponding override implementation on the device.");
    }
}
