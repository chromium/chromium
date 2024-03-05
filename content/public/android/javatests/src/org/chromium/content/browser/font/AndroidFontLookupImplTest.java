// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.test.mock.MockContentProvider;

import androidx.core.provider.FontRequest;
import androidx.core.provider.FontsContractCompat.Columns;
import androidx.core.provider.FontsContractCompat.FontFamilyResult;
import androidx.core.provider.FontsContractCompat.FontInfo;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.stubbing.OngoingStubbing;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.AndroidFontLookup;
import org.chromium.blink.mojom.AndroidFontLookup.FetchAllFontFiles_Response;
import org.chromium.blink.mojom.AndroidFontLookup.GetUniqueNameLookupTable_Response;
import org.chromium.blink.mojom.AndroidFontLookup.MatchLocalFontByUniqueName_Response;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo_base.mojom.ReadOnlyFile;

import java.io.IOException;
import java.util.Map;

/** Tests the {@link AndroidFontLookup} implementation. */
@RunWith(BaseJUnit4ClassRunner.class)
public final class AndroidFontLookupImplTest {
    private static final String FULL_FONT_NAME_1 = "foo";
    private static final String FONT_QUERY_1 = "name=Foo&weight=400";
    private static final String FULL_FONT_NAME_2 = "bar";
    private static final String FONT_QUERY_2 = "name=Bar&weight=400";
    private static final String FULL_FONT_NAME_3 = "bar bold";
    private static final String FONT_QUERY_3 = "name=Bar&weight=700";
    private static final String AUTHORITY = "com.google.android.gms.fonts";
    private static final Uri URI = Uri.parse("content://com.google.android.gms.fonts/123");
    private static final Uri URI2 = Uri.parse("content://com.google.android.gms.fonts/456");
    private static final int FD = 42;
    private static final int FD2 = 43;
    private static final long RUN_LOOP_TIMEOUT_MS = 50;

    @Rule public MojoTestRule mMojoTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    @Mock private FontsContractWrapper mMockFontsContractWrapper;
    @Mock private ParcelFileDescriptor mMockFileDescriptor;
    @Mock private ParcelFileDescriptor mMockFileDescriptor2;
    @Mock private ParcelFileDescriptor mMockCachedFileDescriptor;
    @Mock private ParcelFileDescriptor mMockCachedFileDescriptor2;
    @Mock private ParcelFileDescriptor mMockDuplicateFileDescriptor;
    @Mock private ParcelFileDescriptor mMockDuplicateFileDescriptor2;
    private AdvancedMockContext mMockContext;

    @Mock private GetUniqueNameLookupTable_Response mGetUniqueNameLookupTableCallback;
    @Mock private MatchLocalFontByUniqueName_Response mMatchLocalFontByUniqueNameCallback;
    @Mock private FetchAllFontFiles_Response mFetchAllFontFilesCallback;

    @Captor private ArgumentCaptor<Map<String, ReadOnlyFile>> mFontMapCaptor;

    private AndroidFontLookupImpl mAndroidFontLookup;

    @Before
    public void setUp() throws IOException {
        initMocks(this);
        mMockContext = new AdvancedMockContext();

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        when(mMockFileDescriptor.dup()).thenReturn(mMockCachedFileDescriptor);
        when(mMockFileDescriptor2.dup()).thenReturn(mMockCachedFileDescriptor2);
        when(mMockCachedFileDescriptor.dup()).thenReturn(mMockDuplicateFileDescriptor);
        when(mMockCachedFileDescriptor2.dup()).thenReturn(mMockDuplicateFileDescriptor2);
        when(mMockDuplicateFileDescriptor.detachFd()).thenReturn(FD);
        when(mMockDuplicateFileDescriptor2.detachFd()).thenReturn(FD2);
        mMockContext
                .getMockContentResolver()
                .addProvider(
                        AUTHORITY,
                        new MockContentProvider(mMockContext) {
                            @Override
                            public AssetFileDescriptor openTypedAssetFile(
                                    Uri url, String mimeType, Bundle opts) {
                                if (url.equals(URI)) {
                                    return new AssetFileDescriptor(mMockFileDescriptor, 0, -1);
                                } else if (url.equals(URI2)) {
                                    return new AssetFileDescriptor(mMockFileDescriptor2, 0, -1);
                                } else {
                                    return null;
                                }
                            }
                        });

        Map<String, String> fullFontNameToQuery =
                ImmutableMap.of(
                        FULL_FONT_NAME_1,
                        FONT_QUERY_1,
                        FULL_FONT_NAME_2,
                        FONT_QUERY_2,
                        FULL_FONT_NAME_3,
                        FONT_QUERY_3);

        mAndroidFontLookup =
                new AndroidFontLookupImpl(
                        mMockContext, mMockFontsContractWrapper, fullFontNameToQuery);
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_Available() {
        String[] expected = new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_3, FULL_FONT_NAME_1};

        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback).call(aryEq(expected));
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_Available() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_2).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_3).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.FETCH_ALL_FONT_FILES_HISTOGRAM);
        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mFetchAllFontFilesCallback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(mFontMapCaptor.capture());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertEquals(3, response.size());
        assertNotNull(response.get(FULL_FONT_NAME_1));
        assertNotNull(response.get(FULL_FONT_NAME_2));
        assertNotNull(response.get(FULL_FONT_NAME_3));
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_OneNotAvailable() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_2).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_3)
                .thenReturn(
                        new FontFamilyResult(
                                FontFamilyResult.STATUS_UNEXPECTED_DATA_PROVIDED, null));

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.FETCH_ALL_FONT_FILES_HISTOGRAM);
        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mFetchAllFontFilesCallback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(mFontMapCaptor.capture());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertEquals(2, response.size());
        assertNotNull(response.get(FULL_FONT_NAME_1));
        assertNotNull(response.get(FULL_FONT_NAME_2));

        // Verify the font was removed from the lookup table.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_1}));
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_Empty() throws NameNotFoundException {
        mAndroidFontLookup =
                new AndroidFontLookupImpl(
                        mMockContext, mMockFontsContractWrapper, ImmutableMap.of());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.FETCH_ALL_FONT_FILES_HISTOGRAM);
        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mFetchAllFontFilesCallback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(mFontMapCaptor.capture());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertTrue(response.isEmpty());
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_MultipleFonts() throws NameNotFoundException {
        // All 3 fonts should be found in results.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_3, FULL_FONT_NAME_1}));

        // Bar Bold is not available.
        FontFamilyResult result3 =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY_3).thenReturn(result3);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_3, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());

        // Bar Bold should now be excluded from list.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_1}));
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_UnsupportedFontName() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName("baz", mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadResultStatus() throws NameNotFoundException {
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_UNEXPECTED_DATA_PROVIDED, null);
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_EmptyResults() throws NameNotFoundException {
        FontFamilyResult result = new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadFontInfoStatus() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_FONT_NOT_FOUND);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_Throws() throws NameNotFoundException {
        whenFetchFontsWith(FONT_QUERY_1).thenThrow(new NameNotFoundException());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_NoRetry() throws NameNotFoundException {
        // Request font and fail.
        whenFetchFontsWith(FONT_QUERY_1).thenThrow(new NameNotFoundException());

        {
            HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
            mAndroidFontLookup.matchLocalFontByUniqueName(
                    FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
            watcher.pollInstrumentationThreadUntilSatisfied();
        }
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());

        // Second request should early out with FAILED_AVOID_RETRY.
        {
            HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
            mAndroidFontLookup.matchLocalFontByUniqueName(
                    FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
            watcher.pollInstrumentationThreadUntilSatisfied();
        }
        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(2))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_Success() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        watcher.pollInstrumentationThreadUntilSatisfied();

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(
                        mMatchLocalFontByUniqueNameCallback,
                        timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(notNull());
    }

    private OngoingStubbing<FontFamilyResult> whenFetchFontsWith(String query)
            throws NameNotFoundException {
        return when(
                mMockFontsContractWrapper.fetchFonts(
                        eq(mMockContext),
                        isNull(),
                        argThat((FontRequest r) -> r.getQuery().equals(query))));
    }
}
