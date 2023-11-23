// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.Callback;
import org.chromium.content.browser.TracingControllerAndroidImpl;

/**
 * Controller for Chrome's tracing feature. The embedder may use this class to implement a UI for
 * recording and sharing Chrome performance traces.
 */
public interface TracingControllerAndroid {
    /**
     * Creates a new TracingControllerAndroid instance.
     *
     * @param context the Context in which to create the controller.
     * @return the controller.
     */
    public static TracingControllerAndroid create(Context context) {
        return new TracingControllerAndroidImpl(context);
    }

    /**
     * @return true if a trace is being recorded.
     */
    boolean isTracing();

    /**
     * @return the path of the current output file. Null if isTracing() false.
     */
    String getOutputPath();

    /**
     * Start recording a trace to the specified file (if not null) or to a new file in the Downloads
     * directory.
     *
     * Only one TracingControllerAndroid can be running at the same time. If another profiler is
     * running when this method is called, it will be cancelled. If this profiler is already
     * running, this method does nothing and returns false.
     *
     * @param filename The name of the file to output the profile data to, or null.
     * @param showToasts Whether or not we want to show toasts during this profiling session.
     * When we are timing the profile run we might not want to incur extra draw overhead of showing
     * notifications about the profiling system, or the embedder may want to show such notifications
     * themselves.
     * @param categories Which categories to trace. See TracingController::StartTracing()
     * (in content/public/browser/tracing_controller.h) for the format.
     * @param traceOptions Which trace options to use. See
     * TraceOptions::TraceOptions(const std::string& options_string)
     * (in base/trace_event/trace_event_impl.h) for the format.
     * @param compressFile Whether the trace file should be compressed (gzip).
     * @param useProtobuf Whether to generate a binary protobuf trace or use
     * the legacy JSON format.
     * @return Whether tracing was started successfully.
     */
    boolean startTracing(
            String filename,
            boolean showToasts,
            String categories,
            String traceOptions,
            boolean compressFile,
            boolean useProtobuf);

    /**
     * Stop recording and run |callback| when stopped.
     *
     * @param callback the Callback executed when tracing has stopped.
     */
    void stopTracing(Callback<Void> callback);

    /**
     * Get known tracing categories and run |callback| with the set of known categories.
     *
     * @param callback the callback that receives the result.
     * @return whether initiating the request was successful.
     */
    boolean getKnownCategories(Callback<String[]> callback);

    /**
     * Get the current estimated trace buffer usage and approximate total event count in the buffer.
     *
     * @param callback the callback that receives the result as a Pair of (percentage_full,
     * approximate_event_count).
     * @return whether initiating the request was successful.
     */
    boolean getTraceBufferUsage(Callback<Pair<Float, Long>> callback);

    /**
     * Clean any native dependencies of the controller. After the call, this class instance
     * shouldn't be used.
     */
    void destroy();
}
