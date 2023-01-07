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

import android.app.Activity;

/**
 * Callback for monitoring activity lifecycle events. These callbacks are invoked on the main
 * thread, so any long operations or violating the strict mode policies should be avoided.
 */
public interface ActivityLifecycleCallback {

    /**
     * Called on the main thread after an activity has processed its lifecycle change event
     * (for example onResume or onStart)
     *
     * @param activity The activity
     * @param stage its current stage.
     */
    public void onActivityLifecycleChanged(Activity activity, Stage stage);
}

