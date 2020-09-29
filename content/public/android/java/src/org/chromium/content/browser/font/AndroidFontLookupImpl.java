// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat;
import androidx.core.provider.FontsContractCompat.FontFamilyResult;
import androidx.core.provider.FontsContractCompat.FontInfo;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.AndroidFontLookup;
import org.chromium.content.R;
import org.chromium.mojo.bindings.ExecutorFactory;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.mojom.File;
import org.chromium.services.service_manager.InterfaceFactory;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * Implementation of the Mojo IPC interface that can be called from the renderer side to fetch fonts
 * from GMS Core.
 */
public class AndroidFontLookupImpl implements AndroidFontLookup {
    private static final String TAG = "AndroidFontLookup";
    private static final String READ_ONLY_MODE = "r";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final String FETCH_FONT_NAME_HISTOGRAM = "Android.FontLookup.FetchFontName";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final String FETCH_FONT_RESULT_HISTOGRAM = "Android.FontLookup.FetchFontResult";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final String MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM =
            "Android.FontLookup.MatchLocalFontByUniqueName.Time";
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static final String GMS_FONT_REQUEST_HISTOGRAM = "Android.FontLookup.GmsFontRequest.Time";

    private static final String GOOGLE_SANS_REGULAR = "google sans regular";
    private static final String GOOGLE_SANS_MEDIUM = "google sans medium";
    private static final String GOOGLE_SANS_BOLD = "google sans bold";

    private final Context mAppContext;
    private final FontsContractWrapper mFontsContract;
    /**
     * Map from ICU case folded full font names to corresponding GMS Core font provider query.
     */
    private final Map<String, String> mFullFontNameToQuery;
    /**
     * Collection of fonts (by ICU case folded full font name) that may be available
     * locally from GMS Core. This collection of Android Downloadable fonts should initially match
     * the fonts listed in preloaded_fonts.xml. If/when fonts are determined to be unavailable
     * on-device they will be removed from this set.
     */
    private final Set<String> mExpectedFonts;

    private AndroidFontLookupImpl(Context appContext) {
        this(appContext, new FontsContractWrapper(), createFullFontNameToQueryMap());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    AndroidFontLookupImpl(Context appContext, FontsContractWrapper fontsContract,
            Map<String, String> fullFontNameToQuery) {
        mAppContext = appContext;
        mFontsContract = fontsContract;
        mFullFontNameToQuery = fullFontNameToQuery;
        mExpectedFonts = new HashSet<>(mFullFontNameToQuery.keySet());
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. These values must stay in sync with enums.xml.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @IntDef({FetchFontName.GOOGLE_SANS_REGULAR, FetchFontName.GOOGLE_SANS_MEDIUM,
            FetchFontName.GOOGLE_SANS_BOLD})
    @interface FetchFontName {
        int OTHER = 0;
        int GOOGLE_SANS_REGULAR = 1;
        int GOOGLE_SANS_MEDIUM = 2;
        int GOOGLE_SANS_BOLD = 3;
        int COUNT = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. These values must stay in sync with enums.xml.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @IntDef({FetchFontResult.SUCCESS, FetchFontResult.FAILED_UNEXPECTED_NAME,
            FetchFontResult.FAILED_STATUS_CODE, FetchFontResult.FAILED_NON_UNIQUE_RESULT,
            FetchFontResult.FAILED_RESULT_CODE, FetchFontResult.FAILED_FILE_OPEN,
            FetchFontResult.FAILED_EXCEPTION, FetchFontResult.FAILED_AVOID_RETRY})
    @interface FetchFontResult {
        int SUCCESS = 0;
        int FAILED_UNEXPECTED_NAME = 1;
        int FAILED_STATUS_CODE = 2;
        int FAILED_NON_UNIQUE_RESULT = 3;
        int FAILED_RESULT_CODE = 4;
        int FAILED_FILE_OPEN = 5;
        int FAILED_EXCEPTION = 6;
        int FAILED_AVOID_RETRY = 7;
        int COUNT = 8;
    }

    /**
     * Synchronously returns the list of fonts (by ICU case folded full font name) that may be
     * available from GMS Core on-device. These fonts should have already been preloaded via the
     * "preloaded_fonts" AndroidManifest directive, and have not previously failed a programmatic
     * font fetch request.
     *
     * TODO(crbug.com/1111148): Ensure the font preload by manifest XML is also done for WebView.
     *
     * @param callback The callback to be called with the list of fonts expected (but not
     *         guaranteed) to be available. The list is sorted in ascending order.
     */
    @Override
    public void getUniqueNameLookupTable(GetUniqueNameLookupTableResponse callback) {
        String[] results = mExpectedFonts.toArray(new String[mExpectedFonts.size()]);
        Arrays.sort(results);
        callback.call(results);
    }

    /**
     * Fetches the requested font from GMS Core on a background thread. If the font could not be
     * fetched successfully, it is removed from {@link #mExpectedFonts} and will not be retried this
     * session.
     *
     * @param fontUniqueName The ICU case folded full font name to fetch.
     * @param callback The callback to be called with the resulting opened font file handle, or null
     *         if the font file is not available. Caller is responsible for closing file when done.
     */
    @Override
    public void matchLocalFontByUniqueName(
            @NonNull String fontUniqueName, MatchLocalFontByUniqueNameResponse callback) {
        long startTimeMs = SystemClock.elapsedRealtime();

        logFetchFontName(fontUniqueName);

        // Get executor associated with the current thread for running Mojo callback.
        Core core = CoreImpl.getInstance();
        Executor executor = ExecutorFactory.getExecutorForCurrentThread(core);

        // Post synchronous font request to background worker thread.
        PostTask.postTask(TaskTraits.USER_BLOCKING, () -> {
            File file = null;

            ParcelFileDescriptor fileDescriptor = tryFetchFont(fontUniqueName);
            if (fileDescriptor == null) {
                // Avoid re-requesting this font in future.
                mExpectedFonts.remove(fontUniqueName);
            } else {
                // Wrap file descriptor as an opened Mojo file handle.
                file = new File();
                file.fd = core.wrapFileDescriptor(fileDescriptor);
                file.async = false;
            }

            final File result = file;
            RecordHistogram.recordTimesHistogram(MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM,
                    SystemClock.elapsedRealtime() - startTimeMs);
            executor.execute(() -> callback.call(result));
        });
    }

    /**
     * Tries to fetch the specified font from GMS Core (the Android Downloadable fonts provider).
     *
     * This method makes a synchronous request to GMS Core and should not be called from the IO
     * thread. This requirement may be re-evaluated based on the timing results of {@link
     * #GMS_FONT_REQUEST_HISTOGRAM}.
     *
     * @param fontUniqueName The ICU case folded unique full font name to fetch.
     * @return An opened font file descriptor, or null if the font file is not available.
     */
    private ParcelFileDescriptor tryFetchFont(String fontUniqueName) {
        String query = mFullFontNameToQuery.get(fontUniqueName);
        if (query == null) {
            Log.d(TAG, "Query format not found for full font name: %s", fontUniqueName);
            logFetchFontResult(FetchFontResult.FAILED_UNEXPECTED_NAME);
            return null;
        }

        if (!mExpectedFonts.contains(fontUniqueName)) {
            Log.d(TAG, "Skipping fetch for font that previously failed: %s", fontUniqueName);
            logFetchFontResult(FetchFontResult.FAILED_AVOID_RETRY);
            return null;
        }

        FontRequest request = new FontRequest("com.google.android.gms.fonts",
                "com.google.android.gms", query, R.array.com_google_android_gms_fonts_certs);

        try {
            long startTimeMs = SystemClock.elapsedRealtime();
            FontFamilyResult fontFamilyResult =
                    mFontsContract.fetchFonts(mAppContext, null, request);
            RecordHistogram.recordTimesHistogram(
                    GMS_FONT_REQUEST_HISTOGRAM, SystemClock.elapsedRealtime() - startTimeMs);

            if (fontFamilyResult.getStatusCode() != FontFamilyResult.STATUS_OK) {
                Log.d(TAG, "Font fetch failed with status code: %d",
                        fontFamilyResult.getStatusCode());
                logFetchFontResult(FetchFontResult.FAILED_STATUS_CODE);
                return null;
            }

            FontInfo[] fontInfos = fontFamilyResult.getFonts();
            if (fontInfos.length != 1) {
                Log.d(TAG, "Font fetch did not return a unique result: length = %d",
                        fontInfos.length);
                logFetchFontResult(FetchFontResult.FAILED_NON_UNIQUE_RESULT);
                return null;
            }

            FontInfo fontInfo = fontInfos[0];
            if (fontInfo.getResultCode() != FontsContractCompat.Columns.RESULT_CODE_OK) {
                Log.d(TAG, "Returned font has failed status code: %d", fontInfo.getResultCode());
                logFetchFontResult(FetchFontResult.FAILED_RESULT_CODE);
                return null;
            }

            ContentResolver contentResolver = mAppContext.getContentResolver();
            ParcelFileDescriptor fileDescriptor =
                    contentResolver.openFileDescriptor(fontInfo.getUri(), READ_ONLY_MODE);
            if (fileDescriptor == null) {
                Log.d(TAG, "Unable to open font file at: %s", fontInfo.getUri());
                logFetchFontResult(FetchFontResult.FAILED_FILE_OPEN);
                return null;
            }

            logFetchFontResult(FetchFontResult.SUCCESS);
            return fileDescriptor;
        } catch (NameNotFoundException | IOException | OutOfMemoryError e) {
            Log.d(TAG, "Failed to get font with: %s", e.toString());
            logFetchFontResult(FetchFontResult.FAILED_EXCEPTION);
            return null;
        }
    }

    /**
     * Creates the map from ICU case folded full font name to GMS Core font provider query format,
     * for a selected subset of Android Downloadable fonts.
     *
     * When adding additional fonts to this map:
     * 1. Add the font to preloaded_fonts.xml, or consider alternatives to prefetch new fonts
     *    programmatically at a different time in initialization as this may affect startup
     *    performance.
     * 2. Keys should be ICU case folded full font name. This can be done manually with
     *    icu_fold_case_util.cc, or in Java by importing the ICU4J third_party library. (The
     *    CaseMap.Fold Java API is only available in Android API 29+.)
     * 3. Update the {@link FetchFontName} enum entries.
     *
     * @return The created map from font names to queries.
     */
    private static Map<String, String> createFullFontNameToQueryMap() {
        Map<String, String> map = new HashMap<>();
        map.put(GOOGLE_SANS_REGULAR, createFontQuery("Google Sans", 400));
        map.put(GOOGLE_SANS_MEDIUM, createFontQuery("Google Sans", 500));
        map.put(GOOGLE_SANS_BOLD, createFontQuery("Google Sans", 700));
        return map;
    }

    /**
     * Construct a GMS Core Downloadable fonts query for a font with exact match parameters.
     * (More info: https://developers.google.com/fonts/docs/android#query_format)
     *
     * @param name Font family name (from fonts.google.com).
     * @param weight Font weight.
     * @return Query for Google Fonts provider.
     */
    private static String createFontQuery(String name, int weight) {
        return String.format(Locale.US, "name=%s&weight=%d&besteffort=false", name, weight);
    }

    private static void logFetchFontResult(@FetchFontResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                FETCH_FONT_RESULT_HISTOGRAM, result, FetchFontResult.COUNT);
    }

    private static void logFetchFontName(String fontName) {
        @FetchFontName
        int result;
        switch (fontName) {
            case GOOGLE_SANS_REGULAR:
                result = FetchFontName.GOOGLE_SANS_REGULAR;
                break;
            case GOOGLE_SANS_MEDIUM:
                result = FetchFontName.GOOGLE_SANS_MEDIUM;
                break;
            case GOOGLE_SANS_BOLD:
                result = FetchFontName.GOOGLE_SANS_BOLD;
                break;
            default:
                result = FetchFontName.OTHER;
        }
        RecordHistogram.recordEnumeratedHistogram(
                FETCH_FONT_NAME_HISTOGRAM, result, FetchFontName.COUNT);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * A factory for implementations of the AndroidFontLookup interface.
     */
    public static class Factory implements InterfaceFactory<AndroidFontLookup> {
        /**
         * It's safe to store this as a global because there's usually only one application context
         * per process, see {@link ContextUtils#getApplicationContext()} for more info.
         */
        @SuppressLint("StaticFieldLeak")
        private static AndroidFontLookupImpl sImpl;

        public Factory() {}

        @Override
        public AndroidFontLookup createImpl() {
            if (sImpl == null) {
                sImpl = new AndroidFontLookupImpl(ContextUtils.getApplicationContext());
            }
            return sImpl;
        }
    }
}
