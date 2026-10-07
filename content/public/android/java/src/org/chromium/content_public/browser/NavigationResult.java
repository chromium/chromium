// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.navigation_controller.NavigationNotStartedReason;

/**
 * Represents the outcome of attempting to start a navigation via {@link
 * NavigationController#loadUrl(LoadUrlParams)}.
 *
 * <p>A successful navigation will have a non-null {@link #getNavigationHandle()} and a null {@link
 * #getNotStartedReason()}.
 *
 * <p>When a navigation is not started (e.g. invalid URL, context shutdown), {@link
 * #getNotStartedReason()} will return the reason and {@link #getNavigationHandle()} will be null.
 * In the specific case of {@link NavigationNotStartedReason#DUPLICATE_NAVIGATION_IGNORED}, {@link
 * #getOngoingNavigationHandle()} provides the in-flight {@link NavigationHandle} that was
 * duplicate- suppressed.
 */
@NullMarked
public final class NavigationResult {
    private final @Nullable NavigationHandle mHandle;
    private final @Nullable @NavigationNotStartedReason Integer mNotStartedReason;
    private final @Nullable NavigationHandle mOngoingHandle;

    private NavigationResult(
            @Nullable NavigationHandle handle,
            @Nullable @NavigationNotStartedReason Integer notStartedReason,
            @Nullable NavigationHandle ongoingHandle) {
        mHandle = handle;
        mNotStartedReason = notStartedReason;
        mOngoingHandle = ongoingHandle;
    }

    /**
     * Creates a successful navigation result wrapping the newly initiated {@link NavigationHandle}.
     */
    @CalledByNative
    public static NavigationResult createSuccess(NavigationHandle handle) {
        return new NavigationResult(handle, null, null);
    }

    /** Creates a failed navigation result containing the {@link NavigationNotStartedReason}. */
    @CalledByNative
    public static NavigationResult createFailure(@NavigationNotStartedReason int notStartedReason) {
        return new NavigationResult(null, notStartedReason, null);
    }

    /**
     * Creates a navigation result for an ignored duplicate navigation, containing both the {@link
     * NavigationNotStartedReason#DUPLICATE_NAVIGATION_IGNORED} reason and the existing in-flight
     * {@link NavigationHandle}.
     */
    @CalledByNative
    public static NavigationResult createDuplicateIgnored(NavigationHandle ongoingHandle) {
        return new NavigationResult(
                null, NavigationNotStartedReason.DUPLICATE_NAVIGATION_IGNORED, ongoingHandle);
    }

    /**
     * Returns the {@link NavigationHandle} representing a newly started navigation on success, or
     * null if a new navigation was not started.
     */
    public @Nullable NavigationHandle getNavigationHandle() {
        return mHandle;
    }

    /**
     * Returns the {@link NavigationNotStartedReason} explaining why a new navigation did not start,
     * or null if a new navigation started successfully.
     */
    public @Nullable @NavigationNotStartedReason Integer getNotStartedReason() {
        return mNotStartedReason;
    }

    /**
     * Returns the in-flight {@link NavigationHandle} if the navigation was ignored because an
     * identical navigation is already ongoing, or null otherwise.
     */
    public @Nullable NavigationHandle getOngoingNavigationHandle() {
        return mOngoingHandle;
    }
}
