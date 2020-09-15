// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.test.IsolatedContext;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;

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
import org.mockito.Mock;
import org.mockito.stubbing.OngoingStubbing;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.blink.mojom.AndroidFontLookup;
import org.chromium.blink.mojom.AndroidFontLookup.GetUniqueNameLookupTableResponse;
import org.chromium.blink.mojom.AndroidFontLookup.MatchLocalFontByUniqueNameResponse;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.mojo.MojoTestRule;

/**
 * Tests the {@link AndroidFontLookup} implementation.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public final class AndroidFontLookupImplTest {
    private static final String FULL_FONT_NAME = "foo";
    private static final String FONT_QUERY = "name=Foo&weight=400";
    private static final String AUTHORITY = "com.google.android.gms.fonts";
    private static final Uri URI = Uri.parse("content://com.google.android.gms.fonts/123");
    private static final Uri URI2 = Uri.parse("content://com.google.android.gms.fonts/456");
    private static final int FD = 42;
    private static final int FD2 = 43;
    private static final long RUN_LOOP_TIMEOUT_MS = 50;

    @Rule
    public MojoTestRule mMojoTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    @Mock
    private FontsContractWrapper mMockFontsContractWrapper;
    @Mock
    private ParcelFileDescriptor mMockFileDescriptor;
    @Mock
    private ParcelFileDescriptor mMockFileDescriptor2;
    private Context mMockContext;

    @Mock
    private GetUniqueNameLookupTableResponse mGetUniqueNameLookupTableCallback;
    @Mock
    private MatchLocalFontByUniqueNameResponse mMatchLocalFontByUniqueNameCallback;

    private AndroidFontLookupImpl mAndroidFontLookup;

    @Before
    public void setUp() {
        initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        MockContentResolver resolver = new MockContentResolver();
        MockContext mockContext = new MockContext();
        when(mMockFileDescriptor.detachFd()).thenReturn(FD);
        when(mMockFileDescriptor2.detachFd()).thenReturn(FD2);
        resolver.addProvider(AUTHORITY, new MockContentProvider(mockContext) {
            @Override
            public AssetFileDescriptor openTypedAssetFile(Uri url, String mimeType, Bundle opts) {
                if (url.equals(URI)) {
                    return new AssetFileDescriptor(mMockFileDescriptor, 0, -1);
                } else if (url.equals(URI2)) {
                    return new AssetFileDescriptor(mMockFileDescriptor2, 0, -1);
                } else {
                    return null;
                }
            }
        });
        mMockContext = new IsolatedContext(resolver, mockContext);

        mAndroidFontLookup = new AndroidFontLookupImpl(mMockContext);
        mAndroidFontLookup.setFontsContractForTest(mMockFontsContractWrapper);
        mAndroidFontLookup.setFullFontNameToQueryMapForTest(
                ImmutableMap.of(FULL_FONT_NAME, FONT_QUERY));
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_Empty() throws NameNotFoundException {
        String[] expected = new String[0];
        FontFamilyResult result = new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback).call(aryEq(expected));
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_Available() throws NameNotFoundException {
        String[] expected = new String[] {FULL_FONT_NAME};

        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback).call(aryEq(expected));
    }

    @SmallTest
    @Test
    public void testGetUniqueNameLookupTable_MultipleFonts() throws NameNotFoundException {
        String fullFontName2 = "bar";
        String fontQuery2 = "name=Bar&weight=400";
        String fullFontName3 = "bar bold";
        String fontQuery3 = "name=Bar&weight=700";

        String[] expected = new String[] {FULL_FONT_NAME, fullFontName2};

        mAndroidFontLookup.setFullFontNameToQueryMapForTest(ImmutableMap.of(
                FULL_FONT_NAME, FONT_QUERY, fullFontName2, fontQuery2, fullFontName3, fontQuery3));

        // Foo is available.
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        // Bar is available.
        FontInfo fontInfo2 = new FontInfo(URI2, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result2 =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo2});
        whenFetchFontsWith(fontQuery2).thenReturn(result2);

        // Bar Bold is not available.
        FontFamilyResult result3 =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(fontQuery3).thenReturn(result3);

        mAndroidFontLookup.getUniqueNameLookupTable(mGetUniqueNameLookupTableCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mGetUniqueNameLookupTableCallback).call(aryEq(expected));
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_UnsupportedFontName() {
        mAndroidFontLookup.matchLocalFontByUniqueName("bar", mMatchLocalFontByUniqueNameCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mMatchLocalFontByUniqueNameCallback,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadResultStatus() throws NameNotFoundException {
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_UNEXPECTED_DATA_PROVIDED, null);
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, mMatchLocalFontByUniqueNameCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mMatchLocalFontByUniqueNameCallback,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_EmptyResults() throws NameNotFoundException {
        FontFamilyResult result = new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, mMatchLocalFontByUniqueNameCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mMatchLocalFontByUniqueNameCallback,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadFontInfoStatus() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_FONT_NOT_FOUND);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, mMatchLocalFontByUniqueNameCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mMatchLocalFontByUniqueNameCallback,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(isNull());
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_Success() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, mMatchLocalFontByUniqueNameCallback);

        mMojoTestRule.runLoop(RUN_LOOP_TIMEOUT_MS);
        verify(mMatchLocalFontByUniqueNameCallback,
                timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .call(notNull());
    }

    private OngoingStubbing<FontFamilyResult> whenFetchFontsWith(String query)
            throws NameNotFoundException {
        return when(mMockFontsContractWrapper.fetchFonts(eq(mMockContext), isNull(),
                argThat((FontRequest r) -> r.getQuery().equals(query))));
    }
}
