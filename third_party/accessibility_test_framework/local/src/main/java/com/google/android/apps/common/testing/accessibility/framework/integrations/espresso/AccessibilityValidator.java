/*
 * Copyright (C) 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
package com.google.android.apps.common.testing.accessibility.framework.integrations.espresso;

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import android.content.Context;
import android.util.Log;
import android.view.View;

import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckPreset;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckPresetAndroid;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResult;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResult.AccessibilityCheckResultType;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResultDescriptor;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityCheckResultUtils;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityViewCheckResult;
import com.google.android.apps.common.testing.accessibility.framework.AccessibilityViewHierarchyCheck;
import com.google.android.apps.common.testing.accessibility.framework.Parameters;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.FluentIterable;
import com.google.common.collect.ImmutableList;

import org.checkerframework.checker.nullness.qual.Nullable;
import org.hamcrest.Matcher;

import java.util.ArrayList;
import java.util.List;

/**
 * A configurable executor for the {@link AccessibilityViewHierarchyCheck}s designed for use with
 * Espresso. Clients can call {@link #checkAndReturnResults} on a {@link View} to run all of the
 * checks with the options specified in this object.
 *
 * <p>This file is from accessibility_test_framework 3.1 and has been modified to work with 4.0.
 */
@SuppressWarnings("deprecation")
public final class AccessibilityValidator {

    private static final String TAG = "AccessibilityValidator";
    private AccessibilityCheckPreset preset = AccessibilityCheckPreset.LATEST;
    private boolean runChecksFromRootView = false;

    @Nullable
    private AccessibilityCheckResultType throwExceptionFor = AccessibilityCheckResultType.ERROR;

    // Either resultDescriptor or deprecatedResultDescriptor must have a non-null value, but not
    // both.
    private AccessibilityCheckResult.AccessibilityCheckResultDescriptor deprecatedResultDescriptor =
            null;
    private @Nullable AccessibilityCheckResultDescriptor resultDescriptor =
            new AccessibilityCheckResultDescriptor();

    @Nullable private Matcher<? super AccessibilityViewCheckResult> suppressingMatcher = null;
    private final List<AccessibilityCheckListener> checkListeners = new ArrayList<>();

    public AccessibilityValidator() {}

    /**
     * Runs accessibility checks.
     *
     * @param view the {@link View} to check
     */
    public final void check(View view) {
        checkNotNull(view);
        checkAndReturnResults(view);
    }

    /**
     * Runs accessibility checks and returns the list of results. If the result is not needed, call
     * {@link #check(View)} instead.
     *
     * @param view the {@link View} to check
     * @return an immutable list of the resulting {@link AccessibilityViewCheckResult}s
     */

    // Local change to be immutable list return type declaration.
    public final ImmutableList<AccessibilityViewCheckResult> checkAndReturnResults(View view) {
        if (view != null) {
            View viewToCheck = runChecksFromRootView ? view.getRootView() : view;
            return runAccessibilityChecks(viewToCheck);
        }
        return ImmutableList.<AccessibilityViewCheckResult>of();
    }

    /**
     * Specify the set of checks to be run. The default is {link AccessibilityCheckPreset.LATEST}.
     *
     * @param preset The preset specifying the group of checks to run.
     * @return this
     */
    public AccessibilityValidator setCheckPreset(AccessibilityCheckPreset preset) {
        this.preset = preset;
        return this;
    }

    /**
     * @param runChecksFromRootView {@code true} to check all views in the hierarchy, {@code false}
     *     to check only views in the hierarchy rooted at the passed in view. Default: {@code false}
     * @return this
     */
    public AccessibilityValidator setRunChecksFromRootView(boolean runChecksFromRootView) {
        this.runChecksFromRootView = runChecksFromRootView;
        return this;
    }

    /**
     * Suppresses all results that match the given matcher. Suppressed results will not be included
     * in any logs or cause any {@code Exception} to be thrown
     *
     * @param resultMatcher a matcher that specifies result to be suppressed. If {@code null}, then
     *     any previously set matcher will be removed and the default behavior will be restored.
     * @return this
     */
    public AccessibilityValidator setSuppressingResultMatcher(
            @Nullable Matcher<? super AccessibilityViewCheckResult> resultMatcher) {
        suppressingMatcher = resultMatcher;
        return this;
    }

    /**
     * @param throwExceptionForErrors {@code true} to throw an {@code Exception} when there is at
     *     least one error result, {@code false} to just log the error results to logcat. Default:
     *     {@code true}
     * @return this
     * @deprecated Use {@link #setThrowExceptionFor}
     */
    @Deprecated
    public AccessibilityValidator setThrowExceptionForErrors(boolean throwExceptionForErrors) {
        return setThrowExceptionFor(
                throwExceptionForErrors ? AccessibilityCheckResultType.ERROR : null);
    }

    /**
     * Specifies the types of results that should produce a thrown exception.
     *
     * <ul>
     *   If the value is:
     *   <li>{@link AccessibilityCheckResultType#ERROR}, an exception will be thrown for any ERROR
     *   <li>{@link AccessibilityCheckResultType#WARNING}, an exception will be thrown for any ERROR
     *       or WARNING
     *   <li>{@link AccessibilityCheckResultType#INFO}, an exception will be thrown for any ERROR,
     *       WARNING or INFO
     *   <li>{@code null}, no exception will be thrown
     * </ul>
     *
     * The default is {@code ERROR}.
     *
     * @return this
     */
    public AccessibilityValidator setThrowExceptionFor(
            @Nullable AccessibilityCheckResultType throwFor) {
        checkArgument(
                (throwFor == AccessibilityCheckResultType.ERROR)
                        || (throwFor == AccessibilityCheckResultType.WARNING)
                        || (throwFor == AccessibilityCheckResultType.INFO)
                        || (throwFor == null),
                "Argument was %s but expected ERROR, WARNING, INFO or null.",
                throwFor);
        throwExceptionFor = throwFor;
        return this;
    }

    /**
     * Sets the {@link AccessibilityCheckResult.AccessibilityCheckResultDescriptor} that is used to
     * convert results to readable messages in exceptions and logcat statements.
     *
     * @return this
     * @deprecated Use {@link #setResultDescriptor(AccessibilityCheckResultDescriptor)} instead.
     */
    @Deprecated
    public AccessibilityValidator setResultDescriptor(
            AccessibilityCheckResult.AccessibilityCheckResultDescriptor
                    deprecatedResultDescriptor) {
        this.deprecatedResultDescriptor = checkNotNull(deprecatedResultDescriptor);
        this.resultDescriptor = null;
        return this;
    }

    /**
     * Sets the {@link AccessibilityCheckResultDescriptor} that is used to convert results to
     * readable messages in exceptions and logcat statements.
     *
     * @return this
     */
    public AccessibilityValidator setResultDescriptor(
            AccessibilityCheckResultDescriptor resultDescriptor) {
        this.deprecatedResultDescriptor = null;
        this.resultDescriptor = checkNotNull(resultDescriptor);
        return this;
    }

    /**
     * Adds a listener to receive all {@link AccessibilityCheckResult}s after suppression. Listeners
     * will be called in the order they are added and before any {@link
     * AccessibilityViewCheckException} would be thrown.
     *
     * @return this
     */
    public AccessibilityValidator addCheckListener(AccessibilityCheckListener listener) {
        checkNotNull(listener);
        checkListeners.add(listener);
        return this;
    }

    /**
     * Runs accessibility checks on a {@code View} hierarchy
     *
     * @param view the {@link View} to check
     * @return a list of the results of the checks
     */
    private ImmutableList<AccessibilityViewCheckResult> runAccessibilityChecks(View view) {
        List<AccessibilityViewHierarchyCheck> viewHierarchyChecks =
                new ArrayList<>(AccessibilityCheckPresetAndroid.getViewChecksForPreset(preset));
        Parameters parameters = new Parameters();
        List<AccessibilityViewCheckResult> results = new ArrayList<>();
        for (AccessibilityViewHierarchyCheck check : viewHierarchyChecks) {
            results.addAll(check.runCheckOnViewHierarchy(view, parameters));
        }

        return processResults(view.getContext(), results);
    }

    /**
     * Reports the given check results. Any result matching {@link #suppressingMatcher} is replaced
     * with a copy whose type is set to SUPPRESSED.
     *
     * <ol>
     *   <li>Calls {@link AccessibilityCheckListener#onResults} for any registered listeners.
     *   <li>Throws an {@link AccessibilityViewCheckException} containing all severe results,
     *       depending on the value of {@link #throwExceptionFor}.
     *   <li>Results of type {@code INFO}, {@code WARNING} and {@code ERROR} will be logged to
     *       logcat.
     * </ol>
     *
     * @return The same values as in {@code results}, except that any result that matches {@link
     *     #suppressingMatcher} will be replaced with a copy whose type is SUPPRESSED.
     */
    @VisibleForTesting
    ImmutableList<AccessibilityViewCheckResult> processResults(
            Context context, List<AccessibilityViewCheckResult> results) {
        ImmutableList<AccessibilityViewCheckResult> processedResults =
                suppressMatchingResults(results, suppressingMatcher);
        for (AccessibilityCheckListener checkListener : checkListeners) {
            checkListener.onResults(context, processedResults);
        }

        List<AccessibilityViewCheckResult> infos =
                AccessibilityCheckResultUtils.getResultsForType(
                        processedResults, AccessibilityCheckResultType.INFO);
        List<AccessibilityViewCheckResult> warnings =
                AccessibilityCheckResultUtils.getResultsForType(
                        processedResults, AccessibilityCheckResultType.WARNING);
        List<AccessibilityViewCheckResult> errors =
                AccessibilityCheckResultUtils.getResultsForType(
                        processedResults, AccessibilityCheckResultType.ERROR);

        List<AccessibilityViewCheckResult> severeResults =
                getSevereResults(errors, warnings, infos);

        if (!severeResults.isEmpty()) {
            // This is locally modified to match the execption in 4.0.
            //  if (deprecatedResultDescriptor != null) {
            //    throw new AccessibilityViewCheckException(
            //        severeResults, checkNotNull(deprecatedResultDescriptor));
            //  }
            throw new AccessibilityViewCheckException(
                    severeResults, checkNotNull(resultDescriptor));
        }

        for (AccessibilityViewCheckResult result : infos) {
            Log.i(TAG, describeResult(result));
        }
        for (AccessibilityViewCheckResult result : warnings) {
            Log.w(TAG, describeResult(result));
        }
        for (AccessibilityViewCheckResult result : errors) {
            Log.e(TAG, describeResult(result));
        }
        return processedResults;
    }

    private String describeResult(AccessibilityViewCheckResult result) {
        if (deprecatedResultDescriptor != null) {
            return checkNotNull(deprecatedResultDescriptor).describeResult(result);
        }
        return checkNotNull(resultDescriptor).describeResult(result);
    }

    /**
     * Returns a copy of the list where any result that matches the given matcher is replaced by a
     * copy of the result with the type set to {@code SUPPRESSED}.
     *
     * @param results a list of {@code AccessibilityCheckResult}s to be matched against
     * @param matcher a Matcher that determines whether a given {@code AccessibilityCheckResult}
     *     should be suppressed
     */
    @VisibleForTesting
    static ImmutableList<AccessibilityViewCheckResult> suppressMatchingResults(
            List<AccessibilityViewCheckResult> results,
            @Nullable Matcher<? super AccessibilityViewCheckResult> matcher) {
        if (matcher == null) {
            return ImmutableList.copyOf(results);
        }

        return FluentIterable.from(results)
                .transform(
                        result ->
                                matcher.matches(result) ? result.getSuppressedResultCopy() : result)
                .toList();
    }

    /**
     * Returns the list of those results that should cause an exception to be thrown, depending upon
     * the value of {@link #throwExceptionFor}.
     */
    private List<AccessibilityViewCheckResult> getSevereResults(
            List<AccessibilityViewCheckResult> errors,
            List<AccessibilityViewCheckResult> warnings,
            List<AccessibilityViewCheckResult> infos) {
        if (throwExceptionFor != null) {
            switch (throwExceptionFor) {
                case ERROR:
                    if (!errors.isEmpty()) {
                        return errors;
                    }
                    break;
                case WARNING:
                    if (!(errors.isEmpty() && warnings.isEmpty())) {
                        return new ImmutableList.Builder<AccessibilityViewCheckResult>()
                                .addAll(errors)
                                .addAll(warnings)
                                .build();
                    }
                    break;
                case INFO:
                    if (!(errors.isEmpty() && warnings.isEmpty() && infos.isEmpty())) {
                        return new ImmutableList.Builder<AccessibilityViewCheckResult>()
                                .addAll(errors)
                                .addAll(warnings)
                                .addAll(infos)
                                .build();
                    }
                    break;
                default:
            }
        }
        return ImmutableList.<AccessibilityViewCheckResult>of();
    }

    /** Interface for receiving callbacks when results have been obtained. */
    public static interface AccessibilityCheckListener {
        void onResults(Context context, List<? extends AccessibilityViewCheckResult> results);
    }
}
