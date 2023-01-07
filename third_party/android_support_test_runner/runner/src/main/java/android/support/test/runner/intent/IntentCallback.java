/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.support.test.runner.intent;

import android.content.Intent;

/**
 * Callback for monitoring Intents captured by {@link android.app.Instrumentation}.
 */
public interface IntentCallback {

    /**
     * Called on main thread when an activity is started from the current instrumentation process
     * by the given intent.
     */
    public void onIntentSent(Intent intent);
}
