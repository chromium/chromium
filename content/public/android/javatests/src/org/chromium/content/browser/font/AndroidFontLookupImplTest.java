// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.font;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
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
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo_base.mojom.File;

/**
 * Tests the {@link AndroidFontLookup} implementation.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public final class AndroidFontLookupImplTest {
    private static final String FULL_FONT_NAME = "Foo";
    private static final String FONT_QUERY = "name=Foo&weight=400";
    private static final String AUTHORITY = "com.google.android.gms.fonts";
    private static final Uri URI = Uri.parse("content://com.google.android.gms.fonts/123");
    private static final int FD = 42;

    @Rule
    public MojoTestRule mMojoTestRule = new MojoTestRule(MojoTestRule.MojoCore.INITIALIZE);

    @Mock
    private FontsContractWrapper mMockFontsContractWrapper;
    @Mock
    private ParcelFileDescriptor mMockFileDescriptor;
    private Context mMockContext;

    private AndroidFontLookupImpl mAndroidFontLookup;

    @Before
    public void setUp() {
        initMocks(this);

        MockContentResolver resolver = new MockContentResolver();
        MockContext mockContext = new MockContext();
        when(mMockFileDescriptor.detachFd()).thenReturn(FD);
        resolver.addProvider(AUTHORITY, new MockContentProvider(mockContext) {
            @Override
            public AssetFileDescriptor openTypedAssetFile(Uri url, String mimeType, Bundle opts) {
                assertEquals(URI, url);
                return new AssetFileDescriptor(mMockFileDescriptor, 0, -1);
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
    public void testMatchLocalFontByUniqueName_UnsupportedFontName() {
        mAndroidFontLookup.matchLocalFontByUniqueName(
                "Bar", (File handle) -> { assertNull(handle); });
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadResultStatus() throws NameNotFoundException {
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_UNEXPECTED_DATA_PROVIDED, null);
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, (File handle) -> { assertNull(handle); });
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_EmptyResults() throws NameNotFoundException {
        FontFamilyResult result = new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[0]);
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, (File handle) -> { assertNull(handle); });
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_BadFontInfoStatus() throws NameNotFoundException {
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_FONT_NOT_FOUND);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(
                FULL_FONT_NAME, (File handle) -> { assertNull(handle); });
    }

    @SmallTest
    @Test
    public void testMatchLocalFontByUniqueName_Success() throws NameNotFoundException {
        int expectedHandle = 1;
        FontInfo fontInfo = new FontInfo(URI, 0, 400, false, Columns.RESULT_CODE_OK);
        FontFamilyResult result =
                new FontFamilyResult(FontFamilyResult.STATUS_OK, new FontInfo[] {fontInfo});
        whenFetchFontsWith(FONT_QUERY).thenReturn(result);

        mAndroidFontLookup.matchLocalFontByUniqueName(FULL_FONT_NAME, (File handle) -> {
            assertNotNull(handle);
            assertEquals(expectedHandle, handle.fd.releaseNativeHandle());
        });
    }

    private OngoingStubbing<FontFamilyResult> whenFetchFontsWith(String query)
            throws NameNotFoundException {
        return when(mMockFontsContractWrapper.fetchFonts(eq(mMockContext), isNull(),
                argThat((FontRequest r) -> r.getQuery().equals(query))));
    }
}
