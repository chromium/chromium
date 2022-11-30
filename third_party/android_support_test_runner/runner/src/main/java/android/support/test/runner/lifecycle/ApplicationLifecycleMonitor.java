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
 * Interface for tests to use when they need to be informed of the application lifecycle state.
 * <p>
 * Retrieve instances of the monitor thru ApplicationLifecycleMonitorRegistry.
 * </p>
 * <p>
 * Detecting these lifecycle states requires support from Instrumentation, therefore do not expect
 * an instance to be present under any arbitrary instrumentation.
 * </p>
 */
public interface ApplicationLifecycleMonitor {

    /**
     * Adds a new callback that will be notified when lifecycle changes occur.
     * <p>
     * Implementors will not hold a strong ref to the callback, the code which registers callbacks
     * is responsible for this. Code which registers callbacks should responsibly
     * remove their callback when it is no longer needed.
     * </p>
     * <p>
     * Callbacks may be executed on the main thread of the application, and should take care not to
     * block or otherwise perform expensive operations as it will directly impact the application.
     * </p>
     *
     * @param callback an ApplicationLifecycleCallback
     */
    void addLifecycleCallback(ApplicationLifecycleCallback callback);

    /**
     * Removes a previously registered lifecycle callback.
     */
    void removeLifecycleCallback(ApplicationLifecycleCallback callback);

}
