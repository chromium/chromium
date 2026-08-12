// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.OngoingStubbing;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.DoNotBatch;
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
@DoNotBatch(
        reason =
                "MojoTestRule resets native environment per test method while ThreadLocal"
                        + " PipedExecutor is bound to the thread")
public final class AndroidFontLookupImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public MojoTestRule mMojoTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    private static final long MOJO_RUN_LOOP_TIMEOUT_MS = 10000;
    private static final String FULL_FONT_NAME_1 = "foo";
    private static final String FONT_QUERY_1 = "name=Foo&weight=400";
    private static final String FULL_FONT_NAME_2 = "bar";
    private static final String FONT_QUERY_2 = "name=Bar&weight=400";
    private static final String FULL_FONT_NAME_3 = "bar bold";
    private static final String FONT_QUERY_3 = "name=Bar&weight=700";
    private static final String AUTHORITY = "com.google.android.gms.fonts";
    private static final Uri URI = Uri.parse("content://com.google.android.gms.fonts/123");
    private static final Uri URI2 = Uri.parse("content://com.google.android.gms.fonts/456");

    @Mock private FontsContractWrapper mMockFontsContractWrapper;
    private AdvancedMockContext mMockContext;

    @Mock private GetUniqueNameLookupTable_Response mGetUniqueNameLookupTableCallback;
    @Mock private MatchLocalFontByUniqueName_Response mMatchLocalFontByUniqueNameCallback;
    @Mock private FetchAllFontFiles_Response mFetchAllFontFilesCallback;

    @Captor private ArgumentCaptor<Map<String, ReadOnlyFile>> mFontMapCaptor;
    @Captor private ArgumentCaptor<ReadOnlyFile> mReadOnlyFileCaptor;

    private AndroidFontLookupImpl mAndroidFontLookup;

    @Before
    public void setUp() throws IOException {
        mMockContext = new AdvancedMockContext();

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mMockContext
                .getMockContentResolver()
                .addProvider(
                        AUTHORITY,
                        new MockContentProvider(mMockContext) {
                            @Override
                            public ParcelFileDescriptor openFile(Uri url, String mode) {
                                try {
                                    if (url.equals(URI) || url.equals(URI2)) {
                                        return createTestParcelFileDescriptor();
                                    }
                                    return null;
                                } catch (IOException e) {
                                    return null;
                                }
                            }

                            @Override
                            public AssetFileDescriptor openAssetFile(Uri url, String mode) {
                                try {
                                    if (url.equals(URI) || url.equals(URI2)) {
                                        return new AssetFileDescriptor(
                                                createTestParcelFileDescriptor(),
                                                /* startOffset= */ 0,
                                                /* length= */ -1);
                                    }
                                    return null;
                                } catch (IOException e) {
                                    return null;
                                }
                            }

                            @Override
                            public AssetFileDescriptor openTypedAssetFile(
                                    Uri url, String mimeType, Bundle opts) {
                                try {
                                    if (url.equals(URI) || url.equals(URI2)) {
                                        return new AssetFileDescriptor(
                                                createTestParcelFileDescriptor(),
                                                /* startOffset= */ 0,
                                                /* length= */ -1);
                                    }
                                    return null;
                                } catch (IOException e) {
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

        verify(mGetUniqueNameLookupTableCallback).call(aryEq(expected));
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_Available() throws NameNotFoundException, IOException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_2).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_3).thenReturn(result);

        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mFetchAllFontFilesCallback)
                .call(mFontMapCaptor.capture());

        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mFetchAllFontFilesCallback).call(any());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertEquals(3, response.size());
        assertNotNull(response.get(FULL_FONT_NAME_1));
        assertNotNull(response.get(FULL_FONT_NAME_2));
        assertNotNull(response.get(FULL_FONT_NAME_3));

        for (ReadOnlyFile file : response.values()) {
            file.fd.close();
        }
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_OneNotAvailable()
            throws NameNotFoundException, IOException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_2).thenReturn(result);
        whenFetchFontsWith(FONT_QUERY_3)
                .thenReturn(
                        new FontFamilyResult(
                                FontFamilyResult.STATUS_UNEXPECTED_DATA_PROVIDED, null));

        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mFetchAllFontFilesCallback)
                .call(mFontMapCaptor.capture());

        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mFetchAllFontFilesCallback).call(any());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertEquals(2, response.size());
        assertNotNull(response.get(FULL_FONT_NAME_1));
        assertNotNull(response.get(FULL_FONT_NAME_2));

        for (ReadOnlyFile file : response.values()) {
            file.fd.close();
        }

        // Verify the font was removed from the lookup table.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_1}));
    }

    @SmallTest
    @Test
    public void testFetchAllFontFiles_Empty() {
        mAndroidFontLookup =
                new AndroidFontLookupImpl(
                        mMockContext, mMockFontsContractWrapper, ImmutableMap.of());

        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mFetchAllFontFilesCallback)
                .call(mFontMapCaptor.capture());

        mAndroidFontLookup.fetchAllFontFiles(mFetchAllFontFilesCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mFetchAllFontFilesCallback).call(any());

        Map<String, ReadOnlyFile> response = mFontMapCaptor.getValue();
        assertTrue(response.isEmpty());
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_MultipleFonts() throws NameNotFoundException {
        // All 3 fonts should be found in results.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_3, FULL_FONT_NAME_1}));

        // Bar Bold is not available.
        FontFamilyResult result3 =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY_3).thenReturn(result3);

        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_3, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());

        // Bar Bold should now be excluded from list.
        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);
        verify(mGetUniqueNameLookupTableCallback)
                .call(aryEq(new String[] {FULL_FONT_NAME_2, FULL_FONT_NAME_1}));
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_UnsupportedFontName() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName("baz", mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher.assertExpected();
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
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_EmptyResults() throws NameNotFoundException {
        FontFamilyResult result = new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY_1).thenReturn(result);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher.assertExpected();
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
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_Throws() throws NameNotFoundException {
        whenFetchFontsWith(FONT_QUERY_1).thenThrow(new NameNotFoundException());

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher.assertExpected();
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_NoRetry() throws NameNotFoundException {
        // Request font and fail.
        whenFetchFontsWith(FONT_QUERY_1).thenThrow(new NameNotFoundException());

        HistogramWatcher watcher1 =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(isNull());
        watcher1.assertExpected();

        // Second request should early out with FAILED_AVOID_RETRY.
        HistogramWatcher watcher2 =
                HistogramWatcher.newSingleRecordWatcher(
                        AndroidFontLookupImpl.MATCH_LOCAL_FONT_BY_UNIQUE_NAME_HISTOGRAM);
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(isNull());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback, times(2)).call(isNull());
        watcher2.assertExpected();
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
        doAnswer(
                        invocation -> {
                            mMojoTestRule.quitLoop();
                            return null;
                        })
                .when(mMatchLocalFontByUniqueNameCallback)
                .call(mReadOnlyFileCaptor.capture());

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME_1, mMatchLocalFontByUniqueNameCallback);
        mMojoTestRule.runLoop(MOJO_RUN_LOOP_TIMEOUT_MS);

        verify(mMatchLocalFontByUniqueNameCallback).call(notNull());
        watcher.assertExpected();

        ReadOnlyFile response = mReadOnlyFileCaptor.getValue();
        assertNotNull(response);
        assertNotNull(response.fd);
        response.fd.close();
    }

    private ParcelFileDescriptor createTestParcelFileDescriptor() throws IOException {
        ParcelFileDescriptor[] pipe = ParcelFileDescriptor.createPipe();
        pipe[1].close();
        return pipe[0];
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
