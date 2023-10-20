// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.device.mojom.NdefError;
import org.chromium.device.mojom.NdefErrorType;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefRecordTypeCategory;
import org.chromium.device.mojom.NdefWriteOptions;
import org.chromium.device.mojom.Nfc.MakeReadOnly_Response;
import org.chromium.device.mojom.Nfc.Push_Response;
import org.chromium.device.mojom.Nfc.Watch_Response;
import org.chromium.device.mojom.NfcClient;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for NfcImpl and NdefMessageUtils classes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class NFCTest {
    private TestNfcDelegate mDelegate;
    private int mNextWatchId;
    @Mock private Context mContext;
    @Mock private NfcManager mNfcManager;
    @Mock private NfcAdapter mNfcAdapter;
    @Mock private Activity mActivity;
    @Mock private NfcClient mNfcClient;
    @Mock private NfcTagHandler mNfcTagHandler;
    @Captor private ArgumentCaptor<NdefError> mErrorCaptor;
    @Captor private ArgumentCaptor<int[]> mOnWatchCallbackCaptor;

    // Constants used for the test.
    private static final String DUMMY_EXTERNAL_TYPE = "abc.com:xyz";
    private static final String DUMMY_RECORD_ID = "https://www.example.com/ids/1";
    private static final String TEST_TEXT = "test";
    private static final String TEST_URL = "https://google.com";
    private static final String TEST_JSON = "{\"key1\":\"value1\",\"key2\":2}";
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String OCTET_STREAM_MIME = "application/octet-stream";
    private static final String ENCODING_UTF8 = "utf-8";
    private static final String ENCODING_UTF16 = "utf-16";
    private static final String LANG_EN_US = "en-US";

    /** Class that is used test NfcImpl implementation */
    private static class TestNfcImpl extends NfcImpl {
        public TestNfcImpl(Context context, NfcDelegate delegate) {
            super(0, delegate, null);
        }

        public void processPendingOperationsForTesting(NfcTagHandler handler) {
            super.processPendingOperations(handler);
        }
    }

    private static class TestNfcDelegate implements NfcDelegate {
        Activity mActivity;
        Callback<Activity> mCallback;

        public TestNfcDelegate(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void trackActivityForHost(int hostId, Callback<Activity> callback) {
            mCallback = callback;
        }

        public void invokeCallback() {
            mCallback.onResult(mActivity);
        }

        @Override
        public void stopTrackingActivityForHost(int hostId) {}
    }

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });
        MockitoAnnotations.initMocks(this);
        mDelegate = new TestNfcDelegate(mActivity);
        doReturn(mNfcManager).when(mContext).getSystemService(Context.NFC_SERVICE);
        doReturn(mNfcAdapter).when(mNfcManager).getDefaultAdapter();
        doReturn(true).when(mNfcAdapter).isEnabled();
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        doNothing()
                .when(mNfcAdapter)
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());
        doNothing().when(mNfcAdapter).disableReaderMode(any(Activity.class));
        // Tag handler overrides used to mock connected tag.
        doReturn(false).when(mNfcTagHandler).isTagOutOfRange();
        try {
            doNothing().when(mNfcTagHandler).connect();
            doNothing().when(mNfcTagHandler).write(any(android.nfc.NdefMessage.class));
            doReturn(true).when(mNfcTagHandler).makeReadOnly();
            doReturn(createNdefMessageWithRecordId(DUMMY_RECORD_ID)).when(mNfcTagHandler).read();
        } catch (IOException | FormatException e) {
        }
        NfcBlocklist.overrideNfcBlocklistForTests(/* serverProvidedValues= */ null);
        ContextUtils.initApplicationContextForTests(mContext);
    }

    /** Test that error with type NOT_SUPPORTED is returned if NFC is not supported. */
    @Test
    @Feature({"NFCTest"})
    public void testNFCNotSupported() {
        doReturn(null).when(mNfcManager).getDefaultAdapter();
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Watch_Response mockCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /** Test that error with type SECURITY is returned if permission to use NFC is not granted. */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsNotPermitted() {
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        Watch_Response mockCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NdefErrorType.NOT_ALLOWED, mErrorCaptor.getValue().errorType);
    }

    /** Test that method can be invoked successfully if NFC is supported and adapter is enabled. */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsSupported() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Watch_Response mockCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /** Test conversion from NdefMessage to mojo NdefMessage. */
    @Test
    @Feature({"NFCTest"})
    public void testNdefToMojoConversion() throws UnsupportedEncodingException {
        // Test EMPTY record conversion.
        android.nfc.NdefMessage emptyNdefMessage =
                new android.nfc.NdefMessage(
                        new android.nfc.NdefRecord(
                                android.nfc.NdefRecord.TNF_EMPTY, null, null, null));
        NdefMessage emptyMojoNdefMessage = NdefMessageUtils.toNdefMessage(emptyNdefMessage);
        assertEquals(1, emptyMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, emptyMojoNdefMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_EMPTY, emptyMojoNdefMessage.data[0].recordType);
        assertEquals(null, emptyMojoNdefMessage.data[0].mediaType);
        assertEquals(null, emptyMojoNdefMessage.data[0].id);
        assertNull(emptyMojoNdefMessage.data[0].encoding);
        assertNull(emptyMojoNdefMessage.data[0].lang);
        assertEquals(0, emptyMojoNdefMessage.data[0].data.length);

        // Test url record conversion.
        android.nfc.NdefMessage urlNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformUrlRecord(
                                ApiCompatibilityUtils.getBytesUtf8(TEST_URL),
                                DUMMY_RECORD_ID,
                                /* isAbsUrl= */ false));
        NdefMessage urlMojoNdefMessage = NdefMessageUtils.toNdefMessage(urlNdefMessage);
        assertEquals(1, urlMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, urlMojoNdefMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_URL, urlMojoNdefMessage.data[0].recordType);
        assertEquals(null, urlMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, urlMojoNdefMessage.data[0].id);
        assertNull(urlMojoNdefMessage.data[0].encoding);
        assertNull(urlMojoNdefMessage.data[0].lang);
        assertEquals(TEST_URL, new String(urlMojoNdefMessage.data[0].data));

        // Test absolute-url record conversion.
        android.nfc.NdefMessage absUrlNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformUrlRecord(
                                ApiCompatibilityUtils.getBytesUtf8(TEST_URL),
                                DUMMY_RECORD_ID,
                                /* isAbsUrl= */ true));
        NdefMessage absUrlMojoNdefMessage = NdefMessageUtils.toNdefMessage(absUrlNdefMessage);
        assertEquals(1, absUrlMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, absUrlMojoNdefMessage.data[0].category);
        assertEquals(
                NdefMessageUtils.RECORD_TYPE_ABSOLUTE_URL,
                absUrlMojoNdefMessage.data[0].recordType);
        assertEquals(null, absUrlMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, absUrlMojoNdefMessage.data[0].id);
        assertEquals(TEST_URL, new String(absUrlMojoNdefMessage.data[0].data));

        // Test text record conversion for UTF-8 content.
        android.nfc.NdefMessage utf8TextNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformTextRecord(
                                DUMMY_RECORD_ID,
                                LANG_EN_US,
                                ENCODING_UTF8,
                                ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage utf8TextMojoNdefMessage = NdefMessageUtils.toNdefMessage(utf8TextNdefMessage);
        assertEquals(1, utf8TextMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, utf8TextMojoNdefMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, utf8TextMojoNdefMessage.data[0].recordType);
        assertEquals(null, utf8TextMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, utf8TextMojoNdefMessage.data[0].id);
        assertEquals(ENCODING_UTF8, utf8TextMojoNdefMessage.data[0].encoding);
        assertEquals(LANG_EN_US, utf8TextMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(utf8TextMojoNdefMessage.data[0].data, "UTF-8"));

        // Test text record conversion for UTF-16 content.
        byte[] textBytes = TEST_TEXT.getBytes(StandardCharsets.UTF_16BE);
        byte[] languageCodeBytes = LANG_EN_US.getBytes(StandardCharsets.US_ASCII);
        android.nfc.NdefMessage utf16TextNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformTextRecord(
                                DUMMY_RECORD_ID, LANG_EN_US, ENCODING_UTF16, textBytes));
        NdefMessage utf16TextMojoNdefMessage = NdefMessageUtils.toNdefMessage(utf16TextNdefMessage);
        assertEquals(1, utf16TextMojoNdefMessage.data.length);
        assertEquals(
                NdefRecordTypeCategory.STANDARDIZED, utf16TextMojoNdefMessage.data[0].category);
        assertEquals(
                NdefMessageUtils.RECORD_TYPE_TEXT, utf16TextMojoNdefMessage.data[0].recordType);
        assertEquals(null, utf16TextMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, utf16TextMojoNdefMessage.data[0].id);
        assertEquals(ENCODING_UTF16, utf16TextMojoNdefMessage.data[0].encoding);
        assertEquals(LANG_EN_US, utf16TextMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(utf16TextMojoNdefMessage.data[0].data, "UTF-16"));

        // Test mime record conversion with "text/plain" mime type.
        android.nfc.NdefMessage mimeNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformMimeRecord(
                                TEXT_MIME,
                                DUMMY_RECORD_ID,
                                ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage mimeMojoNdefMessage = NdefMessageUtils.toNdefMessage(mimeNdefMessage);
        assertEquals(1, mimeMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, mimeMojoNdefMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_MIME, mimeMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, mimeMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, mimeMojoNdefMessage.data[0].id);
        assertNull(mimeMojoNdefMessage.data[0].encoding);
        assertNull(mimeMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(mimeMojoNdefMessage.data[0].data));

        // Test mime record conversion with "application/json" mime type.
        android.nfc.NdefMessage jsonNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformMimeRecord(
                                JSON_MIME,
                                DUMMY_RECORD_ID,
                                ApiCompatibilityUtils.getBytesUtf8(TEST_JSON)));
        NdefMessage jsonMojoNdefMessage = NdefMessageUtils.toNdefMessage(jsonNdefMessage);
        assertEquals(1, jsonMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, jsonMojoNdefMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_MIME, jsonMojoNdefMessage.data[0].recordType);
        assertEquals(JSON_MIME, jsonMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, jsonMojoNdefMessage.data[0].id);
        assertNull(jsonMojoNdefMessage.data[0].encoding);
        assertNull(jsonMojoNdefMessage.data[0].lang);
        assertEquals(TEST_JSON, new String(jsonMojoNdefMessage.data[0].data));

        // Test unknown record conversion.
        android.nfc.NdefMessage unknownNdefMessage =
                new android.nfc.NdefMessage(
                        new android.nfc.NdefRecord(
                                android.nfc.NdefRecord.TNF_UNKNOWN,
                                null,
                                ApiCompatibilityUtils.getBytesUtf8(DUMMY_RECORD_ID),
                                ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage unknownMojoNdefMessage = NdefMessageUtils.toNdefMessage(unknownNdefMessage);
        assertEquals(1, unknownMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, unknownMojoNdefMessage.data[0].category);
        assertEquals(
                NdefMessageUtils.RECORD_TYPE_UNKNOWN, unknownMojoNdefMessage.data[0].recordType);
        assertEquals(DUMMY_RECORD_ID, unknownMojoNdefMessage.data[0].id);
        assertNull(unknownMojoNdefMessage.data[0].encoding);
        assertNull(unknownMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(unknownMojoNdefMessage.data[0].data));

        // Test external record conversion.
        android.nfc.NdefMessage extNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformExternalRecord(
                                DUMMY_EXTERNAL_TYPE,
                                DUMMY_RECORD_ID,
                                ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT),
                                /* payloadMessage= */ null));
        NdefMessage extMojoNdefMessage = NdefMessageUtils.toNdefMessage(extNdefMessage);
        assertEquals(1, extMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.EXTERNAL, extMojoNdefMessage.data[0].category);
        assertEquals(DUMMY_EXTERNAL_TYPE, extMojoNdefMessage.data[0].recordType);
        assertEquals(null, extMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, extMojoNdefMessage.data[0].id);
        assertNull(extMojoNdefMessage.data[0].encoding);
        assertNull(extMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(extMojoNdefMessage.data[0].data));

        // Test conversion for external records with the payload being a ndef message.
        android.nfc.NdefMessage payloadMessage =
                new android.nfc.NdefMessage(
                        android.nfc.NdefRecord.createTextRecord(LANG_EN_US, TEST_TEXT));
        byte[] payloadBytes = payloadMessage.toByteArray();
        // Put |payloadBytes| as payload of an external record.
        android.nfc.NdefMessage extNdefMessage1 =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformExternalRecord(
                                DUMMY_EXTERNAL_TYPE,
                                DUMMY_RECORD_ID,
                                payloadBytes,
                                /* payloadMessage= */ null));
        NdefMessage extMojoNdefMessage1 = NdefMessageUtils.toNdefMessage(extNdefMessage1);
        assertEquals(1, extMojoNdefMessage1.data.length);
        assertEquals(NdefRecordTypeCategory.EXTERNAL, extMojoNdefMessage1.data[0].category);
        assertEquals(DUMMY_EXTERNAL_TYPE, extMojoNdefMessage1.data[0].recordType);
        assertEquals(null, extMojoNdefMessage1.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, extMojoNdefMessage1.data[0].id);
        // The embedded ndef message should have content corresponding with the original
        // |payloadMessage|.
        NdefMessage payloadMojoMessage = extMojoNdefMessage1.data[0].payloadMessage;
        assertEquals(1, payloadMojoMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, payloadMojoMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, payloadMojoMessage.data[0].recordType);
        assertEquals(null, payloadMojoMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(payloadMojoMessage.data[0].data));

        // Test conversion for smart-poster records.
        android.nfc.NdefMessage spNdefMessage =
                new android.nfc.NdefMessage(createSmartPosterNdefRecord());
        NdefMessage spMojoNdefMessage = NdefMessageUtils.toNdefMessage(spNdefMessage);
        assertEquals(1, spMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, spMojoNdefMessage.data[0].category);
        assertEquals(
                NdefMessageUtils.RECORD_TYPE_SMART_POSTER, spMojoNdefMessage.data[0].recordType);
        assertEquals(null, spMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, spMojoNdefMessage.data[0].id);
        // The embedded ndef message should contain records that match the ones created by
        // createSmartPosterNdefRecord() previously.
        payloadMojoMessage = spMojoNdefMessage.data[0].payloadMessage;
        assertEquals(7, payloadMojoMessage.data.length);
        // The url record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, payloadMojoMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_URL, payloadMojoMessage.data[0].recordType);
        assertEquals(null, payloadMojoMessage.data[0].mediaType);
        assertEquals(TEST_URL, new String(payloadMojoMessage.data[0].data));
        // The size record.
        assertEquals(NdefRecordTypeCategory.LOCAL, payloadMojoMessage.data[1].category);
        assertEquals(":s", payloadMojoMessage.data[1].recordType);
        assertEquals(null, payloadMojoMessage.data[1].mediaType);
        assertEquals(4, payloadMojoMessage.data[1].data.length);
        assertEquals(4096, ByteBuffer.allocate(4).put(payloadMojoMessage.data[1].data).getInt(0));
        // The type record.
        assertEquals(NdefRecordTypeCategory.LOCAL, payloadMojoMessage.data[2].category);
        assertEquals(":t", payloadMojoMessage.data[2].recordType);
        assertEquals(null, payloadMojoMessage.data[2].mediaType);
        assertEquals(OCTET_STREAM_MIME, new String(payloadMojoMessage.data[2].data));
        // The action record.
        assertEquals(NdefRecordTypeCategory.LOCAL, payloadMojoMessage.data[3].category);
        assertEquals(":act", payloadMojoMessage.data[3].recordType);
        assertEquals(null, payloadMojoMessage.data[3].mediaType);
        assertEquals(1, payloadMojoMessage.data[3].data.length);
        assertEquals(0x01, payloadMojoMessage.data[3].data[0]);
        // The title record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, payloadMojoMessage.data[4].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, payloadMojoMessage.data[4].recordType);
        assertEquals(null, payloadMojoMessage.data[4].mediaType);
        assertEquals(TEST_TEXT, new String(payloadMojoMessage.data[4].data));
        // The icon record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, payloadMojoMessage.data[5].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_MIME, payloadMojoMessage.data[5].recordType);
        assertEquals("image/png", payloadMojoMessage.data[5].mediaType);
        // The application-specific record, e.g. an external type record.
        assertEquals(NdefRecordTypeCategory.EXTERNAL, payloadMojoMessage.data[6].category);
        assertEquals(DUMMY_EXTERNAL_TYPE, payloadMojoMessage.data[6].recordType);
        assertEquals(null, payloadMojoMessage.data[6].mediaType);
        assertEquals(TEST_TEXT, new String(payloadMojoMessage.data[6].data));

        // Test local record conversion.
        android.nfc.NdefMessage localNdefMessage =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformLocalRecord(
                                "xyz",
                                DUMMY_RECORD_ID,
                                ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT),
                                /* payloadMessage= */ null));
        NdefMessage localMojoNdefMessage = NdefMessageUtils.toNdefMessage(localNdefMessage);
        assertEquals(1, localMojoNdefMessage.data.length);
        assertEquals(NdefRecordTypeCategory.LOCAL, localMojoNdefMessage.data[0].category);
        // Is already prefixed with ':'.
        assertEquals(":xyz", localMojoNdefMessage.data[0].recordType);
        assertEquals(null, localMojoNdefMessage.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, localMojoNdefMessage.data[0].id);
        assertNull(localMojoNdefMessage.data[0].encoding);
        assertNull(localMojoNdefMessage.data[0].lang);
        assertEquals(TEST_TEXT, new String(localMojoNdefMessage.data[0].data));

        // Test conversion for local records with the payload being a ndef message.
        payloadMessage =
                new android.nfc.NdefMessage(
                        android.nfc.NdefRecord.createTextRecord(LANG_EN_US, TEST_TEXT));
        payloadBytes = payloadMessage.toByteArray();
        // Put |payloadBytes| as payload of a local record.
        android.nfc.NdefMessage localNdefMessage1 =
                new android.nfc.NdefMessage(
                        NdefMessageUtils.createPlatformLocalRecord(
                                "xyz", DUMMY_RECORD_ID, payloadBytes, /* payloadMessage= */ null));
        NdefMessage localMojoNdefMessage1 = NdefMessageUtils.toNdefMessage(localNdefMessage1);
        assertEquals(1, localMojoNdefMessage1.data.length);
        assertEquals(NdefRecordTypeCategory.LOCAL, localMojoNdefMessage1.data[0].category);
        // Is already prefixed with ':'.
        assertEquals(":xyz", localMojoNdefMessage1.data[0].recordType);
        assertEquals(null, localMojoNdefMessage1.data[0].mediaType);
        assertEquals(DUMMY_RECORD_ID, localMojoNdefMessage1.data[0].id);
        // The embedded ndef message should have content corresponding with the original
        // |payloadMessage|.
        payloadMojoMessage = localMojoNdefMessage1.data[0].payloadMessage;
        assertEquals(1, payloadMojoMessage.data.length);
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, payloadMojoMessage.data[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, payloadMojoMessage.data[0].recordType);
        assertEquals(null, payloadMojoMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(payloadMojoMessage.data[0].data));
    }

    /** Test conversion from mojo NdefMessage to android NdefMessage. */
    @Test
    @Feature({"NFCTest"})
    public void testMojoToNdefConversion()
            throws UnsupportedEncodingException, InvalidNdefMessageException, FormatException {
        // Test url record conversion.
        NdefRecord urlMojoNdefRecord = new NdefRecord();
        urlMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        urlMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_URL;
        urlMojoNdefRecord.id = DUMMY_RECORD_ID;
        urlMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_URL);
        NdefMessage urlMojoNdefMessage = createMojoNdefMessage(urlMojoNdefRecord);
        android.nfc.NdefMessage urlNdefMessage = NdefMessageUtils.toNdefMessage(urlMojoNdefMessage);
        assertEquals(1, urlNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, urlNdefMessage.getRecords()[0].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_URI),
                new String(urlNdefMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(urlNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_URL, urlNdefMessage.getRecords()[0].toUri().toString());

        // Test absolute-url record conversion.
        NdefRecord absUrlMojoNdefRecord = new NdefRecord();
        absUrlMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        absUrlMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_ABSOLUTE_URL;
        absUrlMojoNdefRecord.id = DUMMY_RECORD_ID;
        absUrlMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_URL);
        NdefMessage absUrlMojoNdefMessage = createMojoNdefMessage(absUrlMojoNdefRecord);
        android.nfc.NdefMessage absUrlNdefMessage =
                NdefMessageUtils.toNdefMessage(absUrlMojoNdefMessage);
        assertEquals(1, absUrlNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_ABSOLUTE_URI,
                absUrlNdefMessage.getRecords()[0].getTnf());
        assertEquals(DUMMY_RECORD_ID, new String(absUrlNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_URL, absUrlNdefMessage.getRecords()[0].toUri().toString());

        // Test text record conversion for UTF-8 content.
        NdefRecord utf8TextMojoNdefRecord = new NdefRecord();
        utf8TextMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        utf8TextMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_TEXT;
        utf8TextMojoNdefRecord.id = DUMMY_RECORD_ID;
        utf8TextMojoNdefRecord.encoding = ENCODING_UTF8;
        utf8TextMojoNdefRecord.lang = LANG_EN_US;
        utf8TextMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage utf8TextMojoNdefMessage = createMojoNdefMessage(utf8TextMojoNdefRecord);
        android.nfc.NdefMessage utf8TextNdefMessage =
                NdefMessageUtils.toNdefMessage(utf8TextMojoNdefMessage);
        assertEquals(1, utf8TextNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN,
                utf8TextNdefMessage.getRecords()[0].getTnf());
        assertEquals(DUMMY_RECORD_ID, new String(utf8TextNdefMessage.getRecords()[0].getId()));
        {
            byte[] languageCodeBytes = LANG_EN_US.getBytes(StandardCharsets.US_ASCII);
            ByteBuffer expectedPayload =
                    ByteBuffer.allocate(
                            1 + languageCodeBytes.length + utf8TextMojoNdefRecord.data.length);
            byte status = (byte) languageCodeBytes.length;
            expectedPayload.put(status);
            expectedPayload.put(languageCodeBytes);
            expectedPayload.put(utf8TextMojoNdefRecord.data);
            assertArrayEquals(
                    expectedPayload.array(), utf8TextNdefMessage.getRecords()[0].getPayload());
        }

        // Test text record conversion for UTF-16 content.
        NdefRecord utf16TextMojoNdefRecord = new NdefRecord();
        utf16TextMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        utf16TextMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_TEXT;
        utf16TextMojoNdefRecord.id = DUMMY_RECORD_ID;
        utf16TextMojoNdefRecord.encoding = ENCODING_UTF16;
        utf16TextMojoNdefRecord.lang = LANG_EN_US;
        utf16TextMojoNdefRecord.data = TEST_TEXT.getBytes(Charset.forName("UTF-16"));
        NdefMessage utf16TextMojoNdefMessage = createMojoNdefMessage(utf16TextMojoNdefRecord);
        android.nfc.NdefMessage utf16TextNdefMessage =
                NdefMessageUtils.toNdefMessage(utf16TextMojoNdefMessage);
        assertEquals(1, utf16TextNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN,
                utf16TextNdefMessage.getRecords()[0].getTnf());
        assertEquals(DUMMY_RECORD_ID, new String(utf16TextNdefMessage.getRecords()[0].getId()));
        {
            byte[] languageCodeBytes = LANG_EN_US.getBytes(StandardCharsets.US_ASCII);
            ByteBuffer expectedPayload =
                    ByteBuffer.allocate(
                            1 + languageCodeBytes.length + utf16TextMojoNdefRecord.data.length);
            byte status = (byte) languageCodeBytes.length;
            status |= (byte) (1 << 7);
            expectedPayload.put(status);
            expectedPayload.put(languageCodeBytes);
            expectedPayload.put(utf16TextMojoNdefRecord.data);
            assertArrayEquals(
                    expectedPayload.array(), utf16TextNdefMessage.getRecords()[0].getPayload());
        }

        // Test mime record conversion with "text/plain" mime type.
        NdefRecord mimeMojoNdefRecord = new NdefRecord();
        mimeMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        mimeMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_MIME;
        mimeMojoNdefRecord.mediaType = TEXT_MIME;
        mimeMojoNdefRecord.id = DUMMY_RECORD_ID;
        mimeMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage mimeMojoNdefMessage = createMojoNdefMessage(mimeMojoNdefRecord);
        android.nfc.NdefMessage mimeNdefMessage =
                NdefMessageUtils.toNdefMessage(mimeMojoNdefMessage);
        assertEquals(1, mimeNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, mimeNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEXT_MIME, mimeNdefMessage.getRecords()[0].toMimeType());
        assertEquals(DUMMY_RECORD_ID, new String(mimeNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_TEXT, new String(mimeNdefMessage.getRecords()[0].getPayload()));

        // Test mime record conversion with "application/json" mime type.
        NdefRecord jsonMojoNdefRecord = new NdefRecord();
        jsonMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        jsonMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_MIME;
        jsonMojoNdefRecord.mediaType = JSON_MIME;
        jsonMojoNdefRecord.id = DUMMY_RECORD_ID;
        jsonMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_JSON);
        NdefMessage jsonMojoNdefMessage = createMojoNdefMessage(jsonMojoNdefRecord);
        android.nfc.NdefMessage jsonNdefMessage =
                NdefMessageUtils.toNdefMessage(jsonMojoNdefMessage);
        assertEquals(1, jsonNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, jsonNdefMessage.getRecords()[0].getTnf());
        assertEquals(JSON_MIME, jsonNdefMessage.getRecords()[0].toMimeType());
        assertEquals(DUMMY_RECORD_ID, new String(jsonNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_JSON, new String(jsonNdefMessage.getRecords()[0].getPayload()));

        // Test unknown record conversion.
        NdefRecord unknownMojoNdefRecord = new NdefRecord();
        unknownMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        unknownMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_UNKNOWN;
        unknownMojoNdefRecord.id = DUMMY_RECORD_ID;
        unknownMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage unknownMojoNdefMessage = createMojoNdefMessage(unknownMojoNdefRecord);
        android.nfc.NdefMessage unknownNdefMessage =
                NdefMessageUtils.toNdefMessage(unknownMojoNdefMessage);
        assertEquals(1, unknownNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_UNKNOWN, unknownNdefMessage.getRecords()[0].getTnf());
        assertEquals(DUMMY_RECORD_ID, new String(unknownNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_TEXT, new String(unknownNdefMessage.getRecords()[0].getPayload()));

        // Test external record conversion.
        NdefRecord extMojoNdefRecord = new NdefRecord();
        extMojoNdefRecord.category = NdefRecordTypeCategory.EXTERNAL;
        extMojoNdefRecord.recordType = DUMMY_EXTERNAL_TYPE;
        extMojoNdefRecord.id = DUMMY_RECORD_ID;
        extMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
        android.nfc.NdefMessage extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
        assertEquals(1, extNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, extNdefMessage.getRecords()[0].getTnf());
        assertEquals(DUMMY_EXTERNAL_TYPE, new String(extNdefMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(extNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_TEXT, new String(extNdefMessage.getRecords()[0].getPayload()));

        // Test conversion for external records with the payload being a ndef message.
        NdefRecord payloadMojoRecord = new NdefRecord();
        payloadMojoRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        payloadMojoRecord.recordType = NdefMessageUtils.RECORD_TYPE_URL;
        payloadMojoRecord.id = DUMMY_RECORD_ID;
        payloadMojoRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_URL);
        // Prepare an external record that embeds |payloadMojoRecord| in its payload.
        NdefRecord extMojoNdefRecord1 = new NdefRecord();
        extMojoNdefRecord1.category = NdefRecordTypeCategory.EXTERNAL;
        extMojoNdefRecord1.recordType = DUMMY_EXTERNAL_TYPE;
        extMojoNdefRecord1.id = DUMMY_RECORD_ID;
        // device.mojom.NDEFRecord.data is not allowed to be null, instead, empty byte array is just
        // what's passed from Blink.
        extMojoNdefRecord1.data = new byte[0];
        extMojoNdefRecord1.payloadMessage = createMojoNdefMessage(payloadMojoRecord);
        // Do the conversion.
        android.nfc.NdefMessage extNdefMessage1 =
                NdefMessageUtils.toNdefMessage(createMojoNdefMessage(extMojoNdefRecord1));
        assertEquals(1, extNdefMessage1.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, extNdefMessage1.getRecords()[0].getTnf());
        assertEquals(DUMMY_EXTERNAL_TYPE, new String(extNdefMessage1.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(extNdefMessage1.getRecords()[0].getId()));
        // The payload raw bytes should be able to construct an ndef message containing an ndef
        // record that has content corresponding with the original |payloadMojoRecord|.
        android.nfc.NdefMessage payloadMessage =
                new android.nfc.NdefMessage(extNdefMessage1.getRecords()[0].getPayload());
        assertNotNull(payloadMessage);
        assertEquals(1, payloadMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[0].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_URI),
                new String(payloadMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(payloadMessage.getRecords()[0].getId()));
        assertEquals(TEST_URL, payloadMessage.getRecords()[0].toUri().toString());

        // Test conversion for smart-poster records.
        //
        // Prepare a Mojo NdefMessage |spMojoNdefMessage| by converting an android.nfc.NdefMessage
        // that contains the smart-poster record . This conversion has already been tested OK by
        // testNdefToMojoConversion(), i.e. |spMojoNdefMessage|.is valid and its smart-poster record
        // contains those sub records corresponding to those created by
        // createSmartPosterNdefRecord().
        NdefMessage spMojoNdefMessage =
                NdefMessageUtils.toNdefMessage(
                        new android.nfc.NdefMessage(createSmartPosterNdefRecord()));
        assertNotNull(spMojoNdefMessage.data[0].payloadMessage);
        // Do the conversion.
        android.nfc.NdefMessage spNdefMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        assertEquals(1, spNdefMessage.getRecords().length);
        assertEquals(android.nfc.NdefRecord.TNF_WELL_KNOWN, spNdefMessage.getRecords()[0].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_SMART_POSTER),
                new String(spNdefMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(spNdefMessage.getRecords()[0].getId()));
        // The payload raw bytes of the smart-poster record should be able to construct an
        // NdefMessage which contains records matching those created by
        // createSmartPosterNdefRecord() in the beginning.
        payloadMessage = new android.nfc.NdefMessage(spNdefMessage.getRecords()[0].getPayload());
        assertNotNull(payloadMessage);
        assertEquals(7, payloadMessage.getRecords().length);
        // The url record.
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[0].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_URI),
                new String(payloadMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(payloadMessage.getRecords()[0].getId()));
        assertEquals(TEST_URL, payloadMessage.getRecords()[0].toUri().toString());
        // The size record.
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[1].getTnf());
        assertEquals("s", new String(payloadMessage.getRecords()[1].getType()));
        assertEquals(4, payloadMessage.getRecords()[1].getPayload().length);
        assertEquals(
                4096,
                ByteBuffer.allocate(4).put(payloadMessage.getRecords()[1].getPayload()).getInt(0));
        // The type record.
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[2].getTnf());
        assertEquals("t", new String(payloadMessage.getRecords()[2].getType()));
        assertEquals(OCTET_STREAM_MIME, new String(payloadMessage.getRecords()[2].getPayload()));
        // The action record.
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[3].getTnf());
        assertEquals("act", new String(payloadMessage.getRecords()[3].getType()));
        assertEquals(1, payloadMessage.getRecords()[3].getPayload().length);
        assertEquals(0x01, payloadMessage.getRecords()[3].getPayload()[0]);
        // The title record.
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[4].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_TEXT),
                new String(payloadMessage.getRecords()[4].getType()));
        // The icon record.
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, payloadMessage.getRecords()[5].getTnf());
        assertEquals("image/png", new String(payloadMessage.getRecords()[5].toMimeType()));
        // The application-specific record, e.g. an external type record.
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, payloadMessage.getRecords()[6].getTnf());
        assertEquals(DUMMY_EXTERNAL_TYPE, new String(payloadMessage.getRecords()[6].getType()));
        assertEquals(TEST_TEXT, new String(payloadMessage.getRecords()[6].getPayload()));

        // Test local record conversion.
        NdefRecord localMojoNdefRecord = new NdefRecord();
        localMojoNdefRecord.category = NdefRecordTypeCategory.LOCAL;
        localMojoNdefRecord.recordType = ":xyz";
        localMojoNdefRecord.id = DUMMY_RECORD_ID;
        localMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage localMojoNdefMessage = createMojoNdefMessage(localMojoNdefRecord);
        android.nfc.NdefMessage localNdefMessage =
                NdefMessageUtils.toNdefMessage(localMojoNdefMessage);
        assertEquals(1, localNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, localNdefMessage.getRecords()[0].getTnf());
        // The ':' prefix is already omitted.
        assertEquals("xyz", new String(localNdefMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(localNdefMessage.getRecords()[0].getId()));
        assertEquals(TEST_TEXT, new String(localNdefMessage.getRecords()[0].getPayload()));

        // Test conversion for local records with the payload being a ndef message.
        //
        // Prepare a local record that embeds |payloadMojoRecord| in its payload.
        NdefRecord localMojoNdefRecord1 = new NdefRecord();
        localMojoNdefRecord1.category = NdefRecordTypeCategory.LOCAL;
        localMojoNdefRecord1.recordType = ":xyz";
        localMojoNdefRecord1.id = DUMMY_RECORD_ID;
        // device.mojom.NDEFRecord.data is not allowed to be null, instead, empty byte array is just
        // what's passed from Blink.
        localMojoNdefRecord1.data = new byte[0];
        localMojoNdefRecord1.payloadMessage = createMojoNdefMessage(payloadMojoRecord);
        // Do the conversion.
        android.nfc.NdefMessage localNdefMessage1 =
                NdefMessageUtils.toNdefMessage(createMojoNdefMessage(localMojoNdefRecord1));
        assertEquals(1, localNdefMessage1.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, localNdefMessage1.getRecords()[0].getTnf());
        // The ':' prefix is already omitted.
        assertEquals("xyz", new String(localNdefMessage1.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(localNdefMessage1.getRecords()[0].getId()));
        // The payload raw bytes should be able to construct an ndef message containing an ndef
        // record that has content corresponding with the original |payloadMojoRecord|.
        payloadMessage =
                new android.nfc.NdefMessage(localNdefMessage1.getRecords()[0].getPayload());
        assertNotNull(payloadMessage);
        assertEquals(1, payloadMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, payloadMessage.getRecords()[0].getTnf());
        assertEquals(
                new String(android.nfc.NdefRecord.RTD_URI),
                new String(payloadMessage.getRecords()[0].getType()));
        assertEquals(DUMMY_RECORD_ID, new String(payloadMessage.getRecords()[0].getId()));
        assertEquals(TEST_URL, payloadMessage.getRecords()[0].toUri().toString());

        // Test EMPTY record conversion.
        NdefRecord emptyMojoNdefRecord = new NdefRecord();
        emptyMojoNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        emptyMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_EMPTY;
        NdefMessage emptyMojoNdefMessage = createMojoNdefMessage(emptyMojoNdefRecord);
        android.nfc.NdefMessage emptyNdefMessage =
                NdefMessageUtils.toNdefMessage(emptyMojoNdefMessage);
        assertEquals(1, emptyNdefMessage.getRecords().length);
        assertEquals(android.nfc.NdefRecord.TNF_EMPTY, emptyNdefMessage.getRecords()[0].getTnf());
    }

    /** Test external record conversion with invalid custom type. */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidExternalRecordType() {
        NdefRecord extMojoNdefRecord = new NdefRecord();
        extMojoNdefRecord.category = NdefRecordTypeCategory.EXTERNAL;
        extMojoNdefRecord.id = DUMMY_RECORD_ID;
        extMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        {
            // Must have a ':'.
            extMojoNdefRecord.recordType = "abc.com";
            NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
            android.nfc.NdefMessage extNdefMessage = null;
            try {
                extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage);
        }
        {
            // '~' is allowed in the domain part.
            extMojoNdefRecord.recordType = "abc~123.com:xyz";
            NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
            android.nfc.NdefMessage extNdefMessage = null;
            try {
                extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNotNull(extNdefMessage);
            assertEquals(1, extNdefMessage.getRecords().length);
            assertEquals(
                    android.nfc.NdefRecord.TNF_EXTERNAL_TYPE,
                    extNdefMessage.getRecords()[0].getTnf());
            assertEquals("abc~123.com:xyz", new String(extNdefMessage.getRecords()[0].getType()));
            assertEquals(DUMMY_RECORD_ID, new String(extNdefMessage.getRecords()[0].getId()));
            assertEquals(TEST_TEXT, new String(extNdefMessage.getRecords()[0].getPayload()));
        }
        {
            // '~' is not allowed in the type part.
            extMojoNdefRecord.recordType = "abc.com:xyz~123";
            NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
            android.nfc.NdefMessage extNdefMessage = null;
            try {
                extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage);
        }
        {
            // As the 2 cases above have proved that '~' is allowed in the domain part but not
            // allowed in the type part, from this case we can say that the first occurrence of
            // ':' is used to separate the domain part and the type part, i.e. "xyz~123:uvw" is
            // separated as the type part and is treated as invalid due to the existence of '~'.
            extMojoNdefRecord.recordType = "abc.com:xyz~123:uvw";
            NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
            android.nfc.NdefMessage extNdefMessage = null;
            try {
                extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage);
        }
        {
            // |recordType| is a string mixed with ASCII/non-ASCII, FAIL.
            extMojoNdefRecord.recordType = "example.com:hellö";
            android.nfc.NdefMessage extNdefMessage_nonASCII = null;
            try {
                extNdefMessage_nonASCII =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(extMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage_nonASCII);

            char[] chars = new char[251];
            Arrays.fill(chars, 'a');
            String domain = new String(chars);

            // |recordType|'s length is 255, OK.
            extMojoNdefRecord.recordType = domain + ":xyz";
            android.nfc.NdefMessage extNdefMessage_255 = null;
            try {
                extNdefMessage_255 =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(extMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNotNull(extNdefMessage_255);

            // Exceeding the maximum length 255, FAIL.
            extMojoNdefRecord.recordType = domain + ":xyze";
            android.nfc.NdefMessage extNdefMessage_256 = null;
            try {
                extNdefMessage_256 =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(extMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage_256);
        }
        {
            // '/' is not allowed in the type part.
            extMojoNdefRecord.recordType = "abc.com:xyz/";
            NdefMessage extMojoNdefMessage = createMojoNdefMessage(extMojoNdefRecord);
            android.nfc.NdefMessage extNdefMessage = null;
            try {
                extNdefMessage = NdefMessageUtils.toNdefMessage(extMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(extNdefMessage);
        }
    }

    /** Test local type record conversion with invalid local type. */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidLocalRecordType() {
        NdefRecord localMojoNdefRecord = new NdefRecord();
        localMojoNdefRecord.category = NdefRecordTypeCategory.LOCAL;
        localMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        {
            // Must start with ':'.
            localMojoNdefRecord.recordType = "dummyLocalTypeNotStartingwith:";
            localMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
            NdefMessage localMojoNdefMessage = createMojoNdefMessage(localMojoNdefRecord);
            android.nfc.NdefMessage localNdefMessage = null;
            try {
                localNdefMessage = NdefMessageUtils.toNdefMessage(localMojoNdefMessage);
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(localNdefMessage);
        }
        {
            // |recordType| is a string mixed with ASCII/non-ASCII, FAIL.
            localMojoNdefRecord.recordType = ":hellö";
            android.nfc.NdefMessage localNdefMessage_nonASCII = null;
            try {
                localNdefMessage_nonASCII =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(localMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(localNdefMessage_nonASCII);

            char[] chars = new char[255];
            Arrays.fill(chars, 'a');
            String chars_255 = new String(chars);

            // The length of the real local type is 255, OK.
            localMojoNdefRecord.recordType = ":" + chars_255;
            android.nfc.NdefMessage localNdefMessage_255 = null;
            try {
                localNdefMessage_255 =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(localMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNotNull(localNdefMessage_255);

            // Exceeding the maximum length 255, FAIL.
            localMojoNdefRecord.recordType = ":a" + chars_255;
            android.nfc.NdefMessage localNdefMessage_256 = null;
            try {
                localNdefMessage_256 =
                        NdefMessageUtils.toNdefMessage(createMojoNdefMessage(localMojoNdefRecord));
            } catch (InvalidNdefMessageException e) {
            }
            assertNull(localNdefMessage_256);
        }
    }

    /** Test smart-poster record conversion with invalid sub records. */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidSmartPosterRecord() {
        // Prepare a Mojo NdefMessage |spMojoNdefMessage| that contains the smart-poster record.
        NdefMessage spMojoNdefMessage = null;
        try {
            spMojoNdefMessage =
                    NdefMessageUtils.toNdefMessage(
                            new android.nfc.NdefMessage(createSmartPosterNdefRecord()));
        } catch (UnsupportedEncodingException e) {
        }
        assertNotNull(spMojoNdefMessage);
        // The smart-poster record's payload contains the sub records created by
        // createSmartPosterNdefRecord() previously.
        assertNotNull(spMojoNdefMessage.data[0].payloadMessage);
        assertEquals(7, spMojoNdefMessage.data[0].payloadMessage.data.length);
        NdefRecord[] spEmbeddedRecords = spMojoNdefMessage.data[0].payloadMessage.data;
        // The url record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, spEmbeddedRecords[0].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_URL, spEmbeddedRecords[0].recordType);
        // The size record.
        assertEquals(NdefRecordTypeCategory.LOCAL, spEmbeddedRecords[1].category);
        assertEquals(":s", spEmbeddedRecords[1].recordType);
        // The type record.
        assertEquals(NdefRecordTypeCategory.LOCAL, spEmbeddedRecords[2].category);
        assertEquals(":t", spEmbeddedRecords[2].recordType);
        // The action record.
        assertEquals(NdefRecordTypeCategory.LOCAL, spEmbeddedRecords[3].category);
        assertEquals(":act", spEmbeddedRecords[3].recordType);
        // The title record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, spEmbeddedRecords[4].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, spEmbeddedRecords[4].recordType);
        // The icon record.
        assertEquals(NdefRecordTypeCategory.STANDARDIZED, spEmbeddedRecords[5].category);
        assertEquals(NdefMessageUtils.RECORD_TYPE_MIME, spEmbeddedRecords[5].recordType);
        // The application-specific record, e.g. an external type record.
        assertEquals(NdefRecordTypeCategory.EXTERNAL, spEmbeddedRecords[6].category);
        assertEquals(DUMMY_EXTERNAL_TYPE, spEmbeddedRecords[6].recordType);

        // At first, |spMojoNdefMessage| can be converted to an android.nfc.NdefMessage
        // successfully.
        android.nfc.NdefMessage convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNotNull(convertedMessage);
        // Omit all other records than the url record, still OK.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[1];
        spMojoNdefMessage.data[0].payloadMessage.data[0] = spEmbeddedRecords[0];
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNotNull(convertedMessage);
        // Omit the mandatory url record, FAIL.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[6];
        System.arraycopy(spEmbeddedRecords, 1, spMojoNdefMessage.data[0].payloadMessage.data, 0, 6);
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNull(convertedMessage);
        // Add an extra url record, FAIL because only single url record is allowed.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[8];
        System.arraycopy(spEmbeddedRecords, 0, spMojoNdefMessage.data[0].payloadMessage.data, 0, 7);
        NdefRecord urlRecord = new NdefRecord();
        urlRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        urlRecord.recordType = NdefMessageUtils.RECORD_TYPE_URL;
        urlRecord.data = ApiCompatibilityUtils.getBytesUtf8("https://duplicate.url.record");
        spMojoNdefMessage.data[0].payloadMessage.data[7] = urlRecord;
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNull(convertedMessage);
        // Add an extra size record, FAIL because at most one size record is allowed.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[8];
        System.arraycopy(spEmbeddedRecords, 0, spMojoNdefMessage.data[0].payloadMessage.data, 0, 7);
        NdefRecord sizeRecord = new NdefRecord();
        sizeRecord.category = NdefRecordTypeCategory.LOCAL;
        sizeRecord.recordType = ":s";
        sizeRecord.data = ByteBuffer.allocate(4).putInt(512).array();
        spMojoNdefMessage.data[0].payloadMessage.data[7] = sizeRecord;
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNull(convertedMessage);
        // Add an extra type record, FAIL because at most one type record is allowed.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[8];
        System.arraycopy(spEmbeddedRecords, 0, spMojoNdefMessage.data[0].payloadMessage.data, 0, 7);
        NdefRecord typeRecord = new NdefRecord();
        typeRecord.category = NdefRecordTypeCategory.LOCAL;
        typeRecord.recordType = ":t";
        typeRecord.data = ApiCompatibilityUtils.getBytesUtf8("duplicate/type");
        spMojoNdefMessage.data[0].payloadMessage.data[7] = typeRecord;
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNull(convertedMessage);
        // Add an extra action record, FAIL because at most one action record is allowed.
        spMojoNdefMessage.data[0].payloadMessage.data = new NdefRecord[8];
        System.arraycopy(spEmbeddedRecords, 0, spMojoNdefMessage.data[0].payloadMessage.data, 0, 7);
        NdefRecord actionRecord = new NdefRecord();
        actionRecord.category = NdefRecordTypeCategory.LOCAL;
        actionRecord.recordType = ":act";
        actionRecord.data = ByteBuffer.allocate(4).put((byte) 0x00).array();
        spMojoNdefMessage.data[0].payloadMessage.data[7] = actionRecord;
        convertedMessage = null;
        try {
            convertedMessage = NdefMessageUtils.toNdefMessage(spMojoNdefMessage);
        } catch (InvalidNdefMessageException e) {
        }
        assertNull(convertedMessage);
    }

    /** Test that invalid NdefMessage is rejected with INVALID_MESSAGE error code. */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidNdefMessage() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);
        nfc.push(new NdefMessage(), createNdefWriteOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NdefErrorType.INVALID_MESSAGE, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.suspendNfcOperations() and Nfc.resumeNfcOperations() work correctly. */
    @Test
    @Feature({"NFCTest"})
    public void testResumeSuspend() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        // No activity / client or active pending operations
        nfc.suspendNfcOperations();
        nfc.resumeNfcOperations();

        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        Watch_Response mockCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockCallback);
        nfc.suspendNfcOperations();
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
        nfc.resumeNfcOperations();
        // 1. Enable after watch is called, 2. after resumeNfcOperations is called.
        verify(mNfcAdapter, times(2))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        // Check that watch request was completed successfully.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(mNextWatchId, mOnWatchCallbackCaptor.getValue()[0]);
    }

    /** Test that NFC.watch() is not triggered when NFC operations are suspended. */
    @Test
    @Feature({"NFCTest"})
    public void testWatchWhenOperationsAreSuspended() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        nfc.suspendNfcOperations();
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        Watch_Response mockCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockCallback);

        // Check that watch request was completed successfully even if NFC operations are suspended.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that watch is not triggered when NFC tag is in proximity.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mNfcClient, times(0))
                .onWatch(any(int[].class), nullable(String.class), any(NdefMessage.class));

        nfc.resumeNfcOperations();
        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        // Check that client was notified and watch with correct id was triggered.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mNfcClient, times(1))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(mNextWatchId, mOnWatchCallbackCaptor.getValue()[0]);
    }

    /** Test that Nfc.push() fails if NFC operations are already suspended. */
    @Test
    @Feature({"NFCTest"})
    public void testPushWhenOperationsAreSuspended() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        nfc.suspendNfcOperations();
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback);

        // Check that push request was cancelled with OPERATION_CANCELLED.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.suspendNfcOperations() cancels pending push operation. */
    @Test
    @Feature({"NFCTest"})
    public void testSuspendNfcOperationsCancelPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockPushCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockPushCallback);
        nfc.suspendNfcOperations();

        // Check that push request was cancelled with OPERATION_CANCELLED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.push() successful when NFC tag is connected. */
    @Test
    @Feature({"NFCTest"})
    public void testPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /** Test that Nfc.cancelPush() cancels pending push operation. */
    @Test
    @Feature({"NFCTest"})
    public void testCancelPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockPushCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockPushCallback);
        nfc.cancelPush();

        // Check that push request was cancelled with OPERATION_CANCELLED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.makeReadOnly() fails if NFC operations are already suspended. */
    @Test
    @Feature({"NFCTest"})
    public void testMakeReadOnlyWhenOperationsAreSuspended() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        nfc.suspendNfcOperations();
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockCallback);

        // Check that makeReadOnly request was cancelled with OPERATION_CANCELLED.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.suspendNfcOperations() cancels pending makeReadOnly operation. */
    @Test
    @Feature({"NFCTest"})
    public void testSuspendNfcOperationsCancelMakeReadOnly() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockMakeReadOnlyCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockMakeReadOnlyCallback);
        nfc.suspendNfcOperations();

        // Check that makeReadOnly request was cancelled with OPERATION_CANCELLED.
        verify(mockMakeReadOnlyCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.makeReadOnly() successful when NFC tag is connected. */
    @Test
    @Feature({"NFCTest"})
    public void testMakeReadOnly() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /** Test that Nfc.cancelMakeReadOnly() cancels pending makeReadOnly operation. */
    @Test
    @Feature({"NFCTest"})
    public void testCancelMakeReadOnly() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockMakeReadOnlyCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockMakeReadOnlyCallback);
        nfc.cancelMakeReadOnly();

        // Check that MakeReadOnly request was cancelled with OPERATION_CANCELLED.
        verify(mockMakeReadOnlyCallback).call(mErrorCaptor.capture());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that Nfc.watch() works correctly and client is notified. */
    @Test
    @Feature({"NFCTest"})
    public void testWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        int watchId1 = mNextWatchId++;
        Watch_Response mockWatchCallback1 = mock(Watch_Response.class);
        nfc.watch(watchId1, mockWatchCallback1);

        // Check that watch requests were completed successfully.
        verify(mockWatchCallback1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        int watchId2 = mNextWatchId++;
        Watch_Response mockWatchCallback2 = mock(Watch_Response.class);
        nfc.watch(watchId2, mockWatchCallback2);
        verify(mockWatchCallback2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Mocks 'NFC tag found' event.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and correct watch ids were provided.
        verify(mNfcClient, times(1))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
    }

    /** Test that Nfc.watch() notifies client when tag is not formatted. */
    @Test
    @Feature({"NFCTest"})
    public void testWatchNotFormattedTag() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        int watchId = mNextWatchId++;
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(watchId, mockWatchCallback);
        verify(mockWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Returning null means tag is not formatted.
        doReturn(null).when(mNfcTagHandler).read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and correct watch id was provided.
        verify(mNfcClient, times(1))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(watchId, mOnWatchCallbackCaptor.getValue()[0]);
    }

    /** Test that Nfc.watch() can be cancelled with Nfc.cancelWatch(). */
    @Test
    @Feature({"NFCTest"})
    public void testCancelWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);

        verify(mockWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        nfc.cancelWatch(mNextWatchId);

        // Check that watch is not triggered when NFC tag is in proximity.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mNfcClient, times(0))
                .onWatch(any(int[].class), nullable(String.class), any(NdefMessage.class));
    }

    /**
     * Test that when the tag in proximity is found to be not NDEF compatible, an error event will
     * be dispatched to the client and the pending push and makeReadOnly operations will also be
     * ended with an error.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNonNdefCompatibleTagFound() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        // Prepare at least one watcher, otherwise the error won't be notified.
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);
        // Start a push.
        Push_Response mockPushCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockPushCallback);
        // Start a makeReadOnly.
        MakeReadOnly_Response mockMakeReadOnlyCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockMakeReadOnlyCallback);

        // Pass null tag handler to simulate that the tag is not NDEF compatible.
        nfc.processPendingOperationsForTesting(null);

        // An error is notified.
        verify(mNfcClient, times(1)).onError(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
        // No watch.
        verify(mNfcClient, times(0))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));

        // The pending push failed with the correct error.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);

        // The pending makeReadOnly failed with the correct error.
        verify(mockMakeReadOnlyCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that when the tag in proximity is found to be blocked, an error event will
     * be dispatched to the client and the pending push and makeReadOnly operations will also be
     * ended with an error.
     */
    @Test
    @Feature({"NFCTest"})
    public void testBlockedTagFound() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        // Prepare at least one watcher, otherwise the error won't be notified.
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);
        // Start a push.
        Push_Response mockPushCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockPushCallback);
        // Start a makeReadOnly.
        MakeReadOnly_Response mockMakeReadOnlyCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockMakeReadOnlyCallback);

        // Mocks blocked 'NFC tag found' event.
        NfcBlocklist.getInstance().setIsTagBlockedForTesting(true);
        Tag tag = mock(Tag.class);
        NfcTagHandler nfcTagHandler = NfcTagHandler.create(tag);
        nfc.processPendingOperationsForTesting(nfcTagHandler);

        // An error is notified.
        verify(mNfcClient, times(1)).onError(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
        // No watch.
        verify(mNfcClient, times(0))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));

        // The pending push failed with the correct error.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);

        // The pending makeReadOnly failed with the correct error.
        verify(mockMakeReadOnlyCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that when the tag in proximity is found to be not NDEF compatible, an error event will
     * not be dispatched to the client if there is no watcher present.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNonNdefCompatibleTagFoundWithoutWatcher() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Pass null tag handler to simulate that the tag is not NDEF compatible.
        nfc.processPendingOperationsForTesting(null);

        // An error is NOT notified.
        verify(mNfcClient, times(0)).onError(mErrorCaptor.capture());
        // No watch.
        verify(mNfcClient, times(0))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
    }

    /**
     * Test that when tag is disconnected during read operation, IllegalStateException is handled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTagDisconnectedDuringRead() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);

        // Force read operation to fail
        doThrow(IllegalStateException.class).when(mNfcTagHandler).read();

        // Mocks 'NFC tag found' event.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that the watch was not triggered but an error was dispatched to the client.
        verify(mNfcClient, times(0))
                .onWatch(
                        mOnWatchCallbackCaptor.capture(),
                        nullable(String.class),
                        any(NdefMessage.class));
        verify(mNfcClient, times(1)).onError(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.IO_ERROR, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that when tag is disconnected during write operation, IllegalStateException is handled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTagDisconnectedDuringWrite() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);

        // Force write operation to fail
        doThrow(IllegalStateException.class)
                .when(mNfcTagHandler)
                .write(any(android.nfc.NdefMessage.class));
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());

        // Test that correct error is returned.
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.IO_ERROR, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that when tag is disconnected during makeReadOnly operation, TagLostException is
     * handled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTagDisconnectedDuringMakeReadOnly() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockCallback = mock(MakeReadOnly_Response.class);

        // Force makeReadOnly operation to fail
        doThrow(TagLostException.class).when(mNfcTagHandler).makeReadOnly();
        nfc.makeReadOnly(mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());

        // Test that correct error is returned.
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.IO_ERROR, mErrorCaptor.getValue().errorType);
    }

    /** Test that multiple Nfc.push() invocations do not disable reader mode. */
    @Test
    @Feature({"NFCTest"})
    public void testPushMultipleInvocations() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        Push_Response mockCallback1 = mock(Push_Response.class);
        Push_Response mockCallback2 = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback1);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());
        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /** Test that multiple Nfc.makeReadOnly() invocations do not disable reader mode. */
    @Test
    @Feature({"NFCTest"})
    public void testMakeReadOnlyMultipleInvocations() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        MakeReadOnly_Response mockCallback1 = mock(MakeReadOnly_Response.class);
        MakeReadOnly_Response mockCallback2 = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockCallback1);
        nfc.makeReadOnly(mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());
        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and push operation is cancelled with correct error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushInvocationWithCancel() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);

        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        nfc.cancelPush();

        // Reader mode is disabled.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and makeReadOnly operation is cancelled with correct error
     * code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testMakeReadOnlyInvocationWithCancel() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        MakeReadOnly_Response mockCallback = mock(MakeReadOnly_Response.class);

        nfc.makeReadOnly(mockCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        nfc.cancelMakeReadOnly();

        // Reader mode is disabled.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and two push operations are cancelled with correct
     * error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTwoPushInvocationsWithCancel() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        Push_Response mockCallback1 = mock(Push_Response.class);
        Push_Response mockCallback2 = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback1);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        // The second push should cancel the first push.
        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        // Cancel the second push.
        nfc.cancelPush();

        // Reader mode is disabled after cancelPush is invoked.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback2).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and two makeReadOnly operations are cancelled with correct
     * error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTwoMakeReadOnlyInvocationsWithCancel() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        MakeReadOnly_Response mockCallback1 = mock(MakeReadOnly_Response.class);
        MakeReadOnly_Response mockCallback2 = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockCallback1);
        nfc.makeReadOnly(mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        // The second makeReadOnly should cancel the first makeReadOnly.
        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        // Cancel the second makeReadOnly.
        nfc.cancelMakeReadOnly();

        // Reader mode is disabled after cancelMakeReadOnly is invoked.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback2).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is not disabled when there is an active watch operation and push
     * operation is cancelled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelledPushDontDisableReaderMode() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);

        Push_Response mockPushCallback = mock(Push_Response.class);
        nfc.push(createMojoNdefMessage(), createNdefWriteOptions(), mockPushCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        nfc.cancelPush();

        // Push was cancelled with OPERATION_CANCELLED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        nfc.cancelWatch(mNextWatchId);

        // Reader mode is disabled when there are no pending push / watch operations.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
    }

    /**
     * Test that reader mode is not disabled when there is an active watch operation and
     * makeReadOnly operation is cancelled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelledMakeReadOnlyDontDisableReaderMode() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Watch_Response mockWatchCallback = mock(Watch_Response.class);
        nfc.watch(mNextWatchId, mockWatchCallback);

        MakeReadOnly_Response mockMakeReadOnlyCallback = mock(MakeReadOnly_Response.class);
        nfc.makeReadOnly(mockMakeReadOnlyCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(
                        any(Activity.class),
                        any(ReaderCallback.class),
                        anyInt(),
                        (Bundle) isNull());

        nfc.cancelMakeReadOnly();

        // MakeReadOnly was cancelled with OPERATION_CANCELLED.
        verify(mockMakeReadOnlyCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NdefErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        nfc.cancelWatch(mNextWatchId);

        // Reader mode is disabled when there are no pending makeReadOnly / watch operations.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
    }

    /** Test that Nfc.push() succeeds for NFC messages with EMPTY records. */
    @Test
    @Feature({"NFCTest"})
    public void testPushWithEmptyRecord() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        Push_Response mockCallback = mock(Push_Response.class);

        // Create message with empty record.
        NdefRecord emptyNdefRecord = new NdefRecord();
        emptyNdefRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        emptyNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_EMPTY;
        NdefMessage ndefMessage = createMojoNdefMessage(emptyNdefRecord);

        nfc.push(ndefMessage, createNdefWriteOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /** Creates NdefWriteOptions with default values. */
    private NdefWriteOptions createNdefWriteOptions() {
        NdefWriteOptions pushOptions = new NdefWriteOptions();
        pushOptions.overwrite = true;
        return pushOptions;
    }

    private NdefMessage createMojoNdefMessage() {
        NdefMessage message = new NdefMessage();
        message.data = new NdefRecord[1];

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.category = NdefRecordTypeCategory.STANDARDIZED;
        nfcRecord.recordType = NdefMessageUtils.RECORD_TYPE_TEXT;
        nfcRecord.encoding = ENCODING_UTF8;
        nfcRecord.lang = LANG_EN_US;
        nfcRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        message.data[0] = nfcRecord;
        return message;
    }

    private NdefMessage createMojoNdefMessage(NdefRecord record) {
        NdefMessage message = new NdefMessage();
        message.data = new NdefRecord[1];
        message.data[0] = record;
        return message;
    }

    private android.nfc.NdefMessage createNdefMessageWithRecordId(String id)
            throws UnsupportedEncodingException {
        return new android.nfc.NdefMessage(
                NdefMessageUtils.createPlatformUrlRecord(
                        ApiCompatibilityUtils.getBytesUtf8(TEST_URL), id, /* isAbsUrl= */ false));
    }

    private android.nfc.NdefRecord createSmartPosterNdefRecord()
            throws UnsupportedEncodingException {
        List<android.nfc.NdefRecord> records = new ArrayList<android.nfc.NdefRecord>();
        // The single mandatory url record.
        records.add(
                NdefMessageUtils.createPlatformUrlRecord(
                        ApiCompatibilityUtils.getBytesUtf8(TEST_URL),
                        DUMMY_RECORD_ID,
                        /* isAbsUrl= */ false));
        // Zero or one size record.
        records.add(
                NdefMessageUtils.createPlatformLocalRecord(
                        "s",
                        /* id= */ null,
                        ByteBuffer.allocate(4).putInt(4096).array(),
                        /* payloadMessage= */ null));
        // Zero or one type record.
        records.add(
                NdefMessageUtils.createPlatformLocalRecord(
                        "t",
                        /* id= */ null,
                        ApiCompatibilityUtils.getBytesUtf8(OCTET_STREAM_MIME),
                        /* payloadMessage= */ null));
        // Zero or one action record.
        records.add(
                NdefMessageUtils.createPlatformLocalRecord(
                        "act",
                        /* id= */ null,
                        ByteBuffer.allocate(1).put((byte) 0x01).array(),
                        /* payloadMessage= */ null));
        // Zero or more title record.
        records.add(
                NdefMessageUtils.createPlatformTextRecord(
                        /* id= */ null,
                        LANG_EN_US,
                        ENCODING_UTF8,
                        ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        // Zero or more icon record.
        records.add(
                NdefMessageUtils.createPlatformMimeRecord(
                        "image/png", /* id= */ null, new byte[8182]));
        // Other application-specific records, e.g. an external type record.
        records.add(
                NdefMessageUtils.createPlatformExternalRecord(
                        DUMMY_EXTERNAL_TYPE,
                        /* id= */ null,
                        ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT),
                        /* payloadMessage= */ null));

        android.nfc.NdefRecord[] ndefRecords = new android.nfc.NdefRecord[records.size()];
        records.toArray(ndefRecords);
        android.nfc.NdefMessage payloadMessage = new android.nfc.NdefMessage(ndefRecords);
        return new android.nfc.NdefRecord(
                android.nfc.NdefRecord.TNF_WELL_KNOWN,
                android.nfc.NdefRecord.RTD_SMART_POSTER,
                ApiCompatibilityUtils.getBytesUtf8(DUMMY_RECORD_ID),
                payloadMessage.toByteArray());
    }
}
