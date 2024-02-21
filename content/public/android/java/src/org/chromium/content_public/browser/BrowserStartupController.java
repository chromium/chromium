// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content.browser.BrowserStartupControllerImpl;

/**
 * This class controls how C++ browser main loop is started and ensures it happens only once.
 *
 * It supports kicking off the startup sequence in an asynchronous way. Startup can be called as
 * many times as needed (for instance, multiple activities for the same application), but the
 * browser process will still only be initialized once. All requests to start the browser will
 * always get their callback executed; if the browser process has already been started, the callback
 * is called immediately, else it is called when initialization is complete.
 *
 * All communication with this class must happen on the main thread.
 */
public interface BrowserStartupController {
    /** This provides the interface to the callbacks for successful or failed startup */
    interface StartupCallback {
        void onSuccess();

        void onFailure();
    }

    /**
     * Get BrowserStartupController instance, create a new one if no existing.
     *
     * @return BrowserStartupController instance.
     */
    static BrowserStartupController getInstance() {
        return BrowserStartupControllerImpl.getInstance();
    }

    /**
     * Start the browser process asynchronously. This will set up a queue of UI thread tasks to
     * initialize the browser process.
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param libraryProcessType the type of process the shared library is loaded. It must be
     *                           LibraryProcessType.PROCESS_BROWSER or
     *                           LibraryProcessType.PROCESS_WEBVIEW.
     * @param startGpuProcess Whether to start the GPU process if it is not started. Only has
     *                        effect if browser isn't already started.
     * @param startMinimalBrowser Whether browser startup will be paused after a minimal environment
     *                                is started.
     * @param callback the callback to be called when browser startup is complete.
     */
    void startBrowserProcessesAsync(
            @LibraryProcessType int libraryProcessType,
            boolean startGpuProcess,
            boolean startMinimalBrowser,
            final StartupCallback callback);

    /**
     * Start the browser process synchronously. If the browser is already being started
     * asynchronously then complete startup synchronously
     *
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param libraryProcessType the type of process the shared library is loaded. It must be
     *                           LibraryProcessType.PROCESS_BROWSER or
     *                           LibraryProcessType.PROCESS_WEBVIEW.
     * @param singleProcess true iff the browser should run single-process, ie. keep renderers in
     *                      the browser process
     * @param startGpuProcess Whether to start the GPU process if it is not started. Only has
     *                        effect if browser isn't already started.
     */
    void startBrowserProcessesSync(
            @LibraryProcessType int libraryProcessType,
            boolean singleProcess,
            boolean startGpuProcess);

    /**
     * @return Whether the browser process has been started in "Full Browser" mode successfully. See
     *         {@link #isRunningInMinimalBrowserMode} for information about the other mode of native
     *         startup.
     */
    boolean isFullBrowserStarted();

    /**
     * @return Whether the browser is currently running in minimal browser mode. This is a state
     *         of native where only the minimum required initialization is done (eg: no Profile and
     *         minimal content/ initialization).
     */
    boolean isRunningInMinimalBrowserMode();

    /**
     * @return Whether native is loaded successfully and running in any mode. See {@link
     *     #isRunningInMinimalBrowserMode} and {@link #isFullBrowserStarted} for more information
     *     about the two modes.
     */
    boolean isNativeStarted();

    /**
     * Add startup callback.
     * @param callback Callback to add.
     */
    void addStartupCompletedObserver(StartupCallback callback);

    /**
     * Set a callback that will be run in place of calling ContentMain(). For tests to
     * define their own way of initializing the C++ system.
     */
    void setContentMainCallbackForTests(Runnable r);

    /**
     * @return how Chrome is launched, either in minimal mode or as full browser, as
     * well as either cold start or warm start.
     * See {@link org.chromium.content.browser.ServicificationStartupUma} for more details.
     */
    int getStartupMode(boolean startMinimalBrowser);
}
