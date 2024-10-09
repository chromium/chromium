// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.content.R;
import org.chromium.content_public.browser.TracingControllerAndroid;
import org.chromium.ui.widget.Toast;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/**
 * Controller for Chrome's tracing feature.
 *
 * We don't have any UI per se. Just call startTracing() to start and
 * stopTracing() to stop. We'll report progress to the user with Toasts.
 *
 * If the host application registers this class's BroadcastReceiver, you can
 * also start and stop the tracer with a broadcast intent, as follows:
 * <ul>
 * <li>To start tracing: am broadcast -a org.chromium.content_shell_apk.GPU_PROFILER_START
 * <li>Add "-e file /foo/bar/xyzzy" to log trace data to a specific file.
 * <li>To stop tracing: am broadcast -a org.chromium.content_shell_apk.GPU_PROFILER_STOP
 * </ul>
 * Note that the name of these intents change depending on which application
 * is being traced, but the general form is [app package name].GPU_PROFILER_{START,STOP}.
 */
@JNINamespace("content")
public class TracingControllerAndroidImpl implements TracingControllerAndroid {
    private static final String TAG = "TracingController";

    private static final String ACTION_START = "GPU_PROFILER_START";
    private static final String ACTION_STOP = "GPU_PROFILER_STOP";
    private static final String ACTION_LIST_CATEGORIES = "GPU_PROFILER_LIST_CATEGORIES";
    private static final String FILE_EXTRA = "file";
    private static final String CATEGORIES_EXTRA = "categories";
    private static final String RECORD_CONTINUOUSLY_EXTRA = "continuous";
    private static final String DEFAULT_CHROME_CATEGORIES_PLACE_HOLDER =
            "_DEFAULT_CHROME_CATEGORIES";

    // These strings must match the ones expected by adb_profile_chrome.
    private static final String PROFILER_STARTED_FMT = "Profiler started: %s";
    private static final String PROFILER_FINISHED_FMT = "Profiler finished. Results are in %s.";

    private final Context mContext;
    private final TracingBroadcastReceiver mBroadcastReceiver;
    private final TracingIntentFilter mIntentFilter;
    private boolean mIsTracing;

    // We might not want to always show toasts when we start the profiler, especially if
    // showing the toast impacts performance.  This gives us the chance to disable them.
    private boolean mShowToasts = true;

    private String mFilename;
    private boolean mCompressFile;
    private boolean mUseProtobuf;

    public TracingControllerAndroidImpl(Context context) {
        mContext = context;
        mBroadcastReceiver = new TracingBroadcastReceiver();
        mIntentFilter = new TracingIntentFilter(context);
    }

    /** Get a BroadcastReceiver that can handle profiler intents. */
    public BroadcastReceiver getBroadcastReceiver() {
        return mBroadcastReceiver;
    }

    /** Get an IntentFilter for profiler intents. */
    public IntentFilter getIntentFilter() {
        return mIntentFilter;
    }

    /** Register a BroadcastReceiver in the given context. */
    public void registerReceiver(Context context) {
        ContextUtils.registerExportedBroadcastReceiver(
                context, getBroadcastReceiver(), getIntentFilter(), null);
    }

    /** Unregister the GPU BroadcastReceiver in the given context. */
    public void unregisterReceiver(Context context) {
        context.unregisterReceiver(getBroadcastReceiver());
    }

    @Override
    public boolean isTracing() {
        return mIsTracing;
    }

    @Override
    public String getOutputPath() {
        return mFilename;
    }

    /**
     * Generates a filepath to be used for tracing in the Downloads directory.
     * @param basename The basename to be used, if empty a unique one will be generated.
     */
    @CalledByNative
    private static String generateTracingFilePath(String basename) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            String state = Environment.getExternalStorageState();
            if (!Environment.MEDIA_MOUNTED.equals(state)) {
                return null;
            }

            if (basename.isEmpty()) {
                // Generate a hopefully-unique filename using the UTC timestamp.
                // (Not a huge problem if it isn't unique, we'll just append more data.)
                SimpleDateFormat formatter = new SimpleDateFormat("yyyy-MM-dd-HHmmss", Locale.US);
                formatter.setTimeZone(TimeZone.getTimeZone("UTC"));
                basename = "chrome-profile-results-" + formatter.format(new Date());
            }

            Context context = ContextUtils.getApplicationContext();
            File dir = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
            File file = new File(dir, basename);
            return file.getPath();
        }
    }

    /**
     * Start profiling to a new file in the Downloads directory.
     *
     * Calls #startTracing(String, boolean, String, String, boolean) with a new timestamped
     * filename. Doesn't compress the file or generate protobuf trace data.
     *
     * @see #startTracing(String, boolean, String, String, boolean, boolean)
     */
    public boolean startTracing(boolean showToasts, String categories, String traceOptions) {
        return startTracing(
                null,
                showToasts,
                categories,
                traceOptions,
                /* compressFile= */ false,
                /* useProtobuf= */ false);
    }

    private void initializeNativeControllerIfNeeded() {
        if (mNativeTracingControllerAndroid == 0) {
            mNativeTracingControllerAndroid =
                    TracingControllerAndroidImplJni.get().init(TracingControllerAndroidImpl.this);
        }
    }

    @Override
    public boolean startTracing(
            String filename,
            boolean showToasts,
            String categories,
            String traceOptions,
            boolean compressFile,
            boolean useProtobuf) {
        mShowToasts = showToasts;

        if (filename == null) {
            filename = generateTracingFilePath("");
            if (filename == null) {
                logAndToastError(mContext.getString(R.string.profiler_no_storage_toast));
                return false;
            }
        }

        if (isTracing()) {
            // Don't need a toast because this shouldn't happen via the UI.
            Log.e(TAG, "Received startTracing, but we're already tracing");
            return false;
        }

        // Lazy initialize the native side, to allow construction before the library is loaded.
        initializeNativeControllerIfNeeded();
        if (!TracingControllerAndroidImplJni.get()
                .startTracing(
                        mNativeTracingControllerAndroid,
                        TracingControllerAndroidImpl.this,
                        categories,
                        traceOptions,
                        useProtobuf)) {
            logAndToastError(mContext.getString(R.string.profiler_error_toast));
            return false;
        }

        logForProfiler(String.format(PROFILER_STARTED_FMT, categories));
        showToast(mContext.getString(R.string.profiler_started_toast) + ": " + categories);
        mFilename = filename;
        mCompressFile = compressFile;
        mUseProtobuf = useProtobuf;
        mIsTracing = true;
        return true;
    }

    @Override
    public void stopTracing(Callback<Void> callback) {
        if (isTracing()) {
            TracingControllerAndroidImplJni.get()
                    .stopTracing(
                            mNativeTracingControllerAndroid,
                            TracingControllerAndroidImpl.this,
                            mFilename,
                            mCompressFile,
                            mUseProtobuf,
                            callback);
        }
    }

    /** Called by native code when the profiler's output file is closed. */
    @CalledByNative
    @SuppressWarnings("unchecked")
    protected void onTracingStopped(Object callback) {
        if (!isTracing()) {
            // Don't need a toast because this shouldn't happen via the UI.
            Log.e(TAG, "Received onTracingStopped, but we aren't tracing");
            return;
        }

        logForProfiler(String.format(PROFILER_FINISHED_FMT, mFilename));
        showToast(mContext.getString(R.string.profiler_stopped_toast, mFilename));
        mIsTracing = false;
        mFilename = null;
        mCompressFile = false;

        if (callback != null) ((Callback<Void>) callback).onResult(null);
    }

    /** Get known categories and log them for the profiler. */
    public void getKnownCategories() {
        if (!getKnownCategories(null)) {
            Log.e(TAG, "Unable to fetch tracing category list.");
        }
    }

    @Override
    public boolean getKnownCategories(Callback<String[]> callback) {
        // Lazy initialize the native side, to allow construction before the library is loaded.
        initializeNativeControllerIfNeeded();
        return TracingControllerAndroidImplJni.get()
                .getKnownCategoriesAsync(
                        mNativeTracingControllerAndroid,
                        TracingControllerAndroidImpl.this,
                        callback);
    }

    /**
     * Called by native when the categories requested by getKnownCategories were obtained.
     *
     * @param categories The set of category names.
     * @param callback The callback that was provided to
     *         TracingControllerAndroidImplJni.get().getKnownCategoriesAsync.
     */
    @CalledByNative
    @SuppressWarnings("unchecked")
    public void onKnownCategoriesReceived(String[] categories, Object callback) {
        if (callback != null) {
            ((Callback<String[]>) callback).onResult(categories);
        }
    }

    @Override
    public boolean getTraceBufferUsage(Callback<Pair<Float, Long>> callback) {
        assert callback != null;
        // Lazy initialize the native side, to allow construction before the library is loaded.
        initializeNativeControllerIfNeeded();
        return TracingControllerAndroidImplJni.get()
                .getTraceBufferUsageAsync(
                        mNativeTracingControllerAndroid,
                        TracingControllerAndroidImpl.this,
                        callback);
    }

    @CalledByNative
    @SuppressWarnings("unchecked")
    public void onTraceBufferUsageReceived(
            float percentFull, long approximateEventCount, Object callback) {
        ((Callback<Pair<Float, Long>>) callback)
                .onResult(new Pair<>(percentFull, approximateEventCount));
    }

    @Override
    public void destroy() {
        if (mNativeTracingControllerAndroid != 0) {
            TracingControllerAndroidImplJni.get()
                    .destroy(mNativeTracingControllerAndroid, TracingControllerAndroidImpl.this);
            mNativeTracingControllerAndroid = 0;
        }
    }

    private void logAndToastError(String str) {
        Log.e(TAG, str);
        if (mShowToasts) Toast.makeText(mContext, str, Toast.LENGTH_SHORT).show();
    }

    // The |str| string needs to match the ones that adb_chrome_profiler looks for.
    // TODO(crbug.com/40092856): Replace (users of) this with DevTools' Tracing API.
    private void logForProfiler(String str) {
        Log.i(TAG, str);
    }

    private void showToast(String str) {
        if (mShowToasts) Toast.makeText(mContext, str, Toast.LENGTH_SHORT).show();
    }

    private static class TracingIntentFilter extends IntentFilter {
        TracingIntentFilter(Context context) {
            addAction(context.getPackageName() + "." + ACTION_START);
            addAction(context.getPackageName() + "." + ACTION_STOP);
            addAction(context.getPackageName() + "." + ACTION_LIST_CATEGORIES);
        }
    }

    // TODO(crbug.com/40092856): Replace (users of) this with DevTools' Tracing API.
    class TracingBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().endsWith(ACTION_START)) {
                String categories = intent.getStringExtra(CATEGORIES_EXTRA);
                if (TextUtils.isEmpty(categories)) {
                    categories =
                            TracingControllerAndroidImplJni.get()
                                    .getDefaultCategories(TracingControllerAndroidImpl.this);
                } else {
                    categories =
                            categories.replaceFirst(
                                    DEFAULT_CHROME_CATEGORIES_PLACE_HOLDER,
                                    TracingControllerAndroidImplJni.get()
                                            .getDefaultCategories(
                                                    TracingControllerAndroidImpl.this));
                }
                String traceOptions =
                        intent.getStringExtra(RECORD_CONTINUOUSLY_EXTRA) == null
                                ? "record-until-full"
                                : "record-continuously";
                String filename = intent.getStringExtra(FILE_EXTRA);
                if (filename != null) {
                    startTracing(
                            filename,
                            true,
                            categories,
                            traceOptions,
                            /* compressFile= */ false,
                            /* useProtobuf= */ false);
                } else {
                    startTracing(true, categories, traceOptions);
                }
            } else if (intent.getAction().endsWith(ACTION_STOP)) {
                stopTracing(null);
            } else if (intent.getAction().endsWith(ACTION_LIST_CATEGORIES)) {
                getKnownCategories();
            } else {
                Log.e(TAG, "Unexpected intent: %s", intent);
            }
        }
    }

    private long mNativeTracingControllerAndroid;

    @NativeMethods
    interface Natives {
        long init(TracingControllerAndroidImpl caller);

        void destroy(long nativeTracingControllerAndroid, TracingControllerAndroidImpl caller);

        boolean startTracing(
                long nativeTracingControllerAndroid,
                TracingControllerAndroidImpl caller,
                String categories,
                String traceOptions,
                boolean useProtobuf);

        void stopTracing(
                long nativeTracingControllerAndroid,
                TracingControllerAndroidImpl caller,
                String filename,
                boolean compressFile,
                boolean useProtobuf,
                Callback<Void> callback);

        boolean getKnownCategoriesAsync(
                long nativeTracingControllerAndroid,
                TracingControllerAndroidImpl caller,
                Callback<String[]> callback);

        String getDefaultCategories(TracingControllerAndroidImpl caller);

        boolean getTraceBufferUsageAsync(
                long nativeTracingControllerAndroid,
                TracingControllerAndroidImpl caller,
                Callback<Pair<Float, Long>> callback);
    }
}
