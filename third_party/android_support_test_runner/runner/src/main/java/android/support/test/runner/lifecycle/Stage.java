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

package android.support.test.runner.lifecycle;

/**
 * An enumeration of the lifecycle stages an activity undergoes.
 * <p>
 * See the {@link android.app.Activity} javadoc for detailed documentation.
 * </p>
 */
public enum Stage {
    /** Indicates that onCreate is being called before any onCreate code executes.*/
    PRE_ON_CREATE,
    /** Indicates that onCreate has been called. */
    CREATED,
    /** Indicates that onStart has been called. */
    STARTED,
    /** Indicates that onResume has been called - activity is now visible to user. */
    RESUMED,
    /** Indicates that onPause has been called - activity is no longer in the foreground. */
    PAUSED,
    /** Indicates that onStop has been called - activity is no longer visible to the user. */
    STOPPED,
    /** Indicates that onResume has been called - we have navigated back to the activity. */
    RESTARTED,
    /** Indicates that onDestroy has been called - system is shutting down the activity. */
    DESTROYED
}
