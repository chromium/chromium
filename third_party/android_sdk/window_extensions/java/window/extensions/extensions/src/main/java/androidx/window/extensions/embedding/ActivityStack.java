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

import static androidx.window.extensions.embedding.ActivityEmbeddingComponent.OVERLAY_FEATURE_API_LEVEL;

import android.app.Activity;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;

import androidx.window.extensions.RequiresVendorApiLevel;

import org.jspecify.annotations.NonNull;
import org.jspecify.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Description of a group of activities stacked on top of each other and shown as a single
 * container, all within the same task.
 */
public class ActivityStack {

    private final @NonNull List<Activity> mActivities;

    private final boolean mIsEmpty;

    private final @NonNull Token mToken;

    private final @Nullable String mTag;

    /**
     * The {@code ActivityStack} constructor
     *
     * @param activities {@link Activity Activities} in this application's process that belongs to
     *     this {@code ActivityStack}
     * @param isEmpty Indicates whether there's any {@link Activity} running in this {@code
     *     ActivityStack}
     * @param token The token to identify this {@code ActivityStack}
     * @param tag A unique identifier of {@link ActivityStack}. Only specifies for the overlay
     *     standalone {@link ActivityStack} currently.
     */
    ActivityStack(
            @NonNull List<Activity> activities,
            boolean isEmpty,
            @NonNull Token token,
            @Nullable String tag) {
        Objects.requireNonNull(activities);
        Objects.requireNonNull(token);

        mActivities = new ArrayList<>(activities);
        mIsEmpty = isEmpty;
        mToken = token;
        mTag = tag;
    }

    /**
     * Returns {@link Activity Activities} in this application's process that belongs to this
     * ActivityStack.
     *
     * <p>Note that Activities that are running in other processes are not reported in the returned
     * Activity list. They can be in any position in terms of ordering relative to the activities in
     * the list.
     */
    public @NonNull List<Activity> getActivities() {
        return new ArrayList<>(mActivities);
    }

    /**
     * Returns {@code true} if there's no {@link Activity} running in this ActivityStack.
     *
     * <p>Note that {@link #getActivities()} only report Activity in the process used to create this
     * ActivityStack. That said, if this ActivityStack only contains activities from another
     * process, {@link #getActivities()} will return empty list, while this method will return
     * {@code false}.
     */
    public boolean isEmpty() {
        return mIsEmpty;
    }

    /** Returns a token uniquely identifying the container. */
    @RequiresVendorApiLevel(level = 5)
    public @NonNull Token getActivityStackToken() {
        return mToken;
    }

    /** Returns the associated tag if specified. Otherwise, returns {@code null}. */
    @RequiresVendorApiLevel(level = OVERLAY_FEATURE_API_LEVEL)
    public @Nullable String getTag() {
        return mTag;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof ActivityStack)) return false;
        ActivityStack that = (ActivityStack) o;
        return mActivities.equals(that.mActivities)
                && mIsEmpty == that.mIsEmpty
                && mToken.equals(that.mToken)
                && Objects.equals(mTag, that.mTag);
    }

    @Override
    public int hashCode() {
        int result = (mIsEmpty ? 1 : 0);
        result = result * 31 + mActivities.hashCode();
        result = result * 31 + mToken.hashCode();
        result = result * 31 + Objects.hashCode(mTag);

        return result;
    }

    @Override
    public @NonNull String toString() {
        return "ActivityStack{"
                + "mActivities="
                + mActivities
                + ", mIsEmpty="
                + mIsEmpty
                + ", mToken="
                + mToken
                + ", mTag="
                + mTag
                + '}';
    }

    /** A unique identifier to represent an {@link ActivityStack}. */
    public static final class Token {

        /** An invalid token to provide compatibility value before vendor API level 5. */
        public static final @NonNull Token INVALID_ACTIVITY_STACK_TOKEN = new Token(new Binder());

        private static final String KEY_ACTIVITY_STACK_RAW_TOKEN =
                "androidx.window.extensions" + ".embedding.ActivityStack.Token";

        private final IBinder mToken;

        Token(@NonNull IBinder token) {
            mToken = token;
        }

        /**
         * Creates an {@link ActivityStack} token from binder.
         *
         * @param token the raw binder used by OEM Extensions implementation.
         */
        @RequiresVendorApiLevel(level = 5)
        public static @NonNull Token createFromBinder(@NonNull IBinder token) {
            return new Token(token);
        }

        /**
         * Retrieves an {@link ActivityStack} token from {@link Bundle} if it's valid.
         *
         * @param bundle the {@link Bundle} to retrieve the {@link ActivityStack} token from.
         * @throws IllegalArgumentException if the {@code bundle} isn't valid.
         */
        @RequiresVendorApiLevel(level = 5)
        public static @NonNull Token readFromBundle(@NonNull Bundle bundle) {
            final IBinder token = bundle.getBinder(KEY_ACTIVITY_STACK_RAW_TOKEN);

            if (token == null) {
                throw new IllegalArgumentException("Invalid bundle to create ActivityStack Token");
            }
            return new Token(token);
        }

        /**
         * Converts the token to {@link Bundle}.
         *
         * <p>See {@link ActivityEmbeddingOptionsProperties#KEY_ACTIVITY_STACK_TOKEN} for sample
         * usage.
         */
        @RequiresVendorApiLevel(level = 5)
        public @NonNull Bundle toBundle() {
            final Bundle bundle = new Bundle();
            bundle.putBinder(KEY_ACTIVITY_STACK_RAW_TOKEN, mToken);
            return bundle;
        }

        @NonNull IBinder getRawToken() {
            return mToken;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof Token)) return false;
            Token token = (Token) o;
            return Objects.equals(mToken, token.mToken);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mToken);
        }

        @Override
        public @NonNull String toString() {
            return "Token{" + "mToken=" + mToken + '}';
        }
    }
}
