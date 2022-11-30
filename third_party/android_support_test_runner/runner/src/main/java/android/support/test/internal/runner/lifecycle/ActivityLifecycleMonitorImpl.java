/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.support.test.internal.runner.lifecycle;

import static android.support.test.internal.util.Checks.checkNotNull;

import android.app.Activity;
import android.os.Looper;
import android.support.test.runner.lifecycle.ActivityLifecycleCallback;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitor;
import android.support.test.runner.lifecycle.Stage;
import android.util.Log;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * The lifecycle monitor implementation.
 */
public final class ActivityLifecycleMonitorImpl implements ActivityLifecycleMonitor {
    private static final String TAG = "LifecycleMonitor";
    private final boolean mDeclawThreadCheck;

    public ActivityLifecycleMonitorImpl() {
        this(false);
    }

    // For Testing
    public ActivityLifecycleMonitorImpl(boolean declawThreadCheck) {
        this.mDeclawThreadCheck = declawThreadCheck;
    }

    // Accessed from any thread.
    private List<WeakReference<ActivityLifecycleCallback>> mCallbacks =
            new ArrayList<WeakReference<ActivityLifecycleCallback>>();

    // Only accessed on main thread.
    private List<ActivityStatus> mActivityStatuses = new ArrayList<ActivityStatus>();

    @Override
    public void addLifecycleCallback(ActivityLifecycleCallback callback) {
        // there will never be too many callbacks, so iterating over a list will probably
        // be faster then the constant time costs of setting up and maintaining a map.
        checkNotNull(callback);

        synchronized (mCallbacks) {
            boolean needsAdd = true;
            Iterator<WeakReference<ActivityLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ActivityLifecycleCallback storedCallback = refIter.next().get();
                if (null == storedCallback) {
                    refIter.remove();
                } else if (storedCallback == callback) {
                    needsAdd = false;
                }
            }
            if (needsAdd) {
                mCallbacks.add(new WeakReference<ActivityLifecycleCallback>(callback));
            }
        }
    }

    @Override
    public void removeLifecycleCallback(ActivityLifecycleCallback callback) {
        checkNotNull(callback);

        synchronized (mCallbacks) {
            Iterator<WeakReference<ActivityLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ActivityLifecycleCallback storedCallback = refIter.next().get();
                if (null == storedCallback) {
                    refIter.remove();
                } else if (storedCallback == callback) {
                    refIter.remove();
                }
            }
        }
    }

    @Override
    public Stage getLifecycleStageOf(Activity activity) {
        checkMainThread();
        checkNotNull(activity);
        Iterator<ActivityStatus> statusIterator = mActivityStatuses.iterator();
        while (statusIterator.hasNext()) {
            ActivityStatus status = statusIterator.next();
            Activity statusActivity = status.mActivityRef.get();
            if (null == statusActivity) {
                statusIterator.remove();
            } else if (activity == statusActivity) {
                return status.mLifecycleStage;
            }
        }
        throw new IllegalArgumentException("Unknown activity: " + activity);
    }

    @Override
    public Collection<Activity> getActivitiesInStage(Stage stage) {
        checkMainThread();
        checkNotNull(stage);

        List<Activity> activities = new ArrayList<Activity>();
        Iterator<ActivityStatus> statusIterator = mActivityStatuses.iterator();
        while (statusIterator.hasNext()) {
            ActivityStatus status = statusIterator.next();
            Activity statusActivity = status.mActivityRef.get();
            if (null == statusActivity) {
                statusIterator.remove();
            } else if (stage == status.mLifecycleStage) {
                activities.add(statusActivity);
            }
        }

        return activities;
    }

    /**
     * Called by the runner after a particular onXXX lifecycle method has been called on a given
     * activity.
     */
    public void signalLifecycleChange(Stage stage, Activity activity) {
        // there are never too many activities in existence in an application - so we keep
        // track of everything in a single list.
        Log.d(TAG, "Lifecycle status change: " + activity + " in: " + stage);

        boolean needsAdd = true;
        Iterator<ActivityStatus> statusIterator = mActivityStatuses.iterator();
        while (statusIterator.hasNext()) {
            ActivityStatus status = statusIterator.next();
            Activity statusActivity = status.mActivityRef.get();
            if (null == statusActivity) {
                statusIterator.remove();
            } else if (activity == statusActivity) {
                needsAdd = false;
                status.mLifecycleStage = stage;
            }
        }

        if (needsAdd) {
            mActivityStatuses.add(new ActivityStatus(activity, stage));
        }

        synchronized (mCallbacks) {
            Iterator<WeakReference<ActivityLifecycleCallback>> refIter = mCallbacks.iterator();
            while (refIter.hasNext()) {
                ActivityLifecycleCallback callback = refIter.next().get();
                if (null == callback) {
                    refIter.remove();
                } else {
                    try {
                        Log.d(TAG, "running callback: " + callback);
                        callback.onActivityLifecycleChanged(activity, stage);
                        Log.d(TAG, "callback completes: " + callback);
                    } catch (RuntimeException re) {
                        Log.e(TAG, String.format(
                                "Callback threw exception! (callback: %s activity: %s stage: %s)",
                                callback,
                                activity,
                                stage),
                                re);
                    }
                }
            }
        }
    }

    private void checkMainThread() {
        if (mDeclawThreadCheck) {
            return;
        }

        if (!Thread.currentThread().equals(Looper.getMainLooper().getThread())) {
            throw new IllegalStateException(
                    "Querying activity state off main thread is not allowed.");
        }
    }

    private static class ActivityStatus {
        private final WeakReference<Activity> mActivityRef;
        private Stage mLifecycleStage;

        ActivityStatus(Activity activity, Stage stage) {
            this.mActivityRef = new WeakReference<Activity>(checkNotNull(activity));
            this.mLifecycleStage = checkNotNull(stage);
        }
    }
}
