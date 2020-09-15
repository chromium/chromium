// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;
import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat;
import androidx.core.provider.FontsContractCompat.FontFamilyResult;
import androidx.core.provider.FontsContractCompat.FontInfo;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
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
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
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

    private final Context mAppContext;
    private FontsContractWrapper mFontsContract = new FontsContractWrapper();
    /**
     * Map from ICU case folded full font names to corresponding GMS Core font provider query.
     *
     * This collection of Android Downloadable fonts should match the fonts listed in
     * preloaded_fonts.xml.
     */
    private Map<String, String> mFullFontNameToQuery = createFullFontNameToQueryMap();

    AndroidFontLookupImpl(Context appContext) {
        mAppContext = appContext;
    }

    /**
     * Verifies which fonts are available from GMS Core and can be fetched quickly, and
     * asynchronously responds with that list. These fonts should have already been preloaded via
     * the "preloaded_fonts" AndroidManifest directive. This second programmatic prefetch request is
     * necessary to confirm whether those fonts were successfully downloaded and are now available.
     *
     * TODO(chouinard): Consider requiring the returned list to be sorted.
     *
     * @param callback The callback to be called with the list of available fonts.
     */
    @Override
    public void getUniqueNameLookupTable(GetUniqueNameLookupTableResponse callback) {
        // Get executor associated with the current thread for running Mojo callback.
        Executor executor = ExecutorFactory.getExecutorForCurrentThread(CoreImpl.getInstance());

        PostTask.postTask(TaskTraits.BEST_EFFORT, () -> {
            Set<String> expectedFonts = mFullFontNameToQuery.keySet();
            List<String> availableFonts = new ArrayList<>();

            for (String fontName : expectedFonts) {
                if (tryFetchFont(fontName) != null) {
                    availableFonts.add(fontName);
                }
            }

            String[] results = availableFonts.toArray(new String[availableFonts.size()]);
            executor.execute(() -> callback.call(results));
        });
    }

    /**
     * Fetches the requested font from GMS Core on a background thread.
     *
     * @param fontUniqueName The unique full font name requested.
     * @param callback The callback to be called with the resulting opened font file handle, or null
     *         if the font file is not available. Caller is responsible for closing file when done.
     */
    @Override
    public void matchLocalFontByUniqueName(
            String fontUniqueName, MatchLocalFontByUniqueNameResponse callback) {
        // Get executor associated with the current thread for running Mojo callback.
        Core core = CoreImpl.getInstance();
        Executor executor = ExecutorFactory.getExecutorForCurrentThread(core);

        // Post synchronous font request to background worker thread.
        PostTask.postTask(TaskTraits.USER_BLOCKING, () -> {
            ParcelFileDescriptor fileDescriptor = tryFetchFont(fontUniqueName);
            if (fileDescriptor == null) {
                executor.execute(() -> callback.call(null));
                return;
            }

            // Wrap file descriptor as an opened Mojo file handle.
            File file = new File();
            file.fd = core.wrapFileDescriptor(fileDescriptor);
            file.async = false;

            executor.execute(() -> callback.call(file));
        });
    }

    private ParcelFileDescriptor tryFetchFont(String fontUniqueName) {
        String query = mFullFontNameToQuery.get(fontUniqueName);
        if (query == null) {
            Log.d(TAG, "Query format not found for full font name: %s", fontUniqueName);
            return null;
        }

        FontRequest request = new FontRequest("com.google.android.gms.fonts",
                "com.google.android.gms", query, R.array.com_google_android_gms_fonts_certs);

        try {
            FontFamilyResult fontFamilyResult =
                    mFontsContract.fetchFonts(mAppContext, null, request);

            if (fontFamilyResult.getStatusCode() != FontFamilyResult.STATUS_OK) {
                Log.d(TAG, "Font fetch failed with status code: %d",
                        fontFamilyResult.getStatusCode());
                return null;
            }

            FontInfo[] fontInfos = fontFamilyResult.getFonts();
            if (fontInfos.length != 1) {
                Log.d(TAG, "Font fetch did not return a unique result: length = %d",
                        fontInfos.length);
                return null;
            }

            FontInfo fontInfo = fontInfos[0];
            if (fontInfo.getResultCode() != FontsContractCompat.Columns.RESULT_CODE_OK) {
                Log.d(TAG, "Returned font has failed status code: %d", fontInfo.getResultCode());
                return null;
            }

            ContentResolver contentResolver = mAppContext.getContentResolver();
            ParcelFileDescriptor fileDescriptor =
                    contentResolver.openFileDescriptor(fontInfo.getUri(), READ_ONLY_MODE);
            if (fileDescriptor == null) {
                Log.d(TAG, "Unable to open font file at: %s", fontInfo.getUri());
            }
            return fileDescriptor;
        } catch (NameNotFoundException | IOException | OutOfMemoryError e) {
            Log.d(TAG, "Failed to get font with: %s", e.toString());
            return null;
        }
    }

    /**
     * Creates the map from ICU case folded full font name to GMS Core font provider query format,
     * for a selected subset of Android Downloadable fonts.
     *
     * Note: Because the CaseMap.Fold Java API is only available in Android API 29+, these keys have
     * been manually converted from full font name (i.e. "Google Sans") to ICU case folded full font
     * name using `third_party/blink/common/font_unique_name_lookup/icu_fold_case_util.cc`. When
     * further map entries are added in future, consider importing ICU4J as a third_party library to
     * do this case folding explicitly in Java code instead, or using the native utility via JNI.
     *
     * @return The created map from font names to queries.
     */
    private static Map<String, String> createFullFontNameToQueryMap() {
        Map<String, String> map = new HashMap<>();
        map.put("google sans", "name=Google Sans&weight=400");
        map.put("google sans medium", "name=Google Sans&weight=500");
        return map;
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    @VisibleForTesting
    void setFontsContractForTest(FontsContractWrapper fontsContract) {
        mFontsContract = fontsContract;
    }

    @VisibleForTesting
    void setFullFontNameToQueryMapForTest(Map<String, String> fullFontNameToQuery) {
        mFullFontNameToQuery = fullFontNameToQuery;
    }

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
