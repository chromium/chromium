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
import android.os.Binder;
import android.os.IBinder;

import androidx.annotation.NonNull;
import androidx.annotation.RestrictTo;
import androidx.window.extensions.WindowExtensions;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Description of a group of activities stacked on top of each other and shown as a single
 * container, all within the same task.
 */
public class ActivityStack {

    /** Only used for compatibility with the deprecated constructor. */
    private static final IBinder INVALID_ACTIVITY_STACK_TOKEN = new Binder();

    @NonNull
    private final List<Activity> mActivities;

    private final boolean mIsEmpty;

    @NonNull
    private final IBinder mToken;

    /**
     * The {@code ActivityStack} constructor
     *
     * @param activities {@link Activity Activities} in this application's process that
     *                   belongs to this {@code ActivityStack}
     * @param isEmpty Indicates whether there's any {@link Activity} running in this
     *                {@code ActivityStack}
     * @param token The token to identify this {@code ActivityStack}
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    ActivityStack(@NonNull List<Activity> activities, boolean isEmpty, @NonNull IBinder token) {
        Objects.requireNonNull(activities);
        Objects.requireNonNull(token);
        mActivities = new ArrayList<>(activities);
        mIsEmpty = isEmpty;
        mToken = token;
    }

    /**
     * @deprecated Use the {@link WindowExtensions#VENDOR_API_LEVEL_3} version.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_1}
     */
    @Deprecated
    ActivityStack(@NonNull List<Activity> activities, boolean isEmpty) {
        this(activities, isEmpty, INVALID_ACTIVITY_STACK_TOKEN);
    }

    /**
     * Returns {@link Activity Activities} in this application's process that belongs to this
     * ActivityStack.
     * <p>
     * Note that Activities that are running in other processes are not reported in the returned
     * Activity list. They can be in any position in terms of ordering relative to the activities
     * in the list.
     * </p>
     */
    @NonNull
    public List<Activity> getActivities() {
        return new ArrayList<>(mActivities);
    }

    /**
     * Returns {@code true} if there's no {@link Activity} running in this ActivityStack.
     * <p>
     * Note that {@link #getActivities()} only report Activity in the process used to create this
     * ActivityStack. That said, if this ActivityStack only contains activities from another
     * process, {@link #getActivities()} will return empty list, while this method will return
     * {@code false}.
     * </p>
     */
    public boolean isEmpty() {
        return mIsEmpty;
    }

    /**
     * Returns a token uniquely identifying the container.
     * Since {@link WindowExtensions#VENDOR_API_LEVEL_3}
     */
    @RestrictTo(RestrictTo.Scope.LIBRARY_GROUP)
    @NonNull
    public IBinder getToken() {
        return mToken;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof ActivityStack)) return false;
        ActivityStack that = (ActivityStack) o;
        return mActivities.equals(that.mActivities)
                && mIsEmpty == that.mIsEmpty
                && mToken.equals(that.mToken);
    }

    @Override
    public int hashCode() {
        int result = (mIsEmpty ? 1 : 0);
        result = result * 31 + mActivities.hashCode();
        result = result * 31 + mToken.hashCode();
        return result;
    }

    @NonNull
    @Override
    public String toString() {
        return "ActivityStack{" + "mActivities=" + mActivities
                + ", mIsEmpty=" + mIsEmpty
                + ", mToken=" + mToken
                + '}';
    }
}
