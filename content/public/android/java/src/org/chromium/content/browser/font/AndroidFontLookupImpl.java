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

import org.chromium.base.Consumer;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.AndroidFontLookup;
import org.chromium.content.R;
import org.chromium.mojo.bindings.ExecutorFactory;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.mojo_base.mojom.File;
import org.chromium.services.service_manager.InterfaceFactory;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
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
     * Map from ICU case folded full font name to GMS Core font provider query format.
     */
    private Map<String, String> mFullFontNameToQuery = createFullFontNameToQueryMap();

    AndroidFontLookupImpl(Context appContext) {
        mAppContext = appContext;
    }

    /**
     * Synchronously gets a list of unique font names that are available from GMS Core and can be
     * fetched quickly.
     *
     * @param callback The callback to be called with the list of font names.
     */
    @Override
    public void getUniqueNameLookupTable(GetUniqueNameLookupTableResponse callback) {
        // TODO(crbug.com/1111148): Verify which fonts were successfully preloaded and are available
        // on-device to populate this list.
        String[] results = new String[0];
        callback.call(results);
    }

    /**
     * Fetches the requested font from GMS Core on a background thread.
     *
     * @param fontUniqueName The unique full font name requested.
     * @param callback The callback to be called with the resulting font file handle, or null if the
     *         font file is not available.
     */
    @Override
    public void matchLocalFontByUniqueName(
            String fontUniqueName, MatchLocalFontByUniqueNameResponse callback) {
        String query = mFullFontNameToQuery.get(fontUniqueName);
        if (query == null) {
            Log.d(TAG, "Query format not found for full font name: %s.", fontUniqueName);
            callback.call(null);
            return;
        }

        FontRequest request = new FontRequest("com.google.android.gms.fonts",
                "com.google.android.gms", query, R.array.com_google_android_gms_fonts_certs);

        // Get executor associated with the current thread for running Mojo callback.
        Executor executor = ExecutorFactory.getExecutorForCurrentThread(CoreImpl.getInstance());
        Consumer<File> consumer = (File file) -> {
            executor.execute(() -> callback.call(file));
        };

        // Execute synchronous font request on background worker thread.
        PostTask.postTask(TaskTraits.USER_BLOCKING, () -> { tryFetchFont(request, consumer); });
    }

    private void tryFetchFont(FontRequest request, Consumer<File> consumer) {
        try {
            FontFamilyResult fontFamilyResult =
                    mFontsContract.fetchFonts(mAppContext, null, request);

            if (fontFamilyResult.getStatusCode() != FontFamilyResult.STATUS_OK) {
                Log.d(TAG, "Font fetch failed with status code %d.",
                        fontFamilyResult.getStatusCode());
                consumer.accept(null);
                return;
            }

            FontInfo[] fontInfos = fontFamilyResult.getFonts();
            if (fontInfos.length != 1) {
                Log.d(TAG, "Font fetch did not return a unique result: length = %d",
                        fontInfos.length);
                consumer.accept(null);
                return;
            }

            FontInfo fontInfo = fontInfos[0];
            if (fontInfo.getResultCode() != FontsContractCompat.Columns.RESULT_CODE_OK) {
                Log.d(TAG, "Returned font has failed status code: %d.", fontInfo.getResultCode());
                consumer.accept(null);
                return;
            }

            ContentResolver contentResolver = mAppContext.getContentResolver();
            ParcelFileDescriptor fileDescriptor =
                    contentResolver.openFileDescriptor(fontInfo.getUri(), READ_ONLY_MODE);
            File file = new File();
            file.fd = CoreImpl.getInstance().wrapFileDescriptor(fileDescriptor);
            file.async = false;
            consumer.accept(file);
        } catch (NameNotFoundException | IOException | OutOfMemoryError e) {
            Log.d(TAG, "Failed to get font with: %s", e.toString());
            consumer.accept(null);
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
