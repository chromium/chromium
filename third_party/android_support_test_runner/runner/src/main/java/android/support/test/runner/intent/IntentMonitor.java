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

import android.app.Instrumentation;

/**
 * Interface for tests to use when they need to monitor intents used to start activities from
 * the current {@link Instrumentation}.
 * <p>
 * Retrieve instances of the monitor through {@link IntentMonitorRegistry).
 * <p>
 * Monitoring intents requires support from Instrumentation, therefore do not expect
 * an instance to be present under any arbitrary instrumentation.
 */
public interface IntentMonitor {

    /**
     * Adds an {@link IntentCallback}, which will be notified when intents are sent from the
     * current instrumentation process.
     */
    void addIntentCallback(IntentCallback callback);


    /**
     * Removes a previously registered {@link IntentCallback}.
     */
    void removeIntentCallback(IntentCallback callback);
}
